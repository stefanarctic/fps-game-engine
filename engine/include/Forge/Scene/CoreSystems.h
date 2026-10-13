#pragma once
#include "Forge/ECS/System.h"

namespace Forge {

// Integrates RigidBody velocities and resolves collisions against BoxColliders.
class RigidBodySystem : public System {
public:
    RigidBodySystem() { priority = SystemPriority::Physics; }
    const char* name() const override { return "RigidBodySystem"; }
    void onUpdate(Scene& scene, float dt) override;
};

// Simulates CPU particles for every ParticleEmitter.
class ParticleSystem : public System {
public:
    ParticleSystem() { priority = SystemPriority::Particles; }
    const char* name() const override { return "ParticleSystem"; }
    void onUpdate(Scene& scene, float dt) override;
    void onStop(Scene& scene) override;
};

// Counts down Lifetime components and destroys expired entities.
class LifetimeSystem : public System {
public:
    LifetimeSystem() { priority = SystemPriority::Lifetime; }
    const char* name() const override { return "LifetimeSystem"; }
    void onUpdate(Scene& scene, float dt) override;
};

} // namespace Forge
