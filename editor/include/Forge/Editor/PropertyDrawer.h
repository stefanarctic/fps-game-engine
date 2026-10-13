#pragma once
#include "Forge/ECS/Reflection.h"
#include "Forge/Scene/Scene.h"

namespace Forge::Editor {

// Hooks the inspector uses to record undo steps.
struct PropertyContext {
    Scene* scene = nullptr;
    std::function<void()> beginEdit; // a continuous edit (drag, typing) started
    std::function<void()> endEdit;   // ...and was committed
    std::function<void()> record;    // a discrete change is about to be applied
};

// Draws reflected fields as a two-column (label | value) property grid like Unity's inspector.
namespace PropertyDrawer {
bool begin(const char* id);
void end();
bool drawFields(const TypeMeta& meta, void* object, PropertyContext& ctx);
bool drawField(const FieldMeta& field, void* object, PropertyContext& ctx);

// Individual widgets (must be called between begin/end)
bool vec3(const char* label, glm::vec3& value, float speed, PropertyContext& ctx, const char* tooltip = nullptr);
bool assetField(const char* label, std::string& path, AssetKind kind, PropertyContext& ctx, const char* tooltip = nullptr);
bool entityField(const char* label, Entity& entity, PropertyContext& ctx, const char* tooltip = nullptr);
} // namespace PropertyDrawer

// File extensions accepted by an asset field.
std::vector<std::string> assetExtensions(AssetKind kind);
const char* assetKindName(AssetKind kind);
bool assetMatchesKind(const std::string& path, AssetKind kind);

} // namespace Forge::Editor
