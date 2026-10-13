#include "Forge/Core/Settings.h"
#include "Forge/Core/Application.h"
#include "Forge/Core/FileSystem.h"
#include "Forge/Core/Json.h"
#include "Forge/Core/Log.h"

#include <algorithm>

namespace Forge {

GameSettings& Settings::get() {
    static GameSettings settings;
    return settings;
}

std::string Settings::path() { return FileSystem::join(FileSystem::directory(FileSystem::assetRoot()), "ForgeSettings.json"); }

void Settings::load() {
    std::string text;
    if (!FileSystem::readText(path(), text)) return; // first run: keep defaults
    Json json;
    std::string error;
    if (!Json::parse(text, json, &error)) {
        LOG_WARN("Ignoring invalid settings file '%s': %s", path().c_str(), error.c_str());
        return;
    }
    GameSettings& s = get();
    const GameSettings d;
    s.mouseSensitivity = std::clamp(json["mouseSensitivity"].asFloat(d.mouseSensitivity), 0.05f, 5.0f);
    s.invertY = json["invertY"].asBool(d.invertY);
    s.holdToAim = json["holdToAim"].asBool(d.holdToAim);
    s.fieldOfView = std::clamp(json["fieldOfView"].asFloat(d.fieldOfView), 50.0f, 110.0f);
    s.vsync = json["vsync"].asBool(d.vsync);
    s.fullscreen = json["fullscreen"].asBool(d.fullscreen);
    s.showFps = json["showFps"].asBool(d.showFps);
    s.shadows = json["shadows"].asBool(d.shadows);
    s.ambientOcclusion = json["ambientOcclusion"].asBool(d.ambientOcclusion);
    s.bloom = json["bloom"].asBool(d.bloom);
    s.antiAliasing = json["antiAliasing"].asBool(d.antiAliasing);
}

void Settings::save() {
    const GameSettings& s = get();
    Json json = Json::object();
    json["mouseSensitivity"] = s.mouseSensitivity;
    json["invertY"] = s.invertY;
    json["holdToAim"] = s.holdToAim;
    json["fieldOfView"] = s.fieldOfView;
    json["vsync"] = s.vsync;
    json["fullscreen"] = s.fullscreen;
    json["showFps"] = s.showFps;
    json["shadows"] = s.shadows;
    json["ambientOcclusion"] = s.ambientOcclusion;
    json["bloom"] = s.bloom;
    json["antiAliasing"] = s.antiAliasing;
    if (!FileSystem::writeText(path(), json.dump(2))) LOG_ERROR("Could not write settings to '%s'", path().c_str());
}

void Settings::apply() {
    Application& app = Application::get();
    const GameSettings& s = get();
    app.setVSync(s.vsync);
    // The editor hosts the game inside a panel, so fullscreen is a player-only option.
    if (!app.config().editor) app.setFullscreen(s.fullscreen);
}

} // namespace Forge
