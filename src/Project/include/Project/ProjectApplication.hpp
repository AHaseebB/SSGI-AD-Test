#pragma once

#include <Project.Library/Application.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include <string_view>
#include <vector>
#include <memory>
#include <Project/Camera.hpp>

struct Vertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 Uv;
    glm::vec4 Tangent;
};

struct MeshIndirectInfo
{
    uint32_t Count;
    uint32_t InstanceCount;
    uint32_t FirstIndex;
    int32_t BaseVertex;
    uint32_t BaseInstance;
};

struct MeshCreateInfo
{
    std::vector<Vertex> Vertices;
    std::vector<uint32_t> Indices;
    uint32_t TransformIndex;
    uint32_t BaseColorTexture;
    uint32_t NormalTexture;
    uint32_t MetallicRoughnessTexture;
    //uint32_t OcclusionTexture;
    float MetallicFactor;
    float RoughnessFactor;
    size_t VertexOffset;
    size_t IndexOffset;
    uint32_t VertexBuffer;
    uint32_t IndexBuffer;
};

struct Mesh
{
    uint32_t IndexCount = 0;
    int32_t VertexOffset = 0;
    uint32_t indexOffset = 0;
    uint32_t TransformIndex = 0;
    uint32_t BaseColorTexture = 0;
    uint32_t NormalTexture = 0;
    uint32_t MetallicRoughnessTexture = 0xFFFFFFFFu;
    //uint32_t OcclusionTexture = 0xFFFFFFFFu;
    float MetallicFactor = 1.0f;
    float RoughnessFactor = 1.0f;
};

struct Model
{
    std::vector<Mesh> Meshes;
    std::vector<uint32_t> Textures;
    std::vector<glm::mat4> Transforms;
    uint32_t InputLayout;
    uint32_t VertexBuffer;
    uint32_t IndexBuffer;
    uint32_t TransformData;
};

struct RenderObjectData
{
    uint32_t TransformIndex;
    uint32_t BaseColorIndex;
    uint32_t NormalIndex;
    uint32_t MetallicRoughnessIndex;
    //uint32_t OcclusionIndex;
    float MetallicFactor;
    float RoughnessFactor;
};

struct RenderBatch
{
    std::vector<RenderObjectData> Objects;
    std::vector<MeshIndirectInfo> Commands;
    std::vector<uint32_t> TextureIndices; // indices into _cubes.Textures

    uint32_t ObjectDataBuffer = 0;
    uint32_t CommandBuffer = 0;
};

class ProjectApplication final : public Application
{
protected:
    void AfterCreatedUiContext() override;
    void BeforeDestroyUiContext() override;
    bool Load() override;
    void RenderScene(float deltaTime) override;
    void RenderUI(float deltaTime) override;
    void Update(float deltaTime) override;
    void CreateGBuffer(int width, int height);
    void DestroyGBuffer();
    void CreateFullscreenTriangle();

private:
    Model _cubes;
    uint32_t _shaderProgram;
    Camera _camera;
    uint32_t _ssgiTestProgram = 0;

    bool _freecamEnabled = true;
    int _windowWidth = 1920;
    int _windowHeight = 1080;
    bool _fKeyWasDown = false;
    float _elapsedTime = 0.0f;
    float _fpsTimer = 0.0f;
    float _fpsAccumulatedTime = 0.0f;
    uint32_t _fpsFrameCount = 0;
    float _cachedFPS = 0.0f;
    float _cachedAverageFPS = 0.0f;
    float _cachedFrameTimeMs = 0.0f;

    //bool MakeShader(std::string_view vertexShaderFilePath, std::string_view fragmentShaderFilePath);
    uint32_t MakeShader(std::string_view vertexShaderFilePath, std::string_view fragmentShaderFilePath);
    void LoadModel(std::string_view filePath);
    std::vector<RenderBatch> _renderBatches;
    uint32_t _whiteTextureIndex = 0xFFFFFFFFu;

    uint32_t _gBufferFbo = 0;
    uint32_t _gAlbedoTexture = 0;
    uint32_t _gNormalTexture = 0;
    uint32_t _gMaterialTexture = 0;
    uint32_t _gDepthTexture = 0;

    uint32_t _geometryProgram = 0;
	uint32_t _lightingProgram = 0;

    uint32_t _fullscreenVao = 0;

    int _debugView = 0; // 0 = lit, 1 = albedo, 2 = normal, 3 = material, 4 = depth

public:
    void OnFramebufferResized(int width, int height) override;
};