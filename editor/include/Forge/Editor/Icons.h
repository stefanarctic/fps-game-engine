#pragma once
#include <imgui.h>

namespace Forge::Editor {

// Vector icons drawn with ImDrawList (no icon font / texture atlas needed).
enum class Icon {
    Hand, Move, Rotate, Scale, Play, Pause, Step,
    Folder, File, Model, Material, Texture, Scene, Shader,
    Light, Sun, Spot, Camera, Entity, Info, Warning, Error, Plus,
};

void drawIcon(ImDrawList* dl, Icon icon, ImVec2 center, float size, ImU32 color);
// Toggle-style toolbar button. Returns true when clicked.
bool iconButton(const char* id, Icon icon, bool active, ImVec2 size = ImVec2(28, 24), const char* tooltip = nullptr);

} // namespace Forge::Editor
