#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <Project/ProjectApplication.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <filesystem>
#include <algorithm>
#include <iterator>
#include <fstream>
#include <vector>
#include <queue>
#include <set>
#include <Project/Camera.hpp>

static std::string Slurp(std::string_view path)
{
    std::ifstream file(path.data(), std::ios::binary | std::ios::ate);
    if (!file) return {};
    std::streamsize size = file.tellg();
    if (size <= 0) return {};
    std::string result(size, '\0');
    file.seekg(0);
    file.read(result.data(), size);
    return result;
}


static uint32_t LoadTexture2D(
    const std::string& texturePath,
    std::unordered_map<std::string, size_t>& textureIds,
    std::vector<uint32_t>& textures,
    bool srgb)
{
    const std::string cacheKey = texturePath + (srgb ? "|srgb" : "|linear");

    if (textureIds.contains(cacheKey))
    {
        return static_cast<uint32_t>(textureIds[cacheKey]);
    }

    uint32_t texture = 0;
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);

    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int32_t width = 0;
    int32_t height = 0;
    int32_t channels = STBI_rgb_alpha;
    stbi_uc* textureData = stbi_load(texturePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!textureData)
    {
        spdlog::error("Failed to load texture: {}", texturePath);
        return 0xFFFFFFFFu;;
    }

    const auto levels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
    const GLenum internalFormat = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;

    glTextureStorage2D(texture, levels, internalFormat, width, height);
    glTextureSubImage2D(texture, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, textureData);
    glGenerateTextureMipmap(texture);

    stbi_image_free(textureData);

    textures.emplace_back(texture);
    const uint32_t newIndex = static_cast<uint32_t>(textures.size() - 1);
    textureIds[cacheKey] = newIndex;
    return newIndex;
}

namespace fs = std::filesystem;

static std::string FindTexturePath(const fs::path& basePath, const cgltf_image* image)
{
    std::string texturePath;
    if (!image->uri)
    {
        auto newPath = basePath / image->name;
        if (!newPath.has_extension())
        {
            if (image->mime_type)
            {
                if (std::strcmp(image->mime_type, "image/png") == 0)
                {
                    newPath.replace_extension("png");
                }
                else if (std::strcmp(image->mime_type, "image/jpg") == 0 ||
                    std::strcmp(image->mime_type, "image/jpeg") == 0)
                {
                    newPath.replace_extension("jpg");
                }
            }
        }
        texturePath = newPath.generic_string();
    }
    else
    {
        texturePath = (basePath / image->uri).generic_string();
    }
    return texturePath;
}

void ProjectApplication::AfterCreatedUiContext()
{
}

void ProjectApplication::BeforeDestroyUiContext()
{
}

static uint32_t GetOrAddTextureSlot(RenderBatch& batch, uint32_t globalTextureIndex)
{
    auto it = std::find(batch.TextureIndices.begin(), batch.TextureIndices.end(), globalTextureIndex);
    if (it != batch.TextureIndices.end())
    {
        return static_cast<uint32_t>(std::distance(batch.TextureIndices.begin(), it));
    }

    batch.TextureIndices.push_back(globalTextureIndex);
    return static_cast<uint32_t>(batch.TextureIndices.size() - 1);
}

bool ProjectApplication::Load()
{
    if (!Application::Load())
    {
        spdlog::error("App: Unable to load");
        return false;
    }

    _camera.Set(
        glm::vec3(-7.0f, 0.5f, 0.0f),
        glm::vec3(-5.9f, 1.5f, 1.4f),
        glm::vec3(0.0f, 1.0f, 0.0f));

    int framebufferWidth = 1920;
    int framebufferHeight = 1080;
    if (GLFWwindow* window = glfwGetCurrentContext())
    {
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    }

    _windowWidth = framebufferWidth;
    _windowHeight = framebufferHeight;

    _camera.SetPerspectiveMatrix(glm::perspective(
        glm::radians(80.0f),
        static_cast<float>(_windowWidth) / static_cast<float>(_windowHeight),
        0.1f,
        1000.0f));

    _camera.SetOrthographicMatrix(glm::ortho(
        -10.0f, 10.0f,
        -10.0f, 10.0f,
        0.1f, 1000.0f));

    if (GLFWwindow* window = glfwGetCurrentContext())
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    if (!MakeShader("./data/shaders/main.vs.glsl", "./data/shaders/main.fs.glsl"))
    {
        return false;
    }

    //LoadModel("./data/models/SM_Deccer_Cubes_Textured.gltf");
    LoadModel("./data/models/Sponza/Sponza.gltf");

    return true;
}

void ProjectApplication::Update(float deltaTime)
{
    if (IsKeyPressed(GLFW_KEY_ESCAPE))
    {
        Close();
    }

    const bool fDown = IsKeyPressed(GLFW_KEY_F);
    if (fDown && !_fKeyWasDown)
    {
        _freecamEnabled = !_freecamEnabled;

        if (GLFWwindow* window = glfwGetCurrentContext())
        {
            glfwSetInputMode(
                window,
                GLFW_CURSOR,
                _freecamEnabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        }

        _camera.ResetMouseTracking();
    }
    _fKeyWasDown = fDown;

    if (_freecamEnabled)
    {
        _camera.RespondToMouseMovement();

        const float movement = _camera.GetSpeed() * deltaTime;

        if (IsKeyPressed(GLFW_KEY_W))
        {
            _camera.Advance(movement);
        }
        if (IsKeyPressed(GLFW_KEY_S))
        {
            _camera.Advance(-movement);
        }
        if (IsKeyPressed(GLFW_KEY_A))
        {
            _camera.Strafe(-movement);
        }
        if (IsKeyPressed(GLFW_KEY_D))
        {
            _camera.Strafe(movement);
        }
        if (IsKeyPressed(GLFW_KEY_SPACE))
        {
            _camera.MoveVertical(movement);
        }
        if (IsKeyPressed(GLFW_KEY_LEFT_CONTROL))
        {
            _camera.MoveVertical(-movement);
        }

        if (IsKeyPressed(GLFW_KEY_Q))
        {
            _camera.ChangeSpeed(1.0f);
        }
        else if (IsKeyPressed(GLFW_KEY_LEFT_SHIFT))
        {
            _camera.ChangeSpeed(8.0f);
        }
        else
        {
            _camera.ChangeSpeed(2.5f);
        }
    }

    _elapsedTime += deltaTime;
}

void ProjectApplication::OnFramebufferResized(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    _windowWidth = width;
    _windowHeight = height;

    _camera.SetPerspectiveMatrix(glm::perspective(
        glm::radians(80.0f),
        static_cast<float>(width) / static_cast<float>(height),
        0.1f,
        1000.0f));
}

void ProjectApplication::RenderScene([[maybe_unused]] float deltaTime)
{   
    /*
    const auto projection = glm::perspective(glm::radians(80.0f), 1280.0f / 720.0f, 0.1f, 1000.0f);
    const auto view = glm::lookAt(
        glm::vec3(3 * std::cos(glfwGetTime() / 4), 2, -3 * std::sin(glfwGetTime() / 4)),
        glm::vec3(0, 0, 0),
        glm::vec3(0, 1, 0));
    */
    const auto& projection = _camera.GetPerspectiveMatrix();
    const auto view = _camera.GetViewMatrix();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(_shaderProgram);
    glUniformMatrix4fv(0, 1, false, glm::value_ptr(projection));
    glUniformMatrix4fv(1, 1, false, glm::value_ptr(view));

    const glm::vec3 lightDirection = glm::normalize(glm::vec3(-0.6f, -1.0f, -0.4f));
    const glm::vec3 lightColor(1.0f, 1.0f, 1.0f);
    const float ambientStrength = 0.2f;
    glUniform3fv(18, 1, glm::value_ptr(lightDirection));
    glUniform3fv(19, 1, glm::value_ptr(lightColor));
    glUniform1f(20, ambientStrength);

    const glm::vec3 cameraPosition = _camera.GetPosition();
    glUniform3fv(21, 1, glm::value_ptr(cameraPosition));

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _cubes.TransformData);
    glBindVertexArray(_cubes.InputLayout);

    for (const auto& batch : _renderBatches)
    {
        if (batch.Commands.empty())
        {
            continue;
        }

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, batch.ObjectDataBuffer);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, batch.CommandBuffer);

        const uint32_t boundTextureCount = std::min<uint32_t>(
            static_cast<uint32_t>(batch.TextureIndices.size()),
            16u);

        for (uint32_t offset = 0; offset < boundTextureCount; ++offset)
        {
            glUniform1i(2 + offset, offset);
            glActiveTexture(GL_TEXTURE0 + offset);
            glBindTexture(GL_TEXTURE_2D, _cubes.Textures[batch.TextureIndices[offset]]);
        }

        glMultiDrawElementsIndirect(
            GL_TRIANGLES,
            GL_UNSIGNED_INT,
            nullptr,
            static_cast<GLsizei>(batch.Commands.size()),
            sizeof(MeshIndirectInfo));
    }
}

void ProjectApplication::RenderUI(float deltaTime)
{
    _fpsTimer += deltaTime;
    _fpsAccumulatedTime += deltaTime;
    _fpsFrameCount++;

    if (_fpsTimer >= 0.5f)
    {
        _cachedFPS = (deltaTime > 0.0f) ? (1.0f / deltaTime) : 0.0f;
        _cachedFrameTimeMs = deltaTime * 1000.0f;

        if (_fpsAccumulatedTime > 0.0f)
        {
            _cachedAverageFPS = static_cast<float>(_fpsFrameCount) / _fpsAccumulatedTime;
        }
        else
        {
            _cachedAverageFPS = 0.0f;
        }

        _fpsTimer = 0.0f;
        _fpsAccumulatedTime = 0.0f;
        _fpsFrameCount = 0;
    }

    ImGui::Begin("Window");
    {
        ImGui::TextUnformatted("Hello World!");
        ImGui::Text("Time in seconds since startup: %f", _elapsedTime);
        ImGui::Text("The delta time between frames: %f", deltaTime);
    
        const glm::vec3 pos = _camera.GetPosition();
        const glm::vec3 view = _camera.GetView();
        const glm::vec3 forward = _camera.GetForward();
        const glm::vec3 up = _camera.GetUpVector();
        const glm::vec3 right = _camera.GetStrafeVector();

        ImGui::Text("Position:  %.3f  %.3f  %.3f", pos.x, pos.y, pos.z);
        ImGui::Text("View:      %.3f  %.3f  %.3f", view.x, view.y, view.z);
        ImGui::Text("Forward:   %.3f  %.3f  %.3f", forward.x, forward.y, forward.z);
        ImGui::Text("Up:        %.3f  %.3f  %.3f", up.x, up.y, up.z);
        ImGui::Text("Right:     %.3f  %.3f  %.3f", right.x, right.y, right.z);
        ImGui::Separator();
        ImGui::Text("Speed: %.3f", _camera.GetSpeed());
        ImGui::Text("Freecam (press F key to toggle): %s", _freecamEnabled ? "ON" : "OFF");
		ImGui::SetWindowPos(ImVec2(0, 0));

        ImGui::Separator();
        ImGui::Text("Frame Time: %.3f ms", _cachedFrameTimeMs);
        ImGui::Text("FPS: %.1f", _cachedFPS);
        ImGui::Text("Average FPS (0.5s): %.1f", _cachedAverageFPS);

        float editablePos[3] = { pos.x, pos.y, pos.z };
        if (ImGui::DragFloat3("Position", editablePos, 0.1f))
        {
            const glm::vec3 newPos(editablePos[0], editablePos[1], editablePos[2]);
            _camera.Set(newPos, newPos + _camera.GetForward(), _camera.GetUpVector());
        }
        ImGui::End();
    }
    
    //ImGui::ShowDemoWindow();
}

bool ProjectApplication::MakeShader(std::string_view vertexShaderFilePath, std::string_view fragmentShaderFilePath)
{
    int success = false;
    char log[1024] = {};
    const auto vertexShaderSource = Slurp(vertexShaderFilePath);
    const char* vertexShaderSourcePtr = vertexShaderSource.c_str();
    const auto vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSourcePtr, nullptr);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 1024, nullptr, log);
        spdlog::error(log);
        return false;
    }

    const auto fragmentShaderSource = Slurp(fragmentShaderFilePath);
    const char* fragmentShaderSourcePtr = fragmentShaderSource.c_str();
    const auto fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSourcePtr, nullptr);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 1024, nullptr, log);
        spdlog::error(log);
        return false;
    }

    _shaderProgram = glCreateProgram();
    glAttachShader(_shaderProgram, vertexShader);
    glAttachShader(_shaderProgram, fragmentShader);
    glLinkProgram(_shaderProgram);
    glGetProgramiv(_shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(_shaderProgram, 1024, nullptr, log);
        spdlog::error(log);

        return false;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return true;
}

void ProjectApplication::LoadModel(std::string_view file)
{   
    _cubes.Meshes.clear();
    _cubes.Textures.clear();
    _cubes.Transforms.clear();
    _renderBatches.clear();

    // Read GLTF
    cgltf_options options = {};
    cgltf_data* model = nullptr;

    cgltf_result parseResult = cgltf_parse_file(&options, file.data(), &model);
    if (parseResult != cgltf_result_success || !model)
    {
        spdlog::error("Failed to parse glTF: {}", file.data());
        return;
    }

    cgltf_result loadResult = cgltf_load_buffers(&options, model, file.data());
    if (loadResult != cgltf_result_success)
    {
        spdlog::error("Failed to load glTF buffers: {}", file.data());
        cgltf_free(model);
        return;
    }
    fs::path path(file.data());
    const auto basePath = path.parent_path();
    std::unordered_map<std::string, size_t> textureIds;
    _cubes.Textures.reserve(model->materials_count * 3);
    const std::string whitePath = "./data/models/Sponza/white.png";
    _whiteTextureIndex = LoadTexture2D(
        whitePath,
        textureIds,
        _cubes.Textures,
        true // base colour to sRGB
    );
    
    for (uint32_t i = 0; i < model->materials_count; ++i)
    {
        const auto& material = model->materials[i];

        if (material.has_pbr_metallic_roughness &&
            material.pbr_metallic_roughness.base_color_texture.texture != nullptr)
        {
            const auto* image = material.pbr_metallic_roughness.base_color_texture.texture->image;
            const auto texturePath = FindTexturePath(basePath, image);
            LoadTexture2D(texturePath, textureIds, _cubes.Textures, true);
        }

        if (material.normal_texture.texture != nullptr)
        {
            const auto* image = material.normal_texture.texture->image;
            const auto texturePath = FindTexturePath(basePath, image);
            LoadTexture2D(texturePath, textureIds, _cubes.Textures, false);
        }

        if (material.has_pbr_metallic_roughness &&
            material.pbr_metallic_roughness.metallic_roughness_texture.texture != nullptr)
        {
            const auto* image = material.pbr_metallic_roughness.metallic_roughness_texture.texture->image;
            const auto texturePath = FindTexturePath(basePath, image);
            LoadTexture2D(texturePath, textureIds, _cubes.Textures, false);
        }
        if (material.occlusion_texture.texture != nullptr)
        {
            const auto* image = material.occlusion_texture.texture->image;
            const auto texturePath = FindTexturePath(basePath, image);
            LoadTexture2D(texturePath, textureIds, _cubes.Textures, false);
        }
    }

    uint32_t transformIndex = 0;
    size_t vertexOffset = 0;
    size_t indexOffset = 0;
    std::vector<MeshCreateInfo> meshCreateInfos;
    meshCreateInfos.reserve(1024);
    for (uint32_t i = 0; i < model->scene->nodes_count; ++i)
    {
        std::queue<cgltf_node*> nodes;
        nodes.push(model->scene->nodes[i]);
        while (!nodes.empty())
        {
            const auto* node = nodes.front();
            nodes.pop();
            if (!node->mesh)
            {
                for (uint32_t j = 0; j < node->children_count; ++j)
                {
                    nodes.push(node->children[j]);
                }
                continue;
            }
            for (uint32_t j = 0; j < node->mesh->primitives_count; ++j)
            {
                const auto& primitive = node->mesh->primitives[j];
                const glm::vec3* positionPtr = nullptr;
                const glm::vec3* normalPtr = nullptr;
                const glm::vec2* uvPtr = nullptr;
                const glm::vec4* tangentPtr = nullptr;
                uint64_t vertexCount = 0;
                // Vertices
                for (uint32_t k = 0; k < primitive.attributes_count; ++k)
                {
                    const auto& attribute = primitive.attributes[k];
                    const auto* accessor = attribute.data;
                    const auto* view = accessor->buffer_view;
                    const auto* dataPtr = (const char*)view->buffer->data;
                    switch (attribute.type)
                    {
                        case cgltf_attribute_type_position:
                            vertexCount = accessor->count;
                            positionPtr = (const glm::vec3*)(dataPtr + view->offset + accessor->offset);
                            break;

                        case cgltf_attribute_type_normal:
                            normalPtr = (const glm::vec3*)(dataPtr + view->offset + accessor->offset);
                            break;

                        case cgltf_attribute_type_texcoord:
                            uvPtr = (const glm::vec2*)(dataPtr + view->offset + accessor->offset);
                            break;

                        case cgltf_attribute_type_tangent:
                            tangentPtr = (const glm::vec4*)(dataPtr + view->offset + accessor->offset);
                            break;

                        default: break;
                    }
                }
                std::vector<Vertex> vertices;
                vertices.resize(vertexCount);
                {
                    auto* ptr = vertices.data();
                    for (uint32_t v = 0; v < vertexCount; ++v, ++ptr)
                    {
                        if (positionPtr)
                        {
                            std::memcpy(&ptr->Position, positionPtr + v, sizeof(glm::vec3));
                        }
                        if (normalPtr)
                        {
                            std::memcpy(&ptr->Normal, normalPtr + v, sizeof(glm::vec3));
                        }
                        if (uvPtr)
                        {
                            std::memcpy(&ptr->Uv, uvPtr + v, sizeof(glm::vec2));
                        }
                        if (tangentPtr)
                        {
                            std::memcpy(&ptr->Tangent, tangentPtr + v, sizeof(glm::vec4));
                        }
                    }
                }

                std::vector<uint32_t> indices;
                {
                    const auto* accessor = primitive.indices;
                    const auto* view = accessor->buffer_view;
                    const char* dataPtr = (const char*)view->buffer->data;
                    indices.reserve(accessor->count);
                    switch (accessor->component_type)
                    {
                        case cgltf_component_type_r_8:
                        case cgltf_component_type_r_8u:
                        {
                            const auto* ptr = (const uint8_t*)(dataPtr + view->offset + accessor->offset);
                            std::copy(ptr, ptr + accessor->count, std::back_inserter(indices));
                        } break;

                        case cgltf_component_type_r_16:
                        case cgltf_component_type_r_16u:
                        {
                            const auto* ptr = (const uint16_t*)(dataPtr + view->offset + accessor->offset);
                            std::copy(ptr, ptr + accessor->count, std::back_inserter(indices));
                        } break;

                        case cgltf_component_type_r_32f:
                        case cgltf_component_type_r_32u:
                        {
                            const auto* ptr = (const uint32_t*)(dataPtr + view->offset + accessor->offset);
                            std::copy(ptr, ptr + accessor->count, std::back_inserter(indices));
                        } break;

                        default: break;
                    }
                }
                std::string baseColorURI = "";
                if (primitive.material && primitive.material->pbr_metallic_roughness.base_color_texture.texture != nullptr)
                {
                    baseColorURI = FindTexturePath(basePath, primitive.material->pbr_metallic_roughness.base_color_texture.texture->image);
                }
                else
                {
                    baseColorURI = "";
                }

                std::string normalURI = "";
                if (primitive.material && primitive.material->normal_texture.texture != nullptr)
                {
                    normalURI = FindTexturePath(basePath, primitive.material->normal_texture.texture->image);
                }

                std::string metallicRoughnessURI = "";
                float metallicFactor = 1.0f;
                float roughnessFactor = 1.0f;
                if (primitive.material && primitive.material->has_pbr_metallic_roughness)
                {
                    metallicFactor = primitive.material->pbr_metallic_roughness.metallic_factor;
                    roughnessFactor = primitive.material->pbr_metallic_roughness.roughness_factor;
					//spdlog::info("1: metallic factor = {}, roughness factor = {}", metallicFactor, roughnessFactor);
                    if (primitive.material && primitive.material->pbr_metallic_roughness.metallic_roughness_texture.texture != nullptr)
                    {
                        metallicRoughnessURI = FindTexturePath(basePath, primitive.material->pbr_metallic_roughness.metallic_roughness_texture.texture->image);
						//spdlog::info("2: metallic-roughness texture: {}", metallicRoughnessURI);
                    }
                }

                std::string occlusionURI = "";
                if (primitive.material && primitive.material->occlusion_texture.texture != nullptr)
                {
                    occlusionURI = FindTexturePath(basePath, primitive.material->occlusion_texture.texture->image);
                }

                const auto indexCount = indices.size();
                meshCreateInfos.emplace_back(MeshCreateInfo
                    {
                        std::move(vertices),
                        std::move(indices),
                        transformIndex++,
                        baseColorURI.empty() ? _whiteTextureIndex : static_cast<uint32_t>(textureIds[baseColorURI + "|srgb"]),
                        normalURI.empty() ? 0xFFFFFFFFu : static_cast<uint32_t>(textureIds[normalURI + "|linear"]),
                        metallicRoughnessURI.empty() ? 0xFFFFFFFFu : static_cast<uint32_t>(textureIds[metallicRoughnessURI + "|linear"]),
                        occlusionURI.empty() ? 0xFFFFFFFFu : static_cast<uint32_t>(textureIds[occlusionURI + "|linear"]),
                        metallicFactor,
                        roughnessFactor,
                        vertexOffset,
                        indexOffset,
                        _cubes.VertexBuffer,
                        _cubes.IndexBuffer
                    });
                cgltf_node_transform_world(node, glm::value_ptr(_cubes.Transforms.emplace_back()));
                vertexOffset += vertexCount * sizeof(Vertex);
                indexOffset += indexCount * sizeof(uint32_t);
            }
            for (uint32_t j = 0; j < node->children_count; ++j)
            {
                nodes.push(node->children[j]);
            }
        }
    }

    // Allocate GL buffers
    glCreateVertexArrays(1, &_cubes.InputLayout);
    glCreateBuffers(1, &_cubes.VertexBuffer);
    glCreateBuffers(1, &_cubes.IndexBuffer);
    glCreateBuffers(1, &_cubes.TransformData);

    glNamedBufferData(
        _cubes.TransformData,
        _cubes.Transforms.size() * sizeof(glm::mat4),
        _cubes.Transforms.data(),
        GL_STATIC_DRAW);

    size_t vertexSize = 0;
    size_t indexSize = 0;
    for (const auto& meshCreateInfo : meshCreateInfos)
    {
        vertexSize += meshCreateInfo.Vertices.size() * sizeof(Vertex);
        indexSize += meshCreateInfo.Indices.size() * sizeof(uint32_t);
    }

    glNamedBufferStorage(_cubes.VertexBuffer, vertexSize, nullptr, GL_DYNAMIC_STORAGE_BIT);
    glNamedBufferStorage(_cubes.IndexBuffer, indexSize, nullptr, GL_DYNAMIC_STORAGE_BIT);

    glVertexArrayVertexBuffer(_cubes.InputLayout, 0, _cubes.VertexBuffer, 0, sizeof(Vertex));
    glVertexArrayElementBuffer(_cubes.InputLayout, _cubes.IndexBuffer);

    glEnableVertexArrayAttrib(_cubes.InputLayout, 0);
    glEnableVertexArrayAttrib(_cubes.InputLayout, 1);
    glEnableVertexArrayAttrib(_cubes.InputLayout, 2);
    glEnableVertexArrayAttrib(_cubes.InputLayout, 3);

    glVertexArrayAttribFormat(_cubes.InputLayout, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, Position));
    glVertexArrayAttribFormat(_cubes.InputLayout, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, Normal));
    glVertexArrayAttribFormat(_cubes.InputLayout, 2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, Uv));
    glVertexArrayAttribFormat(_cubes.InputLayout, 3, 4, GL_FLOAT, GL_FALSE, offsetof(Vertex, Tangent));

    glVertexArrayAttribBinding(_cubes.InputLayout, 0, 0);
    glVertexArrayAttribBinding(_cubes.InputLayout, 1, 0);
    glVertexArrayAttribBinding(_cubes.InputLayout, 2, 0);
    glVertexArrayAttribBinding(_cubes.InputLayout, 3, 0);

    for (auto& info : meshCreateInfos)
    {
        info.VertexBuffer = _cubes.VertexBuffer;
        info.IndexBuffer = _cubes.IndexBuffer;
        glNamedBufferSubData(
            info.VertexBuffer,
            info.VertexOffset,
            info.Vertices.size() * sizeof(Vertex),
            info.Vertices.data());
        glNamedBufferSubData(
            info.IndexBuffer,
            info.IndexOffset,
            info.Indices.size() * sizeof(uint32_t),
            info.Indices.data());
        _cubes.Meshes.emplace_back(Mesh
            {
                (uint32_t)info.Indices.size(),
                (int32_t)(info.VertexOffset / sizeof(Vertex)),
                (uint32_t)(info.IndexOffset / sizeof(uint32_t)),
                info.TransformIndex,
                info.BaseColorTexture,
                info.NormalTexture,
                info.MetallicRoughnessTexture,
                info.OcclusionTexture,
                info.MetallicFactor,
                info.RoughnessFactor
            });
    }

    _renderBatches.clear();
    _renderBatches.emplace_back();

    for (const auto& mesh : _cubes.Meshes)
    {
        auto meshTextureIndices = std::vector<uint32_t>{};
        meshTextureIndices.push_back(mesh.BaseColorTexture);

        if (mesh.NormalTexture != 0xFFFFFFFFu)
        {
            if (std::find(meshTextureIndices.begin(), meshTextureIndices.end(), mesh.NormalTexture) == meshTextureIndices.end())
            {
                meshTextureIndices.push_back(mesh.NormalTexture);
            }
        }

        if (mesh.MetallicRoughnessTexture != 0xFFFFFFFFu)
        {
            if (std::find(meshTextureIndices.begin(), meshTextureIndices.end(), mesh.MetallicRoughnessTexture) == meshTextureIndices.end())
            {
                meshTextureIndices.push_back(mesh.MetallicRoughnessTexture);
            }
        }

        if (mesh.OcclusionTexture != 0xFFFFFFFFu)
        {
            if (std::find(meshTextureIndices.begin(), meshTextureIndices.end(), mesh.OcclusionTexture) == meshTextureIndices.end())
            {
                meshTextureIndices.push_back(mesh.OcclusionTexture);
            }
        }

        RenderBatch* batch = &_renderBatches.back();

        std::vector<uint32_t> candidateTextureList = batch->TextureIndices;
        for (uint32_t tex : meshTextureIndices)
        {
            if (std::find(candidateTextureList.begin(), candidateTextureList.end(), tex) == candidateTextureList.end())
            {
                candidateTextureList.push_back(tex);
            }
        }

        if (candidateTextureList.size() > 16)
        {
            _renderBatches.emplace_back();
            batch = &_renderBatches.back();
        }

        const uint32_t localBaseColorSlot = GetOrAddTextureSlot(*batch, mesh.BaseColorTexture);

        uint32_t localNormalSlot = 0xFFFFFFFFu;
        if (mesh.NormalTexture != 0xFFFFFFFFu)
        {
            localNormalSlot = GetOrAddTextureSlot(*batch, mesh.NormalTexture);
        }

        uint32_t localMetallicRoughnessSlot = 0xFFFFFFFFu;
        if (mesh.MetallicRoughnessTexture != 0xFFFFFFFFu)
        {
            localMetallicRoughnessSlot = GetOrAddTextureSlot(*batch, mesh.MetallicRoughnessTexture);
        }

        uint32_t localOcclusionSlot = 0xFFFFFFFFu;
        if (mesh.OcclusionTexture != 0xFFFFFFFFu)
        {
            localOcclusionSlot = GetOrAddTextureSlot(*batch, mesh.OcclusionTexture);
        }

        batch->Commands.emplace_back(MeshIndirectInfo{
            mesh.IndexCount,
            1,
            mesh.indexOffset,
            mesh.VertexOffset,
            1
            });

        batch->Objects.emplace_back(RenderObjectData{
            mesh.TransformIndex,
            localBaseColorSlot,
            localNormalSlot,
            localMetallicRoughnessSlot,
			localOcclusionSlot,
            mesh.MetallicFactor,
            mesh.RoughnessFactor
            });
    }

    for (auto& batch : _renderBatches)
    {
        glCreateBuffers(1, &batch.ObjectDataBuffer);
        glNamedBufferData(
            batch.ObjectDataBuffer,
            batch.Objects.size() * sizeof(RenderObjectData),
            batch.Objects.data(),
            GL_STATIC_DRAW);

        glCreateBuffers(1, &batch.CommandBuffer);
        glNamedBufferData(
            batch.CommandBuffer,
            batch.Commands.size() * sizeof(MeshIndirectInfo),
            batch.Commands.data(),
            GL_STATIC_DRAW);
    }

    for (size_t i = 0; i < _renderBatches.size(); ++i)
    {
        if (_renderBatches[i].TextureIndices.size() > 16)
        {
            spdlog::warn("Batch {} uses {} textures, exceeding 16 texture units", i, _renderBatches[i].TextureIndices.size());
        }
    }
}
