#version 460 core

layout (location = 0) out vec4 oPixel;

layout (binding = 0) uniform sampler2D uInput;
layout (binding = 1) uniform sampler2D uDepth;
layout (binding = 2) uniform sampler2D uNormal;

layout (location = 0) uniform vec2 uTexelSize;
layout (location = 1) uniform int uBlurRadius;
layout (location = 2) uniform float uDepthStrength;
layout (location = 3) uniform float uNormalPower;

void main()
{
    vec2 uv = gl_FragCoord.xy * uTexelSize;

    vec3 centerColor = texture(uInput, uv).rgb;
    float centerDepth = texture(uDepth, uv).r;
    vec3 centerNormal = normalize(texture(uNormal, uv).rgb * 2.0 - 1.0);

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

            float weight = spatialWeight * depthWeight * normalWeight;

            sum += sampleColor * weight;
            weightSum += weight;
        }
    }

    if (weightSum > 0.0)
    {
        oPixel = vec4(sum / weightSum, 1.0);
    }
    else
    {
        oPixel = vec4(centerColor, 1.0);
    }
}