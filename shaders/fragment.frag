#version 460

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 texCoords;
layout(location = 2) in vec3 inNormal;

layout(location = 3) in vec3 inPosition;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(0.5+0.5*inNormal, 1.0);
}