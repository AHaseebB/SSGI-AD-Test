/*
#version 460 core

layout (location = 0) out vec4 oPixel;

layout (location = 0) in vec2 iUvs;
layout (location = 1) in flat uint iBaseColorIndex;

layout (location = 2) uniform sampler2D[16] uTextures;

void main()
{
    oPixel = vec4(texture(uTextures[iBaseColorIndex], iUvs).rgb, 1.0);
}
*/

#version 460 core

layout (location = 0) out vec4 oPixel;

layout (location = 0) in vec2 iUvs;
layout (location = 1) in flat uint iBaseColorIndex;
layout (location = 2) in vec3 iWorldNormal;

layout (location = 2) uniform sampler2D uTextures[16];
layout (location = 18) uniform vec3 uLightDirection;
layout (location = 19) uniform vec3 uLightColor;
layout (location = 20) uniform float uAmbientStrength;

void main()
{
    vec4 texel = texture(uTextures[iBaseColorIndex], iUvs);

    if (texel.a < 0.5)
    {
        discard;
    }

    vec3 N = normalize(iWorldNormal);
    vec3 L = normalize(-uLightDirection);

    float NdotL = max(dot(N, L), 0.0);

    vec3 ambient = uAmbientStrength * texel.rgb;
    vec3 diffuse = NdotL * uLightColor * texel.rgb;

    vec3 finalColor = ambient + diffuse;

    oPixel = vec4(finalColor, texel.a);
}