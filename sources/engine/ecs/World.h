#pragma once

#include "engine/ecs/ComponentPool.h"
#include "engine/ecs/Entity.h"

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace engine {

class World {
public:
    void ReserveEntities(size_t capacity)
    {
        generations.reserve(capacity);
        alive.reserve(capacity);
        freeList.reserve(capacity);
        destroyLater.reserve(capacity);
    }

    void ReserveComponentTypes(size_t count)
    {
        componentPools.reserve(count);
    }

    void LockComponentRegistration()
    {
        componentRegistrationLocked = true;
    }

    Entity CreateEntity()
    {
        AssertCanStructurallyModify();

        uint32_t index = 0;

        if (!freeList.empty()) {
            index = freeList.back();
            freeList.pop_back();
            alive[index] = 1;
        } else {
            assert(generations.size() < std::numeric_limits<uint32_t>::max());
            if (generations.size() == generations.capacity()) {
                WarnEntityCapacityExceeded("generations");
            }
            if (alive.size() == alive.capacity()) {
                WarnEntityCapacityExceeded("alive");
            }

            index = static_cast<uint32_t>(generations.size());
            generations.push_back(1);
            alive.push_back(1);
        }

        return Entity{index, generations[index]};
    }

    void DestroyLater(Entity entity)
    {
        AssertCanStructurallyModify();

        if (IsAlive(entity)) {
            if (destroyLater.size() == destroyLater.capacity()) {
                WarnEntityCapacityExceeded("destroyLater");
            }
            destroyLater.push_back(entity);
        }
    }

    void FlushDestroyedEntities()
    {
        AssertCanStructurallyModify();

        for (Entity entity : destroyLater) {
            if (!IsAlive(entity)) {
                continue;
            }

            for (auto& entry : componentPools) {
                entry.second->RemoveEntity(entity);
            }

            alive[entity.index] = 0;
            ++generations[entity.index];
            if (freeList.size() == freeList.capacity()) {
                WarnEntityCapacityExceeded("freeList");
            }
            freeList.push_back(entity.index);
        }

        destroyLater.clear();
    }

    bool IsAlive(Entity entity) const
    {
        return entity.index < generations.size()
            && alive[entity.index] != 0
            && generations[entity.index] == entity.generation;
    }

    template <typename T>
    void ReserveComponent(size_t capacity)
    {
        GetOrCreatePool<T>().Reserve(capacity);
    }

    template <typename T>
    void Add(Entity entity, T component)
    {
        AssertCanStructurallyModify();
        assert(IsAlive(entity));
        GetOrCreatePool<T>().Add(entity, std::move(component));
    }

    template <typename T>
    bool Has(Entity entity) const
    {
        const ComponentPool<T>* pool = FindPool<T>();
        return pool != nullptr && pool->Has(entity);
    }

    template <typename T>
    T& Get(Entity entity)
    {
        assert(IsAlive(entity));
        ComponentPool<T>* pool = FindPool<T>();
        assert(pool != nullptr);
        return pool->Get(entity);
    }

    template <typename T>
    const T& Get(Entity entity) const
    {
        assert(IsAlive(entity));
        const ComponentPool<T>* pool = FindPool<T>();
        assert(pool != nullptr);
        return pool->Get(entity);
    }

    template <typename T>
    void Remove(Entity entity)
    {
        AssertCanStructurallyModify();

        ComponentPool<T>* pool = FindPool<T>();
        if (pool != nullptr) {
            pool->Remove(entity);
        }
    }

    template <typename... Ts, typename Func>
    void ForEach(Func func)
    {
        static_assert(sizeof...(Ts) > 0, "ForEach requires at least one component type");
        ForEachGuard guard(*this);
        ForEachImpl<Ts...>(func);
    }

private:
    class ForEachGuard {
    public:
        explicit ForEachGuard(World& world)
            : world(world)
        {
            ++world.forEachDepth;
        }

        ~ForEachGuard()
        {
            --world.forEachDepth;
        }

    private:
        World& world;
    };

    void AssertCanStructurallyModify() const
    {
        assert(forEachDepth == 0 && "Do not structurally modify the ECS while iterating with ForEach()");
    }

    static void WarnEntityCapacityExceeded(const char* storageName)
    {
        std::fprintf(
                stderr,
                "[ECS WARNING] Entity capacity exceeded for %s storage. Dynamic allocation may occur. Did you forget to call ReserveEntities() with a high enough capacity?\n",
                storageName
        );
    }

    template <typename T>
    ComponentPool<T>* FindPool()
    {
        const auto it = componentPools.find(std::type_index(typeid(T)));
        if (it == componentPools.end()) {
            return nullptr;
        }

        return static_cast<ComponentPool<T>*>(it->second.get());
    }

    template <typename T>
    const ComponentPool<T>* FindPool() const
    {
        const auto it = componentPools.find(std::type_index(typeid(T)));
        if (it == componentPools.end()) {
            return nullptr;
        }

        return static_cast<const ComponentPool<T>*>(it->second.get());
    }

    template <typename T>
    ComponentPool<T>& GetOrCreatePool()
    {
        const std::type_index typeKey(typeid(T));
        auto it = componentPools.find(typeKey);
        if (it == componentPools.end()) {
            if (componentRegistrationLocked) {
                std::fprintf(
                        stderr,
                        "[ECS WARNING] Creating component pool after component registration was locked: %s. Did you forget to call ReserveComponent<T>() during initialization/load?\n",
                        typeid(T).name()
                );
            }

            auto pool = std::make_unique<ComponentPool<T>>();
            ComponentPool<T>* rawPool = pool.get();
            componentPools.emplace(typeKey, std::move(pool));
            return *rawPool;
        }

        return *static_cast<ComponentPool<T>*>(it->second.get());
    }

    template <typename First, typename... Rest, typename Func>
    void ForEachImpl(Func& func)
    {
        ComponentPool<First>* firstPool = FindPool<First>();
        if (firstPool == nullptr) {
            return;
        }

        const size_t count = firstPool->Size();
        for (size_t i = 0; i < count; ++i) {
            const Entity entity = firstPool->EntityAt(i);
            if (!IsAlive(entity) || !(Has<Rest>(entity) && ...)) {
                continue;
            }

            func(entity, firstPool->ComponentAt(i), Get<Rest>(entity)...);
        }
    }

    std::vector<uint32_t> generations;
    std::vector<uint8_t> alive;
    std::vector<uint32_t> freeList;
    std::vector<Entity> destroyLater;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> componentPools;
    bool componentRegistrationLocked = false;
    uint32_t forEachDepth = 0;
};

} // namespace engine
