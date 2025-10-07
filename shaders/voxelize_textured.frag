#version 460
precision highp int;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 texCoord_priority;
layout(location = 3) in vec3 groot;

struct Node {
    int parent;
    uint occlusion; // amount of total children
    uint emission; // pointer to voxel that sums up all child voxels
    uint generation; // node age
    int children[8];
};

struct Voxel {
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
    uint nodeCount; // reset to root count
};

layout(std430, set = 1, binding = 3) buffer NodeBuffer {
    Node nodes[];
};

layout(std430, set = 1, binding = 4) buffer NodeGenerationBuffer {
    uint nodeGeneration;
};

layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout(push_constant) uniform VoxelizerData {
    vec3 center;
    vec3 resolution; // x: dimensions^3, y: unit length, z: root grid dimensions^3
} data;

int findOrCreateVoxel(ivec3 voxelPosition, uint depth, uint maxDepth) {
    //int nodeIndex = 0;
    int desu = int(data.resolution.z);
    ivec3 root = voxelPosition/int(data.resolution.x);
    int rootIndex = (root.x)+(root.y)*desu+(root.z)*desu*desu;
    int nodeIndex = rootIndex;

    for (uint currentDepth = 0; currentDepth < depth-1; ++currentDepth) {
        uint shift = maxDepth - currentDepth - 1;
        uint childIndex = 0;
        if (((voxelPosition.x >> shift) & 1) != 0) childIndex |= 1;
        if (((voxelPosition.y >> shift) & 1) != 0) childIndex |= 2;
        if (((voxelPosition.z >> shift) & 1) != 0) childIndex |= 4;

        int childPointer = atomicAdd(nodes[nodeIndex].children[childIndex], 0);

        if (childPointer == 0) {
            int prior = atomicCompSwap(nodes[nodeIndex].children[childIndex], childPointer, 2147483647);
            if (prior == childPointer) {
                uint newNodeIndex = atomicAdd(nodeCount, 1);
                nodes[newNodeIndex].parent = nodeIndex;

                if(nodes[newNodeIndex].generation != nodeGeneration) {
                    nodes[newNodeIndex].children[0] = 0;
                    nodes[newNodeIndex].children[1] = 0;
                    nodes[newNodeIndex].children[2] = 0;
                    nodes[newNodeIndex].children[3] = 0;
                    nodes[newNodeIndex].children[4] = 0;
                    nodes[newNodeIndex].children[5] = 0;
                    nodes[newNodeIndex].children[6] = 0;
                    nodes[newNodeIndex].children[7] = 0;
                }

                nodes[newNodeIndex].generation = nodeGeneration;

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
            voxels[voxelIndex].normal = 0;
            voxels[voxelIndex].color = 0;
            int newLeafPointer = -int(voxelIndex + 1);

            uint density = 1u << ((maxDepth-depth)*3);
            int backtrace = nodeIndex;
            while (backtrace > rootIndex) {
                atomicAdd(nodes[backtrace].occlusion, density);
                backtrace = nodes[backtrace].parent;
            }
            atomicAdd(nodes[backtrace].occlusion, density);

            atomicCompSwap(nodes[nodeIndex].children[childIndex], 2147483647, newLeafPointer);
        }
    }

    do {
        childPointer = atomicAdd(nodes[nodeIndex].children[childIndex], 0);
    } while (childPointer == 2147483647);

    return childPointer;
}

vec3 rootBegin(vec3 r) {
    return r*data.resolution.y;
}

void main() {
    ivec2 pixel = ivec2(floor(gl_FragCoord.xy));
    //if(pixel.x == 0 || pixel.y == 0 || pixel.x == int(data.resolution.x)+1 || pixel.y == int(data.resolution.x)+1) return;

    //ivec3 voxelPosition = ivec3(floor(position/(data.resolution.y*data.resolution.z)));
    // Calculate voxel position directly from world position
    ivec3 voxelPosition = ivec3(floor((position + vec3(data.resolution.z * data.resolution.y * 0.5)) / data.resolution.y * data.resolution.x));
    //if(any(lessThan(voxelPosition, ivec3(0))) || any(greaterThanEqual(voxelPosition, ivec3(int(data.resolution.x))))) return;

    if(any(lessThan(voxelPosition, ivec3(0))) || any(greaterThanEqual(voxelPosition, ivec3(int(data.resolution.x*data.resolution.z))))) {
        discard;
    }

    vec3 tmp = abs(groot - min(sign(groot), vec3(0.0)));
    float LOD = max(tmp.x, max(tmp.y, tmp.z))+1.0;

    uint maxdepth = uint(ceil(log2(data.resolution.x)));
    uint depth = maxdepth-uint(log2(LOD)*2.0);
    int voxelPointer = findOrCreateVoxel(voxelPosition, depth, maxdepth);

    if (voxelPointer < 0) {
        uint voxelIndex = uint(-voxelPointer - 1);
        uint normalPacked = packUnorm4x8(vec4((normalize(normal)+vec3(1,1,1))/2, texCoord_priority.z));
        uint oldNormalPacked = atomicMax(voxels[voxelIndex].normal, normalPacked);

        vec4 sampled_color = texture(texSampler, texCoord_priority.xy).xyzw;
        //vec3 sampled_color = vec3(1.0);

        atomicMax(voxels[voxelIndex].color, packUnorm4x8(vec4(sampled_color)));
    }
}