#include "Forge/Renderer/Primitives.h"

#include <cmath>

namespace Forge {
namespace Primitives {

namespace {
const float PI = Math::PI;

void addQuad(std::vector<Vertex>& v, std::vector<uint32_t>& idx, const glm::vec3& c, const glm::vec3& right,
             const glm::vec3& up, const glm::vec3& normal) {
    uint32_t base = (uint32_t)v.size();
    v.push_back({c - right - up, normal, {0, 0}});
    v.push_back({c + right - up, normal, {1, 0}});
    v.push_back({c + right + up, normal, {1, 1}});
    v.push_back({c - right + up, normal, {0, 1}});
    idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}
} // namespace

Ref<Mesh> cube() {
    std::vector<Vertex> v;
    std::vector<uint32_t> i;
    const float h = 0.5f;
    addQuad(v, i, {0, 0, h}, {h, 0, 0}, {0, h, 0}, {0, 0, 1});    // front
    addQuad(v, i, {0, 0, -h}, {-h, 0, 0}, {0, h, 0}, {0, 0, -1}); // back
    addQuad(v, i, {h, 0, 0}, {0, 0, -h}, {0, h, 0}, {1, 0, 0});   // right
    addQuad(v, i, {-h, 0, 0}, {0, 0, h}, {0, h, 0}, {-1, 0, 0});  // left
    addQuad(v, i, {0, h, 0}, {h, 0, 0}, {0, 0, -h}, {0, 1, 0});   // top
    addQuad(v, i, {0, -h, 0}, {h, 0, 0}, {0, 0, h}, {0, -1, 0});  // bottom
    return MakeRef<Mesh>(v, i, "Cube");
}

Ref<Mesh> sphere(int segments, int rings) {
    std::vector<Vertex> v;
    std::vector<uint32_t> idx;
    for (int r = 0; r <= rings; ++r) {
        float phi = PI * (float)r / (float)rings;
        for (int s = 0; s <= segments; ++s) {
            float theta = 2.0f * PI * (float)s / (float)segments;
            glm::vec3 n(std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta));
            v.push_back({n * 0.5f, n, {(float)s / segments, 1.0f - (float)r / rings}});
        }
    }
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < segments; ++s) {
            uint32_t a = (uint32_t)(r * (segments + 1) + s);
            uint32_t b = a + (uint32_t)segments + 1;
            idx.insert(idx.end(), {a, a + 1, b, b, a + 1, b + 1});
        }
    }
    return MakeRef<Mesh>(v, idx, "Sphere");
}

Ref<Mesh> plane(int subdivisions) {
    std::vector<Vertex> v;
    std::vector<uint32_t> idx;
    int n = subdivisions < 1 ? 1 : subdivisions;
    for (int z = 0; z <= n; ++z) {
        for (int x = 0; x <= n; ++x) {
            float fx = (float)x / n, fz = (float)z / n;
            v.push_back({{-5.0f + 10.0f * fx, 0.0f, 5.0f - 10.0f * fz}, {0, 1, 0}, {fx, fz}});
        }
    }
    for (int z = 0; z < n; ++z) {
        for (int x = 0; x < n; ++x) {
            uint32_t a = (uint32_t)(z * (n + 1) + x);
            uint32_t b = a + (uint32_t)n + 1;
            idx.insert(idx.end(), {a, a + 1, b + 1, a, b + 1, b});
        }
    }
    return MakeRef<Mesh>(v, idx, "Plane");
}

Ref<Mesh> quad() {
    std::vector<Vertex> v;
    std::vector<uint32_t> i;
    addQuad(v, i, {0, 0, 0}, {0.5f, 0, 0}, {0, 0.5f, 0}, {0, 0, 1});
    return MakeRef<Mesh>(v, i, "Quad");
}

Ref<Mesh> cylinder(int segments) {
    std::vector<Vertex> v;
    std::vector<uint32_t> idx;
    const float r = 0.5f, h = 1.0f;
    // Side
    for (int s = 0; s <= segments; ++s) {
        float t = 2.0f * PI * (float)s / segments;
        glm::vec3 n(std::cos(t), 0.0f, std::sin(t));
        float u = (float)s / segments;
        v.push_back({{n.x * r, -h, n.z * r}, n, {u, 0}});
        v.push_back({{n.x * r, h, n.z * r}, n, {u, 1}});
    }
    for (int s = 0; s < segments; ++s) {
        uint32_t a = (uint32_t)(s * 2);
        idx.insert(idx.end(), {a, a + 1, a + 3, a, a + 3, a + 2});
    }
    // Caps
    for (int cap = 0; cap < 2; ++cap) {
        float y = cap == 0 ? h : -h;
        glm::vec3 n(0, cap == 0 ? 1.0f : -1.0f, 0);
        uint32_t center = (uint32_t)v.size();
        v.push_back({{0, y, 0}, n, {0.5f, 0.5f}});
        for (int s = 0; s <= segments; ++s) {
            float t = 2.0f * PI * (float)s / segments;
            v.push_back({{std::cos(t) * r, y, std::sin(t) * r}, n, {0.5f + std::cos(t) * 0.5f, 0.5f + std::sin(t) * 0.5f}});
        }
        for (int s = 0; s < segments; ++s) {
            uint32_t a = center + 1 + (uint32_t)s;
            if (cap == 0) idx.insert(idx.end(), {center, a + 1, a});
            else idx.insert(idx.end(), {center, a, a + 1});
        }
    }
    return MakeRef<Mesh>(v, idx, "Cylinder");
}

Ref<Mesh> capsule(int segments, int rings) {
    std::vector<Vertex> v;
    std::vector<uint32_t> idx;
    const float r = 0.5f, halfCyl = 0.5f;
    // Latitude rows: top hemisphere (rings+1 rows), bottom hemisphere (rings+1 rows).
    int rows = 0;
    for (int hemi = 0; hemi < 2; ++hemi) {
        for (int ring = 0; ring <= rings; ++ring) {
            float phi = (hemi == 0 ? 0.0f : PI * 0.5f) + (PI * 0.5f) * (float)ring / rings;
            float yOffset = hemi == 0 ? halfCyl : -halfCyl;
            for (int s = 0; s <= segments; ++s) {
                float theta = 2.0f * PI * (float)s / segments;
                glm::vec3 n(std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta));
                glm::vec3 p = n * r + glm::vec3(0, yOffset, 0);
                v.push_back({p, n, {(float)s / segments, (p.y + 1.0f) * 0.5f}});
            }
            rows++;
        }
    }
    for (int row = 0; row < rows - 1; ++row) {
        for (int s = 0; s < segments; ++s) {
            uint32_t a = (uint32_t)(row * (segments + 1) + s);
            uint32_t b = a + (uint32_t)segments + 1;
            idx.insert(idx.end(), {a, a + 1, b, b, a + 1, b + 1});
        }
    }
    return MakeRef<Mesh>(v, idx, "Capsule");
}

} // namespace Primitives
} // namespace Forge
