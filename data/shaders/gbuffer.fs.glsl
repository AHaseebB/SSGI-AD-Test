#version 460 core

layout (location = 0) out vec4 gAlbedo;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gMaterial;

layout (location = 0) in vec2 iUvs;
layout (location = 1) in flat uint iBaseColorIndex;
layout (location = 2) in flat uint iNormalIndex;
layout (location = 3) in flat uint iMetallicRoughnessIndex;
layout (location = 4) in vec3 iWorldNormal;
layout (location = 5) in vec4 iWorldTangent;
layout (location = 6) in flat float iMetallicFactor;
layout (location = 7) in flat float iRoughnessFactor;

layout (location = 2) uniform sampler2D uTextures[16];

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
    vec4 albedo = texture(uTextures[iBaseColorIndex], iUvs);

    if (albedo.a < 0.5)
    {
        discard;
    }

    float metallic = iMetallicFactor;
    float roughness = iRoughnessFactor;

    if (iMetallicRoughnessIndex != 0xFFFFFFFFu)
    {
        vec4 mr = texture(uTextures[iMetallicRoughnessIndex], iUvs);
        roughness *= mr.g;
        metallic *= mr.b;
    }

    vec3 N = GetShadedNormal();

    gAlbedo = vec4(albedo.rgb, 1.0);
    gNormal = vec4(normalize(N) * 0.5 + 0.5, 1.0);
    gMaterial = vec4(roughness, metallic, 0.0, 1.0);
}