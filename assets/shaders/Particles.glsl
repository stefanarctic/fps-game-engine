#version 330 core
// Camera-facing additive particles with a soft round falloff.

#type vertex
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_UV;
uniform mat4 u_ViewProj;
out vec4 v_Color;
out vec2 v_UV;
void main() {
    v_Color = a_Color;
    v_UV = a_UV;
    gl_Position = u_ViewProj * vec4(a_Position, 1.0);
}

#type fragment
in vec4 v_Color;
in vec2 v_UV;
layout(location = 0) out vec4 o_Color;
void main() {
    float r = length(v_UV * 2.0 - 1.0);
    float falloff = 1.0 - smoothstep(0.2, 1.0, r);
    o_Color = vec4(v_Color.rgb, v_Color.a * falloff);
}
