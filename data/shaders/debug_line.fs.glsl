#version 460 core

layout(location = 0) out vec4 oPixel;
layout(location = 2) uniform vec3 uColor;

void main()
{
    oPixel = vec4(uColor, 1.0);
}