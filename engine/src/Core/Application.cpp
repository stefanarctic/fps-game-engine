#include "Forge/Core/Application.h"
#include "Forge/Assets/AssetManager.h"
#include "Forge/Core/FileSystem.h"
#include "Forge/Core/Input.h"
#include "Forge/Core/Log.h"
#include "Forge/Core/Settings.h"
#include "Forge/Core/Time.h"
#include "Forge/Renderer/DebugDraw.h"
#include "Forge/Scene/Components.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cstdlib>
#include <cstring>

namespace Forge {

Application* Application::s_Instance = nullptr;

AppConfig AppConfig::fromArgs(int argc, char** argv, AppConfig cfg) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](std::string& out) { if (i + 1 < argc) out = argv[++i]; };
        if (a == "--game") cfg.editor = false;
        else if (a == "--editor") cfg.editor = true;
        else if (a == "--play") cfg.autoplay = true;
        else if (a == "--rebuild-scene") cfg.rebuildScene = true;
        else if (a == "--scene") next(cfg.scene);
        else if (a == "--select") next(cfg.select);
        else if (a == "--screenshot") next(cfg.screenshotPath);
        else if (a == "--frames" && i + 1 < argc) cfg.screenshotFrame = std::atoi(argv[++i]);
        else if (a == "--width" && i + 1 < argc) cfg.width = std::atoi(argv[++i]);
        else if (a == "--height" && i + 1 < argc) cfg.height = std::atoi(argv[++i]);
        else if (a == "--novsync") cfg.vsync = false;
        else if (a == "--maximized") cfg.maximized = true;
        else if (a == "--pause") cfg.pauseMenu = PauseMenuStart::Main;
        else if (a == "--pause-settings") cfg.pauseMenu = PauseMenuStart::Settings;
    }
    return cfg;
}

namespace {
void glfwErrorCallback(int code, const char* message) { LOG_ERROR("GLFW error %d: %s", code, message); }
void scrollCallback(GLFWwindow*, double, double dy) { Input::onScroll(dy); }
} // namespace

Application::Application(const AppConfig& config) : m_Config(config) {
    s_Instance = this;
    m_Initialized = init();
}

Application::~Application() {
    shutdown();
    s_Instance = nullptr;
}

bool Application::init() {
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        LOG_ERROR("Failed to initialise GLFW");
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_MAXIMIZED, m_Config.maximized ? GLFW_TRUE : GLFW_FALSE);

    m_Window = glfwCreateWindow(m_Config.width, m_Config.height, m_Config.title.c_str(), nullptr, nullptr);
    if (!m_Window) {
        LOG_ERROR("Failed to create an OpenGL 3.3 window");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(m_Window);
    setVSync(m_Config.vsync);

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        LOG_ERROR("Failed to load OpenGL functions");
        return false;
    }
    LOG_INFO("OpenGL %s | %s", (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));

    // Install our callbacks before ImGui so its backend chains to them.
    glfwSetScrollCallback(m_Window, scrollCallback);
    Input::init(m_Window);

    initImGui();
    registerCoreComponents();

    // Player settings (vsync from the file unless --novsync was given).
    Settings::load();
    setVSync(m_Config.vsync && Settings::get().vsync);
    return true;
}

void Application::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    m_IniPath = FileSystem::join(FileSystem::directory(FileSystem::assetRoot()), "ForgeEditor.ini");
    io.IniFilename = m_IniPath.c_str();

    // Segoe UI is close to Unity's editor font; fall back to ImGui's default.
    const char* regular = "C:/Windows/Fonts/segoeui.ttf";
    const char* bold = "C:/Windows/Fonts/segoeuib.ttf";
    if (FileSystem::exists(regular)) {
        m_Fonts.regular = io.Fonts->AddFontFromFileTTF(regular, 16.0f);
        m_Fonts.bold = FileSystem::exists(bold) ? io.Fonts->AddFontFromFileTTF(bold, 16.0f) : m_Fonts.regular;
        const char* hud = FileSystem::exists(bold) ? bold : regular;
        m_Fonts.medium = io.Fonts->AddFontFromFileTTF(hud, 28.0f);
        m_Fonts.large = io.Fonts->AddFontFromFileTTF(hud, 56.0f);
    } else {
        m_Fonts.regular = io.Fonts->AddFontDefault();
        m_Fonts.bold = m_Fonts.regular;
        ImFontConfig cfg;
        cfg.SizePixels = 26.0f;
        m_Fonts.medium = io.Fonts->AddFontDefault(&cfg);
        cfg.SizePixels = 48.0f;
        m_Fonts.large = io.Fonts->AddFontDefault(&cfg);
    }
    io.FontDefault = m_Fonts.regular;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
}

void Application::pushLayer(Scope<Layer> layer) {
    Layer* raw = layer.get();
    m_Layers.push_back(std::move(layer));
    if (m_Initialized) raw->onAttach();
}

glm::ivec2 Application::framebufferSize() const {
    int w = 0, h = 0;
    glfwGetFramebufferSize(m_Window, &w, &h);
    return {w, h};
}

void Application::setTitle(const std::string& title) {
    if (m_Window) glfwSetWindowTitle(m_Window, title.c_str());
}

void Application::setVSync(bool enabled) {
    m_VSync = enabled;
    glfwSwapInterval(enabled ? 1 : 0);
}

void Application::setFullscreen(bool enabled) {
    if (!m_Window || enabled == m_Fullscreen) return;
    if (enabled) {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
        if (!mode) return;
        glfwGetWindowPos(m_Window, &m_WindowedRect[0], &m_WindowedRect[1]);
        glfwGetWindowSize(m_Window, &m_WindowedRect[2], &m_WindowedRect[3]);
        glfwSetWindowMonitor(m_Window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    } else {
        glfwSetWindowMonitor(m_Window, nullptr, m_WindowedRect[0], m_WindowedRect[1], m_WindowedRect[2], m_WindowedRect[3], 0);
    }
    m_Fullscreen = enabled;
    setVSync(m_VSync); // some drivers reset the swap interval on a mode change
}

bool Application::windowFocused() const { return m_Window && glfwGetWindowAttrib(m_Window, GLFW_FOCUSED) != 0; }

int Application::run() {
    if (!m_Initialized) return 1;
    if (!m_Config.screenshotPath.empty()) LOG_INFO("Will capture '%s' after %d frames", m_Config.screenshotPath.c_str(), m_Config.screenshotFrame);

    double lastShaderCheck = 0.0;
    while (m_Running && !glfwWindowShouldClose(m_Window)) {
        glfwPollEvents();
        Time::tick(glfwGetTime());
        Input::update();
        float dt = Time::unscaledDeltaTime();

        // Shader hot reload: edit a .glsl file and see the result within a second.
        if (Time::time() - lastShaderCheck > 1.0) {
            lastShaderCheck = Time::time();
            AssetManager::reloadShaders(false);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        for (auto& layer : m_Layers) layer->onUpdate(dt);
        for (auto& layer : m_Layers) layer->onImGui();

        ImGui::Render();
        glm::ivec2 fb = framebufferSize();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, fb.x, fb.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        DebugDraw::endFrame(Time::deltaTime());

        if (!m_PendingScreenshot.empty()) {
            captureScreenshot(m_PendingScreenshot);
            m_PendingScreenshot.clear();
        }
        if (!m_Config.screenshotPath.empty() && (int)Time::frameCount() == m_Config.screenshotFrame) {
            captureScreenshot(m_Config.screenshotPath);
            m_Running = false;
        }

        glfwSwapBuffers(m_Window);
    }
    return 0;
}

void Application::captureScreenshot(const std::string& path) {
    glm::ivec2 fb = framebufferSize();
    if (fb.x <= 0 || fb.y <= 0) return;
    std::vector<uint8_t> pixels((size_t)fb.x * (size_t)fb.y * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, fb.x, fb.y, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    // OpenGL rows start at the bottom; PNG rows start at the top.
    std::vector<uint8_t> flipped(pixels.size());
    size_t row = (size_t)fb.x * 4;
    for (int y = 0; y < fb.y; ++y)
        std::memcpy(flipped.data() + row * (size_t)y, pixels.data() + row * (size_t)(fb.y - 1 - y), row);
    for (size_t i = 3; i < flipped.size(); i += 4) flipped[i] = 255;
    if (stbi_write_png(path.c_str(), fb.x, fb.y, 4, flipped.data(), (int)row)) LOG_INFO("Screenshot saved to %s", path.c_str());
    else LOG_ERROR("Failed to write screenshot %s", path.c_str());
}

void Application::shutdown() {
    for (auto it = m_Layers.rbegin(); it != m_Layers.rend(); ++it) (*it)->onDetach();
    m_Layers.clear();
    AssetManager::shutdown();
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    if (m_Window) glfwDestroyWindow(m_Window);
    glfwTerminate();
    m_Window = nullptr;
}

} // namespace Forge
