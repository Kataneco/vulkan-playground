#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 color;

layout(location = 0) out vec4 fragmentColor;

layout(push_constant) uniform block {
    mat4 renderMatrix;
    double time;
} constants;

layout(set = 0, binding = 0) buffer AtomicCounter {
    uint ac;
    uint vac;
};

float rng( in vec2 pos ) {
    return fract(sin(pos.y + pos.x * 78.233) * 43758.5453) * 2.0 - 1.0;
}

float perlin( in float pos ) {
    float a = rng(vec2(floor(pos), 1.0));
    float b = rng(vec2(ceil(pos), 1.0));

    float a_x = rng(vec2(floor(pos), 2.0));
    float b_x = rng(vec2(ceil(pos), 2.0));

    a += a_x * fract(pos);
    b += b_x * (fract(pos) - 1.0);
    return a + (b - a) * smoothstep(0.0, 1.0, fract(pos));
}

void main() {
    const float time = float(constants.time);
    if(gl_VertexIndex == 0) {
        atomicExchange(ac, 0);
        atomicExchange(vac, 0);
    }
    uint vaci = atomicAdd(vac, 1);

    gl_Position = constants.renderMatrix * vec4(position.xyz+(normal.xyz+0.42f*vec3(perlin(time),perlin(time),perlin(time)))*(0.002f+perlin(time)/72)*perlin(time*42), 1.0f);
    fragmentColor = vec4(normal, time*0.42f);
}
