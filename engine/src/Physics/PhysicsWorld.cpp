#include "Forge/Physics/PhysicsWorld.h"
#include "Forge/Scene/Scene.h"

#include <cmath>

namespace Forge {

bool PhysicsWorld::buildCollider(Scene& scene, Entity e, ColliderData& c) const {
    BoxCollider* box = scene.tryGet<BoxCollider>(e);
    if (!box || !scene.isActive(e)) return false;
    const Transform& t = scene.get<Transform>(e);
    c.entity = e;
    c.local = AABB(box->center - box->size * 0.5f, box->center + box->size * 0.5f);
    c.world = t.world;
    c.invWorld = glm::inverse(t.world);
    c.worldBounds = c.local.transformed(t.world);
    c.trigger = box->isTrigger;
    RigidBody* rb = scene.tryGet<RigidBody>(e);
    c.dynamic = rb && !rb->isKinematic;
    return true;
}

void PhysicsWorld::rebuild(Scene& scene) {
    m_Colliders.clear();
    scene.each<BoxCollider>([&](Entity e, BoxCollider&) {
        ColliderData c;
        if (buildCollider(scene, e, c)) m_Colliders.push_back(c);
    });
}

void PhysicsWorld::refresh(Scene& scene, Entity e) {
    for (ColliderData& c : m_Colliders) {
        if (c.entity != e) continue;
        ColliderData updated;
        if (buildCollider(scene, e, updated)) c = updated;
        return;
    }
}

bool PhysicsWorld::raycast(const Ray& ray, float maxDistance, RaycastHit& hit, Entity ignore, bool hitTriggers) const {
    bool found = false;
    float best = maxDistance;
    for (const ColliderData& c : m_Colliders) {
        if (c.entity == ignore || (c.trigger && !hitTriggers)) continue;
        // Cheap broad phase against the world bounds first.
        float tBroad;
        if (!Math::rayAABB(ray, c.worldBounds, tBroad, best)) continue;
        float t;
        glm::vec3 n;
        if (Math::rayOBB(ray, c.local, c.world, c.invWorld, t, &n, best) && t < best) {
            best = t;
            hit.entity = c.entity;
            hit.distance = t;
            hit.point = ray.at(t);
            hit.normal = n;
            found = true;
        }
    }
    return found;
}

std::vector<Entity> PhysicsWorld::overlapBox(const AABB& box, Entity ignore, bool includeTriggers) const {
    std::vector<Entity> out;
    for (const ColliderData& c : m_Colliders) {
        if (c.entity == ignore || (c.trigger && !includeTriggers)) continue;
        if (c.worldBounds.overlaps(box)) out.push_back(c.entity);
    }
    return out;
}

MoveResult PhysicsWorld::moveBox(const glm::vec3& center, const glm::vec3& half, const glm::vec3& delta, Entity ignore) const {
    MoveResult result;
    glm::vec3 pos = center;
    const float skin = 0.001f;

    // Sub-step so fast movers cannot tunnel through thin walls.
    float maxComponent = std::max(std::fabs(delta.x), std::max(std::fabs(delta.y), std::fabs(delta.z)));
    float minHalf = std::max(0.05f, std::min(half.x, std::min(half.y, half.z)));
    int steps = (int)std::ceil(maxComponent / minHalf);
    steps = steps < 1 ? 1 : (steps > 32 ? 32 : steps);
    glm::vec3 stepDelta = delta / (float)steps;

    const int axisOrder[3] = {1, 0, 2};
    for (int s = 0; s < steps; ++s) {
        for (int axis : axisOrder) {
            float d = stepDelta[axis];
            if (d == 0.0f || result.blocked[axis]) continue;
            pos[axis] += d;
            AABB box(pos - half, pos + half);
            for (const ColliderData& c : m_Colliders) {
                if (c.entity == ignore || c.trigger) continue;
                if (!c.worldBounds.overlaps(box)) continue;
                if (d > 0.0f) {
                    pos[axis] = c.worldBounds.min[axis] - half[axis] - skin;
                    if (axis == 1) result.hitCeiling = true;
                } else {
                    pos[axis] = c.worldBounds.max[axis] + half[axis] + skin;
                    if (axis == 1) result.grounded = true;
                }
                result.blocked[axis] = true;
                box = AABB(pos - half, pos + half);
            }
        }
    }
    result.position = pos;
    return result;
}

} // namespace Forge
