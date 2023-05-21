#version 450
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout( push_constant) uniform PushConstantObject {
    mat4 model;
    vec4 data;
} pco;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec3 out_normal;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec3 fragUVW;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * pco.model * vec4(position, 1.0);
    fragUVW = vec3(ubo.model * pco.model * vec4(position, 1.0));
    out_uv = uv;
    out_normal = mat3(ubo.model * pco.model) * normal;
}