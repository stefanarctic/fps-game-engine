#version 330 core
// Screen-space ambient occlusion (normal-oriented hemisphere kernel).
// View-space positions are reconstructed from the prepass depth buffer.

#type vertex
#include "FullscreenVS.glsl"

#type fragment
in vec2 v_UV;
layout(location = 0) out float o_AO;

uniform sampler2D u_Depth;
uniform sampler2D u_Normal;
uniform sampler2D u_Noise;
uniform vec3 u_Samples[64];
uniform int u_KernelSize;
uniform float u_Radius;
uniform float u_Bias;
uniform float u_Power;
uniform mat4 u_Projection;
uniform mat4 u_InvProjection;
uniform vec2 u_NoiseScale;

vec3 viewPosition(vec2 uv) {
    float depth = texture(u_Depth, uv).r;
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = u_InvProjection * clip;
    return view.xyz / view.w;
}

void main() {
    float depth = texture(u_Depth, v_UV).r;
    if (depth >= 1.0) { o_AO = 1.0; return; }

    vec3 P = viewPosition(v_UV);
    vec3 N = texture(u_Normal, v_UV).xyz;
    if (dot(N, N) < 0.25) { o_AO = 1.0; return; }
    N = normalize(N);

    // Random rotation around the normal, tiled every 4x4 pixels (removed by the blur).
    vec3 rnd = normalize(vec3(texture(u_Noise, v_UV * u_NoiseScale).xy, 0.0) + vec3(0.0, 0.0, 1e-3));
    vec3 T = normalize(rnd - N * dot(rnd, N));
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);

    float occlusion = 0.0;
    int count = 0;
    for (int i = 0; i < 64; ++i) {
        if (i >= u_KernelSize) break;
        vec3 s = P + TBN * u_Samples[i] * u_Radius;
        vec4 offset = u_Projection * vec4(s, 1.0);
        vec2 suv = (offset.xy / offset.w) * 0.5 + 0.5;
        if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) continue;
        float sceneZ = viewPosition(suv).z;
        float rangeCheck = smoothstep(0.0, 1.0, u_Radius / max(abs(P.z - sceneZ), 1e-4));
        occlusion += (sceneZ >= s.z + u_Bias ? 1.0 : 0.0) * rangeCheck;
        count++;
    }
    float ao = 1.0 - occlusion / float(max(count, 1));
    o_AO = pow(clamp(ao, 0.0, 1.0), u_Power);
}
