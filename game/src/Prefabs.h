#pragma once
// Runtime-spawned entities (code "prefabs").
#include "GameComponents.h"

namespace FPS::Prefabs {

Entity drone(Forge::Scene& scene, const glm::vec3& position);
Entity plasmaBolt(Forge::Scene& scene, const glm::vec3& position, const glm::vec3& velocity, float damage, Entity owner);
// Player rocket: flies straight, explodes on impact (Projectile::explosionRadius).
Entity rocket(Forge::Scene& scene, const glm::vec3& position, const glm::vec3& velocity, float damage, float radius, Entity owner);
Entity impact(Forge::Scene& scene, const glm::vec3& position, const glm::vec3& normal, const glm::vec4& color);
// Fireball + light. `size` scales the effect (1 = drone death, ~2 = barrel / rocket).
Entity explosion(Forge::Scene& scene, const glm::vec3& position, float size = 1.0f);

} // namespace FPS::Prefabs
