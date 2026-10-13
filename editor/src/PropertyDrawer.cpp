#include "Forge/Editor/PropertyDrawer.h"
#include "Forge/Assets/AssetManager.h"
#include "Forge/Core/FileSystem.h"
#include "Forge/Editor/EditorTheme.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cctype>
#include <cstring>

namespace Forge::Editor {

std::vector<std::string> assetExtensions(AssetKind kind) {
    switch (kind) {
    case AssetKind::Model: return {".obj", ".gltf", ".glb"};
    case AssetKind::Material: return {".mat"};
    case AssetKind::Texture: return {".png", ".jpg", ".jpeg", ".tga", ".bmp"};
    case AssetKind::Scene: return {".scene"};
    case AssetKind::Any: break;
    }
    return {};
}

const char* assetKindName(AssetKind kind) {
    switch (kind) {
    case AssetKind::Model: return "Mesh";
    case AssetKind::Material: return "Material";
    case AssetKind::Texture: return "Texture2D";
    case AssetKind::Scene: return "Scene";
    case AssetKind::Any: break;
    }
    return "Asset";
}

bool assetMatchesKind(const std::string& path, AssetKind kind) {
    if (kind == AssetKind::Any) return true;
    if (kind == AssetKind::Model && path.rfind("builtin:", 0) == 0) return true;
    auto exts = assetExtensions(kind);
    return std::find(exts.begin(), exts.end(), FileSystem::extension(path)) != exts.end();
}

namespace PropertyDrawer {

namespace {
void track(PropertyContext& ctx) {
    if (ImGui::IsItemActivated() && ctx.beginEdit) ctx.beginEdit();
    if (ImGui::IsItemDeactivatedAfterEdit() && ctx.endEdit) ctx.endEdit();
}

void labelCell(const char* label, const char* tooltip) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    if (tooltip && *tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", tooltip);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

bool containsNoCase(const std::string& text, const char* filter) {
    if (!filter || !*filter) return true;
    std::string a = text, b = filter;
    std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return a.find(b) != std::string::npos;
}

std::string displayName(const std::string& path) {
    if (path.empty()) return "None";
    if (path.rfind("builtin:", 0) == 0) return path.substr(8);
    return FileSystem::stem(path);
}

// One axis of a Unity-style XYZ vector field.
bool axisDrag(const char* id, const char* axis, ImU32 color, float* value, float width, float speed, PropertyContext& ctx) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ImGui::GetFrameHeight();
    float labelW = ImGui::CalcTextSize(axis).x + 6.0f;
    ImGui::Dummy(ImVec2(labelW, h));
    dl->AddText(ImVec2(p.x + 2.0f, p.y + (h - ImGui::GetFontSize()) * 0.5f), color, axis);
    ImGui::SameLine(0, 0);
    ImGui::SetNextItemWidth(std::max(width - labelW, 10.0f));
    bool changed = ImGui::DragFloat(id, value, speed, 0.0f, 0.0f, "%.3g");
    track(ctx);
    return changed;
}
} // namespace

bool begin(const char* id) {
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX;
    if (!ImGui::BeginTable(id, 2, flags)) return false;
    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch, 0.38f);
    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 0.62f);
    return true;
}

void end() { ImGui::EndTable(); }

bool vec3(const char* label, glm::vec3& value, float speed, PropertyContext& ctx, const char* tooltip) {
    labelCell(label, tooltip);
    ImGui::PushID(label);
    float total = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    float w = (total - spacing * 2.0f) / 3.0f;
    bool changed = false;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, ImGui::GetStyle().ItemSpacing.y));
    changed |= axisDrag("##x", "X", Theme::AxisX, &value.x, w, speed, ctx);
    ImGui::SameLine();
    changed |= axisDrag("##y", "Y", Theme::AxisY, &value.y, w, speed, ctx);
    ImGui::SameLine();
    changed |= axisDrag("##z", "Z", Theme::AxisZ, &value.z, w, speed, ctx);
    ImGui::PopStyleVar();
    ImGui::PopID();
    return changed;
}

bool assetField(const char* label, std::string& path, AssetKind kind, PropertyContext& ctx, const char* tooltip) {
    labelCell(label, tooltip);
    ImGui::PushID(label);
    bool changed = false;
    float pickerW = ImGui::GetFrameHeight();
    float w = ImGui::GetContentRegionAvail().x - pickerW - ImGui::GetStyle().ItemInnerSpacing.x;

    std::string text = displayName(path) + " (" + assetKindName(kind) + ")";
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
    if (ImGui::Button(text.c_str(), ImVec2(w, 0))) ImGui::OpenPopup("##assetpicker");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    if (!path.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", path.c_str());

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string dropped((const char*)payload->Data);
            if (assetMatchesKind(dropped, kind)) {
                if (ctx.record) ctx.record();
                path = dropped;
                changed = true;
            }
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    if (ImGui::Button("o", ImVec2(pickerW, 0))) ImGui::OpenPopup("##assetpicker");

    if (ImGui::BeginPopup("##assetpicker")) {
        static char filter[64] = "";
        if (ImGui::IsWindowAppearing()) {
            filter[0] = 0;
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth(260);
        ImGui::InputTextWithHint("##filter", "Search...", filter, sizeof(filter));
        ImGui::Separator();
        ImGui::BeginChild("##list", ImVec2(260, 280));
        std::vector<std::string> options;
        if (kind == AssetKind::Model)
            for (const auto& b : AssetManager::builtinMeshes()) options.push_back(b);
        for (const auto& f : FileSystem::findAssets(assetExtensions(kind))) options.push_back(f);
        if (ImGui::Selectable("None", path.empty())) {
            if (ctx.record) ctx.record();
            path.clear();
            changed = true;
            ImGui::CloseCurrentPopup();
        }
        for (const std::string& o : options) {
            if (!containsNoCase(o, filter)) continue;
            if (ImGui::Selectable(o.c_str(), o == path)) {
                if (ctx.record) ctx.record();
                path = o;
                changed = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return changed;
}

bool entityField(const char* label, Entity& entity, PropertyContext& ctx, const char* tooltip) {
    labelCell(label, tooltip);
    ImGui::PushID(label);
    bool changed = false;
    Scene* scene = ctx.scene;
    std::string text = (scene && scene->valid(entity)) ? scene->nameOf(entity) + " (Entity)" : "None (Entity)";
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
    if (ImGui::Button(text.c_str(), ImVec2(-FLT_MIN, 0))) ImGui::OpenPopup("##entitypicker");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    if (scene && ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY")) {
            uint64_t uuid;
            std::memcpy(&uuid, payload->Data, sizeof(uuid));
            Entity e = scene->findByUUID(uuid);
            if (scene->valid(e)) {
                if (ctx.record) ctx.record();
                entity = e;
                changed = true;
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (scene && ImGui::BeginPopup("##entitypicker")) {
        if (ImGui::Selectable("None", entity == NullEntity)) {
            if (ctx.record) ctx.record();
            entity = NullEntity;
            changed = true;
        }
        scene->registry().each<EntityInfo>([&](Entity e, EntityInfo& info) {
            ImGui::PushID((int)e);
            if (ImGui::Selectable(info.name.c_str(), e == entity)) {
                if (ctx.record) ctx.record();
                entity = e;
                changed = true;
            }
            ImGui::PopID();
        });
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return changed;
}

bool drawField(const FieldMeta& f, void* object, PropertyContext& ctx) {
    const char* tip = f.tooltip.empty() ? nullptr : f.tooltip.c_str();
    std::string id = "##" + f.name;
    bool changed = false;
    switch (f.type) {
    case FieldType::Bool: {
        labelCell(f.name.c_str(), tip);
        bool v = f.ref<bool>(object);
        if (ImGui::Checkbox(id.c_str(), &v)) {
            if (ctx.record) ctx.record();
            f.ref<bool>(object) = v;
            changed = true;
        }
        break;
    }
    case FieldType::Int: {
        labelCell(f.name.c_str(), tip);
        int& v = f.ref<int>(object);
        if (f.min < f.max) changed = ImGui::SliderInt(id.c_str(), &v, (int)f.min, (int)f.max);
        else changed = ImGui::DragInt(id.c_str(), &v, f.speed);
        track(ctx);
        break;
    }
    case FieldType::Float: {
        labelCell(f.name.c_str(), tip);
        float& v = f.ref<float>(object);
        if (f.min < f.max) changed = ImGui::SliderFloat(id.c_str(), &v, f.min, f.max, "%.3g");
        else changed = ImGui::DragFloat(id.c_str(), &v, f.speed, 0.0f, 0.0f, "%.3g");
        track(ctx);
        break;
    }
    case FieldType::Vec2: {
        labelCell(f.name.c_str(), tip);
        changed = ImGui::DragFloat2(id.c_str(), &f.ref<glm::vec2>(object).x, f.speed, 0.0f, 0.0f, "%.3g");
        track(ctx);
        break;
    }
    case FieldType::Vec3: changed = vec3(f.name.c_str(), f.ref<glm::vec3>(object), f.speed, ctx, tip); break;
    case FieldType::Vec4: {
        labelCell(f.name.c_str(), tip);
        changed = ImGui::DragFloat4(id.c_str(), &f.ref<glm::vec4>(object).x, f.speed, 0.0f, 0.0f, "%.3g");
        track(ctx);
        break;
    }
    case FieldType::Color3: {
        labelCell(f.name.c_str(), tip);
        ImGuiColorEditFlags flags = ImGuiColorEditFlags_Float | (f.hdr ? ImGuiColorEditFlags_HDR : 0);
        changed = ImGui::ColorEdit3(id.c_str(), &f.ref<glm::vec3>(object).x, flags);
        track(ctx);
        break;
    }
    case FieldType::Color4: {
        labelCell(f.name.c_str(), tip);
        ImGuiColorEditFlags flags = ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar | (f.hdr ? ImGuiColorEditFlags_HDR : 0);
        changed = ImGui::ColorEdit4(id.c_str(), &f.ref<glm::vec4>(object).x, flags);
        track(ctx);
        break;
    }
    case FieldType::String: {
        labelCell(f.name.c_str(), tip);
        std::string& s = f.ref<std::string>(object);
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%s", s.c_str());
        if (ImGui::InputText(id.c_str(), buf, sizeof(buf))) {
            s = buf;
            changed = true;
        }
        track(ctx);
        break;
    }
    case FieldType::Enum: {
        labelCell(f.name.c_str(), tip);
        int& v = f.ref<int>(object);
        const char* preview = (v >= 0 && v < (int)f.enumNames.size()) ? f.enumNames[(size_t)v].c_str() : "?";
        if (ImGui::BeginCombo(id.c_str(), preview)) {
            for (int i = 0; i < (int)f.enumNames.size(); ++i) {
                if (ImGui::Selectable(f.enumNames[(size_t)i].c_str(), i == v) && i != v) {
                    if (ctx.record) ctx.record();
                    v = i;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        break;
    }
    case FieldType::Asset: changed = assetField(f.name.c_str(), f.ref<std::string>(object), f.assetKind, ctx, tip); break;
    case FieldType::Entity: changed = entityField(f.name.c_str(), f.ref<Entity>(object), ctx, tip); break;
    }
    return changed;
}

bool drawFields(const TypeMeta& meta, void* object, PropertyContext& ctx) {
    bool changed = false;
    for (const FieldMeta& f : meta.fields) {
        ImGui::PushID(f.name.c_str());
        changed |= drawField(f, object, ctx);
        ImGui::PopID();
    }
    return changed;
}

} // namespace PropertyDrawer
} // namespace Forge::Editor
