#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexCoord;

layout(set = 0, binding = 0) uniform ObjectData {
    mat4 model;
} objectData;

void main() {
    gl_Position = objectData.model*vec4(position, 1);
    //gl_Position.xyz /= gl_Position.w;
    //gl_Position.w = 1;
    outNormal = normal;
    outTexCoord = texCoord;
}