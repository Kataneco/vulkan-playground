#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexCoord;

layout(set = 0, binding = 0) uniform ObjectData {
    mat4 model;
} objectData;

layout(push_constant) uniform VoxelizerData {
    vec3 center;
    vec3 resolution; // x: dimensions^3, y: unit length, z: root grid dimensions^3
} data;

void main() {
    gl_Position = (objectData.model * vec4(position, 1));
    gl_Position /= gl_Position.w;
    outNormal = normal;
    outTexCoord = texCoord; //vec2(gl_InstanceIndex); //TODO temp
}