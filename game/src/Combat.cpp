#include "Combat.h"
#include "GameSystems.h"
#include "Prefabs.h"

#include <algorithm>
#include <cmath>

namespace FPS::Combat {

using namespace Forge;

glm::vec3 colliderCenter(Scene& scene, Entity e) {
    const Transform& t = scene.get<Transform>(e);
    if (const BoxCollider* box = scene.tryGet<BoxCollider>(e)) return glm::vec3(t.world * glm::vec4(box->center, 1.0f));
    return t.worldPosition();
}

bool applyDamage(Scene& scene, Entity target, float amount, const glm::vec3& direction, Entity instigator) {
    if (!scene.valid(target) || !scene.isActive(target)) return false;
    Health* hp = scene.tryGet<Health>(target);
    if (!hp || hp->current <= 0.0f) return false;

    if (Player* player = scene.tryGet<Player>(target)) {
        if (player->dead) return false;
        hp->current = std::max(0.0f, hp->current - amount);
        player->damageFlash = 1.0f;
        return true;
    }

    hp->current -= amount;
    if (Enemy* enemy = scene.tryGet<Enemy>(target)) {
        enemy->hitFlash = 0.12f;
        enemy->velocity += direction * std::min(amount * 0.14f, 8.0f);
    }
    // Hit marker for anything the player damages.
    if (scene.valid(instigator) && instigator != target && scene.has<Player>(instigator))
        if (GameManager* gm = findGameManager(scene)) gm->hitMarker = 0.15f;
    return true;
}

void explode(Scene& scene, const glm::vec3& center, float radius, float damage, float force, Entity instigator, Entity ignore) {
    Prefabs::explosion(scene, center, std::max(1.0f, radius / 3.0f));
    if (radius <= 0.0f) return;
    const PhysicsWorld& physics = scene.physics();

    // Damage: linear falloff to 25% at the edge, blocked by solid geometry.
    for (Entity e : scene.registry().view<Health, Transform>()) {
        if (e == ignore || !scene.isActive(e)) continue;
        glm::vec3 to = colliderCenter(scene, e) - center;
        float dist = glm::length(to);
        if (dist > radius) continue;
        glm::vec3 dir = dist > 1e-3f ? to / dist : glm::vec3(0, 1, 0);
        if (dist > 0.3f) {
            RaycastHit hit;
            if (physics.raycast(Ray{center, dir}, dist, hit, ignore) && hit.entity != e && hit.distance < dist - 0.4f) continue;
        }
        float falloff = 1.0f - 0.75f * (dist / radius);
        float amount = damage * falloff;
        if (e == instigator) amount *= 0.4f; // self damage (rocket jumps hurt less)

        if (Explosive* ex = scene.tryGet<Explosive>(e)) {
            // Chain reaction: neighbours detonate a moment later instead of all at once.
            Health& hp = scene.get<Health>(e);
            if (hp.current <= 0.0f || ex->exploded) continue;
            hp.current -= amount;
            if (hp.current <= 0.0f && ex->fuse < 0.0f) ex->fuse = Random::range(0.12f, 0.3f);
            continue;
        }
        applyDamage(scene, e, amount, dir, instigator);

        // Knock the player around (rocket jumping).
        if (CharacterController* cc = scene.tryGet<CharacterController>(e)) {
            cc->velocity += dir * force * falloff * 0.6f + glm::vec3(0.0f, force * falloff * 0.35f, 0.0f);
            cc->grounded = false;
        }
    }

    // Impulses on loose props.
    scene.each<RigidBody, Transform>([&](Entity e, RigidBody& rb, Transform&) {
        if (rb.isKinematic || e == ignore || !scene.isActive(e)) return;
        glm::vec3 to = colliderCenter(scene, e) - center;
        float dist = glm::length(to);
        if (dist > radius) return;
        float k = (1.0f - dist / radius) * force / std::max(rb.mass, 0.1f);
        glm::vec3 dir = dist > 1e-3f ? to / dist : glm::vec3(0, 1, 0);
        rb.velocity += dir * k + glm::vec3(0.0f, k * 0.6f, 0.0f);
    });

    // Camera shake for nearby players.
    scene.each<Player, Transform>([&](Entity, Player& p, Transform& t) {
        float dist = glm::distance(t.worldPosition(), center);
        float reach = radius * 3.0f;
        if (dist < reach) p.shake = std::min(1.0f, p.shake + (1.0f - dist / reach) * 0.9f);
    });
}

} // namespace FPS::Combat
