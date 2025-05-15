#version 450

layout(location = 0) in vec2 f_tex;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(f_tex, 0.0, 1.0);
}