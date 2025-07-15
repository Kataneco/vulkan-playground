#version 460
precision highp int;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 texCoord_priority;
layout(location = 3) in vec3 root;

struct Node {
    int parent;
    int childCount;
    int children[8];
};

struct Voxel {
    uint position;
    uint normal;
    uint color;
};

layout(std430, set = 1, binding = 0) buffer VoxelCountBuffer {
    uint voxelCount;
};

layout(std430, set = 1, binding = 1) buffer VoxelBuffer {
    Voxel voxels[];
};

layout(std430, set = 1, binding = 2) buffer NodeCountBuffer {
    uint nodeCount;
};

layout(std430, set = 1, binding = 3) buffer NodeBuffer {
    Node nodes[];
};

layout(push_constant) uniform VoxelizerData {
    vec3 center;
    vec3 resolution; // x: dimensions^3, y: unit length, z: unused
} data;

int findOrCreateVoxel(ivec3 voxelPosition, uint depth, uint maxDepth) {
    int nodeIndex = 0;

    for (uint currentDepth = 0; currentDepth < depth; ++currentDepth) {
        uint shift = maxDepth - currentDepth - 1;
        uint childIndex = 0;
        if (((voxelPosition.x >> shift) & 1) != 0) childIndex |= 1;
        if (((voxelPosition.y >> shift) & 1) != 0) childIndex |= 2;
        if (((voxelPosition.z >> shift) & 1) != 0) childIndex |= 4;

        int childPointer = atomicAdd(nodes[nodeIndex].children[childIndex], 0);

        if (childPointer == 0) {
            int prior = atomicCompSwap(nodes[nodeIndex].children[childIndex], 0, 2147483647);
            if (prior == 0) {
                uint newNodeIndex = atomicAdd(nodeCount, 1);
                nodes[newNodeIndex].parent = nodeIndex;

                atomicCompSwap(nodes[nodeIndex].children[childIndex], 2147483647, int(newNodeIndex));
            }
        }

        do {
            childPointer = atomicAdd(nodes[nodeIndex].children[childIndex], 0);
        } while (childPointer == 2147483647);

        if (childPointer < 0)
        return childPointer;

        nodeIndex = childPointer;
    }

    uint shift = maxDepth - depth;
    uint childIndex = 0;
    if (((voxelPosition.x >> shift) & 1) != 0) childIndex |= 1;
    if (((voxelPosition.y >> shift) & 1) != 0) childIndex |= 2;
    if (((voxelPosition.z >> shift) & 1) != 0) childIndex |= 4;

    int childPointer = atomicAdd(nodes[nodeIndex].children[childIndex], 0);

    if (childPointer == 0) {
        int prior = atomicCompSwap(nodes[nodeIndex].children[childIndex], 0, 2147483647);
        if (prior == 0) {
            uint voxelIndex = atomicAdd(voxelCount, 1);
            voxels[voxelIndex].position = 0;
            int newLeafPointer = -int(voxelIndex + 1);
            atomicCompSwap(nodes[nodeIndex].children[childIndex], 2147483647, newLeafPointer);
        }
    }

    do {
        childPointer = atomicAdd(nodes[nodeIndex].children[childIndex], 0);
    } while (childPointer == 2147483647);

    return childPointer;
}

uint mixNormal(uint existingPacked, vec3 newNormal) {
    vec3 existingNormal = unpackSnorm4x8(existingPacked).xyz;
    vec3 combinedNormal = normalize(existingNormal + newNormal);
    return packSnorm4x8(vec4(combinedNormal, 1.0));
}

uint mixColor(uint existingPacked, vec4 newColor) {
    vec4 existingColor = unpackUnorm4x8(existingPacked);
    vec4 blendedColor = mix(existingColor, newColor, 0.5);
    return packUnorm4x8(blendedColor);
}

uint splitBy3(uint value) {
    value = (value | (value << 16)) & 0x030000FF;
    value = (value | (value << 8)) & 0x0300F00F;
    value = (value | (value << 4)) & 0x030C30C3;
    value = (value | (value << 2)) & 0x09249249;
    return value;
}

uint mortonEncode(in uvec3 position) {
    position = min(position, uvec3(1023));

    uint x = splitBy3(position.x);
    uint y = splitBy3(position.y);
    uint z = splitBy3(position.z);

    return x | (y << 1) | (z << 2);
}

uint compactBits(uint value) {
    value &= 0x09249249;
    value = (value | (value >> 2)) & 0x030C30C3;
    value = (value | (value >> 4)) & 0x0300F00F;
    value = (value | (value >> 8)) & 0x030000FF;
    value = (value | (value >> 16)) & 0x000003FF;
    return value;
}

uvec3 mortonDecode(in uint morton) {
    uvec3 position;

    position.x = compactBits(morton);
    position.y = compactBits(morton >> 1);
    position.z = compactBits(morton >> 2);

    return position;
}

void main() {
    ivec3 voxelPosition = ivec3(floor(position / data.resolution.y + 0.5));

    if (any(lessThan(voxelPosition, ivec3(root))) || any(greaterThanEqual(voxelPosition, ivec3(root)+ivec3(int(data.resolution.x))))) discard;

    uint depth = uint(ceil(log2(data.resolution.x)));
    int voxelPointer = findOrCreateVoxel(voxelPosition, depth, depth);

    if (voxelPointer < 0) {
        uint voxelIndex = uint(-voxelPointer - 1);
        uint positionPacked = mortonEncode(voxelPosition);
        positionPacked |= 1u << 31;

        uint expected = 0;
        bool isNewVoxel = bool(atomicCompSwap(voxels[voxelIndex].position, expected, positionPacked) == 0);

        if (isNewVoxel) {
            atomicExchange(voxels[voxelIndex].normal, packUnorm4x8(vec4((normalize(normal)+vec3(1,1,1))/2, texCoord_priority.z)));
            atomicExchange(voxels[voxelIndex].color, packUnorm4x8(vec4(1.0, 1.0, 1.0, texCoord_priority.x)));
        } else {
        /*
            uint oldNormalPacked = atomicAdd(voxels[voxelIndex].normal, 0);
            uint oldColorPacked = atomicAdd(voxels[voxelIndex].color, 0);

            uint newNormalPacked = mixNormal(oldNormalPacked, normalize(normal));
            uint newColorPacked = mixColor(oldColorPacked, vec4(1.0, 1.0, 1.0, 1.0));

            atomicExchange(voxels[voxelIndex].normal, newNormalPacked);
            atomicExchange(voxels[voxelIndex].color, newColorPacked);
        */
            atomicMax(voxels[voxelIndex].normal, packUnorm4x8(vec4((normalize(normal)+vec3(1,1,1))/2, texCoord_priority.z)));
            //atomicExchange(voxels[voxelIndex].normal, packSnorm4x8(vec4(normalize(normal), texCoord_priority.z)));
        }
    }
}