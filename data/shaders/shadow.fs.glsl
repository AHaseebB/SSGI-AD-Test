#version 460 core

layout (location = 0) in vec2 iUvs;
layout (location = 1) in flat uint iBaseColorIndex;

layout (location = 1) uniform sampler2D uTextures[16];

void main()
{
    vec4 albedo = texture(uTextures[iBaseColorIndex], iUvs); // Sample the base color texture using the index from the vertex shader

    if (albedo.a < 0.5) // Discard fragments with low alpha to create holes in the shadow map for transparent objects
    {
        discard;
    }
}