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
layout (location = 8) uniform int uUseAdaptiveTemporal;

vec3 ReconstructWorldPosition(vec2 uv, float depth) // Reconstruct world position from screen UV and depth using inverse projection and view matrices
{
    float z = depth * 2.0 - 1.0;
    vec4 clip = vec4(uv * 2.0 - 1.0, z, 1.0);
    vec4 viewPos = uInvProjection * clip;
    viewPos /= viewPos.w;
    vec4 worldPos = uInvView * viewPos;
    return worldPos.xyz;
}

vec2 ProjectToPreviousUV(vec3 worldPos) // Project world position to previous frame's clip space and convert to UV coordinates for sampling history texture
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

    if (uHasHistory == 0 || depth >= 1.0) // If no history is available or pixel is at max depth, output current frame color directly without blending
    {
        oPixel = vec4(current, 1.0);
        return;
    }

    vec3 currentNormal = normalize(texture(uNormal, uv).rgb * 2.0 - 1.0);
    vec3 worldPos = ReconstructWorldPosition(uv, depth);

    vec2 prevUV = ProjectToPreviousUV(worldPos);

    if (prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0) // If reprojected UV is outside of screen bounds, reject history and output current frame color directly to avoid sampling invalid data from history texture
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

    if (depthDiff > uDepthThreshold) // If depth difference exceeds threshold, reject history to prevent ghosting from disoccluded or newly revealed areas
    {
        rejectHistory = true;
    }

    if (normalSimilarity < uNormalThreshold) // If normal similarity is below threshold, reject history to prevent ghosting from areas with significant orientation changes (e.g. edges or corners)
    {
        rejectHistory = true;
    }

    if (rejectHistory) // If history is rejected based on depth and normal checks, output current frame color directly without blending to avoid ghosting artifacts
    {
        oPixel = vec4(current, 1.0);
        return;
    }

    vec3 localMin = vec3(1e20);
    vec3 localMax = vec3(-1e20);

    vec2 texelSize = 1.0 / uScreenSize;

    for (int y = -1; y <= 1; ++y) // Iterate over a 3x3 neighborhood around the current pixel to find local minimum and maximum colors in the current filtered frame, which will be used to clamp the history color to prevent excessive ghosting from outliers in the history data
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec2 offset = vec2(x, y) * texelSize;
            vec3 sampleColor = texture(uCurrentFiltered, uv + offset).rgb;

            localMin = min(localMin, sampleColor);
            localMax = max(localMax, sampleColor);
        }
    }
    float alphaToUse = uTemporalAlpha;

    if (uUseAdaptiveTemporal != 0) // If adaptive temporal blending is enabled, modulate the temporal alpha based on depth and normal similarity to allow more blending in stable areas while reducing blending in areas with significant changes to prevent ghosting artifacts
    {
        float depthFactor = clamp(1.0 - depthDiff / uDepthThreshold, 0.0, 1.0);
        float normalFactor = clamp((normalSimilarity - uNormalThreshold) / (1.0 - uNormalThreshold), 0.0, 1.0);
        float stability = depthFactor * normalFactor;
        alphaToUse = uTemporalAlpha * stability;
    }

    vec3 historyClamped = clamp(history, localMin, localMax);
    vec3 accumulated = mix(current, historyClamped, alphaToUse); // Clamp history to local neighborhood to prevent excessive ghosting, then blend with current frame using adaptive alpha
    oPixel = vec4(accumulated, 1.0);
}