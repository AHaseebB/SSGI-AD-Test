#version 460 core

layout (location = 0) in vec3 iPosition;
layout (location = 1) in vec3 iNormal;
layout (location = 2) in vec2 iUv;
layout (location = 3) in vec4 iTangent;

layout (location = 0) out vec2 oUvs;
layout (location = 1) out flat uint oBaseColorIndex;
layout (location = 2) out flat uint oNormalIndex;
layout (location = 3) out flat uint oMetallicRoughnessIndex;
layout (location = 4) out vec3 oWorldNormal;
layout (location = 5) out vec4 oWorldTangent;

layout (location = 0) uniform mat4 uProjection;
layout (location = 1) uniform mat4 uView;

struct ObjectData
{
    uint transformIndex;
    uint baseColorIndex;
    uint normalIndex;
    uint metallicRoughnessIndex;
    float metallicFactor;
    float roughnessFactor;
};

layout (binding = 0) readonly buffer BObjectData
{
    ObjectData objectData[];
};

layout (binding = 1) readonly buffer BTransforms
{
    mat4 transforms[];
};

layout (location = 6) out flat float oMetallicFactor;
layout (location = 7) out flat float oRoughnessFactor;

void main()
{
    uint drawId = gl_DrawID;
    mat4 model = transforms[objectData[drawId].transformIndex];

    oUvs = iUv;
    oBaseColorIndex = objectData[drawId].baseColorIndex;
    oNormalIndex = objectData[drawId].normalIndex;
    oMetallicRoughnessIndex = objectData[drawId].metallicRoughnessIndex;
    oMetallicFactor = objectData[drawId].metallicFactor;
    oRoughnessFactor = objectData[drawId].roughnessFactor;

    mat3 normalMatrix = transpose(inverse(mat3(model)));
    oWorldNormal = normalize(normalMatrix * iNormal);
    oWorldTangent = vec4(normalize(mat3(model) * iTangent.xyz), iTangent.w);

    gl_Position = uProjection * uView * model * vec4(iPosition, 1.0);
}