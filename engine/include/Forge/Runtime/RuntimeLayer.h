#pragma once
#include "Forge/Core/Application.h"
#include "Forge/Core/Game.h"
#include "Forge/Renderer/SceneRenderer.h"

namespace Forge {

// Standalone player: loads the game's startup scene, plays it fullscreen in the
// window and draws the game's HUD. Escape releases the mouse / pauses.
class RuntimeLayer : public Layer {
public:
    explicit RuntimeLayer(Game& game, std::string scenePath = "");

    void onAttach() override;
    void onDetach() override;
    void onUpdate(float dt) override;
    void onImGui() override;

private:
    Game& m_Game;
    std::string m_ScenePath;
    Scope<Scene> m_Scene;
    Scope<SceneRenderer> m_Renderer;
    bool m_Paused = false;
    bool m_ShowStats = false;
};

} // namespace Forge
