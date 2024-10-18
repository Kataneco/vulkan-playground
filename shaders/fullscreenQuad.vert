#version 460

const vec2 quadVertices[4] = vec2[](
vec2(-1.0,  1.0), // Top-left
vec2( 1.0,  1.0), // Top-right
vec2(-1.0, -1.0), // Bottom-left
vec2( 1.0, -1.0)  // Bottom-right
);

const uint quadIndices[6] = {
0, 1, 2, // First triangle
1, 3, 2  // Second triangle
};

void main() {
    gl_Position = vec4(quadVertices[quadIndices[gl_VertexIndex]], 0.0f, 1.0f);
}