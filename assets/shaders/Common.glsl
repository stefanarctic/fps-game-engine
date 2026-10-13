// Shared helpers for surface shaders.
const float PI = 3.14159265359;

uniform int u_FogEnabled;
uniform vec3 u_FogColor;
uniform float u_FogDensity;

// Exponential-squared distance fog.
vec3 applyFog(vec3 color, float distance) {
    if (u_FogEnabled == 0) return color;
    float f = 1.0 - exp(-pow(distance * u_FogDensity, 2.0));
    return mix(color, u_FogColor, clamp(f, 0.0, 1.0));
}

// Samples a texture either with mesh UVs or with world-space triplanar projection.
vec4 sampleSurface(sampler2D tex, vec2 uv, vec3 worldPos, vec3 normal, vec2 tiling, int worldSpace) {
    if (worldSpace == 0) return texture(tex, uv * tiling);
    vec3 w = pow(abs(normal), vec3(4.0));
    w /= (w.x + w.y + w.z + 1e-5);
    vec4 x = texture(tex, worldPos.zy * tiling);
    vec4 y = texture(tex, worldPos.xz * tiling);
    vec4 z = texture(tex, worldPos.xy * tiling);
    return x * w.x + y * w.y + z * w.z;
}
