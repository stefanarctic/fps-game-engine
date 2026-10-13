#version 330 core
// Draws an orange outline around the selection mask (Unity-style).

#type vertex
#include "FullscreenVS.glsl"

#type fragment
in vec2 v_UV;
layout(location = 0) out vec4 o_Color;
uniform sampler2D u_Mask;
uniform vec2 u_TexelSize;
uniform vec4 u_Color;

void main() {
    float center = texture(u_Mask, v_UV).r;
    if (center > 0.5) discard;
    float edge = 0.0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            if (abs(x) + abs(y) > 3) continue;
            edge = max(edge, texture(u_Mask, v_UV + vec2(x, y) * u_TexelSize).r);
        }
    }
    if (edge < 0.5) discard;
    o_Color = u_Color;
}
