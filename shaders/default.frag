#version 450

layout(location = 0) in vec2 f_tex;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D tex;

void main() {
    out_color = vec4(f_tex, 0.0, 1.0);
    out_color = texture(tex, f_tex);
}