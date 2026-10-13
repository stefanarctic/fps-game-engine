#include "Forge/Scene/CoreSystems.h"
#include "Forge/Scene/Scene.h"

#include <cmath>

namespace Forge {

void RigidBodySystem::onUpdate(Scene& scene, float dt) {
    if (dt <= 0.0f) return;
    PhysicsWorld& physics = scene.physics();

    scene.each<RigidBody, BoxCollider, Transform>([&](Entity e, RigidBody& rb, BoxCollider& box, Transform& t) {
        if (rb.isKinematic || !scene.isActive(e)) return;

        if (rb.useGravity) rb.velocity += physics.gravity * dt;
        rb.velocity *= 1.0f / (1.0f + rb.drag * dt);

        // Current world-space bounds of the collider.
        AABB local(box.center - box.size * 0.5f, box.center + box.size * 0.5f);
        AABB world = local.transformed(t.world);
        glm::vec3 center = world.center();
        glm::vec3 half = world.extents();

        MoveResult res = physics.moveBox(center, half, rb.velocity * dt, e);
        for (int axis = 0; axis < 3; ++axis) {
            if (!res.blocked[axis]) continue;
            float v = -rb.velocity[axis] * rb.bounciness;
            rb.velocity[axis] = std::fabs(v) < 0.5f ? 0.0f : v;
        }
        rb.grounded = res.grounded;
        if (rb.grounded) {
            float friction = 1.0f - Math::damp(6.0f, dt);
            rb.velocity.x *= friction;
            rb.velocity.z *= friction;
        }

        glm::vec3 moved = res.position - center;
        if (glm::dot(moved, moved) > 0.0f) {
            scene.setWorldPosition(e, t.worldPosition() + moved);
            t.world = scene.parentWorld(e) * t.localMatrix();
            physics.refresh(scene, e);
        }
    });
}

void ParticleSystem::onUpdate(Scene& scene, float dt) {
    glm::vec3 gravity = scene.physics().gravity;
    scene.each<ParticleEmitter, Transform>([&](Entity e, ParticleEmitter& em, Transform& t) {
        bool active = scene.isActive(e);

        // Simulate existing particles.
        for (size_t i = 0; i < em.particles.size();) {
            Particle& p = em.particles[i];
            p.age += dt;
            if (p.age >= p.lifetime) {
                em.particles[i] = em.particles.back();
                em.particles.pop_back();
                continue;
            }
            p.velocity += gravity * em.gravity * dt;
            p.velocity *= 1.0f / (1.0f + em.drag * dt);
            p.position += p.velocity * dt;
            ++i;
        }

        if (!active || !em.emitting) return;

        // Fresh world matrix: emitters spawned or moved by gameplay code this frame have
        // not been through Scene::updateTransforms yet (t.world would be stale/identity).
        glm::mat4 world = scene.parentWorld(e) * t.localMatrix();
        glm::vec3 origin = glm::vec3(world[3]);
        glm::vec3 dir = glm::mat3(world) * em.direction;
        float len = glm::length(dir);
        dir = len > 1e-5f ? dir / len : glm::vec3(0, 1, 0);

        auto spawn = [&]() {
            if ((int)em.particles.size() >= em.maxParticles) return;
            Particle p;
            p.position = origin;
            glm::vec3 d = glm::normalize(dir + Random::onUnitSphere() * em.spread * 1.5f + glm::vec3(1e-4f));
            p.velocity = d * em.startSpeed * Random::range(0.6f, 1.0f);
            p.lifetime = em.lifetime * Random::range(0.7f, 1.0f);
            em.particles.push_back(p);
        };

        if (!em.burstDone) {
            for (int i = 0; i < em.burst; ++i) spawn();
            em.burstDone = true;
        }
        // Non-looping emitters only emit their initial burst (impacts, explosions).
        if (em.loop && em.rate > 0.0f) {
            em.emitAccumulator += em.rate * dt;
            while (em.emitAccumulator >= 1.0f) {
                spawn();
                em.emitAccumulator -= 1.0f;
            }
        }
    });
}

void ParticleSystem::onStop(Scene& scene) {
    scene.each<ParticleEmitter>([](Entity, ParticleEmitter& em) {
        em.particles.clear();
        em.emitAccumulator = 0.0f;
        em.burstDone = false;
    });
}

void LifetimeSystem::onUpdate(Scene& scene, float dt) {
    scene.each<Lifetime>([&](Entity e, Lifetime& life) {
        life.seconds -= dt;
        if (life.seconds <= 0.0f) scene.destroyEntity(e);
    });
}

} // namespace Forge
