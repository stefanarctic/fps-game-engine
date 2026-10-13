#pragma once
// Math types built on GLM plus the geometric helpers the engine needs.
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cfloat>
#include <cstdint>

namespace Forge {

struct AABB {
    glm::vec3 min{FLT_MAX};
    glm::vec3 max{-FLT_MAX};

    AABB() = default;
    AABB(const glm::vec3& mn, const glm::vec3& mx) : min(mn), max(mx) {}

    bool valid() const { return min.x <= max.x && min.y <= max.y && min.z <= max.z; }
    glm::vec3 center() const { return (min + max) * 0.5f; }
    glm::vec3 extents() const { return (max - min) * 0.5f; }
    glm::vec3 size() const { return max - min; }
    void expand(const glm::vec3& p) { min = glm::min(min, p); max = glm::max(max, p); }
    void expand(const AABB& b) { if (b.valid()) { expand(b.min); expand(b.max); } }
    bool contains(const glm::vec3& p) const { return glm::all(glm::greaterThanEqual(p, min)) && glm::all(glm::lessThanEqual(p, max)); }
    bool overlaps(const AABB& o) const {
        return min.x < o.max.x && max.x > o.min.x && min.y < o.max.y && max.y > o.min.y && min.z < o.max.z && max.z > o.min.z;
    }
    // Bounds of this box after an affine transform (Arvo's method).
    AABB transformed(const glm::mat4& m) const;
};

struct Ray {
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};
    glm::vec3 at(float t) const { return origin + direction * t; }
};

struct Frustum {
    glm::vec4 planes[6]; // ax + by + cz + d >= 0 means inside
    void fromMatrix(const glm::mat4& viewProj);
    bool intersects(const AABB& box) const;
};

namespace Math {

constexpr float PI = 3.14159265358979323846f;

// Slab test. Returns entry distance in tNear (0 if origin inside).
bool rayAABB(const Ray& ray, const AABB& box, float& tNear, float maxDistance = FLT_MAX, glm::vec3* normal = nullptr);
// Ray vs oriented box given by local bounds and world matrix. Distances are in world units
// when the ray direction is normalized.
bool rayOBB(const Ray& ray, const AABB& localBox, const glm::mat4& world, const glm::mat4& invWorld,
            float& t, glm::vec3* worldNormal = nullptr, float maxDistance = FLT_MAX);
// Moller-Trumbore ray/triangle intersection (double sided).
bool rayTriangle(const Ray& ray, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float& t);

// Euler angles are in degrees, applied Y (yaw) * X (pitch) * Z (roll) like Unity.
glm::quat eulerToQuat(const glm::vec3& eulerDegrees);
glm::mat4 eulerToMatrix(const glm::vec3& eulerDegrees);
glm::vec3 matrixToEuler(const glm::mat3& rotation); // degrees
glm::mat4 compose(const glm::vec3& position, const glm::vec3& eulerDegrees, const glm::vec3& scale);
bool decompose(const glm::mat4& m, glm::vec3& position, glm::vec3& eulerDegrees, glm::vec3& scale);

// Direction a camera with this yaw/pitch (degrees) looks at (-Z forward convention).
glm::vec3 directionFromYawPitch(float yawDeg, float pitchDeg);
// Euler (degrees) that orients -Z along `dir`.
glm::vec3 lookRotation(const glm::vec3& dir);

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
// Frame-rate independent exponential smoothing factor.
float damp(float sharpness, float dt);

} // namespace Math

// Fast, seedable random numbers (xorshift).
class Random {
public:
    static void seed(uint32_t s);
    static uint32_t next();
    static float value();                         // [0,1)
    static float range(float a, float b);         // [a,b)
    static int range(int a, int b);               // [a,b)
    static glm::vec3 insideUnitSphere();
    static glm::vec3 onUnitSphere();
};

} // namespace Forge
