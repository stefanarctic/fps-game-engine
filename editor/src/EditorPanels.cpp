// Hierarchy, Inspector, Project, Console and Lighting panels.
#include "Forge/Editor/EditorLayer.h"
#include "Forge/Assets/AssetManager.h"
#include "Forge/Core/FileSystem.h"
#include "Forge/Core/Log.h"
#include "Forge/Editor/EditorTheme.h"
#include "Forge/Editor/Icons.h"
#include "Forge/Scene/SceneSerializer.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cctype>
#include <cstring>

namespace Forge::Editor {

namespace {
bool containsNoCase(const std::string& text, const char* filter) {
    if (!filter || !*filter) return true;
    std::string a = text, b = filter;
    std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return a.find(b) != std::string::npos;
}

Icon iconForEntity(Scene& scene, Entity e) {
    if (Light* l = scene.tryGet<Light>(e)) return l->type == LightType::Directional ? Icon::Sun : (l->type == LightType::Spot ? Icon::Spot : Icon::Light);
    if (scene.has<Camera>(e)) return Icon::Camera;
    if (scene.has<MeshRenderer>(e)) return Icon::Model;
    return Icon::Entity;
}

Icon iconForFile(const std::string& path, bool isDir) {
    if (isDir) return Icon::Folder;
    std::string ext = FileSystem::extension(path);
    if (ext == ".obj" || ext == ".gltf" || ext == ".glb") return Icon::Model;
    if (ext == ".mat") return Icon::Material;
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp") return Icon::Texture;
    if (ext == ".scene") return Icon::Scene;
    if (ext == ".glsl") return Icon::Shader;
    return Icon::File;
}

bool isImage(const std::string& path) { return iconForFile(path, false) == Icon::Texture; }

void setAssetPayload(const std::string& path) {
    ImGui::SetDragDropPayload("ASSET_PATH", path.c_str(), path.size() + 1);
}
} // namespace

// ---------------------------------------------------------------------------
// Hierarchy
// ---------------------------------------------------------------------------
void EditorLayer::drawHierarchy() {
    if (!ImGui::Begin("Hierarchy", &m_ShowHierarchy)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("+", ImVec2(28, 0))) ImGui::OpenPopup("##createmenu");
    if (ImGui::BeginPopup("##createmenu")) {
        drawCreateMenuItems(NullEntity);
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##search", "Search...", m_HierarchyFilter, sizeof(m_HierarchyFilter));

    ImGui::BeginChild("##tree", ImVec2(0, 0), ImGuiChildFlags_None);

    // Scene header row (like Unity's scene root)
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        float h = ImGui::GetFrameHeight();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(48, 48, 48, 255));
        drawIcon(dl, Icon::Scene, ImVec2(p.x + 12, p.y + h * 0.5f), 14.0f, IM_COL32(220, 220, 220, 255));
        ImGui::SetCursorScreenPos(ImVec2(p.x + 24, p.y));
        ImGui::AlignTextToFramePadding();
        ImGui::PushFont(Application::get().fonts().bold);
        ImGui::Text("%s%s", m_Scene->name.c_str(), m_Dirty ? "*" : "");
        ImGui::PopFont();
    }

    if (m_HierarchyFilter[0]) {
        std::vector<Entity> matches;
        m_Scene->registry().each<EntityInfo>([&](Entity e, EntityInfo& info) {
            if (containsNoCase(info.name, m_HierarchyFilter)) matches.push_back(e);
        });
        for (Entity e : matches) {
            ImGui::PushID((int)e);
            if (ImGui::Selectable(m_Scene->nameOf(e).c_str(), e == m_Selected)) select(e);
            ImGui::PopID();
        }
    } else {
        std::vector<Entity> roots = m_Scene->roots();
        for (Entity e : roots) drawHierarchyNode(e);
    }
    m_PendingScroll = NullEntity;

    // Empty area: drop to un-parent, click to deselect, right-click to create.
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton("##empty", ImVec2(std::max(avail.x, 1.0f), std::max(avail.y, 40.0f)));
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) m_Selected = NullEntity;
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY")) {
            uint64_t uuid;
            std::memcpy(&uuid, payload->Data, sizeof(uuid));
            Entity e = m_Scene->findByUUID(uuid);
            if (m_Scene->valid(e) && m_Scene->valid(m_Scene->parentOf(e))) {
                recordUndo();
                m_Scene->setParent(e, NullEntity, true);
            }
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string path((const char*)payload->Data);
            if (assetMatchesKind(path, AssetKind::Model)) instantiateModel(path);
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::BeginPopupContextItem("##emptyctx", ImGuiPopupFlags_MouseButtonRight)) {
        drawCreateMenuItems(NullEntity);
        ImGui::EndPopup();
    }
    ImGui::EndChild();
    ImGui::End();
}

void EditorLayer::drawHierarchyNode(Entity e) {
    if (!m_Scene->valid(e)) return;
    EntityInfo& info = m_Scene->get<EntityInfo>(e);
    std::vector<Entity> children = m_Scene->get<Transform>(e).children;
    uint64_t uuid = info.uuid;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
    if (children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    if (e == m_Selected) flags |= ImGuiTreeNodeFlags_Selected;
    if (m_Scene->valid(m_PendingScroll) && m_Scene->isAncestor(e, m_PendingScroll)) ImGui::SetNextItemOpen(true);

    if (!info.activeInHierarchy) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
    std::string label = "        " + info.name;
    bool open = ImGui::TreeNodeEx((void*)(uintptr_t)uuid, flags, "%s", label.c_str());
    ImGui::PopStyleVar();
    if (!info.activeInHierarchy) ImGui::PopStyleColor();

    ImVec2 rmin = ImGui::GetItemRectMin(), rmax = ImGui::GetItemRectMax();
    float iconX = rmin.x + ImGui::GetTreeNodeToLabelSpacing() + 6.0f;
    drawIcon(ImGui::GetWindowDrawList(), iconForEntity(*m_Scene, e), ImVec2(iconX, (rmin.y + rmax.y) * 0.5f), 13.0f,
             info.activeInHierarchy ? Theme::Icon : Theme::IconDim);

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) select(e);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && ImGui::IsMouseDoubleClicked(0)) focusSelected();
    if (m_PendingScroll == e) ImGui::SetScrollHereY(0.5f);

    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("ENTITY", &uuid, sizeof(uuid));
        ImGui::Text("%s", info.name.c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY")) {
            uint64_t dropped;
            std::memcpy(&dropped, payload->Data, sizeof(dropped));
            Entity child = m_Scene->findByUUID(dropped);
            if (m_Scene->valid(child) && child != e && !m_Scene->isAncestor(child, e)) {
                recordUndo();
                m_Scene->setParent(child, e, true);
            }
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string path((const char*)payload->Data);
            if (assetMatchesKind(path, AssetKind::Model)) {
                Entity m = instantiateModel(path);
                if (m_Scene->valid(m)) {
                    m_Scene->setParent(m, e, false);
                    m_Scene->get<Transform>(m).position = glm::vec3(0.0f);
                }
            } else if (assetMatchesKind(path, AssetKind::Material) && m_Scene->has<MeshRenderer>(e)) {
                recordUndo();
                m_Scene->get<MeshRenderer>(e).material = path;
            }
        }
        ImGui::EndDragDropTarget();
    }

    bool destroyed = false;
    ImGui::PushID((int)uuid);
    if (ImGui::BeginPopupContextItem("##ctx")) {
        select(e);
        if (ImGui::MenuItem("Create Empty Child")) createEntity("GameObject", e);
        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) duplicateSelected();
        if (ImGui::MenuItem("Copy", "Ctrl+C")) copySelected();
        if (ImGui::MenuItem("Delete", "Del")) {
            deleteSelected();
            destroyed = true;
        }
        if (ImGui::MenuItem(info.active ? "Deactivate" : "Activate")) {
            recordUndo();
            m_Scene->get<EntityInfo>(e).active = !m_Scene->get<EntityInfo>(e).active;
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Create")) {
            drawCreateMenuItems(e);
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();

    if (open && !(flags & ImGuiTreeNodeFlags_Leaf)) {
        if (!destroyed)
            for (Entity c : children) drawHierarchyNode(c);
        ImGui::TreePop();
    } else if (open) {
        ImGui::TreePop();
    }
}

// ---------------------------------------------------------------------------
// Inspector
// ---------------------------------------------------------------------------
void EditorLayer::drawInspector() {
    if (!ImGui::Begin("Inspector", &m_ShowInspector)) {
        ImGui::End();
        return;
    }
    if (m_Scene->valid(m_Selected)) drawEntityInspector(m_Selected);
    else if (!m_SelectedAsset.empty()) drawAssetInspector(m_SelectedAsset);
    else ImGui::TextDisabled("Select an entity in the Hierarchy or an asset in the Project window.");
    ImGui::End();
}

void EditorLayer::drawEntityInspector(Entity e) {
    PropertyContext ctx = propertyContext();
    EntityInfo& info = m_Scene->get<EntityInfo>(e);

    // Header: icon, active toggle, name, tag
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(28, 28));
        drawIcon(ImGui::GetWindowDrawList(), iconForEntity(*m_Scene, e), ImVec2(p.x + 14, p.y + 14), 24.0f, Theme::Icon);
        ImGui::SameLine();
        ImGui::BeginGroup();
        bool active = info.active;
        if (ImGui::Checkbox("##active", &active)) {
            recordUndo();
            m_Scene->get<EntityInfo>(e).active = active;
        }
        ImGui::SameLine();
        char name[128];
        std::snprintf(name, sizeof(name), "%s", info.name.c_str());
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText("##name", name, sizeof(name))) m_Scene->get<EntityInfo>(e).name = name;
        if (ImGui::IsItemActivated()) beginEdit();
        if (ImGui::IsItemDeactivatedAfterEdit()) endEdit();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Tag");
        ImGui::SameLine();
        char tag[64];
        std::snprintf(tag, sizeof(tag), "%s", info.tag.c_str());
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
        if (ImGui::InputText("##tag", tag, sizeof(tag))) m_Scene->get<EntityInfo>(e).tag = tag;
        if (ImGui::IsItemActivated()) beginEdit();
        if (ImGui::IsItemDeactivatedAfterEdit()) endEdit();
        ImGui::SameLine();
        ImGui::TextDisabled("ID %016llx", (unsigned long long)info.uuid);
        ImGui::EndGroup();
    }
    ImGui::Spacing();

    Registry& reg = m_Scene->registry();
    for (const auto& meta : ComponentRegistry::instance().components()) {
        if (meta->hidden) continue;
        void* data = meta->get(reg, e);
        if (!data) continue;
        ImGui::PushID(meta->name.c_str());

        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(62, 62, 62, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(72, 72, 72, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(72, 72, 72, 255));
        ImGui::PushFont(Application::get().fonts().bold);
        bool open = ImGui::CollapsingHeader(meta->name.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::PopFont();
        ImGui::PopStyleColor(3);

        bool removed = false;
        if (ImGui::BeginPopupContextItem("##compctx")) {
            if (ImGui::MenuItem("Reset")) {
                recordUndo();
                meta->reset(data);
            }
            if (ImGui::MenuItem("Remove Component", nullptr, false, meta->removable)) {
                recordUndo();
                meta->remove(reg, e);
                removed = true;
            }
            ImGui::EndPopup();
        }
        // "..." button on the header (like Unity's component menu)
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 24.0f);
        if (ImGui::SmallButton("...")) ImGui::OpenPopup("##compctx");

        if (open && !removed) {
            if (PropertyDrawer::begin("##props")) {
                PropertyDrawer::drawFields(*meta, data, ctx);
                PropertyDrawer::end();
            }
            // Component specific helpers
            if (meta->name == "Box Collider") {
                MeshRenderer* mr = m_Scene->tryGet<MeshRenderer>(e);
                if (mr && mr->model && ImGui::Button("Fit To Mesh", ImVec2(-FLT_MIN, 0))) {
                    recordUndo();
                    BoxCollider& box = m_Scene->get<BoxCollider>(e);
                    box.center = mr->model->bounds.center();
                    box.size = glm::max(mr->model->bounds.size(), glm::vec3(0.01f));
                }
            } else if (meta->name == "Mesh Renderer") {
                MeshRenderer& mr = m_Scene->get<MeshRenderer>(e);
                if (mr.model) {
                    size_t tris = 0;
                    for (const SubMesh& sm : mr.model->submeshes) tris += sm.mesh->indexCount() / 3;
                    ImGui::TextDisabled("%zu sub-mesh(es), %zu triangles, %zu material(s)", mr.model->submeshes.size(), tris,
                                        mr.model->materials.size());
                } else if (!mr.mesh.empty()) {
                    ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "Mesh failed to load");
                }
            } else if (meta->name == "Particle System" && m_State != PlayState::Edit) {
                ParticleEmitter& em = m_Scene->get<ParticleEmitter>(e);
                ImGui::TextDisabled("%zu live particles", em.particles.size());
                if (ImGui::Button("Restart")) {
                    em.particles.clear();
                    em.burstDone = false;
                    em.emitting = true;
                }
            }
            ImGui::Spacing();
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    ImGui::Spacing();
    float bw = 220.0f;
    ImGui::SetCursorPosX(std::max((ImGui::GetContentRegionAvail().x - bw) * 0.5f, 0.0f));
    if (ImGui::Button("Add Component", ImVec2(bw, 26))) ImGui::OpenPopup("##addcomp");
    if (ImGui::BeginPopup("##addcomp")) {
        static char filter[64] = "";
        if (ImGui::IsWindowAppearing()) {
            filter[0] = 0;
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth(bw);
        ImGui::InputTextWithHint("##filter", "Search...", filter, sizeof(filter));
        std::string category;
        for (const auto& meta : ComponentRegistry::instance().components()) {
            if (meta->hidden || !meta->removable || meta->has(reg, e)) continue;
            if (!containsNoCase(meta->name, filter)) continue;
            if (meta->category != category) {
                category = meta->category;
                ImGui::SeparatorText(category.c_str());
            }
            if (ImGui::Selectable(meta->name.c_str())) {
                recordUndo();
                meta->add(reg, e);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
}

void EditorLayer::drawAssetInspector(const std::string& path) {
    std::string ext = FileSystem::extension(path);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(28, 28));
    drawIcon(ImGui::GetWindowDrawList(), iconForFile(path, false), ImVec2(p.x + 14, p.y + 14), 24.0f, Theme::Icon);
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::PushFont(Application::get().fonts().bold);
    ImGui::Text("%s", FileSystem::filename(path).c_str());
    ImGui::PopFont();
    ImGui::TextDisabled("%s", path.c_str());
    ImGui::EndGroup();
    ImGui::Separator();

    if (ext == ".mat") {
        Ref<Material> mat = AssetManager::getMaterial(path);
        if (!mat) {
            ImGui::TextColored(ImVec4(1, 0.4f, 0.3f, 1), "Material failed to load");
            return;
        }
        PropertyContext ctx;
        ctx.scene = m_Scene.get();
        static bool dirty = false;
        if (PropertyDrawer::begin("##matprops")) {
            if (PropertyDrawer::drawFields(Material::meta(), mat.get(), ctx)) dirty = true;
            PropertyDrawer::end();
        }
        // Auto-save once the user releases the widget.
        if (dirty && !ImGui::IsAnyItemActive()) {
            AssetManager::saveMaterial(mat, path);
            dirty = false;
        }
        ImGui::Spacing();
        ImVec4 c(mat->albedo.r, mat->albedo.g, mat->albedo.b, 1.0f);
        ImGui::ColorButton("##preview", c, ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetContentRegionAvail().x, 24));
        ImGui::TextDisabled("Changes are saved automatically. Drag the material onto an object to assign it.");
    } else if (ext == ".obj" || ext == ".gltf" || ext == ".glb") {
        Ref<Model> model = AssetManager::getModel(path);
        if (!model) {
            ImGui::TextColored(ImVec4(1, 0.4f, 0.3f, 1), "Model failed to load (see Console)");
            return;
        }
        size_t verts = 0, tris = 0;
        for (const SubMesh& sm : model->submeshes) {
            verts += sm.mesh->vertexCount();
            tris += sm.mesh->indexCount() / 3;
        }
        glm::vec3 size = model->bounds.size();
        ImGui::Text("Format:     %s", ext == ".obj" ? "Wavefront OBJ" : (ext == ".glb" ? "glTF 2.0 binary" : "glTF 2.0"));
        ImGui::Text("Sub-meshes: %zu", model->submeshes.size());
        ImGui::Text("Vertices:   %zu", verts);
        ImGui::Text("Triangles:  %zu", tris);
        ImGui::Text("Bounds:     %.2f x %.2f x %.2f m", size.x, size.y, size.z);
        ImGui::SeparatorText("Materials");
        for (const auto& m : model->materials) {
            ImGui::ColorButton(("##" + m->name).c_str(), ImVec4(m->albedo.r, m->albedo.g, m->albedo.b, 1), 0, ImVec2(14, 14));
            ImGui::SameLine();
            ImGui::Text("%s%s", m->name.c_str(), m->albedoTexture ? "  (textured)" : "");
        }
        ImGui::Spacing();
        if (ImGui::Button("Add To Scene", ImVec2(-FLT_MIN, 0))) instantiateModel(path);
    } else if (isImage(path)) {
        Ref<Texture2D> tex = AssetManager::getTexture(path, true);
        if (tex) {
            ImGui::Text("%d x %d", tex->width(), tex->height());
            float w = std::min(ImGui::GetContentRegionAvail().x, 512.0f);
            float h = w * (float)tex->height() / (float)std::max(tex->width(), 1);
            ImGui::Image((ImTextureID)(uintptr_t)tex->id(), ImVec2(w, h), ImVec2(0, 1), ImVec2(1, 0));
        }
    } else if (ext == ".scene") {
        if (ImGui::Button("Open Scene", ImVec2(-FLT_MIN, 0))) openScene(path);
    } else if (ext == ".glsl") {
        ImGui::TextWrapped("GLSL shader. Edit the file in any text editor; it is recompiled automatically when saved.");
        if (ImGui::Button("Reload All Shaders", ImVec2(-FLT_MIN, 0))) AssetManager::reloadShaders(true);
    }
}

// ---------------------------------------------------------------------------
// Project
// ---------------------------------------------------------------------------
void EditorLayer::drawProject() {
    if (!ImGui::Begin("Project", &m_ShowProject)) {
        ImGui::End();
        return;
    }

    // Breadcrumb
    if (ImGui::Button("Assets")) m_ProjectDir.clear();
    std::string accumulated;
    size_t start = 0;
    while (start < m_ProjectDir.size()) {
        size_t slash = m_ProjectDir.find('/', start);
        std::string part = m_ProjectDir.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        accumulated = accumulated.empty() ? part : accumulated + "/" + part;
        ImGui::SameLine(0, 2);
        ImGui::TextDisabled(">");
        ImGui::SameLine(0, 2);
        if (ImGui::Button(part.c_str())) m_ProjectDir = accumulated;
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    ImGui::SameLine();
    float right = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(right - 340.0f, 0.0f));
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##psearch", "Search...", m_ProjectFilter, sizeof(m_ProjectFilter));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::SliderFloat("##iconsize", &m_ProjectIconSize, 40.0f, 120.0f, "");
    ImGui::Separator();

    if (ImGui::BeginTable("##projtable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("tree", ImGuiTableColumnFlags_WidthFixed, 190.0f);
        ImGui::TableSetupColumn("items", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        // Folder tree
        ImGui::TableSetColumnIndex(0);
        ImGui::BeginChild("##folders");
        std::function<void(const std::string&, const std::string&)> folder = [&](const std::string& dir, const std::string& name) {
            std::vector<FileSystem::DirEntry> entries = FileSystem::listAssets(dir);
            bool hasSub = std::any_of(entries.begin(), entries.end(), [](const FileSystem::DirEntry& d) { return d.isDirectory; });
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (!hasSub) flags |= ImGuiTreeNodeFlags_Leaf;
            if (dir == m_ProjectDir) flags |= ImGuiTreeNodeFlags_Selected;
            if (dir.empty()) flags |= ImGuiTreeNodeFlags_DefaultOpen;
            bool open = ImGui::TreeNodeEx(dir.empty() ? "##root" : dir.c_str(), flags, "         %s", name.c_str());
            ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
            drawIcon(ImGui::GetWindowDrawList(), Icon::Folder, ImVec2(mn.x + ImGui::GetTreeNodeToLabelSpacing() + 8, (mn.y + mx.y) * 0.5f), 14.0f,
                     IM_COL32(190, 160, 90, 255));
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) m_ProjectDir = dir;
            if (open) {
                for (const auto& d : entries)
                    if (d.isDirectory) folder(d.path, d.name);
                ImGui::TreePop();
            }
        };
        folder("", "Assets");
        ImGui::EndChild();

        // Items grid
        ImGui::TableSetColumnIndex(1);
        ImGui::BeginChild("##items");
        std::vector<FileSystem::DirEntry> entries;
        if (m_ProjectFilter[0]) {
            // Search the whole project
            for (const std::string& f : FileSystem::findAssets({".obj", ".gltf", ".glb", ".mat", ".png", ".jpg", ".tga", ".bmp", ".scene", ".glsl", ".bin", ".mtl"})) {
                if (containsNoCase(FileSystem::filename(f), m_ProjectFilter)) entries.push_back({FileSystem::filename(f), f, false});
            }
        } else {
            entries = FileSystem::listAssets(m_ProjectDir);
        }

        float cell = m_ProjectIconSize + 18.0f;
        int columns = std::max(1, (int)(ImGui::GetContentRegionAvail().x / cell));
        int col = 0;
        std::string navigateTo;
        for (const auto& entry : entries) {
            ImGui::PushID(entry.path.c_str());
            if (col > 0) ImGui::SameLine();
            ImGui::BeginGroup();
            ImVec2 pos = ImGui::GetCursorScreenPos();
            float labelH = ImGui::GetTextLineHeight() * 2.0f + 4.0f;
            ImGui::InvisibleButton("##item", ImVec2(cell - 6.0f, m_ProjectIconSize + labelH));
            bool hovered = ImGui::IsItemHovered();
            bool selected = !entry.isDirectory && entry.path == m_SelectedAsset && !m_Scene->valid(m_Selected);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (selected) dl->AddRectFilled(pos, ImVec2(pos.x + cell - 6.0f, pos.y + m_ProjectIconSize + labelH), Theme::Accent, 4.0f);
            else if (hovered) dl->AddRectFilled(pos, ImVec2(pos.x + cell - 6.0f, pos.y + m_ProjectIconSize + labelH), IM_COL32(70, 70, 70, 255), 4.0f);

            ImVec2 iconCenter(pos.x + (cell - 6.0f) * 0.5f, pos.y + m_ProjectIconSize * 0.5f + 2.0f);
            Ref<Texture2D> thumb = (!entry.isDirectory && isImage(entry.path)) ? AssetManager::getTexture(entry.path, true) : nullptr;
            if (thumb) {
                float s = m_ProjectIconSize * 0.42f;
                dl->AddImage((ImTextureID)(uintptr_t)thumb->id(), ImVec2(iconCenter.x - s, iconCenter.y - s), ImVec2(iconCenter.x + s, iconCenter.y + s),
                             ImVec2(0, 1), ImVec2(1, 0));
            } else {
                Icon icon = iconForFile(entry.path, entry.isDirectory);
                ImU32 color = entry.isDirectory ? IM_COL32(190, 160, 90, 255) : Theme::Icon;
                if (icon == Icon::Material) {
                    if (Ref<Material> m = AssetManager::getMaterial(entry.path)) color = ImGui::GetColorU32(ImVec4(m->albedo.r, m->albedo.g, m->albedo.b, 1));
                }
                drawIcon(dl, icon, iconCenter, m_ProjectIconSize * 0.72f, color);
            }
            // Label (two lines max, clipped)
            std::string name = entry.isDirectory ? entry.name : FileSystem::stem(entry.name) + FileSystem::extension(entry.name);
            ImVec4 clip(pos.x, pos.y, pos.x + cell - 6.0f, pos.y + m_ProjectIconSize + labelH);
            ImVec2 textSize = ImGui::CalcTextSize(name.c_str(), nullptr, false, cell - 10.0f);
            dl->AddText(nullptr, 0.0f, ImVec2(pos.x + std::max((cell - 6.0f - textSize.x) * 0.5f, 2.0f), pos.y + m_ProjectIconSize + 4.0f),
                        ImGui::GetColorU32(ImGuiCol_Text), name.c_str(), nullptr, cell - 10.0f, &clip);

            if (ImGui::IsItemClicked() && !entry.isDirectory) {
                m_SelectedAsset = entry.path;
                m_Selected = NullEntity;
            }
            if (hovered && ImGui::IsMouseDoubleClicked(0)) {
                std::string ext = FileSystem::extension(entry.path);
                if (entry.isDirectory) navigateTo = entry.path;
                else if (ext == ".scene") openScene(entry.path);
                else if (assetMatchesKind(entry.path, AssetKind::Model)) instantiateModel(entry.path);
            }
            if (!entry.isDirectory && ImGui::BeginDragDropSource()) {
                setAssetPayload(entry.path);
                ImGui::Text("%s", entry.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (hovered && !entry.isDirectory) ImGui::SetTooltip("%s", entry.path.c_str());
            ImGui::EndGroup();
            ImGui::PopID();
            col = (col + 1) % columns;
        }
        if (!navigateTo.empty()) m_ProjectDir = navigateTo;
        if (entries.empty()) ImGui::TextDisabled("This folder is empty.");

        if (ImGui::BeginPopupContextWindow("##projctx", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Create Material")) {
                std::string dir = m_ProjectDir;
                std::string path;
                for (int i = 0; i < 1000; ++i) {
                    path = FileSystem::join(dir, i == 0 ? "New Material.mat" : "New Material " + std::to_string(i) + ".mat");
                    if (!FileSystem::exists(FileSystem::resolve(path))) break;
                }
                if (AssetManager::createMaterial(path)) {
                    m_SelectedAsset = path;
                    m_Selected = NullEntity;
                }
            }
            if (ImGui::MenuItem("Create Folder")) {
                std::string path;
                for (int i = 0; i < 1000; ++i) {
                    path = FileSystem::join(m_ProjectDir, i == 0 ? "New Folder" : "New Folder " + std::to_string(i));
                    if (!FileSystem::exists(FileSystem::resolve(path))) break;
                }
                FileSystem::createDirectories(FileSystem::resolve(path));
            }
            ImGui::EndPopup();
        }
        ImGui::EndChild();
        ImGui::EndTable();
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Console
// ---------------------------------------------------------------------------
void EditorLayer::drawConsole() {
    if (!ImGui::Begin("Console", &m_ShowConsole)) {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Clear")) {
        Log::clear();
        m_ConsoleSelected = -1;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_ConsoleAutoScroll);
    ImGui::SameLine();
    float right = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(right - 420.0f, 0.0f));
    ImGui::SetNextItemWidth(180);
    ImGui::InputTextWithHint("##cfilter", "Filter...", m_ConsoleFilter, sizeof(m_ConsoleFilter));

    auto toggle = [&](const char* id, Icon icon, bool& value, size_t count) {
        ImGui::SameLine();
        ImVec2 p = ImGui::GetCursorScreenPos();
        char label[32];
        std::snprintf(label, sizeof(label), "     %zu%s", count, id);
        ImGui::PushStyleColor(ImGuiCol_Button, value ? ImGui::GetColorU32(ImGuiCol_Header) : ImGui::GetColorU32(ImGuiCol_Button));
        if (ImGui::Button(label, ImVec2(70, 0))) value = !value;
        ImGui::PopStyleColor();
        drawIcon(ImGui::GetWindowDrawList(), icon, ImVec2(p.x + 13, p.y + ImGui::GetFrameHeight() * 0.5f), 14.0f, Theme::Icon);
    };
    toggle("##i", Icon::Info, m_ConsoleInfo, Log::count(LogLevel::Info) + Log::count(LogLevel::Trace));
    toggle("##w", Icon::Warning, m_ConsoleWarn, Log::count(LogLevel::Warning));
    toggle("##e", Icon::Error, m_ConsoleError, Log::count(LogLevel::Error));
    ImGui::Separator();

    std::vector<LogMessage> messages = Log::snapshot();
    float detailH = m_ConsoleSelected >= 0 ? 70.0f : 0.0f;
    ImGui::BeginChild("##log", ImVec2(0, -detailH), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    int index = 0;
    for (const LogMessage& m : messages) {
        int i = index++;
        if ((m.level <= LogLevel::Info && !m_ConsoleInfo) || (m.level == LogLevel::Warning && !m_ConsoleWarn) ||
            (m.level == LogLevel::Error && !m_ConsoleError))
            continue;
        if (!containsNoCase(m.text, m_ConsoleFilter)) continue;
        ImGui::PushID(i);
        ImVec2 p = ImGui::GetCursorScreenPos();
        int secs = (int)m.time;
        char line[2200];
        std::snprintf(line, sizeof(line), "      [%02d:%02d:%02d] %s", secs / 3600, (secs / 60) % 60, secs % 60, m.text.c_str());
        // Show only the first line in the list.
        char* nl = std::strchr(line, '\n');
        if (nl) *nl = 0;
        if (i % 2) ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y + ImGui::GetTextLineHeightWithSpacing()),
                                                                 IM_COL32(255, 255, 255, 6));
        if (m.level == LogLevel::Error) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 130, 120, 255));
        else if (m.level == LogLevel::Warning) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 210, 110, 255));
        else ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_Text));
        if (ImGui::Selectable(line, m_ConsoleSelected == i)) m_ConsoleSelected = i;
        ImGui::PopStyleColor();
        Icon icon = m.level == LogLevel::Error ? Icon::Error : (m.level == LogLevel::Warning ? Icon::Warning : Icon::Info);
        drawIcon(ImGui::GetWindowDrawList(), icon, ImVec2(p.x + 9, p.y + ImGui::GetTextLineHeight() * 0.5f + 1), 13.0f, Theme::Icon);
        ImGui::PopID();
    }
    uint64_t rev = Log::revision();
    if (m_ConsoleAutoScroll && rev != m_ConsoleRevision) ImGui::SetScrollHereY(1.0f);
    m_ConsoleRevision = rev;
    ImGui::EndChild();

    if (m_ConsoleSelected >= 0 && m_ConsoleSelected < (int)messages.size()) {
        ImGui::Separator();
        ImGui::BeginChild("##detail", ImVec2(0, 0));
        ImGui::TextWrapped("%s", messages[(size_t)m_ConsoleSelected].text.c_str());
        ImGui::EndChild();
    } else {
        m_ConsoleSelected = -1;
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Lighting (scene environment / render settings)
// ---------------------------------------------------------------------------
void EditorLayer::drawLighting() {
    if (!ImGui::Begin("Lighting", &m_ShowLighting)) {
        ImGui::End();
        return;
    }
    PropertyContext ctx = propertyContext();
    ImGui::PushFont(Application::get().fonts().bold);
    ImGui::TextUnformatted("Environment & Rendering");
    ImGui::PopFont();
    ImGui::TextDisabled("Per-scene sky, ambient light, fog, SSAO, shadows and post FX.");
    ImGui::Separator();
    if (PropertyDrawer::begin("##env")) {
        PropertyDrawer::drawFields(environmentMeta(), &m_Scene->environment, ctx);
        PropertyDrawer::end();
    }
    ImGui::Spacing();
    if (ImGui::Button("Reset To Defaults", ImVec2(-FLT_MIN, 0))) {
        recordUndo();
        m_Scene->environment = Environment();
    }
    ImGui::Spacing();
    ImGui::SeparatorText("Scene View Debug");
    const char* views[] = {"Shaded", "Ambient Occlusion", "Normals", "Albedo", "Lighting Only"};
    int dv = (int)m_SceneDebugView;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::Combo("##debugview", &dv, views, 5)) m_SceneDebugView = (DebugView)dv;
    ImGui::End();
}

} // namespace Forge::Editor
