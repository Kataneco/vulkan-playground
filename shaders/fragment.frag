#version 460

layout(location = 0) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) buffer AtomicCounter {
    uint ac;
    uint vac;
};

vec3 hsl2rgb(in vec3 hsl) {
    vec3 rgb = clamp(abs(mod(hsl[0] + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return hsl[2] + hsl[1] * (rgb - 0.5) * (1.0 - abs(2.0 * hsl[2] - 1.0));
}

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
    uint aci = atomicAdd(ac, 1);

    vec3 pack = vec3(float(aci)/(1600*900), 0.75f, 0.5f);
    float sexmod = sin(fragColor.w*7)+perlin(fragColor.x*fragColor.w+rng(fragColor.yz))*perlin(fragColor.y*fragColor.w*7+rng(fragColor.zx))*(sin(fragColor.w)*4.2f+1.2f);
    pack.x += sexmod;
    vec3 color = hsl2rgb(vec3(pack.x, pack.x-perlin(fragColor.w*10), pack.z+0.1f*sexmod));

    outColor = vec4(color, 1.0f);
}