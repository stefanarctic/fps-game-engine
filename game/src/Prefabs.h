#pragma once
// Runtime-spawned entities (code "prefabs").
#include "GameComponents.h"

namespace FPS::Prefabs {

Entity drone(Forge::Scene& scene, const glm::vec3& position);
Entity plasmaBolt(Forge::Scene& scene, const glm::vec3& position, const glm::vec3& velocity, float damage, Entity owner);
Entity impact(Forge::Scene& scene, const glm::vec3& position, const glm::vec3& normal, const glm::vec4& color);
Entity explosion(Forge::Scene& scene, const glm::vec3& position);

} // namespace FPS::Prefabs
