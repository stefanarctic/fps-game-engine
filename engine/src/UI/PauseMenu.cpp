#include "Forge/UI/PauseMenu.h"
#include "Forge/Core/Application.h"
#include "Forge/Core/Settings.h"

#include <imgui.h>

#include <algorithm>

namespace Forge {

namespace {
const ImVec4 kAccent(0.96f, 0.55f, 0.16f, 1.0f);

void pushMenuStyle(float scale) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f * scale, 10.0f * scale));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f * scale, 6.0f * scale));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f * scale, 24.0f * scale));
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 14.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 4.0f * scale);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.08f, 0.10f, 0.94f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.10f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.17f, 0.19f, 0.23f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccent);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.80f, 0.42f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.17f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.21f, 0.23f, 0.27f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.25f, 0.27f, 0.31f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, kAccent);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0.68f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, kAccent);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.93f, 0.94f, 0.96f, 1.0f));
}

void popMenuStyle() {
    ImGui::PopStyleColor(12);
    ImGui::PopStyleVar(7);
}

void centeredText(ImFont* font, float fontScale, const char* text, const ImVec4& color) {
    ImGui::PushFont(font);
    ImGui::SetWindowFontScale(fontScale);
    float w = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX(std::max((ImGui::GetWindowWidth() - w) * 0.5f, 0.0f));
    ImGui::TextColored(color, "%s", text);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopFont();
}

// Label on the left, widget filling the rest of the row.
void rowLabel(const char* label, float labelWidth) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(labelWidth);
    ImGui::SetNextItemWidth(-1.0f);
}

void sectionHeader(const char* text) {
    ImGui::Spacing();
    ImGui::TextColored(kAccent, "%s", text);
    ImGui::Separator();
}
} // namespace

void PauseMenu::open() {
    m_Open = true;
    m_Page = Page::Main;
    m_OpenFrame = ImGui::GetFrameCount();
}

void PauseMenu::openSettings() {
    open();
    m_Page = Page::Settings;
}

void PauseMenu::close() {
    if (m_Page == Page::Settings) leaveSettings();
    m_Open = false;
}

void PauseMenu::leaveSettings() {
    m_Page = Page::Main;
    if (m_SettingsChanged) {
        Settings::save();
        m_SettingsChanged = false;
    }
}

PauseMenu::Action PauseMenu::draw(const Viewport& vp, const char* title, const char* quitLabel) {
    if (!m_Open) return Action::None;
    Action action = Action::None;
    const Fonts& fonts = Application::get().fonts();
    float scale = std::clamp(vp.size.y / 900.0f, 0.7f, 1.5f);

    // Dim the game behind the menu.
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 min(vp.position.x, vp.position.y), max(vp.position.x + vp.size.x, vp.position.y + vp.size.y);
    dl->AddRectFilledMultiColor(min, max, IM_COL32(5, 8, 14, 215), IM_COL32(5, 8, 14, 150), IM_COL32(5, 8, 14, 150),
                                IM_COL32(5, 8, 14, 215));

    bool settings = m_Page == Page::Settings;
    float width = std::min((settings ? 560.0f : 380.0f) * scale, vp.size.x - 20.0f);
    static float lastHeight[2] = {380.0f, 640.0f}; // measured last frame, used to center vertically
    float height = lastHeight[settings ? 1 : 0];
    ImGui::SetCursorScreenPos(ImVec2(min.x + (vp.size.x - width) * 0.5f, min.y + std::max((vp.size.y - height) * 0.5f, 10.0f)));

    pushMenuStyle(scale);
    ImGui::BeginChild("##PauseMenu", ImVec2(width, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);

    centeredText(fonts.large, 0.62f * scale, title, ImVec4(1, 1, 1, 1));
    centeredText(fonts.medium, 0.6f * scale, settings ? "SETTINGS" : "PAUSED", kAccent);
    ImGui::Dummy(ImVec2(0.0f, 6.0f * scale));

    if (!settings) {
        ImGui::PushFont(fonts.medium);
        ImGui::SetWindowFontScale(0.78f * scale);
        ImVec2 button(-1.0f, 52.0f * scale);
        if (ImGui::Button("Resume", button)) action = Action::Resume;
        if (ImGui::Button("Settings", button)) m_Page = Page::Settings;
        if (ImGui::Button(quitLabel, button)) action = Action::Quit;
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0.0f, 2.0f * scale));
        ImGui::SetWindowFontScale(scale);
        const char* hint = "Esc to resume";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(hint).x) * 0.5f);
        ImGui::TextDisabled("%s", hint);
        ImGui::SetWindowFontScale(1.0f);
    } else {
        drawSettings(scale, width);
    }

    ImGui::EndChild();
    lastHeight[settings ? 1 : 0] = ImGui::GetItemRectSize().y;
    popMenuStyle();

    // Keyboard: Escape backs out of the settings page, then resumes.
    bool escape = ImGui::IsKeyPressed(ImGuiKey_Escape, false) && ImGui::GetFrameCount() != m_OpenFrame &&
                  !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive();
    if (escape) {
        if (m_Page == Page::Settings) leaveSettings();
        else action = Action::Resume;
    }
    if (action != Action::None) close();
    return action;
}

void PauseMenu::drawSettings(float scale, float width) {
    GameSettings& s = Settings::get();
    bool changed = false, display = false;
    float label = width * 0.42f;

    ImGui::SetWindowFontScale(1.1f * scale);
    sectionHeader("CONTROLS");
    rowLabel("Mouse sensitivity", label);
    changed |= ImGui::SliderFloat("##sens", &s.mouseSensitivity, 0.1f, 3.0f, "%.2fx");
    rowLabel("Invert Y axis", label);
    changed |= ImGui::Checkbox("##invert", &s.invertY);
    rowLabel("Hold to aim", label);
    changed |= ImGui::Checkbox("##holdaim", &s.holdToAim);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Off: right mouse button toggles aiming");

    sectionHeader("DISPLAY");
    rowLabel("Field of view", label);
    changed |= ImGui::SliderFloat("##fov", &s.fieldOfView, 60.0f, 100.0f, "%.0f deg");
    rowLabel("VSync", label);
    if (ImGui::Checkbox("##vsync", &s.vsync)) changed = display = true;
    if (!Application::get().config().editor) {
        rowLabel("Fullscreen", label);
        if (ImGui::Checkbox("##fullscreen", &s.fullscreen)) changed = display = true;
    }
    rowLabel("Show FPS", label);
    changed |= ImGui::Checkbox("##fps", &s.showFps);

    sectionHeader("GRAPHICS");
    rowLabel("Shadows", label);
    changed |= ImGui::Checkbox("##shadows", &s.shadows);
    rowLabel("Ambient occlusion", label);
    changed |= ImGui::Checkbox("##ssao", &s.ambientOcclusion);
    rowLabel("Bloom", label);
    changed |= ImGui::Checkbox("##bloom", &s.bloom);
    rowLabel("Anti-aliasing (FXAA)", label);
    changed |= ImGui::Checkbox("##fxaa", &s.antiAliasing);

    ImGui::Dummy(ImVec2(0.0f, 8.0f * scale));
    float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button("Reset to Defaults", ImVec2(half, 40.0f * scale))) {
        bool fullscreen = s.fullscreen;
        s = GameSettings();
        display = changed = true;
        if (Application::get().config().editor) s.fullscreen = fullscreen;
    }
    ImGui::SameLine();
    if (ImGui::Button("Back", ImVec2(half, 40.0f * scale))) leaveSettings();
    ImGui::SetWindowFontScale(1.0f);

    if (changed) m_SettingsChanged = true;
    if (display) Settings::apply();
}

} // namespace Forge
