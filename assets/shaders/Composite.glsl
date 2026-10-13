#version 330 core
// HDR -> LDR: bloom add, exposure, ACES filmic tone mapping, gamma, vignette.
// Alpha stores luma for the FXAA pass.

#type vertex
#include "FullscreenVS.glsl"

#type fragment
in vec2 v_UV;
layout(location = 0) out vec4 o_Color;
uniform sampler2D u_Scene;
uniform sampler2D u_Bloom;
uniform float u_BloomIntensity;
uniform float u_Exposure;
uniform float u_Vignette;
uniform int u_Passthrough;

vec3 aces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(u_Scene, v_UV).rgb;
    vec3 color;
    if (u_Passthrough == 1) {
        color = pow(max(hdr, vec3(0.0)), vec3(1.0 / 2.2));
    } else {
        hdr += texture(u_Bloom, v_UV).rgb * u_BloomIntensity;
        color = aces(hdr * u_Exposure);
        color = pow(color, vec3(1.0 / 2.2));
        vec2 d = v_UV - 0.5;
        color *= clamp(1.0 - dot(d, d) * u_Vignette * 2.0, 0.0, 1.0);
    }
    o_Color = vec4(color, dot(color, vec3(0.299, 0.587, 0.114)));
}
