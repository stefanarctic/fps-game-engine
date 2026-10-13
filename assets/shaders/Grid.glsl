#version 330 core
// Infinite editor ground grid on the XZ plane (1m minor, 10m major lines).

#type vertex
uniform mat4 u_InvViewProj;
out vec3 v_Near;
out vec3 v_Far;
vec3 unproject(vec2 ndc, float z) {
    vec4 p = u_InvViewProj * vec4(ndc, z, 1.0);
    return p.xyz / p.w;
}
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vec2 ndc = p * 2.0 - 1.0;
    v_Near = unproject(ndc, -1.0);
    v_Far = unproject(ndc, 1.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}

#type fragment
in vec3 v_Near;
in vec3 v_Far;
layout(location = 0) out vec4 o_Color;
uniform mat4 u_ViewProj;
uniform vec3 u_CameraPos;

float gridLine(vec2 coord, float scale) {
    vec2 c = coord / scale;
    vec2 d = fwidth(c);
    vec2 g = abs(fract(c - 0.5) - 0.5) / d;
    return 1.0 - min(min(g.x, g.y), 1.0);
}

void main() {
    float t = -v_Near.y / (v_Far.y - v_Near.y);
    if (t <= 0.0) discard;
    vec3 P = v_Near + t * (v_Far - v_Near);

    vec4 clip = u_ViewProj * vec4(P, 1.0);
    gl_FragDepth = (clip.z / clip.w) * 0.5 + 0.5;

    float minor = gridLine(P.xz, 1.0);
    float major = gridLine(P.xz, 10.0);
    float dist = length(P.xz - u_CameraPos.xz);
    float fadeRange = 40.0 + abs(u_CameraPos.y) * 6.0;
    float fade = 1.0 - smoothstep(fadeRange * 0.3, fadeRange, dist);

    vec3 color = vec3(0.55);
    float alpha = max(minor * 0.18, major * 0.35);

    // World axes: X in red, Z in blue (like Unity's scene grid).
    vec2 dAxis = fwidth(P.xz);
    if (abs(P.z) < dAxis.y * 1.5) { color = vec3(0.85, 0.25, 0.25); alpha = 0.7; }
    if (abs(P.x) < dAxis.x * 1.5) { color = vec3(0.25, 0.45, 0.9); alpha = 0.7; }

    alpha *= fade;
    if (alpha < 0.01) discard;
    o_Color = vec4(pow(color, vec3(2.2)), alpha);
}
