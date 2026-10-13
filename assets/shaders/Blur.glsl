#version 330 core
// Separable 9-tap gaussian blur (direction given in UV units).

#type vertex
#include "FullscreenVS.glsl"

#type fragment
in vec2 v_UV;
layout(location = 0) out vec4 o_Color;
uniform sampler2D u_Input;
uniform vec2 u_Direction;

void main() {
    const float w[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 result = texture(u_Input, v_UV).rgb * w[0];
    for (int i = 1; i < 5; ++i) {
        result += texture(u_Input, v_UV + u_Direction * float(i)).rgb * w[i];
        result += texture(u_Input, v_UV - u_Direction * float(i)).rgb * w[i];
    }
    o_Color = vec4(result, 1.0);
}
