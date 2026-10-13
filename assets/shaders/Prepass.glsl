#version 330 core
// Depth + view-space normal prepass feeding the SSAO pass.

#type vertex
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
uniform mat4 u_ViewProj;
uniform mat4 u_Model;
uniform mat3 u_NormalMatrix; // inverse transpose of (view * model)
out vec3 v_Normal;
void main() {
    v_Normal = u_NormalMatrix * a_Normal;
    gl_Position = u_ViewProj * u_Model * vec4(a_Position, 1.0);
}

#type fragment
in vec3 v_Normal;
layout(location = 0) out vec3 o_Normal;
void main() {
    vec3 n = normalize(v_Normal);
    o_Normal = gl_FrontFacing ? n : -n;
}
