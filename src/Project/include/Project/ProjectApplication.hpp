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
    void CreateSsgiTargets(int width, int height);
    void DestroySsgiTargets();
    void CreateTemporalTargets(int width, int height);
    void DestroyTemporalTargets();
    void CreateShadowMap();
    void DestroyShadowMap();
    glm::mat4 ComputeLightViewProjection(const glm::vec3& lightDirection) const;

private:
    Model _cubes;
    uint32_t _shaderProgram;
    Camera _camera;

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

    uint32_t _ssgiRawFbo = 0;
    uint32_t _ssgiRawTexture = 0;

    uint32_t _ssgiFilteredFbo = 0;
    uint32_t _ssgiFilteredTexture = 0;

    uint32_t _ssgiGenerateProgram = 0;
    uint32_t _ssgiBlurProgram = 0;
    uint32_t _ssgiCompositeProgram = 0;

    float _ssgiIntensity = 0.5f;
    float _ssgiStepSize = 0.1f;
    int _ssgiMaxSteps = 64;
    float _ssgiThickness = 0.1f;
    int _blurRadius = 2;
    float _blurDepthStrength = 200.0f;
    float _blurNormalPower = 32.0f;
    float _adaptiveDepthScale = 1000.0f;
    float _adaptiveNormalScale = 8.0f;
    bool _useAdaptiveDenoiser = true;
    int _denoiserMode = 2; // 0 = raw, 1 = fixed, 2 = adaptive

    glm::mat4 _previousViewProjection = glm::mat4(1.0f);
    bool _hasHistory = false;
	uint32_t _ssgiHistoryFbo = 0; // keep the history of the raw SSGI results to do temporal reprojection for the adaptive denoiser
    uint32_t _ssgiHistoryTexture = 0;
    uint32_t _ssgiTemporalProgram = 0;
    uint32_t _ssgiTemporalFbo = 0;
    uint32_t _ssgiTemporalTexture = 0;
    float _temporalAlpha = 0.7f;
    float _temporalDepthThreshold = 0.005f;
    float _temporalNormalThreshold = 0.9f;
    bool _useAdaptiveTemporal = true;
    bool _enableSsgi = true;
    bool _enableSpatialFilter = true;
    bool _enableTemporal = true;

    uint32_t _shadowProgram = 0;
    uint32_t _shadowMapFbo = 0;
    uint32_t _shadowMapTexture = 0;
    int _shadowMapSize = 4096;
    bool _enableShadows = true;
    float _shadowBiasMin = 0.0008f;
    float _shadowBiasMax = 0.0060f;
    glm::mat4 _lightViewProjection = glm::mat4(1.0f);
	glm::vec3 _lightDirection = glm::normalize(glm::vec3(-0.6f, -1.0f, -0.4f));
    float _sceneRadius = 30.0f;
	float _lightdistance = 30.0f;
    int _debugView = 0; // 0 = final shaded result, 1 = albedo, 2 = normals, 3 = metallic/roughness, 4 = depth, 5 = world position, 6 = raw SSGI, 7 = spatial filtered SSGI, 8 = temporal accumulated SSGI

public:
	void OnFramebufferResized(int width, int height) override; // overridden to resize G-buffer and SSGI targets when the window is resized
};