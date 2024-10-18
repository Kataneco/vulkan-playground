#version 460

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec4 gPosition;
layout(location = 2) in vec4 gNormalSpec;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outPosition;
layout(location = 2) out vec4 outNormalSpec;

void main() {
    outColor = texture(texSampler, texCoord);
    if (outColor.a > 0.99f) discard;
    outPosition = vec4(gPosition.xyzw);
    outNormalSpec = gNormalSpec;
}