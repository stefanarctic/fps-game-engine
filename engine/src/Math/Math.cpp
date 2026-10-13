#include "Forge/Math/Math.h"

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <algorithm>
#include <cmath>

namespace Forge {

AABB AABB::transformed(const glm::mat4& m) const {
    if (!valid()) return AABB();
    glm::vec3 newMin(m[3]), newMax(m[3]);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            float a = m[j][i] * min[j];
            float b = m[j][i] * max[j];
            newMin[i] += std::min(a, b);
            newMax[i] += std::max(a, b);
        }
    }
    return AABB(newMin, newMax);
}

void Frustum::fromMatrix(const glm::mat4& m) {
    // Gribb/Hartmann plane extraction (column-major GLM matrices).
    glm::vec4 row0(m[0][0], m[1][0], m[2][0], m[3][0]);
    glm::vec4 row1(m[0][1], m[1][1], m[2][1], m[3][1]);
    glm::vec4 row2(m[0][2], m[1][2], m[2][2], m[3][2]);
    glm::vec4 row3(m[0][3], m[1][3], m[2][3], m[3][3]);
    planes[0] = row3 + row0; // left
    planes[1] = row3 - row0; // right
    planes[2] = row3 + row1; // bottom
    planes[3] = row3 - row1; // top
    planes[4] = row3 + row2; // near
    planes[5] = row3 - row2; // far
    for (glm::vec4& p : planes) {
        float len = glm::length(glm::vec3(p));
        if (len > 0.0f) p /= len;
    }
}

bool Frustum::intersects(const AABB& box) const {
    if (!box.valid()) return false;
    for (const glm::vec4& p : planes) {
        glm::vec3 positive(p.x >= 0 ? box.max.x : box.min.x, p.y >= 0 ? box.max.y : box.min.y, p.z >= 0 ? box.max.z : box.min.z);
        if (glm::dot(glm::vec3(p), positive) + p.w < 0.0f) return false;
    }
    return true;
}

namespace Math {

bool rayAABB(const Ray& ray, const AABB& box, float& tNear, float maxDistance, glm::vec3* normal) {
    float tmin = 0.0f, tmax = maxDistance;
    int hitAxis = -1;
    float hitSign = 0.0f;
    for (int i = 0; i < 3; ++i) {
        float d = ray.direction[i];
        float o = ray.origin[i];
        if (std::fabs(d) < 1e-8f) {
            if (o < box.min[i] || o > box.max[i]) return false;
            continue;
        }
        float inv = 1.0f / d;
        float t1 = (box.min[i] - o) * inv;
        float t2 = (box.max[i] - o) * inv;
        float sign = -1.0f;
        if (t1 > t2) { std::swap(t1, t2); sign = 1.0f; }
        if (t1 > tmin) { tmin = t1; hitAxis = i; hitSign = sign; }
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return false;
    }
    tNear = tmin;
    if (normal) {
        *normal = glm::vec3(0.0f);
        if (hitAxis >= 0) (*normal)[hitAxis] = hitSign;
        else *normal = -ray.direction; // origin inside the box
    }
    return true;
}

bool rayOBB(const Ray& ray, const AABB& localBox, const glm::mat4& world, const glm::mat4& invWorld,
            float& t, glm::vec3* worldNormal, float maxDistance) {
    // Transform the ray into box space without renormalizing the direction, so
    // the ray parameter t is identical in both spaces.
    Ray local;
    local.origin = glm::vec3(invWorld * glm::vec4(ray.origin, 1.0f));
    local.direction = glm::vec3(invWorld * glm::vec4(ray.direction, 0.0f));
    glm::vec3 n;
    if (!rayAABB(local, localBox, t, maxDistance, &n)) return false;
    if (worldNormal) {
        glm::mat3 normalMatrix = glm::transpose(glm::mat3(invWorld));
        glm::vec3 wn = normalMatrix * n;
        float len = glm::length(wn);
        *worldNormal = len > 0.0f ? wn / len : -ray.direction;
    }
    (void)world;
    return true;
}

bool rayTriangle(const Ray& ray, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float& t) {
    const float eps = 1e-8f;
    glm::vec3 e1 = b - a, e2 = c - a;
    glm::vec3 p = glm::cross(ray.direction, e2);
    float det = glm::dot(e1, p);
    if (std::fabs(det) < eps) return false;
    float inv = 1.0f / det;
    glm::vec3 s = ray.origin - a;
    float u = glm::dot(s, p) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    glm::vec3 q = glm::cross(s, e1);
    float v = glm::dot(ray.direction, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    t = glm::dot(e2, q) * inv;
    return t >= 0.0f;
}

glm::mat4 eulerToMatrix(const glm::vec3& e) {
    return glm::eulerAngleYXZ(glm::radians(e.y), glm::radians(e.x), glm::radians(e.z));
}

glm::quat eulerToQuat(const glm::vec3& e) { return glm::quat_cast(eulerToMatrix(e)); }

glm::vec3 matrixToEuler(const glm::mat3& r) {
    float y, x, z;
    glm::extractEulerAngleYXZ(glm::mat4(r), y, x, z);
    return glm::degrees(glm::vec3(x, y, z));
}

glm::mat4 compose(const glm::vec3& position, const glm::vec3& eulerDegrees, const glm::vec3& scale) {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
    m *= eulerToMatrix(eulerDegrees);
    m = glm::scale(m, scale);
    return m;
}

bool decompose(const glm::mat4& m, glm::vec3& position, glm::vec3& eulerDegrees, glm::vec3& scale) {
    position = glm::vec3(m[3]);
    glm::vec3 c0(m[0]), c1(m[1]), c2(m[2]);
    scale = glm::vec3(glm::length(c0), glm::length(c1), glm::length(c2));
    if (scale.x < 1e-8f || scale.y < 1e-8f || scale.z < 1e-8f) return false;
    // Handle mirrored matrices by flipping one axis.
    if (glm::dot(glm::cross(c0, c1), c2) < 0.0f) scale.x = -scale.x;
    glm::mat3 rot(c0 / scale.x, c1 / scale.y, c2 / scale.z);
    eulerDegrees = matrixToEuler(rot);
    return true;
}

glm::vec3 directionFromYawPitch(float yawDeg, float pitchDeg) {
    glm::mat4 r = eulerToMatrix(glm::vec3(pitchDeg, yawDeg, 0.0f));
    return glm::normalize(glm::vec3(r * glm::vec4(0, 0, -1, 0)));
}

glm::vec3 lookRotation(const glm::vec3& dir) {
    glm::vec3 d = glm::normalize(dir);
    float yaw = glm::degrees(std::atan2(-d.x, -d.z));
    float pitch = glm::degrees(std::asin(glm::clamp(d.y, -1.0f, 1.0f)));
    return glm::vec3(pitch, yaw, 0.0f);
}

float damp(float sharpness, float dt) { return 1.0f - std::exp(-sharpness * dt); }

} // namespace Math

namespace {
uint32_t g_RandomState = 0x9E3779B9u;
}

void Random::seed(uint32_t s) { g_RandomState = s ? s : 0x9E3779B9u; }

uint32_t Random::next() {
    uint32_t x = g_RandomState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_RandomState = x;
    return x;
}

float Random::value() { return (float)(next() >> 8) * (1.0f / 16777216.0f); }
float Random::range(float a, float b) { return a + (b - a) * value(); }
int Random::range(int a, int b) { return b <= a ? a : a + (int)(next() % (uint32_t)(b - a)); }

glm::vec3 Random::insideUnitSphere() {
    for (int i = 0; i < 16; ++i) {
        glm::vec3 p(range(-1.0f, 1.0f), range(-1.0f, 1.0f), range(-1.0f, 1.0f));
        if (glm::dot(p, p) <= 1.0f) return p;
    }
    return glm::vec3(0.0f);
}

glm::vec3 Random::onUnitSphere() {
    glm::vec3 p = insideUnitSphere();
    float len = glm::length(p);
    return len > 1e-5f ? p / len : glm::vec3(0, 1, 0);
}

} // namespace Forge
