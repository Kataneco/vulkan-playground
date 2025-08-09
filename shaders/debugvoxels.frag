#version 460
precision highp int;

struct Node {
    int parent;
    int children[8];
};

struct Voxel {
    uint normal;
    uint color;
};

layout(set = 0, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
} camera;

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
    vec3 center; // center of the voxel grid
    vec3 resolution; // x: dimensions^3, y: unit length, z: root grid dimensions^3

    vec4 frame; // framebuffer info
} data;

layout(location = 0) out vec4 outColor;

int findVoxel(ivec3 voxelPosition, uint depth, uint maxDepth, out uint lastDepth) {
    //int nodeIndex = 0;
    int desu = int(data.resolution.z);
    ivec3 root = voxelPosition/int(data.resolution.x);
    int rootIndex = (root.x)+(root.y)*desu+(root.z)*desu*desu;
    int nodeIndex = rootIndex;

    // Formality for resolutions not power of two
    //voxelPosition %= int(data.resolution.x);

    for (uint currentDepth = 0; currentDepth < depth; ++currentDepth) {
        lastDepth = currentDepth+1;
        uint shift = maxDepth - currentDepth - 1;
        uint childIndex = 0;
        if (((voxelPosition.x >> shift) & 1) != 0) childIndex |= 1;
        if (((voxelPosition.y >> shift) & 1) != 0) childIndex |= 2;
        if (((voxelPosition.z >> shift) & 1) != 0) childIndex |= 4;

        int childPointer = (nodes[nodeIndex].children[childIndex]);

        if (childPointer == 0) {
            return nodeIndex;
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

    int childPointer = (nodes[nodeIndex].children[childIndex]);

    if (childPointer == 0) {
        return nodeIndex;
    }

    return childPointer;
}

vec3 worldToGrid(vec3 pos) {
    return pos-vec3(-data.resolution.y*data.resolution.z*0.5)+data.center;
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec2 IntersectAABB(vec3 origin, vec3 invDir, vec3 bbMin, vec3 bbMax) {
    vec3 t0 = (bbMin - origin) * invDir;
    vec3 t1 = (bbMax - origin) * invDir;

    vec3 temp = t0;
    t0 = min(temp, t1), t1 = max(temp, t1);

    float tmin = max(max(t0.x, t0.y), t0.z);
    float tmax = min(min(t1.x, t1.y), t1.z);

    return vec2(tmin, tmax);
}

void main() {
    // Grid prep
    const vec3 boxSize = vec3(data.resolution.y*0.5*data.resolution.z);
    const float voxelGridSize = data.resolution.x*data.resolution.z;
    const float voxelSize = (data.resolution.y) / data.resolution.x;

    // Gay ass shit
    vec3 backgroundColor = vec3(0.1, 0.1, 0.2);
    outColor = vec4(backgroundColor, 1.0);

    // matrix inversion for ray prep
    mat4 invView = inverse(camera.view);
    mat4 invProjection = inverse(camera.projection);

    // Ray prep
    vec2 uv = vec2(gl_FragCoord.xy) / vec2(data.frame.xy);
    vec4 clip = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    vec4 viewSpace = vec4(invProjection * clip);
    viewSpace /= viewSpace.w;
    vec4 worldSpace = vec4(invView * vec4(viewSpace.xyz, 0.0));
    vec3 rayDirection = normalize(worldSpace.xyz);
    vec3 rayOrigin = invView[3].xyz-data.center;
    vec3 invRayDirection = 1.0/rayDirection;

    // first hit on grid
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

    // kinda useless now
    bool hitVoxel = false;

    // yeah
    const int MAX_STEPS = 512; // should be at max 128 at production, 16 for realtime vr performance

    const uint maxDepth = uint(log2(data.resolution.x));

    // DDA but needs to be replaced by cone tracing soon
    for(int steps = 0; steps < MAX_STEPS; steps++) {
        // Check current voxel
        uint lastDepth = maxDepth;
        if(all(greaterThanEqual(mapPos, ivec3(0))) && all(lessThan(mapPos, ivec3(voxelGridSize)))) {
            int idx = findVoxel(mapPos, maxDepth, maxDepth, lastDepth);

            if(idx < 0) {
                hitVoxel = true;

                vec3 lnormal = (unpackUnorm4x8(voxels[-idx-1].normal).xyz * 2.0) - vec3(1.0);

                // Apply lighting
                vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
                float diffuse = max(dot(lnormal, lightDir), 0.2); // Ambient + diffuse
                vec3 color = (vec3(0.8, 0.85, 0.9)/2+lnormal/2) * diffuse;

                vec3 viewDir = -rayDirection;
                vec3 halfwayDir = normalize(lightDir + viewDir);
                float spec = pow(max(dot(lnormal, halfwayDir), 0.0), 32.0);
                color += vec3(0.3) * spec;

                outColor += vec4(color, 1.0);

                //float t = float(steps)/float(MAX_STEPS);
                //outColor = vec4(hsv2rgb(vec3(0.7-t*4.14,1.0,1.0)), 1.0);
                //hitcount = 1;

                break;
            }
        }

        int skip = 1 << (maxDepth-lastDepth);
        ivec3 razor = mapPos/skip;

        // LSD mathematics
        float unit_size = data.resolution.y/float(skip);
        vec3 position = voxelRayOrigin+sideDist;
        position /= skip;

        while(razor == mapPos/skip) {
            // Find next voxel boundary
            vec3 mask = vec3(lessThanEqual(sideDist.xyz, min(sideDist.yzx, sideDist.zxy)));

            // Advance ray to next voxel
            sideDist += (mask) * deltaDist;
            mapPos += ivec3((mask)) * rayStep;
        }

        // Exit if outside grid bounds
        if(any(lessThan(mapPos, ivec3(0))) || any(greaterThanEqual(mapPos, ivec3(voxelGridSize)))
        || steps == MAX_STEPS - 1) {
            float t = float(steps)/float(MAX_STEPS);
            outColor = vec4(hsv2rgb(vec3(0.7-t*4.14,1.0,1.0)), 1.0);
            break;
        }
    }
}