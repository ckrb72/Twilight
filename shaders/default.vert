#version 450


layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec2 v_tex;
layout(location = 2) in vec3 v_norm;

layout(location = 0) out vec2 f_tex;
layout(location = 1) out vec3 f_pos;
layout(location = 2) out vec3 f_norm;

layout(push_constant) uniform pc
{
    mat4 projection;
};

void main() {
    gl_Position = projection * vec4(v_pos, 1.0);
    f_tex = v_tex;
    f_pos = v_pos;
    f_norm = v_norm;
}
