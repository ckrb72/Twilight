#version 450


layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_tex;
layout(location = 2) in vec3 v_norm;

layout(location = 0) out vec2 f_tex;
layout(location = 1) out vec3 f_pos;
layout(location = 2) out vec3 f_norm;

layout(set = 0, binding = 0) uniform global_ubo
{
    mat4 projection;
    mat4 view;
}ubo;

layout(push_constant) uniform pconstant
{
    mat4 model;
}pc;

void main() {
    gl_Position = ubo.projection * ubo.view * pc.model * vec4(v_pos, 1.0);
    f_tex = v_tex;
    f_pos = v_pos;      // Multiply times model matrix
    f_norm = v_norm;    // Multiply times inverse_transpose model matrix
}
