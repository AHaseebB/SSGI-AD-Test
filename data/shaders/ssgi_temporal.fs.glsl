#version 460 core

layout (location = 0) out vec4 oPixel;

layout (binding = 0) uniform sampler2D uCurrentFiltered;
layout (binding = 1) uniform sampler2D uHistory;
layout (binding = 2) uniform sampler2D uDepth;
layout (binding = 3) uniform sampler2D uNormal;

layout (location = 0) uniform vec2 uScreenSize;
layout (location = 1) uniform mat4 uInvProjection;
layout (location = 2) uniform mat4 uInvView;
layout (location = 3) uniform mat4 uPreviousViewProjection;
layout (location = 4) uniform float uTemporalAlpha;
layout (location = 5) uniform float uDepthThreshold;
layout (location = 6) uniform float uNormalThreshold;
layout (location = 7) uniform int uHasHistory;

vec3 ReconstructWorldPosition(vec2 uv, float depth)
{
    float z = depth * 2.0 - 1.0;
    vec4 clip = vec4(uv * 2.0 - 1.0, z, 1.0);
    vec4 viewPos = uInvProjection * clip;
    viewPos /= viewPos.w;
    vec4 worldPos = uInvView * viewPos;
    return worldPos.xyz;
}

vec2 ProjectToPreviousUV(vec3 worldPos)
{
    vec4 clip = uPreviousViewProjection * vec4(worldPos, 1.0);
    clip /= clip.w;
    return clip.xy * 0.5 + 0.5;
}

void main()
{
    vec2 uv = gl_FragCoord.xy / uScreenSize;

    vec3 current = texture(uCurrentFiltered, uv).rgb;
    float depth = texture(uDepth, uv).r;

    if (uHasHistory == 0 || depth >= 1.0)
    {
        oPixel = vec4(current, 1.0);
        return;
    }

    vec3 currentNormal = normalize(texture(uNormal, uv).rgb * 2.0 - 1.0);
    vec3 worldPos = ReconstructWorldPosition(uv, depth);

    vec2 prevUV = ProjectToPreviousUV(worldPos);

    if (prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0)
    {
        oPixel = vec4(current, 1.0);
        return;
    }

    vec3 history = texture(uHistory, prevUV).rgb;

    float prevDepthCurrentFrame = texture(uDepth, prevUV).r; // Basic validation using current frame data sampled at reprojected UV
    vec3 prevNormalCurrentFrame = normalize(texture(uNormal, prevUV).rgb * 2.0 - 1.0);

    float depthDiff = abs(prevDepthCurrentFrame - depth);
    float normalSimilarity = dot(currentNormal, prevNormalCurrentFrame);

    bool rejectHistory = false;

    if (depthDiff > uDepthThreshold)
    {
        rejectHistory = true;
    }

    if (normalSimilarity < uNormalThreshold)
    {
        rejectHistory = true;
    }

    if (rejectHistory)
    {
        oPixel = vec4(current, 1.0);
        return;
    }

    vec3 localMin = vec3(1e20);
    vec3 localMax = vec3(-1e20);

    vec2 texelSize = 1.0 / uScreenSize;

    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec2 offset = vec2(x, y) * texelSize;
            vec3 sampleColor = texture(uCurrentFiltered, uv + offset).rgb;

            localMin = min(localMin, sampleColor);
            localMax = max(localMax, sampleColor);
        }
    }

    vec3 historyClamped = clamp(history, localMin, localMax);
    vec3 accumulated = mix(current, historyClamped, uTemporalAlpha);
    oPixel = vec4(accumulated, 1.0);
}