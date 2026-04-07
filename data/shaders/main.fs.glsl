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
layout (location = 2) in flat uint iNormalIndex;
layout (location = 3) in vec3 iWorldNormal;
layout (location = 4) in vec4 iWorldTangent;
layout (location = 5) in vec3 iWorldPosition;
layout (location = 6) in flat uint iMetallicRoughnessIndex;
layout (location = 7) in flat float iMetallicFactor;
layout (location = 8) in flat float iRoughnessFactor;
layout (location = 9) in flat uint iOcclusionIndex;

layout (location = 2) uniform sampler2D uTextures[16];
layout (location = 18) uniform vec3 uLightDirection;
layout (location = 19) uniform vec3 uLightColor;
layout (location = 20) uniform float uAmbientStrength;
layout (location = 21) uniform vec3 uCameraPosition;

vec3 GetShadedNormal()
{
    vec3 N = normalize(iWorldNormal);

    if (iNormalIndex == 0xFFFFFFFFu)
    {
        return N;
    }

    vec3 T = normalize(iWorldTangent.xyz);
    vec3 B = normalize(cross(N, T) * iWorldTangent.w);
    mat3 TBN = mat3(T, B, N);

    vec3 normalSample = texture(uTextures[iNormalIndex], iUvs).xyz;
    normalSample = normalSample * 2.0 - 1.0;

    return normalize(TBN * normalSample);
}

void main()
{
    vec4 texel = texture(uTextures[iBaseColorIndex], iUvs);

    if (texel.a < 0.5)
    {
        discard;
    }

    vec3 N = GetShadedNormal();
    vec3 L = normalize(-uLightDirection);
    vec3 V = normalize(uCameraPosition - iWorldPosition);
    vec3 H = normalize(L + V);

    float metallic = iMetallicFactor;
    float roughness = iRoughnessFactor;

    if (iMetallicRoughnessIndex != 0xFFFFFFFFu)
    {
        vec4 mr = texture(uTextures[iMetallicRoughnessIndex], iUvs);
        roughness *= mr.g;
        metallic *= mr.b;
    }

    float shininess = mix(128.0, 4.0, clamp(roughness, 0.0, 1.0));

    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    float occlusion = 0.5;
    if (iOcclusionIndex != 0xFFFFFFFFu)
    {
        occlusion = texture(uTextures[iOcclusionIndex], iUvs).r;
    }

    vec3 ambient = uAmbientStrength * texel.rgb * occlusion;
    vec3 diffuse = NdotL * uLightColor * texel.rgb;

    float spec = pow(NdotH, shininess);
    vec3 specularColor = mix(vec3(0.04), texel.rgb, metallic);
    vec3 specular = spec * specularColor * uLightColor;

    vec3 finalColor = ambient + diffuse + specular;

    /*if (iMetallicRoughnessIndex == 0xFFFFFFFFu) // Debugging: highlight objects without metallic-roughness texture
    {
        oPixel = vec4(1.0, 0.0, 1.0, 1.0);
        return;
    }*/

    oPixel = vec4(finalColor, texel.a);

}