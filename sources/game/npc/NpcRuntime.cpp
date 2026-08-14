#include "game/npc/NpcRuntime.h"

#include <algorithm>

namespace game {

bool IsValidNpcInstanceId(std::string_view id)
{
    if (id.empty() || id.size() > 63) return false;
    return std::all_of(id.begin(), id.end(), [](char character) {
        return (character >= 'A' && character <= 'Z')
                || (character >= 'a' && character <= 'z')
                || (character >= '0' && character <= '9')
                || character == '_'
                || character == '-';
    });
}

} // namespace game
