#version 450

layout(location = 0) in vec2 f_tex;
layout(location = 1) in vec3 f_pos;
layout(location = 2) in vec3 f_norm;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler2D tex;

vec3 light_pos = vec3(1.0, 1.0, 1.0);
vec3 light_color = vec3(1.0, 1.0, 1.0);

void main() {

    vec3 ambient = 0.1 * light_color;

    vec3 light_dir = normalize(light_pos - f_pos);
    vec3 diffuse = light_color * max(dot(f_norm, light_dir), 0.0);

    out_color = vec4(texture(tex, f_tex).rgb * (ambient + diffuse), 1.0);
}