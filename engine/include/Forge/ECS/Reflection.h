#pragma once
// Lightweight runtime reflection.
//
// Components (and plain structs such as scene Environment or Material) describe
// their editable fields once; the editor inspector, the scene serializer and the
// "Add Component" menu are all driven from these descriptions:
//
//   ComponentRegistry::instance().add<Light>("Light", "Rendering")
//       .enumeration("Type", &Light::type, {"Directional", "Point", "Spot"})
//       .color("Color", &Light::color)
//       .field("Intensity", &Light::intensity, 0.05f, 0.0f, 100.0f);
#include "Forge/Core/Base.h"
#include "Forge/Core/Json.h"
#include "Forge/ECS/Registry.h"

#include <glm/glm.hpp>
#include <initializer_list>

namespace Forge {

enum class FieldType { Bool, Int, Float, Vec2, Vec3, Vec4, Color3, Color4, String, Enum, Asset, Entity };
enum class AssetKind { Any, Model, Material, Texture, Scene };

struct FieldMeta {
    std::string name;
    FieldType type = FieldType::Float;
    size_t offset = 0;
    float speed = 0.1f;
    float min = 0.0f, max = 0.0f;            // slider range when min < max
    bool hdr = false;                        // colors: allow values > 1
    AssetKind assetKind = AssetKind::Any;
    std::vector<std::string> enumNames;
    std::string tooltip;

    template <class V>
    V& ref(void* object) const { return *reinterpret_cast<V*>(static_cast<char*>(object) + offset); }
    template <class V>
    const V& ref(const void* object) const { return *reinterpret_cast<const V*>(static_cast<const char*>(object) + offset); }
};

struct TypeMeta {
    std::string name;
    std::vector<FieldMeta> fields;
};

struct ComponentMeta : TypeMeta {
    ecs::TypeId typeId = 0;
    std::string category = "Misc";
    bool removable = true;   // false for Transform / EntityInfo
    bool hidden = false;     // hidden from the inspector & Add Component menu
    std::function<void*(Registry&, Entity)> add;    // adds if missing, returns pointer
    std::function<void*(Registry&, Entity)> get;    // nullptr if missing
    std::function<bool(Registry&, Entity)> has;
    std::function<void(Registry&, Entity)> remove;
    std::function<void(void*)> reset;               // assign a default-constructed value
};

namespace detail {
template <class T, class M>
size_t memberOffset(M T::*member) {
    alignas(T) static unsigned char storage[sizeof(T)];
    const T* object = reinterpret_cast<const T*>(storage);
    return (size_t)(reinterpret_cast<const unsigned char*>(&(object->*member)) - storage);
}
} // namespace detail

template <class T>
class TypeBuilder {
public:
    explicit TypeBuilder(TypeMeta& meta, ComponentMeta* component = nullptr) : m_Meta(meta), m_Component(component) {}

    TypeBuilder& field(const char* name, bool T::*m) { return push(name, FieldType::Bool, detail::memberOffset(m)); }
    TypeBuilder& field(const char* name, int T::*m, float speed = 1.0f, float min = 0.0f, float max = 0.0f) {
        return push(name, FieldType::Int, detail::memberOffset(m), speed, min, max);
    }
    TypeBuilder& field(const char* name, float T::*m, float speed = 0.1f, float min = 0.0f, float max = 0.0f) {
        return push(name, FieldType::Float, detail::memberOffset(m), speed, min, max);
    }
    TypeBuilder& field(const char* name, glm::vec2 T::*m, float speed = 0.1f) { return push(name, FieldType::Vec2, detail::memberOffset(m), speed); }
    TypeBuilder& field(const char* name, glm::vec3 T::*m, float speed = 0.1f) { return push(name, FieldType::Vec3, detail::memberOffset(m), speed); }
    TypeBuilder& field(const char* name, glm::vec4 T::*m, float speed = 0.1f) { return push(name, FieldType::Vec4, detail::memberOffset(m), speed); }
    TypeBuilder& field(const char* name, std::string T::*m) { return push(name, FieldType::String, detail::memberOffset(m)); }

    TypeBuilder& color(const char* name, glm::vec3 T::*m, bool hdr = false) {
        push(name, FieldType::Color3, detail::memberOffset(m));
        m_Meta.fields.back().hdr = hdr;
        return *this;
    }
    TypeBuilder& color(const char* name, glm::vec4 T::*m, bool hdr = false) {
        push(name, FieldType::Color4, detail::memberOffset(m));
        m_Meta.fields.back().hdr = hdr;
        return *this;
    }
    template <class E>
    TypeBuilder& enumeration(const char* name, E T::*m, std::initializer_list<const char*> names) {
        static_assert(sizeof(E) == sizeof(int), "Reflected enums must be int-sized");
        push(name, FieldType::Enum, detail::memberOffset(m));
        for (const char* n : names) m_Meta.fields.back().enumNames.emplace_back(n);
        return *this;
    }
    TypeBuilder& asset(const char* name, std::string T::*m, AssetKind kind) {
        push(name, FieldType::Asset, detail::memberOffset(m));
        m_Meta.fields.back().assetKind = kind;
        return *this;
    }
    TypeBuilder& entity(const char* name, Entity T::*m) { return push(name, FieldType::Entity, detail::memberOffset(m)); }

    // Applies to the most recently added field.
    TypeBuilder& tooltip(const char* text) {
        if (!m_Meta.fields.empty()) m_Meta.fields.back().tooltip = text;
        return *this;
    }

    // Component options
    TypeBuilder& removable(bool value) { if (m_Component) m_Component->removable = value; return *this; }
    TypeBuilder& hidden(bool value) { if (m_Component) m_Component->hidden = value; return *this; }

private:
    TypeBuilder& push(const char* name, FieldType type, size_t offset, float speed = 0.1f, float min = 0.0f, float max = 0.0f) {
        FieldMeta f;
        f.name = name;
        f.type = type;
        f.offset = offset;
        f.speed = speed;
        f.min = min;
        f.max = max;
        m_Meta.fields.push_back(std::move(f));
        return *this;
    }

    TypeMeta& m_Meta;
    ComponentMeta* m_Component;
};

class ComponentRegistry {
public:
    static ComponentRegistry& instance();

    template <class T>
    TypeBuilder<T> add(const std::string& name, const std::string& category = "Misc") {
        ComponentMeta* existing = find<T>();
        if (existing) {
            existing->fields.clear();
            return TypeBuilder<T>(*existing, existing);
        }
        auto meta = std::make_unique<ComponentMeta>();
        meta->name = name;
        meta->category = category;
        meta->typeId = ecs::typeId<T>();
        meta->add = [](Registry& r, Entity e) -> void* { return r.has<T>(e) ? &r.get<T>(e) : &r.emplace<T>(e); };
        meta->get = [](Registry& r, Entity e) -> void* { return r.tryGet<T>(e); };
        meta->has = [](Registry& r, Entity e) { return r.has<T>(e); };
        meta->remove = [](Registry& r, Entity e) { r.remove<T>(e); };
        meta->reset = [](void* p) { *static_cast<T*>(p) = T(); };
        ComponentMeta* raw = meta.get();
        m_Components.push_back(std::move(meta));
        return TypeBuilder<T>(*raw, raw);
    }

    template <class T>
    ComponentMeta* find() {
        ecs::TypeId id = ecs::typeId<T>();
        for (auto& c : m_Components)
            if (c->typeId == id) return c.get();
        return nullptr;
    }

    ComponentMeta* find(const std::string& name);
    const std::vector<std::unique_ptr<ComponentMeta>>& components() const { return m_Components; }

private:
    std::vector<std::unique_ptr<ComponentMeta>> m_Components;
};

// Describe a plain struct (not a component), e.g. for the Lighting window.
template <class T>
TypeBuilder<T> describe(TypeMeta& meta, const char* name) {
    meta.name = name;
    meta.fields.clear();
    return TypeBuilder<T>(meta);
}

namespace Reflection {
using EntityToId = std::function<uint64_t(Entity)>;
using IdToEntity = std::function<Entity(uint64_t)>;

Json toJson(const TypeMeta& meta, const void* object, const EntityToId& entityToId = nullptr);
void fromJson(const TypeMeta& meta, void* object, const Json& json, const IdToEntity& idToEntity = nullptr);
} // namespace Reflection

} // namespace Forge
