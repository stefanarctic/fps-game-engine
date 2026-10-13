#pragma once
// Damage and explosions shared by weapons, projectiles and explosive props.
#include "GameComponents.h"

namespace FPS::Combat {

// Applies damage to anything with a Health component and plays its hit reaction
// (drone flash + knockback, player damage vignette). Returns false when the
// entity cannot be damaged. `direction` is the travel direction of the hit.
bool applyDamage(Forge::Scene& scene, Entity target, float amount, const glm::vec3& direction, Entity instigator = NullEntity);

// Radial damage with linear falloff and line-of-sight checks, impulses on rigid
// bodies, camera shake and the visual effect. Explosives caught in the blast
// detonate after a short fuse (chain reactions).
void explode(Forge::Scene& scene, const glm::vec3& center, float radius, float damage, float force, Entity instigator = NullEntity,
             Entity ignore = NullEntity);

// World-space center of an entity's collider (falls back to its position).
glm::vec3 colliderCenter(Forge::Scene& scene, Entity e);

} // namespace FPS::Combat
