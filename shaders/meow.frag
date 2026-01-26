#version 460

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform DefaultInputs {
    vec4 frame;
};

void main() {
    outColor = vec4(gl_FragCoord.xy/frame.xy, sin(frame.z), 1.0);
}