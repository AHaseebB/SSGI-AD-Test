#version 460 core

layout (location = 0) out vec4 oPixel;

layout (binding = 0) uniform sampler2D uInput;
layout (binding = 1) uniform sampler2D uDepth;
layout (binding = 2) uniform sampler2D uNormal;

layout (location = 0) uniform vec2 uTexelSize;
layout (location = 1) uniform int uBlurRadius;
layout (location = 2) uniform float uDepthStrength;
layout (location = 3) uniform float uNormalPower;
layout (location = 4) uniform int uDenoiserMode;
layout (location = 5) uniform float uAdaptiveDepthScale;
layout (location = 6) uniform float uAdaptiveNormalScale;

float EstimateEdgeFactor(vec2 uv, vec2 texelSize)
{
    if (uDenoiserMode != 2) // Only calculate edge factor for adaptive denoising mode
    {
        return 0.0;
    }

    float centerDepth = texture(uDepth, uv).r;
    vec3 centerNormal = normalize(texture(uNormal, uv).rgb * 2.0 - 1.0);

    float depthVar = 0.0;
    float normalVar = 0.0;

    const vec2 offsets[4] = vec2[](
        vec2( 1,  0),
        vec2(-1,  0),
        vec2( 0,  1),
        vec2( 0, -1)
    );

    for (int i = 0; i < 4; ++i)
    {
        vec2 sampleUV = uv + offsets[i] * texelSize;
        float d = texture(uDepth, sampleUV).r;
        vec3 n = normalize(texture(uNormal, sampleUV).rgb * 2.0 - 1.0);

        depthVar += abs(d - centerDepth);
        normalVar += 1.0 - max(dot(centerNormal, n), 0.0);
    }

    depthVar *= 0.25;
    normalVar *= 0.25;

    float edgeFactor = clamp(depthVar * uAdaptiveDepthScale + normalVar * uAdaptiveNormalScale, 0.0, 1.0); // Combine depth and normal variance to get edge factor
    return edgeFactor;
}

void main()
{
    vec2 uv = gl_FragCoord.xy * uTexelSize;
    vec3 centerColor = texture(uInput, uv).rgb;
    if (uDenoiserMode == 0)
    {
        oPixel = vec4(centerColor, 1.0);
        return;
    }
    float centerDepth = texture(uDepth, uv).r;
    vec3 centerNormal = normalize(texture(uNormal, uv).rgb * 2.0 - 1.0);
    float edgeFactor = EstimateEdgeFactor(uv, uTexelSize);
    float blurAmount = 1.0 - edgeFactor;

    vec3 sum = vec3(0.0);
    float weightSum = 0.0;

    for (int y = -uBlurRadius; y <= uBlurRadius; ++y)
    {
        for (int x = -uBlurRadius; x <= uBlurRadius; ++x)
        {
            vec2 offset = vec2(x, y) * uTexelSize;
            vec2 sampleUV = uv + offset;

            vec3 sampleColor = texture(uInput, sampleUV).rgb;
            float sampleDepth = texture(uDepth, sampleUV).r;
            vec3 sampleNormal = normalize(texture(uNormal, sampleUV).rgb * 2.0 - 1.0);

            float spatialWeight = exp(-(x * x + y * y) / 8.0);

            float depthDiff = abs(sampleDepth - centerDepth);
            float depthWeight = exp(-depthDiff * uDepthStrength);

            float normalDiff = max(dot(centerNormal, sampleNormal), 0.0);
            float normalWeight = pow(normalDiff, uNormalPower);

            float weight = spatialWeight * depthWeight * normalWeight; // Base weight from spatial, depth, and normal similarity

            if (uDenoiserMode == 2) // If adaptive denoising is enabled, further modulate weight by blur amount based on edge factor
            {
                if (x == 0 && y == 0)
                {
                    weight = 1.0; // Center pixel should not be blurred, so keep its weight as is
                }
                else
                {
                    weight *= blurAmount; // Reduce weight for pixels near edges to preserve details, while allowing more blur in flat areas
                }
            }

            sum += sampleColor * weight;
            weightSum += weight;
        }
    }

    if (weightSum > 0.0) // Avoid division by zero
    {
        oPixel = vec4(sum / weightSum, 1.0); // Normalize by total weight
    }
    else
    {
        oPixel = vec4(centerColor, 1.0); // Fallback to center color if no weights
    }
}