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
    mat4 norm_mat;
}pc;

void main() {
    gl_Position = ubo.projection * pc.model * vec4(v_pos, 1.0);
    f_tex = v_tex;
    f_pos = vec3(pc.model * vec4(v_pos, 1.0));      // Multiply times model matrix
    f_norm = normalize(mat3(transpose(inverse(pc.model))) * v_norm);    // Multiply times transpose_inverse model matrix
}
