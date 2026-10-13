#pragma once
#include "Forge/Core/Json.h"
#include "Forge/Scene/Scene.h"

namespace Forge {

// Scenes are stored as JSON:
// {
//   "name": "Arena",
//   "environment": { ...reflected Environment fields... },
//   "entities": [
//     { "uuid": "0123abcd...", "name": "Crate", "tag": "Untagged", "active": true,
//       "parent": "0000000000000000",
//       "components": { "Transform": {...}, "Mesh Renderer": {...} } }
//   ]
// }
// Components are written through the reflection data registered in the
// ComponentRegistry, so game-specific components serialize automatically.
namespace SceneSerializer {

Json serialize(Scene& scene);
bool deserialize(Scene& scene, const Json& json);

bool save(Scene& scene, const std::string& assetPath);
bool load(Scene& scene, const std::string& assetPath);

// Serializes the given entities and all of their descendants.
Json serializeEntities(Scene& scene, const std::vector<Entity>& entities);
// Instantiates previously serialized entities with fresh UUIDs (prefab style).
// Entity references between the copied entities are remapped. Returns the new roots.
std::vector<Entity> instantiate(Scene& scene, const Json& entities, Entity parent = NullEntity);

} // namespace SceneSerializer
} // namespace Forge
