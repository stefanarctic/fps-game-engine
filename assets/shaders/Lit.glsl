#version 330 core
// Standard lit surface: Cook-Torrance PBR (metallic/roughness) with a
// directional sun (PCF shadows), up to 16 point/spot lights, hemispheric
// ambient light modulated by screen-space ambient occlusion, and fog.

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

// Material
uniform vec4 u_Albedo;
uniform sampler2D u_AlbedoMap;
uniform float u_Metallic;
uniform float u_Roughness;
uniform vec3 u_Emission;
uniform vec2 u_Tiling;
uniform int u_WorldSpaceUV;

// Camera / debug
uniform vec3 u_CameraPos;
uniform int u_DebugView; // 0 lit, 1 AO, 2 normals, 3 albedo, 4 lighting only

// Environment
uniform vec3 u_SkyColor;
uniform vec3 u_HorizonColor;
uniform vec3 u_GroundColor;
uniform float u_AmbientIntensity;

// Sun
uniform int u_HasSun;
uniform vec3 u_SunDirection; // direction the light travels
uniform vec3 u_SunColor;

// Punctual lights
#define MAX_LIGHTS 16
uniform int u_LightCount;
uniform vec4 u_LightPosRange[MAX_LIGHTS];  // xyz position, w range
uniform vec4 u_LightColorType[MAX_LIGHTS]; // rgb color * intensity, w type (1 point, 2 spot)
uniform vec4 u_LightDirCone[MAX_LIGHTS];   // xyz spot direction, w cos(outer half angle)

// Shadows
uniform int u_ShadowsEnabled;
uniform sampler2D u_ShadowMap;
uniform mat4 u_LightSpace;
uniform float u_ShadowBias;
uniform float u_ShadowSoftness;
uniform vec2 u_ShadowTexel;

// SSAO
uniform int u_UseSSAO;
uniform sampler2D u_SSAO;
uniform vec2 u_ScreenSize;

float distributionGGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d + 1e-6);
}

float geometrySchlickGGX(float NdotX, float k) { return NdotX / (NdotX * (1.0 - k) + k); }

float geometrySmith(float NdotV, float NdotL, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return geometrySchlickGGX(NdotV, k) * geometrySchlickGGX(NdotL, k);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) { return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); }

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

// Radiance leaving the surface towards V for one light. Light intensities use
// Unity's convention (intensity 1 = fully lit white diffuse), hence the * PI.
vec3 shade(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo, float metallic, float roughness) {
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) return vec3(0.0);
    float NdotV = max(dot(N, V), 1e-4);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlick(HdotV, F0);
    float D = distributionGGX(NdotH, roughness);
    float G = geometrySmith(NdotV, NdotL, roughness);
    vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 1e-4);
    vec3 kd = (vec3(1.0) - F) * (1.0 - metallic);
    return (kd * albedo / PI + specular) * radiance * PI * NdotL;
}

float sunShadow(vec3 N, vec3 L) {
    if (u_ShadowsEnabled == 0) return 1.0;
    // Normal offset reduces acne on surfaces facing away from the light.
    vec3 p = v_WorldPos + N * 0.03;
    vec4 ls = u_LightSpace * vec4(p, 1.0);
    vec3 proj = ls.xyz / ls.w * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) return 1.0;
    float bias = max(u_ShadowBias * 3.0 * (1.0 - dot(N, L)), u_ShadowBias);
    float shadow = 0.0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            float depth = texture(u_ShadowMap, proj.xy + vec2(x, y) * u_ShadowTexel * u_ShadowSoftness).r;
            shadow += (proj.z - bias > depth) ? 1.0 : 0.0;
        }
    }
    shadow /= 25.0;
    // Fade out towards the edge of the shadow distance.
    vec2 edge = abs(proj.xy * 2.0 - 1.0);
    float fade = smoothstep(0.8, 1.0, max(edge.x, edge.y));
    return 1.0 - shadow * (1.0 - fade);
}

vec3 hemisphere(vec3 dir) {
    float y = dir.y;
    return y >= 0.0 ? mix(u_HorizonColor, u_SkyColor, y) : mix(u_HorizonColor, u_GroundColor, -y);
}

void main() {
    vec3 N = normalize(v_Normal);
    if (!gl_FrontFacing) N = -N;
    vec3 V = normalize(u_CameraPos - v_WorldPos);

    vec4 tex = sampleSurface(u_AlbedoMap, v_UV, v_WorldPos, N, u_Tiling, u_WorldSpaceUV);
    vec3 albedo = u_Albedo.rgb * tex.rgb;
    float metallic = clamp(u_Metallic, 0.0, 1.0);
    float roughness = clamp(u_Roughness, 0.03, 1.0);

    float ao = 1.0;
    if (u_UseSSAO == 1) ao = texture(u_SSAO, gl_FragCoord.xy / u_ScreenSize).r;

    if (u_DebugView == 1) { o_Color = vec4(vec3(pow(ao, 2.2)), 1.0); return; }
    if (u_DebugView == 2) { o_Color = vec4(pow(N * 0.5 + 0.5, vec3(2.2)), 1.0); return; }
    if (u_DebugView == 3) { o_Color = vec4(albedo, 1.0); return; }
    if (u_DebugView == 4) { albedo = vec3(0.7); metallic = 0.0; }

    vec3 direct = vec3(0.0);
    if (u_HasSun == 1) {
        vec3 L = normalize(-u_SunDirection);
        direct += shade(N, V, L, u_SunColor, albedo, metallic, roughness) * sunShadow(N, L);
    }
    for (int i = 0; i < MAX_LIGHTS; ++i) {
        if (i >= u_LightCount) break;
        vec3 toLight = u_LightPosRange[i].xyz - v_WorldPos;
        float dist = length(toLight);
        float range = u_LightPosRange[i].w;
        if (dist >= range) continue;
        vec3 L = toLight / max(dist, 1e-4);
        // Unity-style falloff: bright core, smooth fade to zero at the range.
        float nd = dist / range;
        float attenuation = 1.0 / (1.0 + 25.0 * nd * nd) * clamp((1.0 - nd) * 5.0, 0.0, 1.0);
        if (u_LightColorType[i].w > 1.5) {
            float cosOuter = u_LightDirCone[i].w;
            float cosInner = mix(cosOuter, 1.0, 0.25);
            attenuation *= smoothstep(cosOuter, cosInner, dot(-L, normalize(u_LightDirCone[i].xyz)));
        }
        direct += shade(N, V, L, u_LightColorType[i].rgb * attenuation, albedo, metallic, roughness);
    }

    // Ambient: hemisphere light, diffuse + rough specular, occluded by SSAO.
    float NdotV = max(dot(N, V), 1e-4);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kd = (1.0 - F) * (1.0 - metallic);
    vec3 diffuseAmbient = hemisphere(N) * albedo * kd;
    vec3 R = reflect(-V, N);
    vec3 specularAmbient = hemisphere(R) * F * (1.0 - roughness * 0.75);
    vec3 ambient = (diffuseAmbient + specularAmbient) * u_AmbientIntensity * ao;

    // A little occlusion on direct light too keeps contact areas grounded.
    vec3 color = direct * mix(1.0, ao, 0.25) + ambient + u_Emission;
    color = applyFog(color, length(u_CameraPos - v_WorldPos));
    o_Color = vec4(color, 1.0);
}
