#pragma once

#include "game/items/ItemDefinitions.h"

#include <raylib.h>

#include <string>
#include <vector>

namespace game {

inline constexpr int kItemIconCellPixels = 128;
inline constexpr int kItemIconPaddingPixels = 8;
inline constexpr int kItemIconMaximumColumns = 8;

struct ItemIconRegion {
    std::string definitionId;
    Rectangle source = {};
};

struct ItemIconAtlasLayout {
    int columns = 0;
    int rows = 0;
    int widthPixels = 0;
    int heightPixels = 0;
    std::vector<ItemIconRegion> regions;
};

struct ItemIconCameraFit {
    bool valid = false;
    Vector3 target = {};
    Vector3 position = {};
    Vector3 up{0.0f, 1.0f, 0.0f};
    float orthographicSize = 0.0f;
    float nearPlane = 0.01f;
    float farPlane = 1.0f;
};

bool BuildItemIconAtlasLayout(
        const ItemRegistry& registry,
        ItemIconAtlasLayout& outLayout,
        std::string& error);
ItemIconCameraFit BuildItemIconCameraFit(BoundingBox localBounds);
const ItemIconRegion* FindItemIconRegion(
        const ItemIconAtlasLayout& layout,
        std::string_view definitionId);

} // namespace game
