#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

layout(push_constant) uniform PushConstantObject {
    layout (offset = 0) mat4 proj;
} pco;

layout(location = 0) out vec3 fragUVW;


void main() {
    gl_Position = pco.proj * vec4(position, 1.0);
    fragUVW = position;
}