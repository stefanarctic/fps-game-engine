#pragma once
#include <Forge/Forge.h>

namespace FPS {

// "Arena": a wave-based first person shooter built on Forge.
class FPSGame : public Forge::Game {
public:
    const char* name() const override { return "Arena FPS"; }
    void registerComponents() override;
    void setupScene(Forge::Scene& scene) override;
    void createDefaultScene(Forge::Scene& scene) override;
    std::string startupScene() const override { return "scenes/Arena.scene"; }
    void onGUI(Forge::Scene& scene, const Forge::Viewport& viewport) override;
    void onEditorMenu(Forge::Scene& scene) override;
};

// Builds the arena level procedurally (ArenaBuilder.cpp).
void buildArena(Forge::Scene& scene);
// Draws the in-game HUD (Hud.cpp).
void drawHud(Forge::Scene& scene, const Forge::Viewport& viewport);

} // namespace FPS
