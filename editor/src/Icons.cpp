#include "Forge/Editor/Icons.h"
#include "Forge/Editor/EditorTheme.h"

#include <cmath>

namespace Forge::Editor {

namespace {
ImVec2 add(ImVec2 a, ImVec2 b) { return ImVec2(a.x + b.x, a.y + b.y); }
ImVec2 off(ImVec2 c, float x, float y, float s) { return ImVec2(c.x + x * s, c.y + y * s); }
constexpr float kPi = 3.14159265f;
} // namespace

void drawIcon(ImDrawList* dl, Icon icon, ImVec2 c, float size, ImU32 col) {
    float s = size * 0.5f; // half extent
    float t = std::fmax(1.0f, size * 0.09f);
    switch (icon) {
    case Icon::Hand: {
        dl->AddRectFilled(off(c, -0.45f, -0.05f, s), off(c, 0.45f, 0.75f, s), col, s * 0.3f);
        for (int i = 0; i < 4; ++i) {
            float x = -0.42f + i * 0.28f;
            dl->AddRectFilled(off(c, x, -0.75f + (i == 0 || i == 3 ? 0.2f : 0.0f), s), off(c, x + 0.18f, 0.1f, s), col, s * 0.1f);
        }
        break;
    }
    case Icon::Move: {
        dl->AddLine(off(c, -0.85f, 0, s), off(c, 0.85f, 0, s), col, t);
        dl->AddLine(off(c, 0, -0.85f, s), off(c, 0, 0.85f, s), col, t);
        dl->AddTriangleFilled(off(c, 0, -1, s), off(c, -0.3f, -0.6f, s), off(c, 0.3f, -0.6f, s), col);
        dl->AddTriangleFilled(off(c, 0, 1, s), off(c, 0.3f, 0.6f, s), off(c, -0.3f, 0.6f, s), col);
        dl->AddTriangleFilled(off(c, -1, 0, s), off(c, -0.6f, 0.3f, s), off(c, -0.6f, -0.3f, s), col);
        dl->AddTriangleFilled(off(c, 1, 0, s), off(c, 0.6f, -0.3f, s), off(c, 0.6f, 0.3f, s), col);
        break;
    }
    case Icon::Rotate: {
        dl->PathArcTo(c, s * 0.7f, -kPi * 0.25f, kPi * 1.4f, 20);
        dl->PathStroke(col, 0, t);
        ImVec2 tip = off(c, std::cos(-kPi * 0.25f) * 0.7f, std::sin(-kPi * 0.25f) * 0.7f, s);
        dl->AddTriangleFilled(add(tip, ImVec2(-s * 0.35f, -s * 0.15f)), add(tip, ImVec2(s * 0.3f, -s * 0.35f)), add(tip, ImVec2(s * 0.15f, s * 0.3f)), col);
        break;
    }
    case Icon::Scale: {
        dl->AddRect(off(c, -0.8f, -0.8f, s), off(c, 0.8f, 0.8f, s), col, 0, 0, t);
        dl->AddRectFilled(off(c, -0.8f, 0.0f, s), off(c, 0.0f, 0.8f, s), col);
        dl->AddLine(off(c, 0.0f, 0.0f, s), off(c, 0.55f, -0.55f, s), col, t);
        dl->AddTriangleFilled(off(c, 0.8f, -0.8f, s), off(c, 0.25f, -0.7f, s), off(c, 0.7f, -0.25f, s), col);
        break;
    }
    case Icon::Play:
        dl->AddTriangleFilled(off(c, -0.55f, -0.75f, s), off(c, 0.75f, 0.0f, s), off(c, -0.55f, 0.75f, s), col);
        break;
    case Icon::Pause:
        dl->AddRectFilled(off(c, -0.6f, -0.7f, s), off(c, -0.15f, 0.7f, s), col);
        dl->AddRectFilled(off(c, 0.15f, -0.7f, s), off(c, 0.6f, 0.7f, s), col);
        break;
    case Icon::Step:
        dl->AddTriangleFilled(off(c, -0.7f, -0.7f, s), off(c, 0.4f, 0.0f, s), off(c, -0.7f, 0.7f, s), col);
        dl->AddRectFilled(off(c, 0.45f, -0.7f, s), off(c, 0.75f, 0.7f, s), col);
        break;
    case Icon::Folder:
        dl->AddRectFilled(off(c, -0.9f, -0.7f, s), off(c, -0.1f, -0.4f, s), col, s * 0.08f);
        dl->AddRectFilled(off(c, -0.9f, -0.5f, s), off(c, 0.9f, 0.75f, s), col, s * 0.1f);
        break;
    case Icon::File:
    case Icon::Shader: {
        dl->AddRectFilled(off(c, -0.6f, -0.85f, s), off(c, 0.6f, 0.85f, s), col, s * 0.08f);
        ImU32 lines = IM_COL32(60, 60, 60, 255);
        for (int i = 0; i < 4; ++i) dl->AddLine(off(c, -0.4f, -0.45f + i * 0.3f, s), off(c, 0.4f, -0.45f + i * 0.3f, s), lines, t);
        break;
    }
    case Icon::Model: {
        // Isometric cube
        ImVec2 top = off(c, 0, -0.9f, s), l = off(c, -0.8f, -0.45f, s), r = off(c, 0.8f, -0.45f, s), m = off(c, 0, 0.0f, s);
        ImVec2 bl = off(c, -0.8f, 0.45f, s), br = off(c, 0.8f, 0.45f, s), bm = off(c, 0, 0.9f, s);
        dl->AddQuadFilled(top, r, m, l, IM_COL32(150, 190, 235, 255));
        dl->AddQuadFilled(l, m, bm, bl, IM_COL32(90, 140, 200, 255));
        dl->AddQuadFilled(m, r, br, bm, IM_COL32(60, 105, 165, 255));
        (void)col;
        break;
    }
    case Icon::Material: {
        dl->AddCircleFilled(c, s * 0.85f, col, 24);
        dl->AddCircleFilled(off(c, -0.3f, -0.3f, s), s * 0.25f, IM_COL32(255, 255, 255, 110), 16);
        break;
    }
    case Icon::Texture: {
        dl->AddRectFilled(off(c, -0.85f, -0.85f, s), off(c, 0.85f, 0.85f, s), IM_COL32(70, 110, 160, 255), s * 0.08f);
        dl->AddTriangleFilled(off(c, -0.75f, 0.75f, s), off(c, -0.2f, -0.1f, s), off(c, 0.3f, 0.75f, s), IM_COL32(110, 190, 90, 255));
        dl->AddTriangleFilled(off(c, 0.0f, 0.75f, s), off(c, 0.4f, 0.2f, s), off(c, 0.8f, 0.75f, s), IM_COL32(80, 160, 70, 255));
        dl->AddCircleFilled(off(c, 0.45f, -0.45f, s), s * 0.18f, IM_COL32(250, 220, 90, 255), 12);
        (void)col;
        break;
    }
    case Icon::Scene: {
        dl->AddRectFilled(off(c, -0.85f, -0.85f, s), off(c, 0.85f, 0.85f, s), IM_COL32(40, 40, 40, 255), s * 0.15f);
        dl->AddTriangleFilled(off(c, 0, -0.6f, s), off(c, 0.6f, 0.5f, s), off(c, -0.6f, 0.5f, s), col);
        break;
    }
    case Icon::Light:
        dl->AddCircleFilled(off(c, 0, -0.15f, s), s * 0.55f, col, 20);
        dl->AddRectFilled(off(c, -0.25f, 0.35f, s), off(c, 0.25f, 0.8f, s), col, s * 0.05f);
        break;
    case Icon::Sun:
        dl->AddCircleFilled(c, s * 0.4f, col, 20);
        for (int i = 0; i < 8; ++i) {
            float a = (float)i / 8.0f * 2.0f * kPi;
            dl->AddLine(off(c, std::cos(a) * 0.6f, std::sin(a) * 0.6f, s), off(c, std::cos(a) * 0.95f, std::sin(a) * 0.95f, s), col, t);
        }
        break;
    case Icon::Spot:
        dl->AddTriangleFilled(off(c, 0, -0.85f, s), off(c, 0.7f, 0.7f, s), off(c, -0.7f, 0.7f, s), col);
        break;
    case Icon::Camera:
        dl->AddRectFilled(off(c, -0.9f, -0.45f, s), off(c, 0.35f, 0.5f, s), col, s * 0.1f);
        dl->AddTriangleFilled(off(c, 0.35f, 0.0f, s), off(c, 0.9f, -0.45f, s), off(c, 0.9f, 0.5f, s), col);
        break;
    case Icon::Entity:
        dl->AddRect(off(c, -0.7f, -0.7f, s), off(c, 0.7f, 0.7f, s), col, s * 0.1f, 0, t);
        dl->AddLine(off(c, -0.7f, -0.7f, s), off(c, 0.7f, 0.7f, s), col, t * 0.6f);
        break;
    case Icon::Info:
        dl->AddCircleFilled(c, s * 0.85f, IM_COL32(200, 200, 200, 255), 20);
        dl->AddRectFilled(off(c, -0.1f, -0.15f, s), off(c, 0.1f, 0.55f, s), IM_COL32(40, 40, 40, 255));
        dl->AddCircleFilled(off(c, 0, -0.45f, s), s * 0.12f, IM_COL32(40, 40, 40, 255), 8);
        (void)col;
        break;
    case Icon::Warning:
        dl->AddTriangleFilled(off(c, 0, -0.9f, s), off(c, 0.9f, 0.8f, s), off(c, -0.9f, 0.8f, s), IM_COL32(240, 190, 40, 255));
        dl->AddRectFilled(off(c, -0.08f, -0.35f, s), off(c, 0.08f, 0.3f, s), IM_COL32(40, 40, 40, 255));
        dl->AddRectFilled(off(c, -0.08f, 0.45f, s), off(c, 0.08f, 0.62f, s), IM_COL32(40, 40, 40, 255));
        (void)col;
        break;
    case Icon::Error:
        dl->AddCircleFilled(c, s * 0.85f, IM_COL32(220, 70, 60, 255), 20);
        dl->AddLine(off(c, -0.35f, -0.35f, s), off(c, 0.35f, 0.35f, s), IM_COL32(255, 255, 255, 255), t * 1.4f);
        dl->AddLine(off(c, 0.35f, -0.35f, s), off(c, -0.35f, 0.35f, s), IM_COL32(255, 255, 255, 255), t * 1.4f);
        (void)col;
        break;
    case Icon::Plus:
        dl->AddLine(off(c, -0.6f, 0, s), off(c, 0.6f, 0, s), col, t * 1.4f);
        dl->AddLine(off(c, 0, -0.6f, s), off(c, 0, 0.6f, s), col, t * 1.4f);
        break;
    }
}

bool iconButton(const char* id, Icon icon, bool active, ImVec2 size, const char* tooltip) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, size);
    bool hovered = ImGui::IsItemHovered();
    bool held = ImGui::IsItemActive();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 bg = active ? Theme::Accent : (held ? IM_COL32(50, 50, 50, 255) : (hovered ? IM_COL32(80, 80, 80, 255) : IM_COL32(64, 64, 64, 255)));
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bg, 3.0f);
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(30, 30, 30, 255), 3.0f);
    float iconSize = (size.y < size.x ? size.y : size.x) * 0.62f;
    drawIcon(dl, icon, ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f), iconSize, active ? IM_COL32(255, 255, 255, 255) : Theme::Icon);
    if (tooltip && hovered) ImGui::SetTooltip("%s", tooltip);
    return clicked;
}

} // namespace Forge::Editor
