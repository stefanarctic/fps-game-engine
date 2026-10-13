#pragma once
// Minimal game physics: oriented box colliders for raycasts, axis-aligned
// bounds for movement. Good enough for FPS level geometry, crates and enemies.
#include "Forge/ECS/Registry.h"
#include "Forge/Math/Math.h"

namespace Forge {

class Scene;

struct ColliderData {
    Entity entity = NullEntity;
    AABB local;              // collider box in entity space
    AABB worldBounds;        // axis-aligned bounds in world space
    glm::mat4 world{1.0f};
    glm::mat4 invWorld{1.0f};
    bool trigger = false;
    bool dynamic = false;    // has a non-kinematic RigidBody
};

struct RaycastHit {
    Entity entity = NullEntity;
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f;
};

struct MoveResult {
    glm::vec3 position{0.0f};
    bool grounded = false;
    bool hitCeiling = false;
    glm::bvec3 blocked{false};
};

class PhysicsWorld {
public:
    glm::vec3 gravity{0.0f, -20.0f, 0.0f};

    // Collects every BoxCollider in the scene. Called by Scene::update.
    void rebuild(Scene& scene);
    // Refreshes a single collider after its entity moved mid-frame.
    void refresh(Scene& scene, Entity entity);

    // Closest hit along the ray (direction must be normalized).
    bool raycast(const Ray& ray, float maxDistance, RaycastHit& hit, Entity ignore = NullEntity, bool hitTriggers = false) const;
    // Every collider whose world bounds overlap the box.
    std::vector<Entity> overlapBox(const AABB& box, Entity ignore = NullEntity, bool includeTriggers = false) const;

    // Moves an axis-aligned box by `delta`, sliding along solid colliders.
    // Resolves Y first, then X and Z, sub-stepping large moves to avoid tunnelling.
    MoveResult moveBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec3& delta,
                       Entity ignore = NullEntity) const;

    const std::vector<ColliderData>& colliders() const { return m_Colliders; }

private:
    bool buildCollider(Scene& scene, Entity e, ColliderData& out) const;
    std::vector<ColliderData> m_Colliders;
};

} // namespace Forge
