#version 460 core

layout (location = 0) in vec3 iPosition;
layout (location = 2) in vec2 iUv;

layout (location = 0) out vec2 oUvs;
layout (location = 1) out flat uint oBaseColorIndex;

layout (location = 0) uniform mat4 uLightViewProjection;

struct ObjectData // Matches the layout of the object data buffer in the application
{
    uint transformIndex;
    uint baseColorIndex;
    uint normalIndex;
    uint metallicRoughnessIndex;
    float metallicFactor;
    float roughnessFactor;
};

layout (binding = 0) readonly buffer BObjectData // Array of object data for each draw call, indexed by gl_DrawID
{
    ObjectData objectData[];
};

layout (binding = 1) readonly buffer BTransforms // Array of transformation matrices for all objects, indexed by objectData.transformIndex
{
    mat4 transforms[];
};

void main()
{
    uint drawId = gl_DrawID; // Get the current draw call ID
    mat4 model = transforms[objectData[drawId].transformIndex]; // Fetch the model matrix for this draw call using the transform index from object data

    oUvs = iUv; // Pass through UVs
    oBaseColorIndex = objectData[drawId].baseColorIndex; // Pass through base color index (not used for shadow mapping but included for completeness)

    gl_Position = uLightViewProjection * model * vec4(iPosition, 1.0);
}