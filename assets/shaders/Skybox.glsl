#version 330 core
// Procedural gradient sky with a sun disc (similar to Unity's procedural skybox).

#type vertex
uniform mat4 u_InvViewProj; // inverse(projection * rotation-only view)
out vec3 v_Direction;
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vec2 ndc = p * 2.0 - 1.0;
    vec4 world = u_InvViewProj * vec4(ndc, 1.0, 1.0);
    v_Direction = world.xyz / world.w;
    gl_Position = vec4(ndc, 1.0, 1.0); // at the far plane
}

#type fragment
in vec3 v_Direction;
layout(location = 0) out vec4 o_Color;

uniform vec3 u_SkyColor;
uniform vec3 u_HorizonColor;
uniform vec3 u_GroundColor;
uniform int u_HasSun;
uniform vec3 u_SunDirection;
uniform vec3 u_SunColor;

void main() {
    vec3 d = normalize(v_Direction);
    float y = d.y;
    vec3 sky = mix(u_HorizonColor, u_SkyColor, pow(clamp(y, 0.0, 1.0), 0.45));
    vec3 ground = mix(u_HorizonColor, u_GroundColor, pow(clamp(-y, 0.0, 1.0), 0.3));
    vec3 color = mix(ground, sky, smoothstep(-0.01, 0.01, y));
    if (u_HasSun == 1) {
        vec3 s = normalize(-u_SunDirection);
        float c = max(dot(d, s), 0.0);
        color += u_SunColor * (pow(c, 1500.0) * 25.0 + pow(c, 64.0) * 0.3 + pow(c, 6.0) * 0.05);
    }
    o_Color = vec4(color, 1.0);
}
