#include "Forge/Scene/SceneSerializer.h"
#include "Forge/Core/FileSystem.h"
#include "Forge/Core/Log.h"

#include <cstdio>
#include <cstdlib>
#include <unordered_map>

namespace Forge {
namespace SceneSerializer {

namespace {
std::string idToString(uint64_t id) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)id);
    return buf;
}

uint64_t parseId(const Json& j) {
    if (j.isString()) return std::strtoull(j.asString().c_str(), nullptr, 16);
    return (uint64_t)j.asNumber(0);
}

Json serializeEntity(Scene& scene, Entity e) {
    Registry& reg = scene.registry();
    const EntityInfo& info = reg.get<EntityInfo>(e);
    Entity parent = scene.parentOf(e);

    Json je = Json::object();
    je["uuid"] = Json(idToString(info.uuid));
    je["name"] = Json(info.name);
    je["tag"] = Json(info.tag);
    je["active"] = Json(info.active);
    je["parent"] = Json(idToString(scene.valid(parent) ? reg.get<EntityInfo>(parent).uuid : 0));

    auto entityToId = [&scene](Entity ref) -> uint64_t {
        if (!scene.valid(ref)) return 0;
        return scene.get<EntityInfo>(ref).uuid;
    };

    Json comps = Json::object();
    for (const auto& meta : ComponentRegistry::instance().components()) {
        void* data = meta->get(reg, e);
        if (!data) continue;
        comps[meta->name] = Reflection::toJson(*meta, data, entityToId);
    }
    je["components"] = std::move(comps);
    return je;
}

void collect(Scene& scene, Entity e, std::vector<Entity>& out) {
    out.push_back(e);
    for (Entity c : scene.get<Transform>(e).children) collect(scene, c, out);
}

// Creates entities from a JSON array. When `fresh` is set, new UUIDs are generated
// and references between the created entities are remapped.
std::vector<Entity> createEntities(Scene& scene, const Json& list, bool fresh, Entity attachTo) {
    std::vector<Entity> created;
    std::vector<Entity> roots;
    std::unordered_map<uint64_t, Entity> remap; // uuid in file -> new entity
    if (!list.isArray()) return roots;

    // Pass 1: create entities (keeps file order, parents always precede children).
    for (const Json& je : list.items()) {
        uint64_t fileId = parseId(je["uuid"]);
        Entity e = scene.createEntity(je["name"].asString("GameObject"), NullEntity, fresh ? 0 : fileId);
        EntityInfo& info = scene.get<EntityInfo>(e);
        info.tag = je["tag"].asString("Untagged");
        info.active = je["active"].asBool(true);
        remap[fileId] = e;
        created.push_back(e);
    }

    // Pass 2: hierarchy.
    for (size_t i = 0; i < created.size(); ++i) {
        uint64_t parentId = parseId(list[i]["parent"]);
        Entity parent = NullEntity;
        auto it = remap.find(parentId);
        if (it != remap.end()) parent = it->second;
        else if (parentId != 0 && !fresh) parent = scene.findByUUID(parentId);

        if (scene.valid(parent)) {
            scene.setParent(created[i], parent, false);
        } else {
            if (scene.valid(attachTo)) scene.setParent(created[i], attachTo, false);
            roots.push_back(created[i]);
        }
    }

    // Pass 3: components (entity references resolved through the remap table).
    auto idToEntity = [&](uint64_t id) -> Entity {
        auto it = remap.find(id);
        if (it != remap.end()) return it->second;
        return scene.findByUUID(id);
    };
    Registry& reg = scene.registry();
    for (size_t i = 0; i < created.size(); ++i) {
        const Json& comps = list[i]["components"];
        if (!comps.isObject()) continue;
        for (const auto& member : comps.members()) {
            ComponentMeta* meta = ComponentRegistry::instance().find(member.first);
            if (!meta) {
                LOG_WARN("Unknown component '%s' skipped", member.first.c_str());
                continue;
            }
            void* data = meta->add(reg, created[i]);
            Reflection::fromJson(*meta, data, member.second, idToEntity);
        }
    }
    scene.updateTransforms();
    return roots;
}
} // namespace

Json serialize(Scene& scene) {
    Json root = Json::object();
    root["name"] = Json(scene.name);
    root["environment"] = Reflection::toJson(environmentMeta(), &scene.environment);
    std::vector<Entity> all;
    for (Entity r : scene.roots()) collect(scene, r, all);
    Json list = Json::array();
    for (Entity e : all) list.push(serializeEntity(scene, e));
    root["entities"] = std::move(list);
    return root;
}

bool deserialize(Scene& scene, const Json& json) {
    if (!json.isObject()) return false;
    scene.clear();
    scene.name = json["name"].asString("Untitled");
    scene.environment = Environment();
    Reflection::fromJson(environmentMeta(), &scene.environment, json["environment"]);
    createEntities(scene, json["entities"], false, NullEntity);
    return true;
}

bool save(Scene& scene, const std::string& assetPath) {
    std::string text = serialize(scene).dump(2);
    if (!FileSystem::writeText(FileSystem::resolve(assetPath), text)) {
        LOG_ERROR("Failed to save scene '%s'", assetPath.c_str());
        return false;
    }
    scene.path = assetPath;
    LOG_INFO("Saved scene '%s' (%zu entities)", assetPath.c_str(), scene.entityCount());
    return true;
}

bool load(Scene& scene, const std::string& assetPath) {
    std::string text;
    if (!FileSystem::readText(FileSystem::resolve(assetPath), text)) {
        LOG_ERROR("Failed to read scene '%s'", assetPath.c_str());
        return false;
    }
    Json json;
    std::string error;
    if (!Json::parse(text, json, &error)) {
        LOG_ERROR("Scene '%s' is not valid JSON: %s", assetPath.c_str(), error.c_str());
        return false;
    }
    if (!deserialize(scene, json)) return false;
    scene.path = assetPath;
    LOG_INFO("Loaded scene '%s' (%zu entities)", assetPath.c_str(), scene.entityCount());
    return true;
}

Json serializeEntities(Scene& scene, const std::vector<Entity>& entities) {
    std::vector<Entity> all;
    for (Entity e : entities)
        if (scene.valid(e)) collect(scene, e, all);
    Json list = Json::array();
    for (Entity e : all) {
        Json je = serializeEntity(scene, e);
        // Roots of the selection lose their parent link.
        if (std::find(entities.begin(), entities.end(), e) != entities.end()) je["parent"] = Json(idToString(0));
        list.push(std::move(je));
    }
    return list;
}

std::vector<Entity> instantiate(Scene& scene, const Json& entities, Entity parent) {
    return createEntities(scene, entities, true, parent);
}

} // namespace SceneSerializer
} // namespace Forge
