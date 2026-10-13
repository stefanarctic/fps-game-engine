#version 330 core
// Bloom bright-pass with a soft knee, downsampled to half resolution.

#type vertex
#include "FullscreenVS.glsl"

#type fragment
in vec2 v_UV;
layout(location = 0) out vec4 o_Color;
uniform sampler2D u_Input;
uniform float u_Threshold;
uniform vec2 u_TexelSize;

void main() {
    vec3 c = texture(u_Input, v_UV + vec2(-1.0, -1.0) * u_TexelSize).rgb;
    c += texture(u_Input, v_UV + vec2(1.0, -1.0) * u_TexelSize).rgb;
    c += texture(u_Input, v_UV + vec2(-1.0, 1.0) * u_TexelSize).rgb;
    c += texture(u_Input, v_UV + vec2(1.0, 1.0) * u_TexelSize).rgb;
    c = min(c * 0.25, vec3(64.0)); // tame fireflies

    float brightness = max(c.r, max(c.g, c.b));
    float knee = max(u_Threshold * 0.5, 1e-4);
    float soft = clamp(brightness - u_Threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);
    float contribution = max(soft, brightness - u_Threshold) / max(brightness, 1e-4);
    o_Color = vec4(c * contribution, 1.0);
}
