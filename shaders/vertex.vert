#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 gPosition;
layout(location = 2) out vec4 gNormalSpec;

layout(push_constant) uniform block {
    float time;
} constants;

layout(set = 0, binding = 1) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    const float time = float(constants.time);
    fragTexCoord = texCoord;
    gPosition = ubo.model * vec4(position.xyz, 1.0f);
    gNormalSpec = vec4(normal.xyz, 0.0f);
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(position.xyz, 1.0f);
}