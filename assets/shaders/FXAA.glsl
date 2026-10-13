#version 330 core
// Fast approximate anti-aliasing (FXAA 3.11 "lite" variant).

#type vertex
#include "FullscreenVS.glsl"

#type fragment
in vec2 v_UV;
layout(location = 0) out vec4 o_Color;
uniform sampler2D u_Input;
uniform vec2 u_TexelSize;
uniform int u_Enabled;

#define FXAA_REDUCE_MIN (1.0 / 128.0)
#define FXAA_REDUCE_MUL (1.0 / 8.0)
#define FXAA_SPAN_MAX 8.0

void main() {
    vec3 rgbM = texture(u_Input, v_UV).rgb;
    if (u_Enabled == 0) { o_Color = vec4(rgbM, 1.0); return; }

    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(texture(u_Input, v_UV + vec2(-1.0, -1.0) * u_TexelSize).rgb, luma);
    float lumaNE = dot(texture(u_Input, v_UV + vec2(1.0, -1.0) * u_TexelSize).rgb, luma);
    float lumaSW = dot(texture(u_Input, v_UV + vec2(-1.0, 1.0) * u_TexelSize).rgb, luma);
    float lumaSE = dot(texture(u_Input, v_UV + vec2(1.0, 1.0) * u_TexelSize).rgb, luma);
    float lumaM = dot(rgbM, luma);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-FXAA_SPAN_MAX), vec2(FXAA_SPAN_MAX)) * u_TexelSize;

    vec3 rgbA = 0.5 * (texture(u_Input, v_UV + dir * (1.0 / 3.0 - 0.5)).rgb +
                       texture(u_Input, v_UV + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (texture(u_Input, v_UV + dir * -0.5).rgb +
                                     texture(u_Input, v_UV + dir * 0.5).rgb);
    float lumaB = dot(rgbB, luma);
    o_Color = vec4((lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB, 1.0);
}
