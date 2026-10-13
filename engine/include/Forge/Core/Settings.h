#pragma once
#include "Forge/Core/Base.h"

namespace Forge {

// Player-facing options shared by the engine and games: edited from the pause
// menu, persisted to ForgeSettings.json next to the asset folder.
// Games read them every frame (e.g. field of view, mouse sensitivity); the
// engine applies the display and graphics options.
struct GameSettings {
    // Controls
    float mouseSensitivity = 1.0f;  // multiplier on the game's own sensitivity
    bool invertY = false;
    bool holdToAim = true;          // false: right mouse toggles aiming

    // Display
    float fieldOfView = 75.0f;      // vertical, degrees
    bool vsync = true;
    bool fullscreen = false;        // standalone player only
    bool showFps = false;

    // Graphics (combined with the scene's Environment toggles)
    bool shadows = true;
    bool ambientOcclusion = true;
    bool bloom = true;
    bool antiAliasing = true;
};

class Settings {
public:
    static GameSettings& get();
    static void load();
    static void save();
    // Pushes display options (vsync, fullscreen) to the application window.
    static void apply();
    static std::string path();
};

} // namespace Forge
