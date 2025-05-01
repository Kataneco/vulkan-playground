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
layout(location = 2) out vec3 outPosition;

// Splits a 10-bit integer (value) so that its bits are separated by 2 zeros
uint splitBy3(uint value) {
    // Insert 2 zeros after each bit of the original value
    value = (value | (value << 16)) & 0x030000FF;
    value = (value | (value << 8)) & 0x0300F00F;
    value = (value | (value << 4)) & 0x030C30C3;
    value = (value | (value << 2)) & 0x09249249;
    return value;
}

// Morton encode for 3D coordinates - each coordinate can use 10 bits (0-1023 range)
uint mortonEncode(in uvec3 position) {
    // Ensure coordinates don't exceed 10 bits (0-1023)
    position = min(position, uvec3(1023));

    // Interleave bits
    uint x = splitBy3(position.x);
    uint y = splitBy3(position.y);
    uint z = splitBy3(position.z);

    // Combine the interleaved bits
    return x | (y << 1) | (z << 2);
}

// Extract every third bit and compact
uint compactBits(uint value) {
    // Extract every third bit and compact them
    value &= 0x09249249;
    value = (value | (value >> 2)) & 0x030C30C3;
    value = (value | (value >> 4)) & 0x0300F00F;
    value = (value | (value >> 8)) & 0x030000FF;
    value = (value | (value >> 16)) & 0x000003FF;
    return value;
}

// Morton decode to get 3D coordinates back
uvec3 mortonDecode(in uint morton) {
    uvec3 position;

    // Extract the interleaved bits
    position.x = compactBits(morton);
    position.y = compactBits(morton >> 1);
    position.z = compactBits(morton >> 2);

    return position;
}

void main() {
    const uint vertexIndex = CUBE_INDICES[gl_VertexIndex];
    vec3 localPosition = vec3(CUBE_VERTICES[vertexIndex]);
    //localPosition *= 0.3001;
    localPosition *= 1.0001;

    Voxel voxel = voxels[gl_InstanceIndex];

    vec3 worldPosition = (localPosition * (data.resolution.y/data.resolution.x)) + vec3(mortonDecode(voxel.position))/(data.resolution.x-1)*data.resolution.y;
    worldPosition += data.center-vec3(data.resolution.y/2);

    //worldPosition += unpackSnorm4x8(voxel.normal).xyz*0.005f;

    gl_Position = data.proj * vec4(worldPosition, 1);
    outColor = unpackUnorm4x8(voxel.color);
    outNormal = (unpackUnorm4x8(voxel.normal).xyz*2)-vec3(1,1,1);
    outPosition = worldPosition;
}