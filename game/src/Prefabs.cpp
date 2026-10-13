#include "Prefabs.h"

#include <algorithm>

namespace FPS::Prefabs {

using namespace Forge;

Entity drone(Scene& scene, const glm::vec3& position) {
    Entity e = scene.createEntity("Drone");
    EntityInfo& info = scene.get<EntityInfo>(e);
    info.tag = "Enemy";
    scene.get<Transform>(e).position = position;
    MeshRenderer& mr = scene.add<MeshRenderer>(e);
    mr.mesh = "models/drone.glb";
    BoxCollider& box = scene.add<BoxCollider>(e);
    box.size = glm::vec3(1.3f, 0.8f, 1.3f);
    Enemy& enemy = scene.add<Enemy>(e);
    enemy.phase = Random::range(0.0f, 6.28f);
    enemy.fireTimer = Random::range(1.0f, 2.5f);
    enemy.strafeDir = Random::value() < 0.5f ? -1.0f : 1.0f;
    Health& hp = scene.add<Health>(e);
    hp.current = hp.max = 60.0f;

    // Thruster glow
    ParticleEmitter& em = scene.add<ParticleEmitter>(e);
    em.rate = 25.0f;
    em.lifetime = 0.35f;
    em.startSpeed = 2.0f;
    em.direction = glm::vec3(0, -1, 0);
    em.spread = 0.15f;
    em.startSize = 0.22f;
    em.endSize = 0.0f;
    em.startColor = glm::vec4(1.0f, 0.35f, 0.25f, 0.8f);
    em.endColor = glm::vec4(1.0f, 0.1f, 0.05f, 0.0f);
    return e;
}

Entity plasmaBolt(Scene& scene, const glm::vec3& position, const glm::vec3& velocity, float damage, Entity owner) {
    Entity e = scene.createEntity("Plasma Bolt");
    Transform& t = scene.get<Transform>(e);
    t.position = position;
    t.scale = glm::vec3(0.22f);
    MeshRenderer& mr = scene.add<MeshRenderer>(e);
    mr.mesh = "builtin:Sphere";
    mr.material = "materials/Plasma.mat";
    mr.castShadows = false;
    Projectile& p = scene.add<Projectile>(e);
    p.velocity = velocity;
    p.damage = damage;
    p.owner = owner;
    Light& l = scene.add<Light>(e);
    l.type = LightType::Point;
    l.color = glm::vec3(1.0f, 0.45f, 0.2f);
    l.intensity = 2.5f;
    l.range = 5.0f;
    ParticleEmitter& em = scene.add<ParticleEmitter>(e);
    em.rate = 60.0f;
    em.lifetime = 0.25f;
    em.startSpeed = 0.3f;
    em.spread = 1.0f;
    em.startSize = 0.2f; // particles are simulated in world space, sizes are in meters
    em.endSize = 0.0f;
    em.startColor = glm::vec4(1.0f, 0.5f, 0.2f, 0.9f);
    em.endColor = glm::vec4(1.0f, 0.15f, 0.05f, 0.0f);
    scene.add<Lifetime>(e).seconds = 6.0f;
    return e;
}

Entity impact(Scene& scene, const glm::vec3& position, const glm::vec3& normal, const glm::vec4& color) {
    Entity e = scene.createEntity("Impact");
    scene.get<Transform>(e).position = position + normal * 0.02f;
    ParticleEmitter& em = scene.add<ParticleEmitter>(e);
    em.loop = false;
    em.rate = 0.0f;
    em.burst = 14;
    em.lifetime = 0.35f;
    em.startSpeed = 5.0f;
    em.spread = 0.55f;
    em.direction = normal;
    em.gravity = 1.0f;
    em.drag = 2.0f;
    em.startSize = 0.09f;
    em.endSize = 0.0f;
    em.startColor = color;
    em.endColor = glm::vec4(glm::vec3(color) * 0.5f, 0.0f);
    scene.add<Lifetime>(e).seconds = 0.6f;
    return e;
}

Entity rocket(Scene& scene, const glm::vec3& position, const glm::vec3& velocity, float damage, float radius, Entity owner) {
    Entity e = scene.createEntity("Rocket");
    Transform& t = scene.get<Transform>(e);
    t.position = position;
    t.rotation = Math::lookRotation(velocity);
    MeshRenderer& mr = scene.add<MeshRenderer>(e);
    mr.mesh = "models/rocket.obj";
    mr.castShadows = false;
    Projectile& p = scene.add<Projectile>(e);
    p.velocity = velocity;
    p.damage = damage;
    p.owner = owner;
    p.explosionRadius = radius;
    Light& l = scene.add<Light>(e);
    l.type = LightType::Point;
    l.color = glm::vec3(1.0f, 0.6f, 0.3f);
    l.intensity = 3.0f;
    l.range = 6.0f;
    // Exhaust flame, emitted backwards (local +Z)...
    ParticleEmitter& em = scene.add<ParticleEmitter>(e);
    em.rate = 80.0f;
    em.lifetime = 0.18f;
    em.startSpeed = 3.0f;
    em.direction = glm::vec3(0, 0, 1);
    em.spread = 0.1f;
    em.startSize = 0.25f;
    em.endSize = 0.05f;
    em.startColor = glm::vec4(4.0f, 2.0f, 0.6f, 1.0f);
    em.endColor = glm::vec4(1.0f, 0.3f, 0.05f, 0.0f);
    // ...and a grey smoke trail that lingers behind it.
    Entity trail = scene.createEntity("Smoke Trail", e);
    scene.get<Transform>(trail).position = glm::vec3(0, 0, 0.25f);
    ParticleEmitter& smoke = scene.add<ParticleEmitter>(trail);
    smoke.blend = ParticleBlend::Alpha;
    smoke.rate = 60.0f;
    smoke.maxParticles = 200;
    smoke.lifetime = 1.1f;
    smoke.startSpeed = 0.6f;
    smoke.direction = glm::vec3(0, 0, 1);
    smoke.spread = 0.6f;
    smoke.drag = 1.5f;
    smoke.gravity = -0.03f;
    smoke.startSize = 0.18f;
    smoke.endSize = 0.9f;
    smoke.startColor = glm::vec4(0.75f, 0.72f, 0.68f, 0.5f);
    smoke.endColor = glm::vec4(0.45f, 0.45f, 0.45f, 0.0f);
    scene.add<Lifetime>(e).seconds = 5.0f;
    return e;
}

Entity explosion(Scene& scene, const glm::vec3& position, float size) {
    Entity e = scene.createEntity("Explosion");
    scene.get<Transform>(e).position = position;
    ParticleEmitter& fire = scene.add<ParticleEmitter>(e);
    fire.loop = false;
    fire.rate = 0.0f;
    fire.burst = (int)(70 * std::min(size, 2.0f));
    fire.maxParticles = 160;
    fire.lifetime = 0.8f;
    fire.startSpeed = 7.0f * size;
    fire.spread = 1.0f;
    fire.gravity = 0.4f;
    fire.drag = 3.0f;
    fire.startSize = 0.6f * size;
    fire.endSize = 0.05f;
    fire.startColor = glm::vec4(4.0f, 1.8f, 0.6f, 1.0f);
    fire.endColor = glm::vec4(0.6f, 0.1f, 0.05f, 0.0f);
    scene.add<Lifetime>(e).seconds = 2.2f;

    if (size > 1.2f) {
        // Big blasts: dark debris thrown upwards plus a slow smoke column.
        Entity debris = scene.createEntity("Debris", e);
        ParticleEmitter& d = scene.add<ParticleEmitter>(debris);
        d.blend = ParticleBlend::Alpha;
        d.loop = false;
        d.rate = 0.0f;
        d.burst = 26;
        d.lifetime = 1.6f;
        d.startSpeed = 9.0f;
        d.direction = glm::vec3(0, 1, 0);
        d.spread = 0.55f;
        d.gravity = 1.0f;
        d.drag = 0.4f;
        d.startSize = 0.14f;
        d.endSize = 0.1f;
        d.startColor = glm::vec4(0.08f, 0.07f, 0.06f, 1.0f);
        d.endColor = glm::vec4(0.05f, 0.05f, 0.05f, 0.0f);

        Entity smoke = scene.createEntity("Smoke", e);
        ParticleEmitter& s = scene.add<ParticleEmitter>(smoke);
        s.blend = ParticleBlend::Alpha;
        s.loop = false;
        s.rate = 0.0f;
        s.burst = 30;
        s.lifetime = 2.0f;
        s.startSpeed = 2.5f;
        s.direction = glm::vec3(0, 1, 0);
        s.spread = 0.5f;
        s.gravity = -0.08f;
        s.drag = 1.5f;
        s.startSize = 0.8f;
        s.endSize = 2.2f;
        s.startColor = glm::vec4(0.35f, 0.33f, 0.31f, 0.55f);
        s.endColor = glm::vec4(0.2f, 0.2f, 0.2f, 0.0f);
    }

    Entity flash = scene.createEntity("Explosion Light", e);
    Light& l = scene.add<Light>(flash);
    l.type = LightType::Point;
    l.color = glm::vec3(1.0f, 0.6f, 0.3f);
    l.intensity = 8.0f * size;
    l.range = 10.0f * size;
    scene.add<Lifetime>(flash).seconds = 0.12f + 0.06f * size;
    return e;
}

} // namespace FPS::Prefabs
