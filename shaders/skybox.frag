#version 450

layout(location = 0) in vec3 fragUVW;

layout(set = 0, binding = 1) uniform samplerCube samplerCubeMap; 

// layout(set = 1, binding = 2) uniform sampler2D ao_sampler;
// layout(set = 1, binding = 3) uniform sampler2D metal_roughness_sampler;


layout(location = 0) out vec4 out_color;

void main() {
    vec3 env_color = texture(samplerCubeMap, fragUVW).rgb;
    env_color = env_color / (env_color + vec3(1.0));
    env_color = pow(env_color, vec3(1.0 / 2.2));
    out_color = vec4(env_color, 1.0);
}