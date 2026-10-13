#pragma once
#include "Forge/Core/Game.h"

namespace Forge {

// In-game pause menu (Resume / Settings / Quit) with a settings page that edits
// the persistent GameSettings. Used by both the standalone player and the
// editor's Game view; games get it for free.
//
//   if (escapePressed) menu.open();
//   if (menu.isOpen()) switch (menu.draw(viewport, "My Game")) { ... }
//
// draw() must be called inside an ImGui window that covers the viewport.
class PauseMenu {
public:
    enum class Action { None, Resume, Quit };

    void open();
    void openSettings(); // opens straight on the settings page
    void close();
    bool isOpen() const { return m_Open; }

    // Escape on the main page resumes, on the settings page goes back.
    Action draw(const Viewport& viewport, const char* title, const char* quitLabel = "Quit");

private:
    enum class Page { Main, Settings };
    void drawSettings(float scale, float width);
    void leaveSettings();

    Page m_Page = Page::Main;
    bool m_Open = false;
    int m_OpenFrame = -1;
    bool m_SettingsChanged = false;
};

} // namespace Forge
