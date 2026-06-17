#pragma once

#include <cstdint>

namespace engine {

struct Entity {
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
};

inline bool operator==(Entity lhs, Entity rhs)
{
    return lhs.index == rhs.index && lhs.generation == rhs.generation;
}

inline bool operator!=(Entity lhs, Entity rhs)
{
    return !(lhs == rhs);
}

inline Entity NullEntity()
{
    return Entity{};
}

inline bool IsNull(Entity entity)
{
    return entity.index == UINT32_MAX;
}

} // namespace engine
