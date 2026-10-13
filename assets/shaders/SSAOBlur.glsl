#version 330 core
// 4x4 box blur matching the SSAO noise tile size.

#type vertex
#include "FullscreenVS.glsl"

#type fragment
in vec2 v_UV;
layout(location = 0) out float o_AO;
uniform sampler2D u_Input;
uniform vec2 u_TexelSize;

void main() {
    float sum = 0.0;
    for (int x = -2; x < 2; ++x)
        for (int y = -2; y < 2; ++y)
            sum += texture(u_Input, v_UV + (vec2(x, y) + 0.5) * u_TexelSize).r;
    o_AO = sum / 16.0;
}
