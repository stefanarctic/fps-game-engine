#pragma once
#include "Forge/Core/Application.h"
#include "Forge/Core/Game.h"
#include "Forge/Renderer/SceneRenderer.h"
#include "Forge/UI/PauseMenu.h"

namespace Forge {

// Standalone player: loads the game's startup scene, plays it fullscreen in the
// window and draws the game's HUD. Escape (or losing window focus) pauses the
// game and opens the pause menu.
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
    void pause();
    void resume();

    PauseMenu m_PauseMenu;
    bool m_Paused = false;
    bool m_WasFocused = false;
};

} // namespace Forge
