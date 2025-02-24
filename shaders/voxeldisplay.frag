#version 460

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightDir = normalize(vec3(1.0, 1.0, -1.0));
    float diffuse = max(dot(normalize(inNormal), lightDir), 0.2);

    vec3 debugColor = fract(inNormal * 0.5 + 0.5);
    outColor = vec4((debugColor/2)+(inColor*diffuse/2).xyz, 1.0);
}