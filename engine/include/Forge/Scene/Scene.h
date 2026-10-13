#pragma once
#include "Forge/Core/Base.h"
#include "Forge/ECS/Reflection.h"
#include "Forge/ECS/Registry.h"
#include "Forge/ECS/System.h"
#include "Forge/Physics/PhysicsWorld.h"
#include "Forge/Scene/Components.h"

#include <algorithm>
#include <unordered_map>

namespace Forge {

// Per-scene rendering environment (Unity's "Lighting" window).
struct Environment {
    // Gradient sky + hemispheric ambient light
    glm::vec3 skyColor{0.36f, 0.55f, 0.85f};
    glm::vec3 horizonColor{0.78f, 0.80f, 0.82f};
    glm::vec3 groundColor{0.32f, 0.30f, 0.28f};
    float ambientIntensity = 0.6f;
    float skyIntensity = 1.0f;

    bool fog = true;
    glm::vec3 fogColor{0.65f, 0.70f, 0.76f};
    float fogDensity = 0.012f;

    // Screen-space ambient occlusion
    bool ssao = true;
    float ssaoRadius = 0.6f;
    float ssaoBias = 0.03f;
    float ssaoPower = 1.6f;
    int ssaoSamples = 24;

    // Directional shadows
    bool shadows = true;
    float shadowDistance = 45.0f;
    float shadowBias = 0.0008f;
    float shadowSoftness = 1.0f;

    // Post processing
    float exposure = 1.0f;
    bool bloom = true;
    float bloomThreshold = 1.0f;
    float bloomIntensity = 0.6f;
    bool fxaa = true;
    float vignette = 0.25f;
};

TypeMeta& environmentMeta();

class Scene {
public:
    explicit Scene(const std::string& name = "Untitled");
    ~Scene();
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // --- Entities ------------------------------------------------------------
    Entity createEntity(const std::string& name = "GameObject", Entity parent = NullEntity, uint64_t uuid = 0);
    // Destroys the entity and its children. Deferred to end of frame while systems run.
    void destroyEntity(Entity e);
    void flushDestroyed();
    bool valid(Entity e) const { return m_Registry.valid(e); }
    void clear();
    size_t entityCount() const { return m_Registry.aliveCount(); }

    Entity duplicate(Entity e);

    // --- Hierarchy -----------------------------------------------------------
    void setParent(Entity child, Entity parent, bool keepWorldTransform = true);
    Entity parentOf(Entity e);
    bool isAncestor(Entity ancestor, Entity e);
    const std::vector<Entity>& roots() const { return m_Roots; }
    void setSiblingIndex(Entity e, size_t index);

    Entity findByName(const std::string& name);
    Entity findByUUID(uint64_t uuid) const;
    Entity findChild(Entity parent, const std::string& name);
    std::vector<Entity> findWithTag(const std::string& tag);
    bool isActive(Entity e);
    const std::string& nameOf(Entity e);

    // --- Components ------------------------------------------------------------
    Registry& registry() { return m_Registry; }
    template <class T, class... Args> T& add(Entity e, Args&&... args) { return m_Registry.emplace<T>(e, std::forward<Args>(args)...); }
    template <class T> T& get(Entity e) { return m_Registry.get<T>(e); }
    template <class T> T* tryGet(Entity e) { return m_Registry.tryGet<T>(e); }
    template <class T> bool has(Entity e) const { return m_Registry.has<T>(e); }
    template <class T> void remove(Entity e) { m_Registry.remove<T>(e); }
    template <class... Ts, class F> void each(F&& f) { m_Registry.each<Ts...>(std::forward<F>(f)); }

    // --- Transforms ------------------------------------------------------------
    void updateTransforms();
    glm::mat4 parentWorld(Entity e);
    void setWorldPosition(Entity e, const glm::vec3& position);
    void setWorldMatrix(Entity e, const glm::mat4& world);

    // --- Systems ---------------------------------------------------------------
    template <class T, class... Args>
    T& addSystem(Args&&... args) {
        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *system;
        m_Systems.push_back(std::move(system));
        std::stable_sort(m_Systems.begin(), m_Systems.end(),
                         [](const Scope<System>& a, const Scope<System>& b) { return a->priority < b->priority; });
        return ref;
    }
    template <class T>
    T* getSystem() {
        for (auto& s : m_Systems)
            if (auto* p = dynamic_cast<T*>(s.get())) return p;
        return nullptr;
    }
    const std::vector<Scope<System>>& systems() const { return m_Systems; }

    // Runs systems (only edit-mode systems unless playing), physics and transforms.
    void update(float dt);
    void beginPlay();
    void endPlay();
    bool isPlaying() const { return m_Playing; }

    Entity primaryCamera();
    PhysicsWorld& physics() { return m_Physics; }

    Environment environment;
    std::string name;
    std::string path; // asset path of the file this scene was loaded from / saved to

private:
    void updateTransformRecursive(Entity e, const glm::mat4& parentWorld, bool parentActive);
    void destroyRecursive(Entity e);
    void detachFromParent(Entity e);

    Registry m_Registry;
    std::vector<Entity> m_Roots;
    std::unordered_map<uint64_t, Entity> m_UUIDs;
    std::vector<Scope<System>> m_Systems;
    std::vector<Entity> m_PendingDestroy;
    PhysicsWorld m_Physics;
    bool m_Playing = false;
    bool m_Updating = false;
};

uint64_t generateUUID();

} // namespace Forge
