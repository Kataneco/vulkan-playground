#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoords;

layout(location = 0) out vec4 fragmentColor;
layout(location = 1) out vec2 fragmentTexCoords;
layout(location = 2) out vec3 fragmentNormal;

layout(location = 3) out vec3 fragmentPosition;

layout(push_constant) uniform block {
    mat4 renderMatrix;
    float time;
} constants;

void main() {
    const float time = float(constants.time);
    gl_Position = constants.renderMatrix * vec4(position.xyz, 1.0f);
    fragmentColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    fragmentTexCoords = texCoords;
    fragmentNormal = normal;
    fragmentPosition = position*0.42f;
}
