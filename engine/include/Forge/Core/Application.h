#pragma once
#include "Forge/Core/Base.h"

#include <glm/vec2.hpp>

struct GLFWwindow;
struct ImFont;

namespace Forge {

enum class PauseMenuStart { Closed, Main, Settings };

struct AppConfig {
    std::string title = "Forge Engine";
    int width = 1600;
    int height = 900;
    bool vsync = true;
    bool maximized = false;

    // Command line options
    bool editor = true;            // --game runs the standalone player instead
    std::string scene;             // --scene <asset path>
    bool autoplay = false;         // --play : enter play mode immediately (editor)
    std::string select;            // --select <entity name> : select an entity at startup (editor)
    bool rebuildScene = false;     // --rebuild-scene : regenerate the game's default scene and save it
    std::string screenshotPath;    // --screenshot <file.png>
    int screenshotFrame = 120;     // --frames <n> : capture after n frames, then exit
    PauseMenuStart pauseMenu = PauseMenuStart::Closed; // --pause / --pause-settings : open the pause menu once playing

    static AppConfig fromArgs(int argc, char** argv, AppConfig defaults = AppConfig());
};

// A layer receives the frame callbacks. The editor and the standalone runtime
// are both layers; games may push additional ones (debug overlays, menus...).
class Layer {
public:
    virtual ~Layer() = default;
    virtual void onAttach() {}
    virtual void onDetach() {}
    virtual void onUpdate(float dt) { FORGE_UNUSED(dt); }  // simulate + render scenes into offscreen targets
    virtual void onImGui() {}                               // build UI (editor panels, HUD)
};

struct Fonts {
    ImFont* regular = nullptr;
    ImFont* bold = nullptr;
    ImFont* large = nullptr;   // HUD headlines
    ImFont* medium = nullptr;  // HUD numbers
};

class Application {
public:
    explicit Application(const AppConfig& config);
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void pushLayer(Scope<Layer> layer);
    int run();
    void close() { m_Running = false; }

    static Application& get() { return *s_Instance; }
    GLFWwindow* window() const { return m_Window; }
    const AppConfig& config() const { return m_Config; }
    glm::ivec2 framebufferSize() const;
    const Fonts& fonts() const { return m_Fonts; }
    void setTitle(const std::string& title);
    void setVSync(bool enabled);
    bool vsync() const { return m_VSync; }
    // Borderless fullscreen on the primary monitor (restores the previous window rect).
    void setFullscreen(bool enabled);
    bool isFullscreen() const { return m_Fullscreen; }
    bool windowFocused() const;

    // Saves the next presented frame (whole window) as a PNG.
    void requestScreenshot(const std::string& path) { m_PendingScreenshot = path; }

private:
    bool init();
    void shutdown();
    void initImGui();
    void captureScreenshot(const std::string& path);

    static Application* s_Instance;
    AppConfig m_Config;
    GLFWwindow* m_Window = nullptr;
    std::vector<Scope<Layer>> m_Layers;
    Fonts m_Fonts;
    std::string m_IniPath;
    std::string m_PendingScreenshot;
    bool m_Running = true;
    bool m_VSync = true;
    bool m_Fullscreen = false;
    int m_WindowedRect[4] = {100, 100, 1600, 900};
    bool m_Initialized = false;
};

} // namespace Forge
