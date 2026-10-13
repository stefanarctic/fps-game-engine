#pragma once
#include "Forge/Core/Base.h"

#include <glm/vec2.hpp>

struct ImDrawList;

namespace Forge {

class Scene;

// Screen-space rectangle (pixels) where the game is displayed: the whole window
// in the standalone player, the Game view panel inside the editor.
struct Viewport {
    glm::vec2 position{0.0f};
    glm::vec2 size{0.0f};
    ImDrawList* drawList = nullptr; // where the HUD should draw
};

// The interface a game implements to plug into Forge. The same Game object is
// used by the editor (play mode inside the Game view) and the standalone player.
class Game {
public:
    virtual ~Game() = default;

    virtual const char* name() const = 0;

    // Register gameplay components with the ComponentRegistry (called once).
    virtual void registerComponents() {}
    // Add gameplay systems to a scene (called for every new or loaded scene).
    virtual void setupScene(Scene& scene) { FORGE_UNUSED(scene); }
    // Fill an empty scene with default content (used when no scene file exists).
    virtual void createDefaultScene(Scene& scene) { FORGE_UNUSED(scene); }
    // Asset path of the scene opened at startup.
    virtual std::string startupScene() const { return ""; }
    // Draw the in-game HUD with ImGui (only while playing).
    virtual void onGUI(Scene& scene, const Viewport& viewport) { FORGE_UNUSED(scene); FORGE_UNUSED(viewport); }
    // Extra entries in the editor's main menu bar.
    virtual void onEditorMenu(Scene& scene) { FORGE_UNUSED(scene); }
};

} // namespace Forge
