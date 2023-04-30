#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 out_uv;
layout(set = 1, binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(texSampler, out_uv);
}