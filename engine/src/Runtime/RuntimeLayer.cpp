#include "Forge/Runtime/RuntimeLayer.h"
#include "Forge/Core/FileSystem.h"
#include "Forge/Core/Input.h"
#include "Forge/Core/Log.h"
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
    Application::get().setTitle(std::string(m_Game.name()) + " - Forge Engine");
}

void RuntimeLayer::onDetach() {
    if (m_Scene) m_Scene->endPlay();
    Input::setCursorLocked(false);
    m_Renderer.reset();
    m_Scene.reset();
}

void RuntimeLayer::onUpdate(float dt) {
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        m_Paused = !m_Paused;
        Input::setCursorLocked(!m_Paused);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F3, false)) m_ShowStats = !m_ShowStats;
    Input::setEnabled(!m_Paused);

    if (!m_Paused) m_Scene->update(dt * Time::timeScale());

    glm::ivec2 fb = Application::get().framebufferSize();
    if (fb.x <= 0 || fb.y <= 0) return;
    Entity cam = m_Scene->primaryCamera();
    CameraData camera = CameraData::fromEntity(*m_Scene, cam, (float)fb.x / (float)fb.y);
    RenderOptions options;
    m_Renderer->render(*m_Scene, camera, fb.x, fb.y, options);
}

void RuntimeLayer::onImGui() {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    bg->AddImage((ImTextureID)(uintptr_t)m_Renderer->outputTexture(), ImVec2(0, 0), io.DisplaySize, ImVec2(0, 1), ImVec2(1, 0));

    Viewport vp;
    vp.position = glm::vec2(0.0f);
    vp.size = glm::vec2(io.DisplaySize.x, io.DisplaySize.y);
    vp.drawList = ImGui::GetForegroundDrawList();
    m_Game.onGUI(*m_Scene, vp);

    if (m_ShowStats) {
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
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        fg->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, 140));
        const char* title = "PAUSED";
        ImFont* big = Application::get().fonts().large;
        ImVec2 ts = big->CalcTextSizeA(big->FontSize, FLT_MAX, 0.0f, title);
        fg->AddText(big, big->FontSize, ImVec2((io.DisplaySize.x - ts.x) * 0.5f, io.DisplaySize.y * 0.4f), IM_COL32_WHITE, title);
        const char* hint = "Click to resume  -  Alt+F4 to quit  -  F3 stats";
        ImVec2 hs = ImGui::CalcTextSize(hint);
        fg->AddText(ImVec2((io.DisplaySize.x - hs.x) * 0.5f, io.DisplaySize.y * 0.4f + ts.y + 10), IM_COL32(220, 220, 220, 255), hint);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_Paused = false;
            Input::setCursorLocked(true);
        }
    }
}

} // namespace Forge
