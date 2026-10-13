#version 330 core
// Writes 1 where the selected meshes are (selection outline mask).

#type vertex
layout(location = 0) in vec3 a_Position;
uniform mat4 u_ViewProj;
uniform mat4 u_Model;
void main() { gl_Position = u_ViewProj * u_Model * vec4(a_Position, 1.0); }

#type fragment
layout(location = 0) out float o_Mask;
void main() { o_Mask = 1.0; }
