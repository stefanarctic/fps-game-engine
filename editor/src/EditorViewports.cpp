// Scene view (editor camera, gizmos, picking) and Game view (play mode, HUD).
#include "Forge/Editor/EditorLayer.h"
#include "Forge/Assets/AssetManager.h"
#include "Forge/Core/FileSystem.h"
#include "Forge/Core/Input.h"
#include "Forge/Core/Log.h"
#include "Forge/Core/Time.h"
#include "Forge/Editor/EditorTheme.h"
#include "Forge/Editor/Icons.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

#include <algorithm>

namespace Forge::Editor {

namespace {
Ray screenRay(const glm::mat4& viewProj, const glm::vec2& origin, const glm::vec2& size, const glm::vec2& mouse) {
    glm::vec2 ndc((mouse.x - origin.x) / size.x * 2.0f - 1.0f, 1.0f - (mouse.y - origin.y) / size.y * 2.0f);
    glm::mat4 inv = glm::inverse(viewProj);
    glm::vec4 n = inv * glm::vec4(ndc, -1.0f, 1.0f);
    glm::vec4 f = inv * glm::vec4(ndc, 1.0f, 1.0f);
    glm::vec3 nearP = glm::vec3(n) / n.w, farP = glm::vec3(f) / f.w;
    return Ray{nearP, glm::normalize(farP - nearP)};
}

bool toScreen(const glm::mat4& viewProj, const glm::vec3& p, const glm::vec2& origin, const glm::vec2& size, ImVec2& out) {
    glm::vec4 clip = viewProj * glm::vec4(p, 1.0f);
    if (clip.w <= 0.05f) return false;
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    out = ImVec2(origin.x + (ndc.x * 0.5f + 0.5f) * size.x, origin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * size.y);
    return true;
}

void viewToolbarToggle(const char* label, bool& value) {
    ImGui::PushStyleColor(ImGuiCol_Button, value ? ImGui::GetColorU32(ImGuiCol_Header) : ImGui::GetColorU32(ImGuiCol_Button));
    if (ImGui::SmallButton(label)) value = !value;
    ImGui::PopStyleColor();
}
} // namespace

// ---------------------------------------------------------------------------
// Scene view
// ---------------------------------------------------------------------------
void EditorLayer::drawSceneView() {
    if (m_FocusSceneTab) {
        ImGui::SetNextWindowFocus();
        m_FocusSceneTab = false;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool visible = ImGui::Begin("Scene", &m_ShowSceneView, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    if (!visible) {
        m_SceneHovered = false;
        ImGui::End();
        return;
    }

    // View toolbar
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + 6, ImGui::GetCursorPosY() + 4));
    const char* views[] = {"Shaded", "Ambient Occlusion", "Normals", "Albedo", "Lighting Only"};
    int dv = (int)m_SceneDebugView;
    ImGui::SetNextItemWidth(150);
    if (ImGui::Combo("##shading", &dv, views, 5)) m_SceneDebugView = (DebugView)dv;
    ImGui::SameLine();
    viewToolbarToggle("Grid", m_ShowGrid);
    ImGui::SameLine();
    viewToolbarToggle("Gizmos", m_ShowGizmos);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110);
    ImGui::SliderFloat("##speed", &m_Camera.flySpeed, 0.5f, 100.0f, "Speed %.1f", ImGuiSliderFlags_Logarithmic);
    ImGui::SameLine();
    ImGui::TextDisabled("RMB+WASD fly | Alt+LMB orbit | MMB pan | F frame");
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    glm::vec2 size(std::max(avail.x, 8.0f), std::max(avail.y, 8.0f));
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    glm::vec2 origin(cursor.x, cursor.y);

    ImVec2 mouse = ImGui::GetMousePos();
    bool mouseInside = mouse.x >= origin.x && mouse.y >= origin.y && mouse.x < origin.x + size.x && mouse.y < origin.y + size.y;
    m_SceneHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && mouseInside;

    // Camera controls
    m_CameraBusy = m_Camera.update(ImGui::GetIO().DeltaTime, m_SceneHovered && !m_GameCaptured);

    // Render
    float aspect = size.x / size.y;
    glm::mat4 view = m_Camera.view();
    glm::mat4 proj = m_Camera.projection(aspect);
    CameraData cam;
    cam.view = view;
    cam.projection = proj;
    cam.position = m_Camera.position;
    cam.nearClip = m_Camera.nearClip;
    cam.farClip = m_Camera.farClip;
    cam.fieldOfView = m_Camera.fieldOfView;
    cam.clear = CameraClear::Skybox;

    buildGizmoLines();
    RenderOptions options;
    options.editor = true;
    options.grid = m_ShowGrid;
    options.debugView = m_SceneDebugView;
    options.gizmos = m_ShowGizmos ? &m_Gizmos : nullptr;
    if (m_Scene->valid(m_Selected)) options.outline.push_back(m_Selected);
    m_SceneRenderer->render(*m_Scene, cam, (int)size.x, (int)size.y, options);

    ImGui::Image((ImTextureID)(uintptr_t)m_SceneRenderer->outputTexture(), ImVec2(size.x, size.y), ImVec2(0, 1), ImVec2(1, 0));

    // Drop assets into the scene
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string path((const char*)payload->Data);
            Ray ray = screenRay(proj * view, origin, size, glm::vec2(mouse.x, mouse.y));
            m_Scene->physics().rebuild(*m_Scene);
            RaycastHit hit;
            glm::vec3 at;
            if (m_Scene->physics().raycast(ray, 500.0f, hit)) at = hit.point;
            else if (ray.direction.y < -0.01f) at = ray.at(-ray.origin.y / ray.direction.y);
            else at = ray.at(10.0f);
            if (assetMatchesKind(path, AssetKind::Model)) {
                instantiateModel(path, &at);
            } else if (assetMatchesKind(path, AssetKind::Scene)) {
                openScene(path);
            } else if (assetMatchesKind(path, AssetKind::Material)) {
                Entity target = pickEntity(ray);
                if (m_Scene->valid(target) && m_Scene->has<MeshRenderer>(target)) {
                    recordUndo();
                    m_Scene->get<MeshRenderer>(target).material = path;
                    select(target);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    glm::mat4 viewProj = proj * view;
    bool clickedIcon = false;
    if (m_ShowGizmos) drawSceneIcons(viewProj, origin, size, clickedIcon);
    drawTransformGizmo(view, proj, origin, size);

    // Click to select
    if (m_SceneHovered && !clickedIcon && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyAlt &&
        !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() && !m_CameraBusy) {
        select(pickEntity(screenRay(viewProj, origin, size, glm::vec2(mouse.x, mouse.y))));
    }

    // Orientation gizmo (top-right), like Unity's scene gizmo
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 c(origin.x + size.x - 50.0f, origin.y + 50.0f);
        dl->AddCircleFilled(c, 38.0f, IM_COL32(30, 30, 30, 110), 32);
        struct Axis { glm::vec3 dir; ImU32 color; const char* label; };
        Axis axes[3] = {{{1, 0, 0}, Theme::AxisX, "x"}, {{0, 1, 0}, Theme::AxisY, "y"}, {{0, 0, 1}, Theme::AxisZ, "z"}};
        glm::mat3 rot(view);
        std::sort(std::begin(axes), std::end(axes), [&](const Axis& a, const Axis& b) { return (rot * a.dir).z < (rot * b.dir).z; });
        for (const Axis& a : axes) {
            glm::vec3 v = rot * a.dir;
            ImVec2 tip(c.x + v.x * 28.0f, c.y - v.y * 28.0f);
            dl->AddLine(c, tip, a.color, 2.5f);
            dl->AddCircleFilled(tip, 7.0f, a.color, 12);
            dl->AddText(ImVec2(tip.x - 3.5f, tip.y - 8.0f), IM_COL32(20, 20, 20, 255), a.label);
        }
        dl->AddText(ImVec2(c.x - 14.0f, c.y + 40.0f), IM_COL32(200, 200, 200, 200), "Persp");
    }

    // Stats overlay
    if (m_State == PlayState::Edit) {
        const RenderStats& s = m_SceneRenderer->stats();
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%d draws  %dk tris  %d lights", s.drawCalls, s.triangles / 1000, s.lights);
        ImGui::GetWindowDrawList()->AddText(ImVec2(origin.x + 8, origin.y + size.y - 22), IM_COL32(220, 220, 220, 160), buf);
    }
    ImGui::End();
}

void EditorLayer::buildGizmoLines() {
    m_Gizmos.clear();
    if (!m_Scene->valid(m_Selected)) return;
    Entity e = m_Selected;
    const Transform& t = m_Scene->get<Transform>(e);
    glm::vec3 pos = t.worldPosition();
    const glm::vec4 green(0.55f, 1.0f, 0.55f, 1.0f), yellow(1.0f, 0.95f, 0.5f, 1.0f), white(0.9f, 0.9f, 0.9f, 1.0f);

    if (BoxCollider* box = m_Scene->tryGet<BoxCollider>(e))
        m_Gizmos.box(t.world, AABB(box->center - box->size * 0.5f, box->center + box->size * 0.5f), green);
    if (CharacterController* cc = m_Scene->tryGet<CharacterController>(e)) {
        glm::vec3 h(cc->radius, cc->height * 0.5f, cc->radius);
        m_Gizmos.box(AABB(pos - h, pos + h), green);
    }
    if (Light* l = m_Scene->tryGet<Light>(e)) {
        glm::vec3 fwd = t.forward();
        if (l->type == LightType::Directional) {
            glm::vec3 r = t.right(), u = t.up();
            for (int i = 0; i < 5; ++i) {
                glm::vec3 o = pos + (i == 0 ? glm::vec3(0) : (i == 1 ? r : (i == 2 ? -r : (i == 3 ? u : -u)))) * 0.4f;
                m_Gizmos.arrow(o, o + fwd * 2.0f, yellow);
            }
            m_Gizmos.circle(pos, fwd, 0.4f, yellow);
        } else if (l->type == LightType::Point) {
            m_Gizmos.sphere(pos, l->range, yellow);
        } else {
            m_Gizmos.cone(pos, fwd, l->range, l->spotAngle, yellow);
        }
    }
    if (Camera* c = m_Scene->tryGet<Camera>(e)) {
        CameraData cd = CameraData::fromEntity(*m_Scene, e, 16.0f / 9.0f);
        Camera clipped = *c;
        clipped.farClip = std::min(c->farClip, 25.0f);
        m_Gizmos.frustum(glm::inverse(clipped.projection(16.0f / 9.0f) * cd.view), white);
    }
    if (ParticleEmitter* em = m_Scene->tryGet<ParticleEmitter>(e)) {
        glm::vec3 dir = glm::mat3(t.world) * em->direction;
        if (glm::length(dir) > 1e-4f) m_Gizmos.cone(pos, glm::normalize(dir), 1.0f, 10.0f + em->spread * 160.0f, glm::vec4(0.6f, 0.8f, 1.0f, 1.0f));
    }
}

void EditorLayer::drawSceneIcons(const glm::mat4& viewProj, const glm::vec2& origin, const glm::vec2& size, bool& clickedIcon) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(ImVec2(origin.x, origin.y), ImVec2(origin.x + size.x, origin.y + size.y), true);
    ImVec2 mouse = ImGui::GetMousePos();
    auto drawFor = [&](Entity e, Icon icon, ImU32 color) {
        if (!m_Scene->isActive(e)) return;
        ImVec2 p;
        if (!toScreen(viewProj, m_Scene->get<Transform>(e).worldPosition(), origin, size, p)) return;
        bool selected = e == m_Selected;
        dl->AddCircleFilled(p, 13.0f, IM_COL32(30, 30, 30, selected ? 220 : 150), 20);
        if (selected) dl->AddCircle(p, 13.0f, IM_COL32(255, 160, 40, 255), 20, 2.0f);
        drawIcon(dl, icon, p, 16.0f, color);
        float dx = mouse.x - p.x, dy = mouse.y - p.y;
        if (m_SceneHovered && dx * dx + dy * dy < 13.0f * 13.0f && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver()) {
            select(e);
            clickedIcon = true;
        }
    };
    m_Scene->each<Light>([&](Entity e, Light& l) {
        ImU32 c = ImGui::GetColorU32(ImVec4(std::min(l.color.r * 1.2f, 1.0f), std::min(l.color.g * 1.2f, 1.0f), std::min(l.color.b * 1.2f, 1.0f), 1.0f));
        drawFor(e, l.type == LightType::Directional ? Icon::Sun : (l.type == LightType::Spot ? Icon::Spot : Icon::Light), c);
    });
    m_Scene->each<Camera>([&](Entity e, Camera&) { drawFor(e, Icon::Camera, IM_COL32(220, 220, 220, 255)); });
    dl->PopClipRect();
}

Entity EditorLayer::pickEntity(const Ray& ray) {
    Entity best = NullEntity;
    float bestT = FLT_MAX;
    m_Scene->each<MeshRenderer, Transform>([&](Entity e, MeshRenderer& mr, Transform& t) {
        if (!mr.model || !mr.visible || !m_Scene->isActive(e)) return;
        AABB worldBounds = mr.model->bounds.transformed(t.world);
        float tb;
        if (!Math::rayAABB(ray, worldBounds, tb, bestT)) return;
        glm::mat4 inv = glm::inverse(t.world);
        Ray local{glm::vec3(inv * glm::vec4(ray.origin, 1.0f)), glm::vec3(inv * glm::vec4(ray.direction, 0.0f))};
        float hit;
        if (mr.model->raycast(local, hit) && hit < bestT) {
            bestT = hit;
            best = e;
        }
    });
    return best;
}

void EditorLayer::drawTransformGizmo(const glm::mat4& view, const glm::mat4& proj, const glm::vec2& origin, const glm::vec2& size) {
    bool usable = m_Scene->valid(m_Selected) && m_Tool != Tool::Hand && !m_CameraBusy;
    ImGuizmo::Enable(usable);
    if (!usable) {
        if (m_GizmoWasUsing) endEdit();
        m_GizmoWasUsing = false;
        return;
    }
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(origin.x, origin.y, size.x, size.y);

    Transform& t = m_Scene->get<Transform>(m_Selected);
    glm::mat4 parent = m_Scene->parentWorld(m_Selected);
    glm::mat4 world = parent * t.localMatrix();

    ImGuizmo::OPERATION op = m_Tool == Tool::Move ? ImGuizmo::TRANSLATE : (m_Tool == Tool::Rotate ? ImGuizmo::ROTATE : ImGuizmo::SCALE);
    ImGuizmo::MODE mode = (m_LocalSpace || m_Tool == Tool::Scale) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
    bool snap = m_Snap || ImGui::GetIO().KeyCtrl;
    float snapValue = m_Tool == Tool::Move ? m_SnapMove : (m_Tool == Tool::Rotate ? m_SnapRotate : m_SnapScale);
    float snapValues[3] = {snapValue, snapValue, snapValue};

    bool changed = ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), op, mode, glm::value_ptr(world), nullptr,
                                        snap ? snapValues : nullptr);
    bool using_ = ImGuizmo::IsUsing();
    if (using_ && !m_GizmoWasUsing) beginEdit();
    if (!using_ && m_GizmoWasUsing) endEdit();
    m_GizmoWasUsing = using_;

    if (changed) {
        glm::mat4 local = glm::inverse(parent) * world;
        glm::vec3 pos, rot, scl;
        if (Math::decompose(local, pos, rot, scl)) {
            if (op == ImGuizmo::TRANSLATE) t.position = pos;
            else if (op == ImGuizmo::ROTATE) t.rotation = rot;
            else t.scale = scl;
            m_Scene->updateTransforms();
        }
    }
}

// ---------------------------------------------------------------------------
// Game view
// ---------------------------------------------------------------------------
void EditorLayer::drawGameView() {
    if (m_FocusGameTab) {
        ImGui::SetNextWindowFocus();
        m_FocusGameTab = false;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool visible = ImGui::Begin("Game", &m_ShowGameView, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    if (!visible) {
        if (m_GameCaptured) {
            m_GameCaptured = false;
            Input::setCursorLocked(false);
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        }
        ImGui::End();
        return;
    }

    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + 6, ImGui::GetCursorPosY() + 4));
    ImGui::TextDisabled("Display 1");
    ImGui::SameLine();
    ImGui::TextDisabled("|  Free Aspect");
    ImGui::SameLine();
    viewToolbarToggle("Stats", m_ShowStats);
    if (m_State != PlayState::Edit) {
        ImGui::SameLine();
        ImGui::TextDisabled(m_GameCaptured ? "|  Esc releases the mouse" : "|  Click to capture the mouse");
    }
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    glm::vec2 size(std::max(avail.x, 8.0f), std::max(avail.y, 8.0f));
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    Entity camEntity = m_Scene->primaryCamera();
    if (m_Scene->valid(camEntity)) {
        CameraData cam = CameraData::fromEntity(*m_Scene, camEntity, size.x / size.y);
        RenderOptions options;
        m_GameRenderer->render(*m_Scene, cam, (int)size.x, (int)size.y, options);
        ImGui::Image((ImTextureID)(uintptr_t)m_GameRenderer->outputTexture(), ImVec2(size.x, size.y), ImVec2(0, 1), ImVec2(1, 0));
    } else {
        ImGui::InvisibleButton("##nocam", ImVec2(size.x, size.y));
        dl->AddRectFilled(cursor, ImVec2(cursor.x + size.x, cursor.y + size.y), IM_COL32(0, 0, 0, 255));
        const char* msg = "Display 1\nNo cameras rendering";
        ImVec2 ts = ImGui::CalcTextSize(msg);
        dl->AddText(ImVec2(cursor.x + (size.x - ts.x) * 0.5f, cursor.y + (size.y - ts.y) * 0.5f), IM_COL32(200, 200, 200, 255), msg);
    }
    bool hovered = ImGui::IsItemHovered();

    // Gameplay HUD
    if (m_State != PlayState::Edit) {
        dl->PushClipRect(cursor, ImVec2(cursor.x + size.x, cursor.y + size.y), true);
        Viewport vp;
        vp.position = glm::vec2(cursor.x, cursor.y);
        vp.size = size;
        vp.drawList = dl;
        m_Game.onGUI(*m_Scene, vp);
        dl->PopClipRect();
    }

    // Mouse capture for gameplay
    ImGuiIO& io = ImGui::GetIO();
    if (m_State == PlayState::Playing && !m_GameCaptured && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        m_GameCaptured = true;
        Input::setCursorLocked(true);
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    }
    if (m_GameCaptured && (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || m_State != PlayState::Playing)) {
        m_GameCaptured = false;
        Input::setCursorLocked(false);
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    }

    if (m_ShowStats) {
        const RenderStats& s = m_GameRenderer->stats();
        ImVec2 p(cursor.x + size.x - 250.0f, cursor.y + 8.0f);
        dl->AddRectFilled(p, ImVec2(p.x + 240.0f, p.y + 112.0f), IM_COL32(20, 20, 20, 200), 4.0f);
        char buf[256];
        std::snprintf(buf, sizeof(buf),
                      "Statistics\nFPS: %.0f (%.2f ms)\nDraw calls: %d\nTriangles: %d\nShadow casters: %d\nLights: %d  Particles: %d",
                      Time::fps(), Time::fps() > 0 ? 1000.0f / Time::fps() : 0.0f, s.drawCalls, s.triangles, s.shadowCasters, s.lights,
                      s.particles);
        dl->AddText(ImVec2(p.x + 8, p.y + 6), IM_COL32(230, 230, 230, 255), buf);
    }
    ImGui::End();
}

} // namespace Forge::Editor
