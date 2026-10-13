#include "FPSGame.h"
#include "GameComponents.h"
#include "GameSystems.h"
#include "Prefabs.h"

#include <imgui.h>

namespace FPS {

using namespace Forge;

void FPSGame::registerComponents() { registerGameComponents(); }

void FPSGame::setupScene(Scene& scene) {
    scene.addSystem<GameManagerSystem>();
    scene.addSystem<PlayerSystem>();
    scene.addSystem<WeaponSystem>();
    scene.addSystem<EnemySystem>();
    scene.addSystem<ProjectileSystem>();
    scene.addSystem<ExplosiveSystem>();
    scene.addSystem<PickupSystem>();
    scene.addSystem<SpinnerSystem>();
}

void FPSGame::createDefaultScene(Scene& scene) { buildArena(scene); }

void FPSGame::onGUI(Scene& scene, const Viewport& viewport) { drawHud(scene, viewport); }

void FPSGame::onEditorMenu(Scene& scene) {
    if (!ImGui::BeginMenu("Arena")) return;
    bool editing = !scene.isPlaying();
    if (ImGui::MenuItem("Rebuild Arena Level", nullptr, false, editing)) {
        std::string path = scene.path;
        scene.clear();
        buildArena(scene);
        scene.path = path;
        LOG_INFO("Arena rebuilt (save with Ctrl+S to keep it)");
    }
    if (ImGui::MenuItem("Spawn Drone", nullptr, false, !editing)) Prefabs::drone(scene, glm::vec3(0, 3, 0));
    if (ImGui::MenuItem("Restart Game", nullptr, false, !editing)) GameManagerSystem::restart(scene);
    if (ImGui::MenuItem("Reset Explosive Barrels", nullptr, false, !editing)) ExplosiveSystem::reset(scene);
    ImGui::EndMenu();
}

} // namespace FPS
