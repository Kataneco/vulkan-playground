#version 450

layout(push_constant) uniform VoxelizerData {
    mat4 invView;
    mat4 invProjection;
    ivec2 resolution;
} data;

layout(location = 0) out vec4 outColor;

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

layout(std430, set = 0, binding = 0) readonly buffer VoxelCountBuffer {
    uint voxelCount;
};

layout(std430, set = 0, binding = 1) readonly buffer VoxelBuffer {
    Voxel voxels[];
};

layout(std430, set = 0, binding = 2) readonly buffer NodeCountBuffer {
    uint nodeCount;
};

layout(std430, set = 0, binding = 3) readonly buffer NodeBuffer {
    Node nodes[];
};

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

float sdBox( vec3 p, vec3 b ) {
    vec3 q = abs(p) - b;
    return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

int findVoxel(ivec3 voxelPosition, uint depth, uint maxDepth) {
    int nodeIndex = 0;

    for (uint currentDepth = 0; currentDepth < depth; ++currentDepth) {
        uint shift = maxDepth - currentDepth - 1;
        uint childIndex = 0;
        if (((voxelPosition.x >> shift) & 1) != 0) childIndex |= 1;
        if (((voxelPosition.y >> shift) & 1) != 0) childIndex |= 2;
        if (((voxelPosition.z >> shift) & 1) != 0) childIndex |= 4;

        int childPointer = nodes[nodeIndex].children[childIndex];

        if (childPointer == 0) {
            return 0;
        }

        if (childPointer < 0)
        return childPointer;

        nodeIndex = childPointer;
    }

    uint shift = maxDepth - depth;
    uint childIndex = 0;
    if (((voxelPosition.x >> shift) & 1) != 0) childIndex |= 1;
    if (((voxelPosition.y >> shift) & 1) != 0) childIndex |= 2;
    if (((voxelPosition.z >> shift) & 1) != 0) childIndex |= 4;

    int childPointer = nodes[nodeIndex].children[childIndex];

    if (childPointer == 0) {
        return 0;
    }

    return childPointer;
}

void main() {
    const vec3 boxSize = vec3(2.0);
    const float voxelGridSize = 512.0;
    const float voxelSize = (boxSize.x*2) / voxelGridSize;

    vec3 backgroundColor = vec3(0.1, 0.1, 0.2);
    outColor = vec4(backgroundColor, 1.0);

    vec2 uv = vec2(gl_FragCoord.xy) / vec2(data.resolution);
    vec4 clip = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    vec4 viewSpace = vec4(data.invProjection * clip);
    viewSpace /= viewSpace.w;
    vec4 worldSpace = vec4(data.invView * vec4(viewSpace.xyz, 0.0));
    vec3 rayDirection = normalize(worldSpace.xyz);
    vec3 rayOrigin = data.invView[3].xyz;

    vec3 boxMin = -boxSize;
    vec3 boxMax = boxSize;
    vec3 tMin = (boxMin - rayOrigin) / rayDirection;
    vec3 tMax = (boxMax - rayOrigin) / rayDirection;
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);

    if(tNear >= tFar || tFar < 0.0) {
        return;
    }

    float startT = max(tNear, 0.0);
    vec3 currentPosition = rayOrigin + startT * rayDirection;

    // Prepare for DDA algorithm
    // Convert ray to voxel grid space where each voxel is 1 unit
    vec3 voxelRayOrigin = (currentPosition + boxSize) * (voxelGridSize / (boxSize.x*2));

    // Get initial voxel cell
    ivec3 mapPos = ivec3(floor(voxelRayOrigin));

    // Calculate ray step direction and initial side distances
    ivec3 rayStep = ivec3(sign(rayDirection));

    // Length of ray movement needed for one voxel in each direction
    vec3 deltaDist = abs(vec3(length(rayDirection)) / rayDirection);

    // Distance to first voxel boundary
    vec3 sideDist = vec3((rayStep * (vec3(mapPos) - voxelRayOrigin) + (rayStep * 0.5) + 0.5) * deltaDist);

    // Variables to track ray traversal
    bool hitVoxel = false;
    vec3 normal = vec3(0.0);

    // Maximum number of steps to prevent infinite loops
    const int MAX_STEPS = 512; // Increased for dense grid

    int hitcount = 0;

    // DDA algorithm for fast voxel traversal
    for(int steps = 0; steps < MAX_STEPS; steps++) {
        // Check current voxel
        if(all(greaterThanEqual(mapPos, ivec3(0))) && all(lessThan(mapPos, ivec3(voxelGridSize)))) {
            int idx = findVoxel(mapPos, uint(log2(voxelGridSize)), uint(log2(voxelGridSize)));

            if(idx < 0) {
                // Hit a voxel!
                hitVoxel = true;

                // Use stored normal or calculate from hit direction
                vec3 lnormal = (unpackUnorm4x8(voxels[-idx-1].normal).xyz * 2.0) - vec3(1.0);

                // Apply lighting
                vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
                float diffuse = max(dot(lnormal, lightDir), 0.2); // Ambient + diffuse
                vec3 color = (vec3(0.8, 0.85, 0.9)/2+lnormal/2) * diffuse;

                vec3 viewDir = -rayDirection;
                vec3 halfwayDir = normalize(lightDir + viewDir);
                float spec = pow(max(dot(lnormal, halfwayDir), 0.0), 32.0);
                color += vec3(0.3) * spec;

                if(unpackUnorm4x8(voxels[-idx-1].color).w == 1) {
                    rayDirection = rayDirection - 2*(dot(rayDirection, lnormal))*lnormal;
                    // Calculate ray step direction and initial side distances
                    rayStep = ivec3(sign(rayDirection));

                    // Length of ray movement needed for one voxel in each direction
                    deltaDist = abs(vec3(length(rayDirection)) / rayDirection);

                    // Distance to first voxel boundary
                    sideDist = vec3((rayStep * (vec3(mapPos) - voxelRayOrigin) + (rayStep * 0.5) + 0.5) * deltaDist);
                }

                hitcount++;
                outColor += vec4(color, 1.0);
                if(unpackUnorm4x8(voxels[-idx-1].color).w != 1) break;
            }
        }

        // Find next voxel boundary
        bvec3 mask = lessThanEqual(sideDist.xyz, min(sideDist.yzx, sideDist.zxy));

        // Advance ray to next voxel
        sideDist += vec3(mask) * deltaDist;
        mapPos += ivec3(vec3(mask)) * rayStep;

        // Update normal based on which face was hit
        normal = -vec3(mask) * vec3(rayStep);

        // Exit if outside grid bounds
        if(any(lessThan(mapPos, ivec3(0))) || any(greaterThanEqual(mapPos, ivec3(voxelGridSize)))
        || steps == MAX_STEPS - 1) {
            // Optional: Use a gradient based on ray travel distance for nice background
            float t = length(vec3(mapPos) / voxelGridSize);
            outColor = vec4(mix(backgroundColor, vec3(0.12, 0.0, 0.12), t), 1.0);
            break;
        }
    }

    outColor /= hitcount > 0 ? hitcount : 1;
}