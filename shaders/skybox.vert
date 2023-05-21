#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

layout(set = 0, binding = 0) uniform UniformBuffer {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec3 fragUVW;

void main() {
    gl_Position = ubo.proj * mat4(mat3(ubo.view)) * vec4(position, 1.0);
    fragUVW = position;
}