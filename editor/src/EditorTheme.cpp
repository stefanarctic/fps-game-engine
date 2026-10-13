#include "Forge/Editor/EditorTheme.h"

namespace Forge::Editor::Theme {

namespace {
ImVec4 rgb(int r, int g, int b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); }
constexpr ImGuiCol kTinted[] = {ImGuiCol_WindowBg, ImGuiCol_ChildBg, ImGuiCol_PopupBg, ImGuiCol_TitleBg, ImGuiCol_TitleBgActive,
                                ImGuiCol_MenuBarBg, ImGuiCol_Tab, ImGuiCol_TabSelected, ImGuiCol_TabDimmed, ImGuiCol_TabDimmedSelected,
                                ImGuiCol_FrameBg, ImGuiCol_Header, ImGuiCol_Button, ImGuiCol_DockingEmptyBg};
} // namespace

void apply() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowPadding = ImVec2(6, 6);
    s.FramePadding = ImVec2(6, 3);
    s.ItemSpacing = ImVec2(6, 4);
    s.ItemInnerSpacing = ImVec2(4, 4);
    s.IndentSpacing = 14.0f;
    s.ScrollbarSize = 13.0f;
    s.GrabMinSize = 10.0f;
    s.WindowBorderSize = 1.0f;
    s.ChildBorderSize = 1.0f;
    s.PopupBorderSize = 1.0f;
    s.FrameBorderSize = 1.0f;
    s.TabBorderSize = 0.0f;
    s.WindowRounding = 0.0f;
    s.ChildRounding = 2.0f;
    s.FrameRounding = 3.0f;
    s.PopupRounding = 3.0f;
    s.ScrollbarRounding = 6.0f;
    s.GrabRounding = 3.0f;
    s.TabRounding = 3.0f;
    s.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    s.WindowMenuButtonPosition = ImGuiDir_None;
    s.DockingSeparatorSize = 2.0f;
    s.SeparatorTextBorderSize = 1.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = rgb(210, 210, 210);
    c[ImGuiCol_TextDisabled] = rgb(128, 128, 128);
    c[ImGuiCol_WindowBg] = rgb(56, 56, 56);
    c[ImGuiCol_ChildBg] = rgb(56, 56, 56, 0.0f);
    c[ImGuiCol_PopupBg] = rgb(45, 45, 45);
    c[ImGuiCol_Border] = rgb(25, 25, 25);
    c[ImGuiCol_BorderShadow] = rgb(0, 0, 0, 0.0f);
    c[ImGuiCol_FrameBg] = rgb(42, 42, 42);
    c[ImGuiCol_FrameBgHovered] = rgb(50, 50, 50);
    c[ImGuiCol_FrameBgActive] = rgb(35, 35, 35);
    c[ImGuiCol_TitleBg] = rgb(40, 40, 40);
    c[ImGuiCol_TitleBgActive] = rgb(40, 40, 40);
    c[ImGuiCol_TitleBgCollapsed] = rgb(40, 40, 40);
    c[ImGuiCol_MenuBarBg] = rgb(30, 30, 30);
    c[ImGuiCol_ScrollbarBg] = rgb(45, 45, 45);
    c[ImGuiCol_ScrollbarGrab] = rgb(94, 94, 94);
    c[ImGuiCol_ScrollbarGrabHovered] = rgb(110, 110, 110);
    c[ImGuiCol_ScrollbarGrabActive] = rgb(130, 130, 130);
    c[ImGuiCol_CheckMark] = rgb(210, 210, 210);
    c[ImGuiCol_SliderGrab] = rgb(130, 130, 130);
    c[ImGuiCol_SliderGrabActive] = rgb(160, 160, 160);
    c[ImGuiCol_Button] = rgb(88, 88, 88);
    c[ImGuiCol_ButtonHovered] = rgb(103, 103, 103);
    c[ImGuiCol_ButtonActive] = rgb(70, 96, 124);
    c[ImGuiCol_Header] = rgb(44, 93, 135);
    c[ImGuiCol_HeaderHovered] = rgb(69, 69, 69);
    c[ImGuiCol_HeaderActive] = rgb(44, 93, 135);
    c[ImGuiCol_Separator] = rgb(35, 35, 35);
    c[ImGuiCol_SeparatorHovered] = rgb(58, 121, 187);
    c[ImGuiCol_SeparatorActive] = rgb(58, 121, 187);
    c[ImGuiCol_ResizeGrip] = rgb(0, 0, 0, 0.0f);
    c[ImGuiCol_ResizeGripHovered] = rgb(58, 121, 187);
    c[ImGuiCol_ResizeGripActive] = rgb(58, 121, 187);
    c[ImGuiCol_Tab] = rgb(40, 40, 40);
    c[ImGuiCol_TabHovered] = rgb(64, 64, 64);
    c[ImGuiCol_TabSelected] = rgb(60, 60, 60);
    c[ImGuiCol_TabSelectedOverline] = rgb(58, 121, 187);
    c[ImGuiCol_TabDimmed] = rgb(40, 40, 40);
    c[ImGuiCol_TabDimmedSelected] = rgb(56, 56, 56);
    c[ImGuiCol_TabDimmedSelectedOverline] = rgb(56, 56, 56, 0.0f);
    c[ImGuiCol_DockingPreview] = rgb(58, 121, 187, 0.6f);
    c[ImGuiCol_DockingEmptyBg] = rgb(30, 30, 30);
    c[ImGuiCol_PlotLines] = rgb(160, 160, 160);
    c[ImGuiCol_PlotHistogram] = rgb(58, 121, 187);
    c[ImGuiCol_TableHeaderBg] = rgb(48, 48, 48);
    c[ImGuiCol_TableBorderStrong] = rgb(30, 30, 30);
    c[ImGuiCol_TableBorderLight] = rgb(40, 40, 40);
    c[ImGuiCol_TableRowBg] = rgb(56, 56, 56, 0.0f);
    c[ImGuiCol_TableRowBgAlt] = rgb(255, 255, 255, 0.025f);
    c[ImGuiCol_TextSelectedBg] = rgb(58, 121, 187, 0.5f);
    c[ImGuiCol_DragDropTarget] = rgb(58, 121, 187);
    c[ImGuiCol_NavHighlight] = rgb(58, 121, 187);
    c[ImGuiCol_ModalWindowDimBg] = rgb(0, 0, 0, 0.45f);
}

void pushPlayModeTint() {
    const ImVec4 tint(0.80f, 0.84f, 0.95f, 1.0f);
    for (ImGuiCol col : kTinted) {
        ImVec4 v = ImGui::GetStyleColorVec4(col);
        ImGui::PushStyleColor(col, ImVec4(v.x * tint.x, v.y * tint.y, v.z * tint.z, v.w));
    }
}

void popPlayModeTint() { ImGui::PopStyleColor((int)(sizeof(kTinted) / sizeof(kTinted[0]))); }

} // namespace Forge::Editor::Theme
