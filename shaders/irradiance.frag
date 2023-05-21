#version 450

layout(location = 0) in vec3 fragUVW;

layout(set = 0, binding = 0) uniform samplerCube samplerCubeMap; 

// layout(set = 1, binding = 2) uniform sampler2D ao_sampler;
// layout(set = 1, binding = 3) uniform sampler2D metal_roughness_sampler;

layout(location = 0) out vec4 out_color;

#define PI 3.1415926535897932384626433832795

void main() {
    vec3 N = normalize(fragUVW);
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up = cross(N, right);

    vec3 irradiance = vec3(0.0);
    float delta = 0.025;
    uint sample_count = 0;
    for (float phi = 0.0; phi < 2.0 * PI; phi += delta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += delta) {
            vec3 tangent = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            vec3 sample_vec = tangent.x * right + tangent.y * up + tangent.z * N;
            irradiance += texture(samplerCubeMap, sample_vec).rgb * cos(theta) * sin(theta);
            sample_count++;
        }
    }

    irradiance = PI * irradiance * (1.0 / float(sample_count));
    out_color = vec4(irradiance, 1.0);
}