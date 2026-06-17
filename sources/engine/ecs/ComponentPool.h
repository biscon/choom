#pragma once

#include "engine/ecs/Entity.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace engine {

class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual void RemoveEntity(Entity entity) = 0;
};

template <typename T>
class ComponentPool final : public IComponentPool {
public:
    void Reserve(size_t capacity)
    {
        denseEntities.reserve(capacity);
        denseComponents.reserve(capacity);
        sparse.reserve(capacity);
    }

    void Add(Entity entity, T component)
    {
        EnsureSparse(entity.index);

        if (Has(entity)) {
            denseComponents[sparse[entity.index]] = std::move(component);
            return;
        }

        if (denseEntities.size() == denseEntities.capacity()) {
            WarnCapacityExceeded("dense entity");
        }
        if (denseComponents.size() == denseComponents.capacity()) {
            WarnCapacityExceeded("dense component");
        }

        sparse[entity.index] = denseEntities.size();
        denseEntities.push_back(entity);
        denseComponents.push_back(std::move(component));
    }

    bool Has(Entity entity) const
    {
        if (entity.index >= sparse.size()) {
            return false;
        }

        const size_t denseIndex = sparse[entity.index];
        return denseIndex != InvalidDenseIndex()
            && denseIndex < denseEntities.size()
            && denseEntities[denseIndex] == entity;
    }

    T& Get(Entity entity)
    {
        assert(Has(entity));
        return denseComponents[sparse[entity.index]];
    }

    const T& Get(Entity entity) const
    {
        assert(Has(entity));
        return denseComponents[sparse[entity.index]];
    }

    void Remove(Entity entity)
    {
        if (!Has(entity)) {
            return;
        }

        const size_t removedIndex = sparse[entity.index];
        const size_t lastIndex = denseEntities.size() - 1;

        if (removedIndex != lastIndex) {
            denseEntities[removedIndex] = denseEntities[lastIndex];
            denseComponents[removedIndex] = std::move(denseComponents[lastIndex]);
            sparse[denseEntities[removedIndex].index] = removedIndex;
        }

        denseEntities.pop_back();
        denseComponents.pop_back();
        sparse[entity.index] = InvalidDenseIndex();
    }

    void RemoveEntity(Entity entity) override
    {
        Remove(entity);
    }

    size_t Size() const
    {
        return denseEntities.size();
    }

    Entity EntityAt(size_t denseIndex) const
    {
        assert(denseIndex < denseEntities.size());
        return denseEntities[denseIndex];
    }

    T& ComponentAt(size_t denseIndex)
    {
        assert(denseIndex < denseComponents.size());
        return denseComponents[denseIndex];
    }

private:
    static size_t InvalidDenseIndex()
    {
        return std::numeric_limits<size_t>::max();
    }

    void EnsureSparse(uint32_t entityIndex)
    {
        if (entityIndex >= sparse.size()) {
            const size_t requiredSize = static_cast<size_t>(entityIndex) + 1;
            if (requiredSize > sparse.capacity()) {
                WarnCapacityExceeded("sparse");
            }
            sparse.resize(requiredSize, InvalidDenseIndex());
        }
    }

    static void WarnCapacityExceeded(const char* storageName)
    {
        std::fprintf(
                stderr,
                "[ECS WARNING] ComponentPool capacity exceeded for %s storage: %s. Dynamic allocation may occur. Did you forget to reserve enough component capacity?\n",
                storageName,
                typeid(T).name()
        );
    }

    std::vector<Entity> denseEntities;
    std::vector<T> denseComponents;
    std::vector<size_t> sparse;
};

} // namespace engine
