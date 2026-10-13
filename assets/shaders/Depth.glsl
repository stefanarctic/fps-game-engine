#version 330 core
// Shadow map depth-only pass.

#type vertex
layout(location = 0) in vec3 a_Position;
uniform mat4 u_LightSpace;
uniform mat4 u_Model;
void main() {
    gl_Position = u_LightSpace * u_Model * vec4(a_Position, 1.0);
}

#type fragment
void main() {}
