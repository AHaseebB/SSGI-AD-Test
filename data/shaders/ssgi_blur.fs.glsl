#version 460 core

layout (location = 0) out vec4 oPixel;

layout (binding = 0) uniform sampler2D uInput;
layout (binding = 1) uniform sampler2D uDepth;
layout (location = 0) uniform vec2 uTexelSize;

void main()
{
    vec2 uv = gl_FragCoord.xy * uTexelSize;
    float centerDepth = texture(uDepth, uv).r;

    vec3 sum = vec3(0.0);
    float weightSum = 0.0;

    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec2 offset = vec2(x, y) * uTexelSize;
            vec2 sampleUV = uv + offset;

            float sampleDepth = texture(uDepth, sampleUV).r;
            float depthDiff = abs(sampleDepth - centerDepth);

            float w = depthDiff < 0.002 ? 1.0 : 0.0;

            sum += texture(uInput, sampleUV).rgb * w;
            weightSum += w;
        }
    }

    if (weightSum > 0.0)
    {
        oPixel = vec4(sum / weightSum, 1.0);
    }
    else
    {
        oPixel = vec4(texture(uInput, uv).rgb, 1.0);
    }
}