#pragma once
// Forge Editor: a Unity-style editor built with Dear ImGui (docking).
//
//  +--------------------------------------------------------------------+
//  | File Edit Assets GameObject Component Window Help                   |
//  | [Hand Move Rotate Scale]      [ > ][ || ][ >| ]                     |
//  +-----------+------------------------------------+-------------------+
//  | Hierarchy | Scene | Game                       | Inspector         |
//  |           |                                    | Lighting          |
//  |           |                                    |                   |
//  +-----------+------------------------------------+                   |
//  | Project | Console                              |                   |
//  +------------------------------------------------+-------------------+
#include "Forge/Core/Application.h"
#include "Forge/Core/Game.h"
#include "Forge/Editor/EditorCamera.h"
#include "Forge/Editor/PropertyDrawer.h"
#include "Forge/Renderer/SceneRenderer.h"
#include "Forge/UI/PauseMenu.h"

namespace Forge::Editor {

enum class PlayState { Edit, Playing, Paused };
enum class Tool { Hand, Move, Rotate, Scale };

class EditorLayer : public Layer {
public:
    EditorLayer(Game& game, const AppConfig& config);

    void onAttach() override;
    void onDetach() override;
    void onUpdate(float dt) override;
    void onImGui() override;

private:
    // Scene management
    void newScene();
    bool openScene(const std::string& path);
    void saveScene();
    void saveSceneAs(const std::string& path);
    void updateTitle();

    // Play mode (the edit-time scene is snapshotted to JSON and restored on stop)
    void play();
    void stop();
    void togglePause();

    // Undo / redo (whole-scene JSON snapshots; cheap for game-sized scenes)
    struct Snapshot {
        std::string scene;
        uint64_t selection = 0;
    };
    Snapshot takeSnapshot();
    void restoreSnapshot(const Snapshot& s);
    void recordUndo();     // call right before a discrete change
    void beginEdit();      // continuous edit started (drag / typing)
    void endEdit();        // continuous edit committed
    void undo();
    void redo();
    void markDirty() { m_Dirty = true; }
    PropertyContext propertyContext();

    // Entities
    void select(Entity e);
    Entity createEntity(const std::string& name, Entity parent = NullEntity);
    Entity createPrimitive(const std::string& name, const std::string& mesh);
    Entity createLight(LightType type);
    Entity createCamera();
    Entity instantiateModel(const std::string& path, const glm::vec3* position = nullptr);
    void deleteSelected();
    void duplicateSelected();
    void copySelected();
    void paste();
    void focusSelected();
    glm::vec3 spawnPosition();

    // Panels
    void drawDockspace();
    void buildDefaultLayout(unsigned int dockspaceId);
    void drawMenuBar();
    void drawToolbar();
    void drawCreateMenuItems(Entity parent);
    void drawHierarchy();
    void drawHierarchyNode(Entity e);
    void drawInspector();
    void drawEntityInspector(Entity e);
    void drawAssetInspector(const std::string& path);
    void drawProject();
    void drawConsole();
    void drawSceneView();
    void drawGameView();
    void drawLighting();
    void drawPopups();
    void handleShortcuts();

    // Scene view helpers
    void buildGizmoLines();
    void drawSceneIcons(const glm::mat4& viewProj, const glm::vec2& origin, const glm::vec2& size, bool& clickedIcon);
    Entity pickEntity(const Ray& ray);
    void drawTransformGizmo(const glm::mat4& view, const glm::mat4& proj, const glm::vec2& origin, const glm::vec2& size);

    Game& m_Game;
    AppConfig m_Config;
    Scope<Scene> m_Scene;
    PlayState m_State = PlayState::Edit;
    std::string m_PlaySnapshot;
    bool m_StepFrame = false;

    Entity m_Selected = NullEntity;
    std::string m_SelectedAsset;
    std::string m_Clipboard;

    EditorCamera m_Camera;
    Scope<SceneRenderer> m_SceneRenderer;
    Scope<SceneRenderer> m_GameRenderer;
    LineBatch m_Gizmos;

    Tool m_Tool = Tool::Move;
    bool m_LocalSpace = false;
    bool m_Snap = false;
    float m_SnapMove = 0.5f, m_SnapRotate = 15.0f, m_SnapScale = 0.1f;
    bool m_GizmoWasUsing = false;
    DebugView m_SceneDebugView = DebugView::Lit;
    bool m_ShowGrid = true;
    bool m_ShowGizmos = true;
    bool m_ShowStats = false;

    bool m_SceneHovered = false;
    bool m_CameraBusy = false;
    bool m_GameCaptured = false;
    PauseMenu m_PauseMenu;           // in-game pause menu (Esc in the Game view); freezes the simulation
    void captureGameMouse(bool capture);
    bool m_FocusGameTab = false;
    bool m_FocusSceneTab = false;

    // Undo
    std::vector<Snapshot> m_UndoStack;
    std::vector<Snapshot> m_RedoStack;
    Snapshot m_PendingEdit;
    bool m_EditOpen = false;
    bool m_Dirty = false;

    // Panels
    bool m_ShowHierarchy = true, m_ShowInspector = true, m_ShowProject = true, m_ShowConsole = true;
    bool m_ShowSceneView = true, m_ShowGameView = true, m_ShowLighting = true, m_ShowImGuiDemo = false;
    bool m_ShowAbout = false;
    bool m_ResetLayout = false;
    bool m_OpenScenePopup = false;
    bool m_SaveAsPopup = false;
    char m_SaveAsName[128] = "NewScene";
    char m_HierarchyFilter[128] = "";
    std::string m_ProjectDir;
    char m_ProjectFilter[128] = "";
    float m_ProjectIconSize = 64.0f;
    bool m_ConsoleInfo = true, m_ConsoleWarn = true, m_ConsoleError = true, m_ConsoleAutoScroll = true;
    char m_ConsoleFilter[128] = "";
    uint64_t m_ConsoleRevision = 0;
    int m_ConsoleSelected = -1;
    Entity m_PendingScroll = NullEntity;
};

} // namespace Forge::Editor
