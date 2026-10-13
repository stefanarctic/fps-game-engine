#pragma once
#include <imgui.h>

namespace Forge::Editor {

// Colors close to Unity's dark "Professional" editor skin.
namespace Theme {
constexpr ImU32 Accent = IM_COL32(44, 93, 135, 255);        // selection blue
constexpr ImU32 AccentBright = IM_COL32(58, 121, 187, 255);
constexpr ImU32 AxisX = IM_COL32(219, 62, 29, 255);
constexpr ImU32 AxisY = IM_COL32(139, 220, 0, 255);
constexpr ImU32 AxisZ = IM_COL32(58, 138, 236, 255);
constexpr ImU32 Icon = IM_COL32(196, 196, 196, 255);
constexpr ImU32 IconDim = IM_COL32(140, 140, 140, 255);

void apply();
// Pushes a slightly darker / tinted palette while in play mode (like Unity's play mode tint).
void pushPlayModeTint();
void popPlayModeTint();
} // namespace Theme

} // namespace Forge::Editor
