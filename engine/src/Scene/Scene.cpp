#include "Forge/Scene/Scene.h"
#include "Forge/Core/Log.h"
#include "Forge/Scene/CoreSystems.h"
#include "Forge/Scene/SceneSerializer.h"

#include <chrono>
#include <random>

namespace Forge {

uint64_t generateUUID() {
    static std::mt19937_64 rng([] {
        std::random_device rd;
        uint64_t seed = ((uint64_t)rd() << 32) ^ (uint64_t)rd();
        seed ^= (uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return seed;
    }());
    uint64_t id = 0;
    while (id == 0) id = rng();
    return id;
}

TypeMeta& environmentMeta() {
    static TypeMeta meta;
    static bool init = false;
    if (!init) {
        init = true;
        describe<Environment>(meta, "Environment")
            .color("Sky Color", &Environment::skyColor)
            .color("Horizon Color", &Environment::horizonColor)
            .color("Ground Color", &Environment::groundColor)
            .field("Ambient Intensity", &Environment::ambientIntensity, 0.01f, 0.0f, 4.0f)
            .field("Sky Intensity", &Environment::skyIntensity, 0.01f, 0.0f, 4.0f)
            .field("Fog", &Environment::fog)
            .color("Fog Color", &Environment::fogColor)
            .field("Fog Density", &Environment::fogDensity, 0.001f, 0.0f, 0.2f)
            .field("SSAO", &Environment::ssao).tooltip("Screen-space ambient occlusion")
            .field("SSAO Radius", &Environment::ssaoRadius, 0.01f, 0.05f, 4.0f)
            .field("SSAO Bias", &Environment::ssaoBias, 0.001f, 0.0f, 0.2f)
            .field("SSAO Power", &Environment::ssaoPower, 0.01f, 0.1f, 8.0f)
            .field("SSAO Samples", &Environment::ssaoSamples, 1.0f, 4.0f, 64.0f)
            .field("Shadows", &Environment::shadows)
            .field("Shadow Distance", &Environment::shadowDistance, 0.5f, 5.0f, 300.0f)
            .field("Shadow Bias", &Environment::shadowBias, 0.0001f, 0.0f, 0.02f)
            .field("Shadow Softness", &Environment::shadowSoftness, 0.01f, 0.0f, 4.0f)
            .field("Exposure", &Environment::exposure, 0.01f, 0.05f, 8.0f)
            .field("Bloom", &Environment::bloom)
            .field("Bloom Threshold", &Environment::bloomThreshold, 0.01f, 0.0f, 10.0f)
            .field("Bloom Intensity", &Environment::bloomIntensity, 0.01f, 0.0f, 4.0f)
            .field("FXAA", &Environment::fxaa)
            .field("Vignette", &Environment::vignette, 0.01f, 0.0f, 1.0f);
    }
    return meta;
}

Scene::Scene(const std::string& sceneName) : name(sceneName) {
    // Built-in systems every scene gets.
    addSystem<RigidBodySystem>();
    addSystem<ParticleSystem>();
    addSystem<LifetimeSystem>();
}

Scene::~Scene() = default;

Entity Scene::createEntity(const std::string& entityName, Entity parent, uint64_t uuid) {
    Entity e = m_Registry.create();
    EntityInfo& info = m_Registry.emplace<EntityInfo>(e);
    info.name = entityName;
    if (uuid == 0 || m_UUIDs.count(uuid)) uuid = generateUUID();
    info.uuid = uuid;
    m_UUIDs[uuid] = e;
    m_Registry.emplace<Transform>(e);
    m_Roots.push_back(e);
    if (m_Registry.valid(parent)) setParent(e, parent, false);
    return e;
}

void Scene::destroyEntity(Entity e) {
    if (!valid(e)) return;
    if (m_Updating) {
        m_PendingDestroy.push_back(e);
        return;
    }
    destroyRecursive(e);
}

void Scene::flushDestroyed() {
    std::vector<Entity> pending;
    pending.swap(m_PendingDestroy);
    for (Entity e : pending)
        if (valid(e)) destroyRecursive(e);
}

void Scene::destroyRecursive(Entity e) {
    std::vector<Entity> children = m_Registry.get<Transform>(e).children;
    for (Entity c : children)
        if (valid(c)) destroyRecursive(c);
    detachFromParent(e);
    m_UUIDs.erase(m_Registry.get<EntityInfo>(e).uuid);
    m_Registry.destroy(e);
}

void Scene::detachFromParent(Entity e) {
    Transform& t = m_Registry.get<Transform>(e);
    if (valid(t.parent)) {
        auto& siblings = m_Registry.get<Transform>(t.parent).children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), e), siblings.end());
    } else {
        m_Roots.erase(std::remove(m_Roots.begin(), m_Roots.end(), e), m_Roots.end());
    }
    t.parent = NullEntity;
}

void Scene::clear() {
    m_Registry.clear();
    m_Roots.clear();
    m_UUIDs.clear();
    m_PendingDestroy.clear();
    m_Physics.rebuild(*this);
}

Entity Scene::duplicate(Entity e) {
    if (!valid(e)) return NullEntity;
    Json data = SceneSerializer::serializeEntities(*this, {e});
    std::vector<Entity> roots = SceneSerializer::instantiate(*this, data, parentOf(e));
    if (roots.empty()) return NullEntity;
    // Place the copy right after the original among its siblings.
    Entity parent = parentOf(e);
    const std::vector<Entity>& siblings = valid(parent) ? get<Transform>(parent).children : m_Roots;
    auto it = std::find(siblings.begin(), siblings.end(), e);
    if (it != siblings.end()) setSiblingIndex(roots[0], (size_t)(it - siblings.begin()) + 1);
    return roots[0];
}

void Scene::setParent(Entity child, Entity parent, bool keepWorldTransform) {
    if (!valid(child) || child == parent) return;
    if (valid(parent) && isAncestor(child, parent)) {
        LOG_WARN("Cannot parent an entity to one of its own children");
        return;
    }
    Transform& t = m_Registry.get<Transform>(child);
    glm::mat4 world = parentWorld(child) * t.localMatrix();

    detachFromParent(child);
    if (valid(parent)) {
        t.parent = parent;
        m_Registry.get<Transform>(parent).children.push_back(child);
    } else {
        m_Roots.push_back(child);
    }

    if (keepWorldTransform) setWorldMatrix(child, world);
}

Entity Scene::parentOf(Entity e) {
    if (!valid(e)) return NullEntity;
    return m_Registry.get<Transform>(e).parent;
}

bool Scene::isAncestor(Entity ancestor, Entity e) {
    Entity p = parentOf(e);
    while (valid(p)) {
        if (p == ancestor) return true;
        p = parentOf(p);
    }
    return false;
}

void Scene::setSiblingIndex(Entity e, size_t index) {
    if (!valid(e)) return;
    Entity parent = parentOf(e);
    std::vector<Entity>& list = valid(parent) ? m_Registry.get<Transform>(parent).children : m_Roots;
    auto it = std::find(list.begin(), list.end(), e);
    if (it == list.end()) return;
    list.erase(it);
    if (index > list.size()) index = list.size();
    list.insert(list.begin() + (std::ptrdiff_t)index, e);
}

Entity Scene::findByName(const std::string& entityName) {
    Entity found = NullEntity;
    m_Registry.each<EntityInfo>([&](Entity e, EntityInfo& info) {
        if (found == NullEntity && info.name == entityName) found = e;
    });
    return found;
}

Entity Scene::findByUUID(uint64_t uuid) const {
    auto it = m_UUIDs.find(uuid);
    return it == m_UUIDs.end() ? NullEntity : it->second;
}

Entity Scene::findChild(Entity parent, const std::string& childName) {
    if (!valid(parent)) return NullEntity;
    for (Entity c : m_Registry.get<Transform>(parent).children) {
        if (m_Registry.get<EntityInfo>(c).name == childName) return c;
        Entity deep = findChild(c, childName);
        if (deep != NullEntity) return deep;
    }
    return NullEntity;
}

std::vector<Entity> Scene::findWithTag(const std::string& tag) {
    std::vector<Entity> out;
    m_Registry.each<EntityInfo>([&](Entity e, EntityInfo& info) {
        if (info.tag == tag) out.push_back(e);
    });
    return out;
}

bool Scene::isActive(Entity e) {
    EntityInfo* info = m_Registry.tryGet<EntityInfo>(e);
    return info && info->activeInHierarchy;
}

const std::string& Scene::nameOf(Entity e) {
    static const std::string empty;
    EntityInfo* info = m_Registry.tryGet<EntityInfo>(e);
    return info ? info->name : empty;
}

glm::mat4 Scene::parentWorld(Entity e) {
    Entity p = parentOf(e);
    if (!valid(p)) return glm::mat4(1.0f);
    return parentWorld(p) * m_Registry.get<Transform>(p).localMatrix();
}

void Scene::setWorldMatrix(Entity e, const glm::mat4& world) {
    if (!valid(e)) return;
    Transform& t = m_Registry.get<Transform>(e);
    glm::mat4 local = glm::inverse(parentWorld(e)) * world;
    glm::vec3 pos, rot, scl;
    if (Math::decompose(local, pos, rot, scl)) {
        t.position = pos;
        t.rotation = rot;
        t.scale = scl;
    }
}

void Scene::setWorldPosition(Entity e, const glm::vec3& position) {
    if (!valid(e)) return;
    Transform& t = m_Registry.get<Transform>(e);
    t.position = glm::vec3(glm::inverse(parentWorld(e)) * glm::vec4(position, 1.0f));
}

void Scene::updateTransforms() {
    for (Entity root : m_Roots) updateTransformRecursive(root, glm::mat4(1.0f), true);
}

void Scene::updateTransformRecursive(Entity e, const glm::mat4& parentWorldMatrix, bool parentActive) {
    Transform& t = m_Registry.get<Transform>(e);
    EntityInfo& info = m_Registry.get<EntityInfo>(e);
    t.world = parentWorldMatrix * t.localMatrix();
    info.activeInHierarchy = parentActive && info.active;
    for (Entity c : t.children) updateTransformRecursive(c, t.world, info.activeInHierarchy);
}

void Scene::update(float dt) {
    m_Updating = true;
    updateTransforms();
    m_Physics.rebuild(*this);
    for (auto& system : m_Systems) {
        if (!system->enabled) continue;
        if (!m_Playing && !system->runInEditMode) continue;
        system->onUpdate(*this, dt);
    }
    updateTransforms();
    m_Updating = false;
    flushDestroyed();
}

void Scene::beginPlay() {
    if (m_Playing) return;
    m_Playing = true;
    updateTransforms();
    m_Physics.rebuild(*this);
    m_Updating = true;
    for (auto& system : m_Systems)
        if (system->enabled) system->onStart(*this);
    m_Updating = false;
    flushDestroyed();
    updateTransforms();
}

void Scene::endPlay() {
    if (!m_Playing) return;
    for (auto& system : m_Systems)
        if (system->enabled) system->onStop(*this);
    m_Playing = false;
}

Entity Scene::primaryCamera() {
    Entity primary = NullEntity, fallback = NullEntity;
    m_Registry.each<Camera, EntityInfo>([&](Entity e, Camera& cam, EntityInfo& info) {
        if (!info.activeInHierarchy) return;
        if (cam.primary && primary == NullEntity) primary = e;
        if (fallback == NullEntity) fallback = e;
    });
    return primary != NullEntity ? primary : fallback;
}

} // namespace Forge
