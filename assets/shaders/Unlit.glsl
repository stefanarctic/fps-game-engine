#version 330 core
// Unlit surface: albedo color * texture + emission. Ignores all lights.

#type vertex
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_UV;

uniform mat4 u_ViewProj;
uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_UV;

void main() {
    vec4 world = u_Model * vec4(a_Position, 1.0);
    v_WorldPos = world.xyz;
    v_Normal = u_NormalMatrix * a_Normal;
    v_UV = a_UV;
    gl_Position = u_ViewProj * world;
}

#type fragment
#include "Common.glsl"

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_UV;
layout(location = 0) out vec4 o_Color;

uniform vec4 u_Albedo;
uniform sampler2D u_AlbedoMap;
uniform vec3 u_Emission;
uniform vec2 u_Tiling;
uniform int u_WorldSpaceUV;
uniform vec3 u_CameraPos;
uniform int u_DebugView;

void main() {
    vec3 N = normalize(v_Normal);
    vec4 tex = sampleSurface(u_AlbedoMap, v_UV, v_WorldPos, N, u_Tiling, u_WorldSpaceUV);
    vec3 color = u_Albedo.rgb * tex.rgb;
    if (u_DebugView == 1) { o_Color = vec4(1.0); return; }
    if (u_DebugView == 2) { o_Color = vec4(pow(N * 0.5 + 0.5, vec3(2.2)), 1.0); return; }
    if (u_DebugView == 3) { o_Color = vec4(color, 1.0); return; }
    color += u_Emission;
    color = applyFog(color, length(u_CameraPos - v_WorldPos));
    o_Color = vec4(color, 1.0);
}
