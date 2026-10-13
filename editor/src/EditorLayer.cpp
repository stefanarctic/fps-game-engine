#include "Forge/Editor/EditorLayer.h"
#include "Forge/Assets/AssetManager.h"
#include "Forge/Core/FileSystem.h"
#include "Forge/Core/Input.h"
#include "Forge/Core/Log.h"
#include "Forge/Core/Time.h"
#include "Forge/Editor/EditorTheme.h"
#include "Forge/Editor/Icons.h"
#include "Forge/Scene/SceneSerializer.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

#include <cstring>

namespace Forge::Editor {

EditorLayer::EditorLayer(Game& game, const AppConfig& config) : m_Game(game), m_Config(config) {}

// ---------------------------------------------------------------------------
// Lifetime
// ---------------------------------------------------------------------------
void EditorLayer::onAttach() {
    registerCoreComponents();
    m_Game.registerComponents();
    Theme::apply();

    m_SceneRenderer = MakeScope<SceneRenderer>();
    m_GameRenderer = MakeScope<SceneRenderer>();

    std::string path = m_Config.scene.empty() ? m_Game.startupScene() : m_Config.scene;
    if (m_Config.rebuildScene || path.empty() || !FileSystem::exists(FileSystem::resolve(path)) || !openScene(path)) {
        m_Scene = MakeScope<Scene>(path.empty() ? "Untitled" : FileSystem::stem(path));
        m_Game.setupScene(*m_Scene);
        m_Game.createDefaultScene(*m_Scene);
        m_Scene->path = path;
        m_Scene->updateTransforms();
        if (!path.empty()) {
            LOG_INFO("Created default scene; saving it to '%s'", path.c_str());
            SceneSerializer::save(*m_Scene, path);
        }
    }
    updateTitle();
    LOG_INFO("Forge Editor ready. RMB+WASD to fly, W/E/R gizmos, Ctrl+P to play.");
}

void EditorLayer::onDetach() {
    if (m_State != PlayState::Edit) stop();
    Input::setCursorLocked(false);
    m_SceneRenderer.reset();
    m_GameRenderer.reset();
    m_Scene.reset();
}

void EditorLayer::updateTitle() {
    std::string title = std::string(m_Game.name()) + " - " + (m_Scene ? m_Scene->name : "") + (m_Dirty ? "*" : "") + " - Forge Editor";
    Application::get().setTitle(title);
}

void EditorLayer::onUpdate(float dt) {
    static bool first = true;
    if (first) {
        first = false;
        if (!m_Config.select.empty()) select(m_Scene->findByName(m_Config.select));
        if (m_Config.autoplay) play();
        if (m_Config.autoplay && m_Config.pauseMenu == PauseMenuStart::Main) m_PauseMenu.open();
        if (m_Config.autoplay && m_Config.pauseMenu == PauseMenuStart::Settings) m_PauseMenu.openSettings();
    }

    Input::setEnabled(m_GameCaptured);
    switch (m_State) {
    case PlayState::Playing:
        // The in-game pause menu freezes the simulation like the standalone player does.
        if (m_PauseMenu.isOpen()) m_Scene->updateTransforms();
        else m_Scene->update(dt * Time::timeScale());
        break;
    case PlayState::Paused:
        if (m_StepFrame) {
            m_Scene->update(1.0f / 60.0f);
            m_StepFrame = false;
        } else {
            m_Scene->updateTransforms();
        }
        break;
    case PlayState::Edit: m_Scene->update(dt); break;
    }
}

void EditorLayer::onImGui() {
    bool tinted = m_State != PlayState::Edit;
    if (tinted) Theme::pushPlayModeTint();
    ImGuizmo::BeginFrame();

    drawDockspace();
    if (m_ShowHierarchy) drawHierarchy();
    if (m_ShowSceneView) drawSceneView();
    if (m_ShowGameView) drawGameView();
    if (m_ShowInspector) drawInspector();
    if (m_ShowLighting) drawLighting();
    if (m_ShowProject) drawProject();
    if (m_ShowConsole) drawConsole();
    if (m_ShowImGuiDemo) ImGui::ShowDemoWindow(&m_ShowImGuiDemo);
    drawPopups();
    handleShortcuts();

    // Put the most useful tabs in front once the default layout exists.
    static int frame = 0;
    if (++frame == 3) {
        ImGui::SetWindowFocus("Inspector");
        ImGui::SetWindowFocus("Project");
        ImGui::SetWindowFocus(m_State == PlayState::Edit ? "Scene" : "Game");
    }

    if (tinted) Theme::popPlayModeTint();
    updateTitle();
}

// ---------------------------------------------------------------------------
// Scenes
// ---------------------------------------------------------------------------
void EditorLayer::newScene() {
    if (m_State != PlayState::Edit) stop();
    m_Scene = MakeScope<Scene>("Untitled");
    m_Game.setupScene(*m_Scene);
    // Same defaults as a new Unity scene.
    Entity cam = m_Scene->createEntity("Main Camera");
    m_Scene->add<Camera>(cam);
    m_Scene->get<EntityInfo>(cam).tag = "MainCamera";
    m_Scene->get<Transform>(cam).position = glm::vec3(0, 1, 10);
    m_Scene->get<Transform>(cam).rotation = glm::vec3(0, 0, 0);
    Entity sun = m_Scene->createEntity("Directional Light");
    Light& l = m_Scene->add<Light>(sun);
    l.type = LightType::Directional;
    l.color = glm::vec3(1.0f, 0.96f, 0.84f);
    m_Scene->get<Transform>(sun).position = glm::vec3(0, 3, 0);
    m_Scene->get<Transform>(sun).rotation = glm::vec3(-50, -30, 0);
    m_Scene->updateTransforms();
    m_Selected = NullEntity;
    m_UndoStack.clear();
    m_RedoStack.clear();
    m_Dirty = false;
    LOG_INFO("New scene");
}

bool EditorLayer::openScene(const std::string& path) {
    if (m_State != PlayState::Edit) stop();
    auto scene = MakeScope<Scene>(FileSystem::stem(path));
    m_Game.setupScene(*scene);
    if (!SceneSerializer::load(*scene, path)) return false;
    m_Scene = std::move(scene);
    m_Selected = NullEntity;
    m_SelectedAsset.clear();
    m_UndoStack.clear();
    m_RedoStack.clear();
    m_Dirty = false;
    return true;
}

void EditorLayer::saveScene() {
    if (m_State != PlayState::Edit) {
        LOG_WARN("Exit play mode before saving the scene");
        return;
    }
    if (m_Scene->path.empty()) {
        m_SaveAsPopup = true;
        return;
    }
    if (SceneSerializer::save(*m_Scene, m_Scene->path)) m_Dirty = false;
}

void EditorLayer::saveSceneAs(const std::string& path) {
    m_Scene->name = FileSystem::stem(path);
    m_Scene->path = path;
    saveScene();
}

// ---------------------------------------------------------------------------
// Play mode
// ---------------------------------------------------------------------------
void EditorLayer::play() {
    if (m_State != PlayState::Edit) return;
    if (m_EditOpen) endEdit();
    m_PlaySnapshot = SceneSerializer::serialize(*m_Scene).dump(0);
    m_State = PlayState::Playing;
    m_Scene->beginPlay();
    m_FocusGameTab = true;
    LOG_INFO("Entered play mode");
}

void EditorLayer::stop() {
    if (m_State == PlayState::Edit) return;
    uint64_t selection = m_Scene->valid(m_Selected) ? m_Scene->get<EntityInfo>(m_Selected).uuid : 0;
    m_Scene->endPlay();
    Json json;
    if (Json::parse(m_PlaySnapshot, json)) SceneSerializer::deserialize(*m_Scene, json);
    m_Selected = m_Scene->findByUUID(selection);
    m_State = PlayState::Edit;
    m_PauseMenu.close();
    captureGameMouse(false);
    Input::setEnabled(false);
    DebugDraw::clearAll();
    m_FocusSceneTab = true;
    LOG_INFO("Exited play mode (scene restored)");
}

void EditorLayer::togglePause() {
    if (m_State == PlayState::Playing) m_State = PlayState::Paused;
    else if (m_State == PlayState::Paused) m_State = PlayState::Playing;
}

void EditorLayer::captureGameMouse(bool capture) {
    if (capture == m_GameCaptured) return;
    m_GameCaptured = capture;
    Input::setCursorLocked(capture);
    // While captured ImGui ignores the mouse so the game owns it.
    if (capture) ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
    else ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
}

// ---------------------------------------------------------------------------
// Undo
// ---------------------------------------------------------------------------
EditorLayer::Snapshot EditorLayer::takeSnapshot() {
    Snapshot s;
    s.scene = SceneSerializer::serialize(*m_Scene).dump(0);
    s.selection = m_Scene->valid(m_Selected) ? m_Scene->get<EntityInfo>(m_Selected).uuid : 0;
    return s;
}

void EditorLayer::restoreSnapshot(const Snapshot& s) {
    Json json;
    if (!Json::parse(s.scene, json)) return;
    SceneSerializer::deserialize(*m_Scene, json);
    m_Selected = m_Scene->findByUUID(s.selection);
}

void EditorLayer::recordUndo() {
    if (m_State != PlayState::Edit) return;
    m_UndoStack.push_back(takeSnapshot());
    if (m_UndoStack.size() > 100) m_UndoStack.erase(m_UndoStack.begin());
    m_RedoStack.clear();
    m_Dirty = true;
}

void EditorLayer::beginEdit() {
    if (m_State != PlayState::Edit || m_EditOpen) return;
    m_PendingEdit = takeSnapshot();
    m_EditOpen = true;
}

void EditorLayer::endEdit() {
    if (!m_EditOpen) return;
    m_EditOpen = false;
    m_UndoStack.push_back(std::move(m_PendingEdit));
    if (m_UndoStack.size() > 100) m_UndoStack.erase(m_UndoStack.begin());
    m_RedoStack.clear();
    m_Dirty = true;
}

void EditorLayer::undo() {
    if (m_State != PlayState::Edit || m_UndoStack.empty()) return;
    m_RedoStack.push_back(takeSnapshot());
    restoreSnapshot(m_UndoStack.back());
    m_UndoStack.pop_back();
    m_Dirty = true;
}

void EditorLayer::redo() {
    if (m_State != PlayState::Edit || m_RedoStack.empty()) return;
    m_UndoStack.push_back(takeSnapshot());
    restoreSnapshot(m_RedoStack.back());
    m_RedoStack.pop_back();
    m_Dirty = true;
}

PropertyContext EditorLayer::propertyContext() {
    PropertyContext ctx;
    ctx.scene = m_Scene.get();
    ctx.beginEdit = [this] { beginEdit(); };
    ctx.endEdit = [this] { endEdit(); };
    ctx.record = [this] { recordUndo(); };
    return ctx;
}

// ---------------------------------------------------------------------------
// Entities
// ---------------------------------------------------------------------------
void EditorLayer::select(Entity e) {
    m_Selected = m_Scene->valid(e) ? e : NullEntity;
    if (m_Selected != NullEntity) {
        m_SelectedAsset.clear();
        m_PendingScroll = m_Selected;
    }
}

glm::vec3 EditorLayer::spawnPosition() {
    glm::vec3 p = m_Camera.position + m_Camera.forward() * 10.0f;
    // Drop onto whatever is below the pivot (or the ground plane).
    RaycastHit hit;
    Ray down{p + glm::vec3(0, 50, 0), glm::vec3(0, -1, 0)};
    m_Scene->physics().rebuild(*m_Scene);
    if (m_Scene->physics().raycast(down, 200.0f, hit)) p.y = hit.point.y + 0.5f;
    else p.y = std::max(p.y, 0.5f);
    return glm::round(p * 2.0f) * 0.5f;
}

Entity EditorLayer::createEntity(const std::string& name, Entity parent) {
    recordUndo();
    Entity e = m_Scene->createEntity(name, parent);
    if (!m_Scene->valid(parent)) m_Scene->get<Transform>(e).position = spawnPosition();
    m_Scene->updateTransforms();
    select(e);
    return e;
}

Entity EditorLayer::createPrimitive(const std::string& name, const std::string& mesh) {
    Entity e = createEntity(name);
    m_Scene->add<MeshRenderer>(e).mesh = mesh;
    BoxCollider& box = m_Scene->add<BoxCollider>(e);
    if (Ref<Model> model = AssetManager::getModel(mesh)) {
        box.center = model->bounds.center();
        box.size = glm::max(model->bounds.size(), glm::vec3(0.01f));
    }
    return e;
}

Entity EditorLayer::createLight(LightType type) {
    const char* names[] = {"Directional Light", "Point Light", "Spot Light"};
    Entity e = createEntity(names[(int)type]);
    Light& l = m_Scene->add<Light>(e);
    l.type = type;
    if (type == LightType::Directional) m_Scene->get<Transform>(e).rotation = glm::vec3(-50, -30, 0);
    if (type == LightType::Spot) m_Scene->get<Transform>(e).rotation = glm::vec3(-90, 0, 0);
    if (type != LightType::Directional) m_Scene->get<Transform>(e).position.y += 2.0f;
    return e;
}

Entity EditorLayer::createCamera() {
    Entity e = createEntity("Camera");
    m_Scene->add<Camera>(e).primary = false;
    m_Scene->get<Transform>(e).position.y += 1.0f;
    return e;
}

Entity EditorLayer::instantiateModel(const std::string& path, const glm::vec3* position) {
    Ref<Model> model = AssetManager::getModel(path);
    if (!model) return NullEntity;
    Entity e = createEntity(FileSystem::stem(path));
    if (position) m_Scene->get<Transform>(e).position = *position - glm::vec3(0, model->bounds.min.y, 0);
    m_Scene->add<MeshRenderer>(e).mesh = path;
    return e;
}

void EditorLayer::deleteSelected() {
    if (!m_Scene->valid(m_Selected)) return;
    recordUndo();
    m_Scene->destroyEntity(m_Selected);
    m_Selected = NullEntity;
}

void EditorLayer::duplicateSelected() {
    if (!m_Scene->valid(m_Selected)) return;
    recordUndo();
    select(m_Scene->duplicate(m_Selected));
    m_Scene->updateTransforms();
}

void EditorLayer::copySelected() {
    if (!m_Scene->valid(m_Selected)) return;
    m_Clipboard = SceneSerializer::serializeEntities(*m_Scene, {m_Selected}).dump(0);
}

void EditorLayer::paste() {
    if (m_Clipboard.empty()) return;
    Json json;
    if (!Json::parse(m_Clipboard, json)) return;
    recordUndo();
    std::vector<Entity> roots = SceneSerializer::instantiate(*m_Scene, json);
    if (!roots.empty()) select(roots[0]);
}

void EditorLayer::focusSelected() {
    if (!m_Scene->valid(m_Selected)) return;
    const Transform& t = m_Scene->get<Transform>(m_Selected);
    glm::vec3 center = t.worldPosition();
    float radius = 1.0f;
    if (MeshRenderer* mr = m_Scene->tryGet<MeshRenderer>(m_Selected)) {
        if (mr->model) {
            AABB b = mr->model->bounds.transformed(t.world);
            center = b.center();
            radius = std::max(glm::length(b.extents()), 0.5f);
        }
    }
    m_Camera.focus(center, radius);
}

// ---------------------------------------------------------------------------
// Dockspace, menu bar and toolbar
// ---------------------------------------------------------------------------
void EditorLayer::drawDockspace() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(25, 25, 25, 255));
    ImGui::Begin("##ForgeEditorRoot", nullptr, flags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    drawMenuBar();
    drawToolbar();

    ImGuiID dockId = ImGui::GetID("ForgeDockspace");
    ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockId);
    if (m_ResetLayout || node == nullptr || (!node->IsSplitNode() && node->Windows.Size == 0)) {
        buildDefaultLayout(dockId);
        m_ResetLayout = false;
    }
    ImGui::DockSpace(dockId, ImVec2(0, 0), ImGuiDockNodeFlags_None);
    ImGui::End();
}

void EditorLayer::buildDefaultLayout(unsigned int dockspaceId) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, vp->WorkSize);

    ImGuiID main = dockspaceId;
    ImGuiID right = ImGui::DockBuilderSplitNode(main, ImGuiDir_Right, 0.25f, nullptr, &main);
    ImGuiID bottom = ImGui::DockBuilderSplitNode(main, ImGuiDir_Down, 0.30f, nullptr, &main);
    ImGuiID left = ImGui::DockBuilderSplitNode(main, ImGuiDir_Left, 0.22f, nullptr, &main);

    ImGui::DockBuilderDockWindow("Hierarchy", left);
    ImGui::DockBuilderDockWindow("Scene", main);
    ImGui::DockBuilderDockWindow("Game", main);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Lighting", right);
    ImGui::DockBuilderDockWindow("Project", bottom);
    ImGui::DockBuilderDockWindow("Console", bottom);
    ImGui::DockBuilderFinish(dockspaceId);
}

void EditorLayer::drawCreateMenuItems(Entity parent) {
    if (ImGui::MenuItem("Create Empty", "Ctrl+Shift+N")) createEntity("GameObject", parent);
    if (ImGui::BeginMenu("3D Object")) {
        const char* prims[] = {"Cube", "Sphere", "Capsule", "Cylinder", "Plane", "Quad"};
        for (const char* p : prims) {
            if (ImGui::MenuItem(p)) {
                Entity e = createPrimitive(p, std::string("builtin:") + p);
                if (m_Scene->valid(parent)) m_Scene->setParent(e, parent, true);
            }
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Light")) {
        if (ImGui::MenuItem("Directional Light")) createLight(LightType::Directional);
        if (ImGui::MenuItem("Point Light")) createLight(LightType::Point);
        if (ImGui::MenuItem("Spot Light")) createLight(LightType::Spot);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Effects")) {
        if (ImGui::MenuItem("Particle System")) {
            Entity e = createEntity("Particle System", parent);
            m_Scene->add<ParticleEmitter>(e);
        }
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Camera")) createCamera();
}

void EditorLayer::drawMenuBar() {
    if (!ImGui::BeginMenuBar()) return;
    bool editing = m_State == PlayState::Edit;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Scene", "Ctrl+N")) newScene();
        if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) m_OpenScenePopup = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Save", "Ctrl+S", false, editing)) saveScene();
        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, editing)) m_SaveAsPopup = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) Application::get().close();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, editing && !m_UndoStack.empty())) undo();
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, editing && !m_RedoStack.empty())) redo();
        ImGui::Separator();
        bool sel = m_Scene->valid(m_Selected);
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, sel)) copySelected();
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, !m_Clipboard.empty())) paste();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, sel)) duplicateSelected();
        if (ImGui::MenuItem("Delete", "Del", false, sel)) deleteSelected();
        ImGui::Separator();
        if (ImGui::MenuItem("Frame Selected", "F", false, sel)) focusSelected();
        ImGui::Separator();
        if (ImGui::MenuItem(editing ? "Play" : "Stop", "Ctrl+P")) editing ? play() : stop();
        if (ImGui::MenuItem("Pause", "Ctrl+Shift+P", m_State == PlayState::Paused, !editing)) togglePause();
        if (ImGui::MenuItem("Step", nullptr, false, m_State == PlayState::Paused)) m_StepFrame = true;
        ImGui::Separator();
        bool vsync = Application::get().vsync();
        if (ImGui::MenuItem("VSync", nullptr, &vsync)) Application::get().setVSync(vsync);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Assets")) {
        if (ImGui::MenuItem("Create Material")) {
            std::string dir = m_ProjectDir.empty() ? "materials" : m_ProjectDir;
            std::string path;
            for (int i = 0; i < 1000; ++i) {
                path = FileSystem::join(dir, i == 0 ? "New Material.mat" : "New Material " + std::to_string(i) + ".mat");
                if (!FileSystem::exists(FileSystem::resolve(path))) break;
            }
            if (AssetManager::createMaterial(path)) m_SelectedAsset = path, m_Selected = NullEntity;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reload Shaders")) {
            int n = AssetManager::reloadShaders(true);
            LOG_INFO("Reloaded %d shaders", n);
        }
        if (ImGui::MenuItem("Reload Assets")) {
            AssetManager::reloadAssets();
            m_Scene->each<MeshRenderer>([](Entity, MeshRenderer& mr) {
                mr.loadedMesh = "\x01";
                mr.loadedMaterial = "\x01";
            });
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("GameObject")) {
        drawCreateMenuItems(NullEntity);
        ImGui::Separator();
        if (ImGui::MenuItem("Create Empty Child", nullptr, false, m_Scene->valid(m_Selected))) createEntity("GameObject", m_Selected);
        if (ImGui::MenuItem("Clear Parent", nullptr, false, m_Scene->valid(m_Scene->parentOf(m_Selected)))) {
            recordUndo();
            m_Scene->setParent(m_Selected, NullEntity, true);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Component")) {
        bool sel = m_Scene->valid(m_Selected);
        std::string category;
        for (const auto& meta : ComponentRegistry::instance().components()) {
            if (meta->hidden || !meta->removable) continue;
            if (meta->category != category) {
                category = meta->category;
                ImGui::SeparatorText(category.c_str());
            }
            bool has = sel && meta->has(m_Scene->registry(), m_Selected);
            if (ImGui::MenuItem(meta->name.c_str(), nullptr, has, sel && !has)) {
                recordUndo();
                meta->add(m_Scene->registry(), m_Selected);
            }
        }
        ImGui::EndMenu();
    }
    m_Game.onEditorMenu(*m_Scene);
    if (ImGui::BeginMenu("Window")) {
        ImGui::MenuItem("Scene", nullptr, &m_ShowSceneView);
        ImGui::MenuItem("Game", nullptr, &m_ShowGameView);
        ImGui::MenuItem("Hierarchy", nullptr, &m_ShowHierarchy);
        ImGui::MenuItem("Inspector", nullptr, &m_ShowInspector);
        ImGui::MenuItem("Lighting", nullptr, &m_ShowLighting);
        ImGui::MenuItem("Project", nullptr, &m_ShowProject);
        ImGui::MenuItem("Console", nullptr, &m_ShowConsole);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout")) {
            m_ShowSceneView = m_ShowGameView = m_ShowHierarchy = m_ShowInspector = true;
            m_ShowLighting = m_ShowProject = m_ShowConsole = true;
            m_ResetLayout = true;
        }
        ImGui::MenuItem("ImGui Demo", nullptr, &m_ShowImGuiDemo);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About Forge Engine")) m_ShowAbout = true;
        ImGui::EndMenu();
    }

    // Right-aligned frame stats
    char stats[64];
    std::snprintf(stats, sizeof(stats), "%.0f FPS  %.2f ms", Time::fps(), Time::fps() > 0 ? 1000.0f / Time::fps() : 0.0f);
    float w = ImGui::CalcTextSize(stats).x;
    ImGui::SameLine(ImGui::GetWindowWidth() - w - 16.0f);
    ImGui::TextDisabled("%s", stats);
    ImGui::EndMenuBar();
}

void EditorLayer::drawToolbar() {
    const float height = 34.0f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(25, 25, 25, 255));
    ImGui::BeginChild("##toolbar", ImVec2(0, height), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::SetCursorPos(ImVec2(8, 5));

    if (iconButton("##hand", Icon::Hand, m_Tool == Tool::Hand, ImVec2(30, 24), "View tool (Q)")) m_Tool = Tool::Hand;
    ImGui::SameLine(0, 1);
    if (iconButton("##move", Icon::Move, m_Tool == Tool::Move, ImVec2(30, 24), "Move tool (W)")) m_Tool = Tool::Move;
    ImGui::SameLine(0, 1);
    if (iconButton("##rotate", Icon::Rotate, m_Tool == Tool::Rotate, ImVec2(30, 24), "Rotate tool (E)")) m_Tool = Tool::Rotate;
    ImGui::SameLine(0, 1);
    if (iconButton("##scale", Icon::Scale, m_Tool == Tool::Scale, ImVec2(30, 24), "Scale tool (R)")) m_Tool = Tool::Scale;

    ImGui::SameLine(0, 12);
    ImGui::SetCursorPosY(5);
    if (ImGui::Button(m_LocalSpace ? "Local" : "Global", ImVec2(64, 24))) m_LocalSpace = !m_LocalSpace;
    ImGui::SameLine(0, 4);
    ImGui::SetCursorPosY(5);
    ImGui::PushStyleColor(ImGuiCol_Button, m_Snap ? ImGui::GetColorU32(ImGuiCol_Header) : ImGui::GetColorU32(ImGuiCol_Button));
    if (ImGui::Button("Snap", ImVec2(52, 24))) m_Snap = !m_Snap;
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Snap: %.2fm / %.0f deg / %.2f (hold Ctrl to toggle)", m_SnapMove, m_SnapRotate, m_SnapScale);

    // Play / Pause / Step centered like Unity
    float center = ImGui::GetWindowWidth() * 0.5f;
    ImGui::SameLine(center - 50.0f);
    ImGui::SetCursorPosY(5);
    bool playing = m_State != PlayState::Edit;
    if (iconButton("##play", Icon::Play, playing, ImVec2(32, 24), playing ? "Stop (Ctrl+P)" : "Play (Ctrl+P)")) playing ? stop() : play();
    ImGui::SameLine(0, 1);
    if (iconButton("##pause", Icon::Pause, m_State == PlayState::Paused, ImVec2(32, 24), "Pause (Ctrl+Shift+P)")) togglePause();
    ImGui::SameLine(0, 1);
    if (iconButton("##step", Icon::Step, false, ImVec2(32, 24), "Step one frame")) {
        if (m_State == PlayState::Playing) m_State = PlayState::Paused;
        if (m_State == PlayState::Paused) m_StepFrame = true;
    }

    // Scene name on the right
    std::string label = m_Scene->name + (m_Dirty ? "*" : "");
    float w = ImGui::CalcTextSize(label.c_str()).x;
    ImGui::SameLine(ImGui::GetWindowWidth() - w - 20.0f);
    ImGui::SetCursorPosY(9);
    ImGui::TextDisabled("%s", label.c_str());

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------
// Popups and shortcuts
// ---------------------------------------------------------------------------
void EditorLayer::drawPopups() {
    if (m_OpenScenePopup) {
        ImGui::OpenPopup("Open Scene");
        m_OpenScenePopup = false;
    }
    if (m_SaveAsPopup) {
        ImGui::OpenPopup("Save Scene As");
        std::snprintf(m_SaveAsName, sizeof(m_SaveAsName), "%s", m_Scene->name.c_str());
        m_SaveAsPopup = false;
    }
    if (m_ShowAbout) {
        ImGui::OpenPopup("About Forge Engine");
        m_ShowAbout = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420, 360), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Open Scene")) {
        ImGui::TextDisabled("Scenes in the asset folder:");
        ImGui::BeginChild("##scenes", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 4), ImGuiChildFlags_Borders);
        for (const std::string& s : FileSystem::findAssets({".scene"})) {
            if (ImGui::Selectable(s.c_str(), s == m_Scene->path, ImGuiSelectableFlags_AllowDoubleClick) && ImGui::IsMouseDoubleClicked(0)) {
                openScene(s);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndChild();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) ImGui::CloseCurrentPopup();
        ImGui::SameLine();
        ImGui::TextDisabled("Double-click a scene to open it");
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Save Scene As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("assets/scenes/");
        ImGui::SameLine(0, 0);
        ImGui::SetNextItemWidth(220);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputText("##name", m_SaveAsName, sizeof(m_SaveAsName), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine(0, 0);
        ImGui::Text(".scene");
        if ((ImGui::Button("Save", ImVec2(100, 0)) || enter) && m_SaveAsName[0]) {
            saveSceneAs(std::string("scenes/") + m_SaveAsName + ".scene");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("About Forge Engine", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushFont(Application::get().fonts().bold);
        ImGui::Text("Forge Engine 1.0");
        ImGui::PopFont();
        ImGui::Text("ECS game engine + Unity-style editor (C++17, OpenGL 3.3).");
        ImGui::Separator();
        ImGui::BulletText("Scene view: RMB + WASD/QE fly, MMB pan, Alt+LMB orbit, wheel zoom, F frame");
        ImGui::BulletText("Tools: Q view, W move, E rotate, R scale, hold Ctrl to snap");
        ImGui::BulletText("Ctrl+P play/stop, Ctrl+S save, Ctrl+Z/Y undo/redo, Ctrl+D duplicate");
        ImGui::BulletText("Game view: click to capture the mouse, Esc opens the pause menu and releases it");
        ImGui::BulletText("Shaders hot-reload when their .glsl files change");
        if (ImGui::Button("Close", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void EditorLayer::handleShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    bool ctrl = io.KeyCtrl, shift = io.KeyShift;

    // Play mode toggles work even while the game owns the mouse.
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_P, false)) {
        if (shift) togglePause();
        else (m_State == PlayState::Edit) ? play() : stop();
        return;
    }
    if (m_GameCaptured || io.WantTextInput || m_CameraBusy) return;

    if (ctrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_S, false)) shift ? (void)(m_SaveAsPopup = true) : saveScene();
        if (ImGui::IsKeyPressed(ImGuiKey_N, false)) shift ? (void)createEntity("GameObject") : newScene();
        if (ImGui::IsKeyPressed(ImGuiKey_O, false)) m_OpenScenePopup = true;
        if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) shift ? redo() : undo();
        if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) redo();
        if (ImGui::IsKeyPressed(ImGuiKey_D, false)) duplicateSelected();
        if (ImGui::IsKeyPressed(ImGuiKey_C, false)) copySelected();
        if (ImGui::IsKeyPressed(ImGuiKey_V, false)) paste();
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) m_Tool = Tool::Hand;
    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) m_Tool = Tool::Move;
    if (ImGui::IsKeyPressed(ImGuiKey_E, false)) m_Tool = Tool::Rotate;
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) m_Tool = Tool::Scale;
    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) focusSelected();
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) deleteSelected();
}

} // namespace Forge::Editor
