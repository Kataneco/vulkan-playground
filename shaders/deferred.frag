#version 460
layout(set = 0, binding = 0, rgba8) uniform readonly image2D gColor; //albedo
layout(set = 0, binding = 1, rgba32f) uniform readonly image2D gPosition; //fragment position (world space)
layout(set = 0, binding = 2, rgba32f) uniform readonly image2D gNormalSpec; //normal at xyz, specular at w
layout(push_constant) uniform PushConstants {
    mat4x4 projection;
    ivec2 screenSize;
    float time;
} pc;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 3) uniform sampler2D texNoise;
layout(set = 0, binding = 4) uniform readonly block {
    vec3 samples[64];
} ssaoKernel;
const float radius = 0.55;
const float bias = 0.025;
const int kernelSize = 4;

void main() {
    const vec2 noiseScale = vec2(pc.screenSize) / 2.0f;
    const vec2 TexCoords = gl_FragCoord.xy / pc.screenSize;
    const vec4 fragPos = imageLoad(gPosition, ivec2(gl_FragCoord.xy));
    const vec4 fragColor = imageLoad(gColor, ivec2(gl_FragCoord.xy));
    const vec3 normal = imageLoad(gNormalSpec, ivec2(gl_FragCoord.xy)).xyz;
    const vec3 randomVec = texture(texNoise, TexCoords * noiseScale).xyz;
    const vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    const vec3 bitangent = cross(normal, tangent);
    const mat3 TBN = mat3(tangent, bitangent, normal);
    const mat4x4 projection = pc.projection;
    float occlusion = 0.0;
    for (int i = 0; i < kernelSize; ++i) {
        vec3 samplePos = TBN * ssaoKernel.samples[i];
        samplePos = fragPos.xyz + samplePos * radius;
        vec4 offset = vec4(samplePos, 1.0);
        offset = projection * offset;
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;
        float sampleDepth = imageLoad(gPosition, ivec2(offset.xy * pc.screenSize)).z;
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }
    occlusion = 1.0 - (occlusion / kernelSize);
    float ambient = -0.05f;
    outColor = vec4(fragColor.xyz * clamp((occlusion+ambient), 0, 1), 1.0f);
}
