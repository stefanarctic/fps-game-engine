#include "Forge/ECS/Reflection.h"

#include <cstdio>
#include <cstdlib>

namespace Forge {

ComponentRegistry& ComponentRegistry::instance() {
    static ComponentRegistry registry;
    return registry;
}

ComponentMeta* ComponentRegistry::find(const std::string& name) {
    for (auto& c : m_Components)
        if (c->name == name) return c.get();
    return nullptr;
}

namespace Reflection {

namespace {
template <int N, class V>
Json vecToJson(const V& v) {
    Json a = Json::array();
    for (int i = 0; i < N; ++i) a.push(Json(v[i]));
    return a;
}

template <int N, class V>
void vecFromJson(V& v, const Json& j) {
    if (!j.isArray()) return;
    for (int i = 0; i < N && i < (int)j.size(); ++i) v[i] = j[(size_t)i].asFloat(v[i]);
}

std::string idToString(uint64_t id) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)id);
    return buf;
}
} // namespace

Json toJson(const TypeMeta& meta, const void* object, const EntityToId& entityToId) {
    Json out = Json::object();
    for (const FieldMeta& f : meta.fields) {
        switch (f.type) {
        case FieldType::Bool: out[f.name] = Json(f.ref<bool>(object)); break;
        case FieldType::Int: out[f.name] = Json(f.ref<int>(object)); break;
        case FieldType::Enum: {
            int v = f.ref<int>(object);
            if (v >= 0 && v < (int)f.enumNames.size()) out[f.name] = Json(f.enumNames[(size_t)v]);
            else out[f.name] = Json(v);
            break;
        }
        case FieldType::Float: out[f.name] = Json(f.ref<float>(object)); break;
        case FieldType::Vec2: out[f.name] = vecToJson<2>(f.ref<glm::vec2>(object)); break;
        case FieldType::Vec3:
        case FieldType::Color3: out[f.name] = vecToJson<3>(f.ref<glm::vec3>(object)); break;
        case FieldType::Vec4:
        case FieldType::Color4: out[f.name] = vecToJson<4>(f.ref<glm::vec4>(object)); break;
        case FieldType::String:
        case FieldType::Asset: out[f.name] = Json(f.ref<std::string>(object)); break;
        case FieldType::Entity: {
            Entity e = f.ref<Entity>(object);
            uint64_t id = (entityToId && e != NullEntity) ? entityToId(e) : 0;
            out[f.name] = Json(idToString(id));
            break;
        }
        }
    }
    return out;
}

void fromJson(const TypeMeta& meta, void* object, const Json& json, const IdToEntity& idToEntity) {
    if (!json.isObject()) return;
    for (const FieldMeta& f : meta.fields) {
        if (!json.has(f.name)) continue;
        const Json& j = json[f.name];
        switch (f.type) {
        case FieldType::Bool: f.ref<bool>(object) = j.asBool(f.ref<bool>(object)); break;
        case FieldType::Int: f.ref<int>(object) = j.asInt(f.ref<int>(object)); break;
        case FieldType::Enum: {
            if (j.isString()) {
                for (size_t i = 0; i < f.enumNames.size(); ++i)
                    if (f.enumNames[i] == j.asString()) f.ref<int>(object) = (int)i;
            } else {
                f.ref<int>(object) = j.asInt(f.ref<int>(object));
            }
            break;
        }
        case FieldType::Float: f.ref<float>(object) = j.asFloat(f.ref<float>(object)); break;
        case FieldType::Vec2: vecFromJson<2>(f.ref<glm::vec2>(object), j); break;
        case FieldType::Vec3:
        case FieldType::Color3: vecFromJson<3>(f.ref<glm::vec3>(object), j); break;
        case FieldType::Vec4:
        case FieldType::Color4: vecFromJson<4>(f.ref<glm::vec4>(object), j); break;
        case FieldType::String:
        case FieldType::Asset: f.ref<std::string>(object) = j.asString(f.ref<std::string>(object)); break;
        case FieldType::Entity: {
            uint64_t id = j.isString() ? std::strtoull(j.asString().c_str(), nullptr, 16) : (uint64_t)j.asNumber(0);
            f.ref<Entity>(object) = (id != 0 && idToEntity) ? idToEntity(id) : NullEntity;
            break;
        }
        }
    }
}

} // namespace Reflection
} // namespace Forge
