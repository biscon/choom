#pragma once

#include <algorithm>

namespace game {

struct Health {
    int baseMaximum = 100;
    int maximum = 100;
    int current = 100;
};

inline Health MakeHealth(int baseMaximum)
{
    const int normalized = std::max(1, baseMaximum);
    return Health{normalized, normalized, normalized};
}

inline int ApplyDamage(Health& health, int damage)
{
    if (damage <= 0 || health.current <= 0) return 0;
    const int applied = std::min(health.current, damage);
    health.current -= applied;
    return applied;
}

inline bool IsDepleted(const Health& health)
{
    return health.current <= 0;
}

} // namespace game
