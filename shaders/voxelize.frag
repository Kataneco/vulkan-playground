#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

struct Node {
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
    vec3 resolution; // x: dimensions^3, y: unit length, z: unassigned
} data;

// Find or create a leaf node for the given voxel position
// Returns negative index for leaf nodes
int findOrCreateVoxel(ivec3 voxelPos, uint depth, uint maxDepth) {
    // Start from root node at index 0
    int nodeIndex = 0;

    // Maximum depth check to prevent infinite loops
    for (uint currentDepth = 0; currentDepth < maxDepth; currentDepth++) {
        // Exit if we've reached the desired depth
        if (currentDepth == depth) {
            break;
        }

        // Calculate which octant the voxel position falls into
        uint childIdx = 0;
        uint shift = maxDepth - currentDepth - 1;

        if (((voxelPos.x >> shift) & 1)==1) childIdx |= 1; // x1 vs x0
        if (((voxelPos.y >> shift) & 1)==1) childIdx |= 2; // y1 vs y0
        if (((voxelPos.z >> shift) & 1)==1) childIdx |= 4; // z1 vs z0

        // Get the child pointer for this position
        int childPtr = nodes[nodeIndex].children[childIdx];

        // If pointer is 0, create a new node
        if (childPtr == 0) {
            // Allocate a new node
            uint newNodeIndex = atomicAdd(nodeCount, 1);

            // Link this new node to the parent
            int newNodePtr = int(newNodeIndex); // Positive for branch nodes

            nodes[nodeIndex].children[childIdx] = newNodePtr;
            childPtr = newNodePtr;
        }

        // If it's a negative pointer, we've already hit a leaf node
        if (childPtr < 0) {
            return childPtr; // Leaf node already exists
        }

        // Move to the child node (subtract 1 because indices are 1-based for branch nodes)
        nodeIndex = childPtr;
    }

    // We're at the correct depth, add a leaf for this position
    uint childIdx = 0;
    uint shift = maxDepth - depth;

    if (((voxelPos.x >> shift) & 1)==1) childIdx |= 1;
    if (((voxelPos.y >> shift) & 1)==1) childIdx |= 2;
    if (((voxelPos.z >> shift) & 1)==1) childIdx |= 4;

    // Get the existing pointer for this position
    int leafPtr = nodes[nodeIndex].children[childIdx];

    // If already a leaf, return its index
    if (leafPtr < 0) {
        return leafPtr;
    }

    // If 0, create a new leaf index and link it
    if (leafPtr == 0) {
        // Add a new voxel
        uint voxelIndex = atomicAdd(voxelCount, 1);
        voxels[voxelIndex].position = 0;

        // Store leaf index as negative (to mark as leaf)
        // We use (voxelIndex + 1) to make sure even index 0 becomes negative when negated
        int newLeafPtr = -int(voxelIndex + 1);

        // Link to parent
        nodes[nodeIndex].children[childIdx] = newLeafPtr;

        return newLeafPtr;
    }

    // If branch node exists at this position but we need a leaf,
    // we'd need to handle this case (not implemented here)
    return 0;
}

// Helper function to unpack, mix, and repack normal data
uint mixNormal(uint existingPacked, vec3 newNormal) {
    // Unpack existing normal
    vec4 existingNormalVec = unpackSnorm4x8(existingPacked);
    vec3 existingNormal = existingNormalVec.xyz;

    // Mix normals and normalize
    vec3 mixedNormal = normalize(existingNormal+newNormal);

    // Repack and return
    return packSnorm4x8(vec4(mixedNormal, existingNormalVec.w));
}

// Helper function to unpack, mix, and repack color data
uint mixColor(uint existingPacked, vec4 newColor) {
    // Unpack existing color
    vec4 existingColor = unpackUnorm4x8(existingPacked);

    // Mix colors
    vec4 mixedColor = mix(existingColor, newColor, 0.5f);

    // Repack and return
    return packUnorm4x8(mixedColor);
}

void main() {
    // Calculate voxel position from vertex position
    ivec3 voxelPos = ivec3(floor(position-data.center + vec3(sign(position.x)*0.5f, sign(position.y)*0.5f, sign(position.z)*0.5f)));

    if(abs(voxelPos.x) > 256) discard;
    else if(abs(voxelPos.y) > 256) discard;
    else if(abs(voxelPos.z) > 256) discard;

    // Find or create the voxel in the octree
    // Parameters: voxelPos, current depth, max depth
    int voxelPtr = findOrCreateVoxel(voxelPos, int(log2(data.resolution.x)), int(log2(256))); // Example depth values

    // If voxel pointer is negative, we have a valid leaf node
    if (voxelPtr < 0) {
        // Convert back to voxel index
        uint voxelIndex = uint(-voxelPtr - 1);
        // For a new voxel, set the position directly
        // Note: Position typically shouldn't be mixed as it defines the voxel's location
        uint positionPacked = packSnorm4x8(vec4(voxelPos, 0) / data.resolution.x);

        // For existing voxels, mix the normal and color with previous values
        // Check if this is a new voxel by seeing if position is 0 (assuming 0 is never a valid packed position)
        bool isNewVoxel = bool(voxels[voxelIndex].position == 0);

        if (isNewVoxel) {
            // New voxel - set initial values
            voxels[voxelIndex].position = positionPacked;
            voxels[voxelIndex].normal = packSnorm4x8(vec4(normal, 1));
            voxels[voxelIndex].color = packUnorm4x8(vec4(1, 1, 1, 1));
        } else {
            // Existing voxel - mix with previous values
            // Position remains the same
            voxels[voxelIndex].normal = mixNormal(voxels[voxelIndex].normal, normal);
            voxels[voxelIndex].color = mixColor(voxels[voxelIndex].color, vec4(1, 1, 1, 1));
        }
    }
}