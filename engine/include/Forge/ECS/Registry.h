#pragma once
// Sparse-set Entity Component System.
//
//  * An Entity is a 32-bit handle: 20 bits of slot index + 12 bits of version.
//    Destroying an entity bumps the version, so stale handles are detected.
//  * Each component type lives in its own ComponentPool<T>: a sparse array
//    (entity index -> dense slot) plus densely packed entity/component arrays.
//  * Components are stored in a std::deque so references stay valid when other
//    components of the same type are added (no reallocation on push_back).
//    Removing a component swaps the last element into the hole.
#include "Forge/Core/Base.h"

#include <cassert>
#include <deque>
#include <limits>
#include <type_traits>

namespace Forge {

using Entity = uint32_t;
constexpr Entity NullEntity = 0xFFFFFFFFu;

namespace ecs {
constexpr uint32_t kIndexBits = 20;
constexpr uint32_t kIndexMask = (1u << kIndexBits) - 1u;
constexpr uint32_t kVersionMask = 0xFFFu;
inline uint32_t indexOf(Entity e) { return e & kIndexMask; }
inline uint32_t versionOf(Entity e) { return (e >> kIndexBits) & kVersionMask; }
inline Entity makeEntity(uint32_t index, uint32_t version) { return (version << kIndexBits) | (index & kIndexMask); }

using TypeId = uint32_t;
inline TypeId nextTypeId() {
    static TypeId counter = 0;
    return counter++;
}
template <class T>
TypeId typeId() {
    static const TypeId id = nextTypeId();
    return id;
}
} // namespace ecs

class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual bool has(Entity e) const = 0;
    virtual void remove(Entity e) = 0;
    virtual void clear() = 0;
    virtual size_t size() const = 0;
    virtual const std::vector<Entity>& entities() const = 0;
};

template <class T>
class ComponentPool final : public IComponentPool {
public:
    static constexpr uint32_t npos = std::numeric_limits<uint32_t>::max();

    template <class... Args>
    T& emplace(Entity e, Args&&... args) {
        uint32_t idx = ecs::indexOf(e);
        if (has(e)) {
            T& existing = m_Data[m_Sparse[idx]];
            existing = construct(std::forward<Args>(args)...);
            return existing;
        }
        if (idx >= m_Sparse.size()) m_Sparse.resize((size_t)idx + 1, npos);
        m_Sparse[idx] = (uint32_t)m_Dense.size();
        m_Dense.push_back(e);
        m_Data.push_back(construct(std::forward<Args>(args)...));
        return m_Data.back();
    }

    // Aggregates (plain component structs) are brace-initialized, other types use constructors.
    template <class... Args>
    static T construct(Args&&... args) {
        if constexpr (std::is_aggregate_v<T> && sizeof...(Args) > 0) return T{std::forward<Args>(args)...};
        else return T(std::forward<Args>(args)...);
    }

    bool has(Entity e) const override {
        uint32_t idx = ecs::indexOf(e);
        return idx < m_Sparse.size() && m_Sparse[idx] != npos && m_Dense[m_Sparse[idx]] == e;
    }

    T& get(Entity e) {
        assert(has(e));
        return m_Data[m_Sparse[ecs::indexOf(e)]];
    }

    T* tryGet(Entity e) { return has(e) ? &m_Data[m_Sparse[ecs::indexOf(e)]] : nullptr; }

    void remove(Entity e) override {
        if (!has(e)) return;
        uint32_t idx = ecs::indexOf(e);
        uint32_t slot = m_Sparse[idx];
        uint32_t last = (uint32_t)m_Dense.size() - 1;
        if (slot != last) {
            m_Data[slot] = std::move(m_Data[last]);
            m_Dense[slot] = m_Dense[last];
            m_Sparse[ecs::indexOf(m_Dense[slot])] = slot;
        }
        m_Data.pop_back();
        m_Dense.pop_back();
        m_Sparse[idx] = npos;
    }

    void clear() override {
        m_Sparse.clear();
        m_Dense.clear();
        m_Data.clear();
    }

    size_t size() const override { return m_Dense.size(); }
    const std::vector<Entity>& entities() const override { return m_Dense; }

private:
    std::vector<uint32_t> m_Sparse;
    std::vector<Entity> m_Dense;
    std::deque<T> m_Data;
};

class Registry {
public:
    Registry() = default;
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    Entity create() {
        uint32_t index;
        if (!m_FreeList.empty()) {
            index = m_FreeList.back();
            m_FreeList.pop_back();
        } else {
            index = (uint32_t)m_Versions.size();
            assert(index <= ecs::kIndexMask && "Too many entities");
            m_Versions.push_back(0);
            m_Alive.push_back(false);
        }
        m_Alive[index] = true;
        m_AliveCount++;
        return ecs::makeEntity(index, m_Versions[index]);
    }

    void destroy(Entity e) {
        if (!valid(e)) return;
        for (auto& pool : m_Pools)
            if (pool) pool->remove(e);
        uint32_t index = ecs::indexOf(e);
        m_Alive[index] = false;
        m_Versions[index] = (m_Versions[index] + 1) & ecs::kVersionMask;
        m_FreeList.push_back(index);
        m_AliveCount--;
    }

    bool valid(Entity e) const {
        if (e == NullEntity) return false;
        uint32_t index = ecs::indexOf(e);
        return index < m_Versions.size() && m_Alive[index] && m_Versions[index] == ecs::versionOf(e);
    }

    size_t aliveCount() const { return m_AliveCount; }

    void clear() {
        for (auto& pool : m_Pools)
            if (pool) pool->clear();
        m_Versions.clear();
        m_Alive.clear();
        m_FreeList.clear();
        m_AliveCount = 0;
    }

    template <class T, class... Args>
    T& emplace(Entity e, Args&&... args) {
        assert(valid(e));
        return pool<T>().emplace(e, std::forward<Args>(args)...);
    }

    template <class T>
    T& get(Entity e) { return pool<T>().get(e); }

    template <class T>
    T* tryGet(Entity e) {
        auto* p = findPool<T>();
        return p ? p->tryGet(e) : nullptr;
    }

    template <class T>
    bool has(Entity e) const {
        auto* p = findPool<T>();
        return p && p->has(e);
    }

    template <class T>
    void remove(Entity e) {
        if (auto* p = findPool<T>()) p->remove(e);
    }

    // Calls f(Entity) for every live entity.
    template <class F>
    void eachEntity(F&& f) const {
        for (uint32_t i = 0; i < (uint32_t)m_Versions.size(); ++i)
            if (m_Alive[i]) f(ecs::makeEntity(i, m_Versions[i]));
    }

    // Calls f(Entity, Ts&...) for every entity that owns all of Ts.
    // Iterates a snapshot of the smallest pool so entities/components may be
    // created or destroyed safely from inside the callback.
    template <class... Ts, class F>
    void each(F&& f) {
        static_assert(sizeof...(Ts) > 0, "each<Ts...> needs at least one component type");
        const IComponentPool* pools[] = {findPool<Ts>()...};
        const IComponentPool* smallest = nullptr;
        for (const IComponentPool* p : pools) {
            if (!p) return; // some component type has never been used
            if (!smallest || p->size() < smallest->size()) smallest = p;
        }
        std::vector<Entity> snapshot = smallest->entities();
        for (Entity e : snapshot) {
            if (!valid(e) || !(has<Ts>(e) && ...)) continue;
            f(e, get<Ts>(e)...);
        }
    }

    // Returns the entities owning all of Ts (snapshot).
    template <class... Ts>
    std::vector<Entity> view() {
        std::vector<Entity> out;
        each<Ts...>([&](Entity e, Ts&...) { out.push_back(e); });
        return out;
    }

    template <class T>
    size_t count() const {
        auto* p = findPool<T>();
        return p ? p->size() : 0;
    }

private:
    template <class T>
    ComponentPool<T>& pool() {
        ecs::TypeId id = ecs::typeId<T>();
        if (id >= m_Pools.size()) m_Pools.resize((size_t)id + 1);
        if (!m_Pools[id]) m_Pools[id] = std::make_unique<ComponentPool<T>>();
        return *static_cast<ComponentPool<T>*>(m_Pools[id].get());
    }

    template <class T>
    ComponentPool<T>* findPool() const {
        ecs::TypeId id = ecs::typeId<T>();
        if (id >= m_Pools.size() || !m_Pools[id]) return nullptr;
        return static_cast<ComponentPool<T>*>(m_Pools[id].get());
    }

    std::vector<std::unique_ptr<IComponentPool>> m_Pools;
    std::vector<uint32_t> m_Versions;
    std::vector<bool> m_Alive;
    std::vector<uint32_t> m_FreeList;
    size_t m_AliveCount = 0;
};

} // namespace Forge
