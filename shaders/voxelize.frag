#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

struct Voxel {
    vec3 position;
    uint normal;
    uint color;
};

layout(std430, set = 1, binding = 0) buffer VoxelCountBuffer {
    uint voxelCount;
};

layout(std430, set = 1, binding = 1) writeonly buffer VoxelBuffer {
    Voxel voxels[];
};

layout(push_constant) uniform VoxelizerData {
    vec3 center;
    vec3 resolution; // xy: dimensions, z: voxel size
} data;

void main() {
    uint index = atomicAdd(voxelCount, 1);
    voxels[index].position = position;
    voxels[index].normal = packSnorm4x8(vec4(normal, 0));
    voxels[index].color = packUnorm4x8(vec4(1,1,1,1));
}