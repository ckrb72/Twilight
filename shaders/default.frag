#version 450

layout(location = 0) in vec2 f_tex;
layout(location = 1) in vec3 f_pos;
layout(location = 2) in vec3 f_norm;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D tex;

void main() {
    out_color = vec4(f_norm, 1.0);
    //out_color = texture(tex, f_tex);
}