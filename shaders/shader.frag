#version 450

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec2 out_uv;
layout(set = 1, binding = 0) uniform sampler2D color_sampler;
layout(set = 1, binding = 1) uniform sampler2D normal_sampler;
layout(set = 1, binding = 2) uniform sampler2D ao_sampler;
layout(set = 1, binding = 3) uniform sampler2D metal_roughness_sampler;


layout(location = 0) out vec4 out_color;

void main() {
    out_color = vec4(texture(metal_roughness_sampler, out_uv).rgb, 2.2);
}