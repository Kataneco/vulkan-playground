#version 460

const vec3 CUBE_VERTICES[8] = vec3[8](
vec3(0.0, 0.0, 0.0),
vec3(1.0, 0.0, 0.0),
vec3(0.0, 1.0, 0.0),
vec3(1.0, 1.0, 0.0),
vec3(0.0, 0.0, 1.0),
vec3(1.0, 0.0, 1.0),
vec3(0.0, 1.0, 1.0),
vec3(1.0, 1.0, 1.0)
);

const int CUBE_INDICES[36] = int[36](
0, 1, 2,  1, 3, 2,  // front
4, 6, 5,  5, 6, 7,  // back
0, 2, 4,  4, 2, 6,  // left
1, 5, 3,  5, 7, 3,  // right
2, 3, 6,  6, 3, 7,  // top
0, 4, 1,  1, 4, 5   // bottom
);

struct Voxel {
    uint position;
    uint normal;
    uint color;
};

layout(std430, set = 0, binding = 0) readonly buffer VoxelBuffer {
    Voxel voxels[];
};

layout(push_constant) uniform VoxelDisplayData {
    mat4 proj;
    vec3 center;
    vec3 resolution;
} data;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec3 outNormal;

void main() {
    const uint vertexIndex = CUBE_INDICES[gl_VertexIndex];
    const vec3 localPosition = vec3(CUBE_VERTICES[vertexIndex]);

    Voxel voxel = voxels[gl_InstanceIndex];

    vec3 worldPosition = (localPosition * (data.resolution.y/data.resolution.x)*(4+0.1f)) + unpackSnorm4x8(voxel.position).xyz;
    worldPosition += data.center;

    gl_Position = data.proj * vec4(worldPosition, 1);
    outColor = unpackUnorm4x8(voxel.color);
    outNormal = unpackSnorm4x8(voxel.normal).xyz;
}