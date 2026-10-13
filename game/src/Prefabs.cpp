#include "Prefabs.h"

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

Entity explosion(Scene& scene, const glm::vec3& position) {
    Entity e = scene.createEntity("Explosion");
    scene.get<Transform>(e).position = position;
    ParticleEmitter& fire = scene.add<ParticleEmitter>(e);
    fire.loop = false;
    fire.rate = 0.0f;
    fire.burst = 70;
    fire.maxParticles = 128;
    fire.lifetime = 0.8f;
    fire.startSpeed = 7.0f;
    fire.spread = 1.0f;
    fire.gravity = 0.4f;
    fire.drag = 3.0f;
    fire.startSize = 0.6f;
    fire.endSize = 0.05f;
    fire.startColor = glm::vec4(4.0f, 1.8f, 0.6f, 1.0f);
    fire.endColor = glm::vec4(0.6f, 0.1f, 0.05f, 0.0f);
    scene.add<Lifetime>(e).seconds = 1.2f;

    Entity flash = scene.createEntity("Explosion Light", e);
    Light& l = scene.add<Light>(flash);
    l.type = LightType::Point;
    l.color = glm::vec3(1.0f, 0.6f, 0.3f);
    l.intensity = 8.0f;
    l.range = 10.0f;
    scene.add<Lifetime>(flash).seconds = 0.15f;
    return e;
}

} // namespace FPS::Prefabs
