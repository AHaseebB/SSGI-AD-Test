#version 460 core

layout(location = 0) in vec3 iPosition;

layout(location = 0) uniform mat4 uProjection;
layout(location = 1) uniform mat4 uView;

void main()
{
    gl_Position = uProjection * uView * vec4(iPosition, 1.0);
}