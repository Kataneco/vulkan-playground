#version 460

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inPosition;

layout(location = 0) out vec4 outColor;

void main() {
    // Fixed world-space position of the "sun"
    vec3 lightPos = vec3(10.0, 10.0, -10.0);

    // Calculate light direction from fragment to light
    vec3 lightDir = normalize(lightPos - inPosition);
    vec3 normal = normalize(inNormal);

    // Diffuse shading
    float diffuse = max(dot(normal, lightDir), 0.2);

    // Visualize normals as RGB (mapped to 0–1)
    vec3 debugColor = normal * 0.5 + 0.5;

    // Blend between debug normal color and lit object color
    vec3 finalColor = 0.5 * debugColor + 0.5 * (inColor.rgb * diffuse);
    outColor = vec4(finalColor, 0.97);
}
