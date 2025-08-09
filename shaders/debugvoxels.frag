#version 460
precision highp int;

struct Node {
    int parent;
    int occlusion; // amount of total children
    int emission; // pointer to voxel that sums up all child voxels
    uint generation; // node age
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

layout(std430, set = 1, binding = 4) buffer NodeGenerationBuffer {
    uint nodeGeneration;
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

        if (childPointer < 0) {
            return childPointer;
        }

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
    return pos-vec3(-data.resolution.y*data.resolution.z*0.5);//+data.center;
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

void nodeAABB(in vec3 pos, in uint depth, out vec3 bbMin, out vec3 bbMax) {
    int skip = 1 << (depth);
    float unit_size = data.resolution.y/float(skip);

    bbMin = floor(pos/unit_size)*unit_size;
    bbMax = bbMin + vec3(unit_size);
}

void main() {
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

    const uint maxDepth = uint(log2(data.resolution.x));

    float gridSize = data.resolution.y * data.resolution.z;
    vec3 gridMin = vec3(-gridSize * 0.5);
    vec3 gridMax = vec3(gridSize * 0.5);

    vec2 tGrid = IntersectAABB(rayOrigin, invRayDirection, gridMin, gridMax);
    if (tGrid.y < 0.0 || tGrid.x > tGrid.y) {
        return;
    }

    float t = max(tGrid.x, 0.0);
    float maxT = tGrid.y;

    const float stepSize = data.resolution.y;
    const int maxSteps = 256;

    for (int step = 0; step < maxSteps && t < maxT; ++step) {
        outColor.rgb = hsv2rgb(vec3(float(step)/maxSteps, 1.0, 1.0));

        vec3 rayPos = rayOrigin + rayDirection * t;
        vec3 gridPos = worldToGrid(rayPos);

        ivec3 voxelCoord = ivec3(floor(gridPos * (data.resolution.x/data.resolution.y)));

        uint foundDepth = maxDepth;
        int result = findVoxel(voxelCoord, maxDepth, maxDepth, foundDepth);

        if (result < 0) {
            int voxelIndex = -result - 1;
            Voxel voxel = voxels[voxelIndex];

            vec3 color = unpackUnorm4x8(voxel.color).rgb;
            vec3 normal = (unpackUnorm4x8(voxel.normal).xyz * 2.0) - vec3(1.0);

            // Simple lighting
            vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
            float lighting = max(0.2, dot(normal, lightDir));

            outColor = vec4(color * lighting, 1.0);
            return;
        }

        vec3 bbMin, bbMax;
        nodeAABB(rayPos, foundDepth, bbMin, bbMax);

        vec2 tNode = IntersectAABB(rayOrigin, invRayDirection, bbMin, bbMax);

        t = tNode.y + stepSize * 0.001;
    }
}