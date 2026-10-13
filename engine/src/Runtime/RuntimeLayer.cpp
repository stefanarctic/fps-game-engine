#include "Forge/Runtime/RuntimeLayer.h"
#include "Forge/Core/FileSystem.h"
#include "Forge/Core/Input.h"
#include "Forge/Core/Log.h"
#include "Forge/Core/Settings.h"
#include "Forge/Core/Time.h"
#include "Forge/Scene/SceneSerializer.h"

#include <imgui.h>

namespace Forge {

RuntimeLayer::RuntimeLayer(Game& game, std::string scenePath) : m_Game(game), m_ScenePath(std::move(scenePath)) {}

void RuntimeLayer::onAttach() {
    registerCoreComponents();
    m_Game.registerComponents();

    m_Scene = MakeScope<Scene>("Untitled");
    m_Game.setupScene(*m_Scene);
    std::string path = m_ScenePath.empty() ? m_Game.startupScene() : m_ScenePath;
    if (path.empty() || !FileSystem::exists(FileSystem::resolve(path)) || !SceneSerializer::load(*m_Scene, path)) {
        LOG_INFO("Building default scene");
        m_Game.createDefaultScene(*m_Scene);
    }
    m_Renderer = MakeScope<SceneRenderer>();
    m_Scene->beginPlay();
    Input::setEnabled(true);
    Input::setCursorLocked(true);
    Application& app = Application::get();
    app.setTitle(std::string(m_Game.name()) + " - Forge Engine");
    if (Settings::get().fullscreen) app.setFullscreen(true);
    if (app.config().pauseMenu != PauseMenuStart::Closed) {
        pause();
        if (app.config().pauseMenu == PauseMenuStart::Settings) m_PauseMenu.openSettings();
    }
}

void RuntimeLayer::onDetach() {
    if (m_Scene) m_Scene->endPlay();
    Input::setCursorLocked(false);
    m_Renderer.reset();
    m_Scene.reset();
}

void RuntimeLayer::pause() {
    if (m_Paused) return;
    m_Paused = true;
    m_PauseMenu.open();
    Input::setCursorLocked(false);
}

void RuntimeLayer::resume() {
    m_Paused = false;
    m_PauseMenu.close();
    Input::setCursorLocked(true);
}

void RuntimeLayer::onUpdate(float dt) {
    Application& app = Application::get();
    if (!m_Paused && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) pause();
    // Alt-tabbing away pauses the game (but an unfocused start, e.g. automated screenshots, does not).
    bool focused = app.windowFocused();
    if (m_WasFocused && !focused && !m_Paused) pause();
    m_WasFocused = focused;
    if (ImGui::IsKeyPressed(ImGuiKey_F3, false)) {
        Settings::get().showFps = !Settings::get().showFps;
        Settings::save();
    }
    Input::setEnabled(!m_Paused);

    if (!m_Paused) m_Scene->update(dt * Time::timeScale());

    glm::ivec2 fb = app.framebufferSize();
    if (fb.x <= 0 || fb.y <= 0) return;
    Entity cam = m_Scene->primaryCamera();
    CameraData camera = CameraData::fromEntity(*m_Scene, cam, (float)fb.x / (float)fb.y);
    m_Renderer->render(*m_Scene, camera, fb.x, fb.y, RenderOptions::fromSettings());
}

void RuntimeLayer::onImGui() {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    bg->AddImage((ImTextureID)(uintptr_t)m_Renderer->outputTexture(), ImVec2(0, 0), io.DisplaySize, ImVec2(0, 1), ImVec2(1, 0));

    Viewport vp;
    vp.position = glm::vec2(0.0f);
    vp.size = glm::vec2(io.DisplaySize.x, io.DisplaySize.y);
    vp.drawList = bg; // behind every ImGui window, so the pause menu and stats stay on top of the HUD
    m_Game.onGUI(*m_Scene, vp);

    if (Settings::get().showFps) {
        const RenderStats& s = m_Renderer->stats();
        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::SetNextWindowBgAlpha(0.6f);
        ImGui::Begin("##stats", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs);
        ImGui::Text("%.0f FPS (%.2f ms)", Time::fps(), 1000.0f / (Time::fps() > 0 ? Time::fps() : 1.0f));
        ImGui::Text("Draw calls %d  Triangles %d", s.drawCalls, s.triangles);
        ImGui::Text("Entities %zu  Lights %d  Particles %d", m_Scene->entityCount(), s.lights, s.particles);
        ImGui::End();
    }

    if (m_Paused) {
        // A transparent full-window host for the menu widgets.
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("##PauseHost", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoMove);
        ImGui::PopStyleVar(2);
        switch (m_PauseMenu.draw(vp, m_Game.name())) {
        case PauseMenu::Action::Resume: resume(); break;
        case PauseMenu::Action::Quit: Application::get().close(); break;
        case PauseMenu::Action::None: break;
        }
        ImGui::End();
    }
}

} // namespace Forge
