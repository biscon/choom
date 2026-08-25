#include "game/items/ItemIconLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace game {
namespace {

Vector3 Subtract(Vector3 left, Vector3 right)
{
    return Vector3{left.x - right.x, left.y - right.y, left.z - right.z};
}

float Dot(Vector3 left, Vector3 right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

Vector3 Cross(Vector3 left, Vector3 right)
{
    return Vector3{
            left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

Vector3 Normalize(Vector3 value)
{
    const float length = std::sqrt(Dot(value, value));
    return length > 0.0f
            ? Vector3{value.x / length, value.y / length, value.z / length}
            : Vector3{};
}

bool Finite(Vector3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y)
            && std::isfinite(value.z);
}

} // namespace

bool BuildItemIconAtlasLayout(
        const ItemRegistry& registry,
        ItemIconAtlasLayout& outLayout,
        std::string& error)
{
    outLayout = ItemIconAtlasLayout{};
    const std::vector<std::string> ids = SortedItemDefinitionIds(registry);
    if (ids.empty()) {
        error.clear();
        return true;
    }
    if (ids.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        error = "Item icon atlas contains too many definitions";
        return false;
    }
    outLayout.columns = std::min(
            kItemIconMaximumColumns, static_cast<int>(ids.size()));
    outLayout.rows = (static_cast<int>(ids.size()) + outLayout.columns - 1)
            / outLayout.columns;
    if (outLayout.rows > std::numeric_limits<int>::max() / kItemIconCellPixels) {
        error = "Item icon atlas dimensions overflow";
        outLayout = ItemIconAtlasLayout{};
        return false;
    }
    outLayout.widthPixels = outLayout.columns * kItemIconCellPixels;
    outLayout.heightPixels = outLayout.rows * kItemIconCellPixels;
    outLayout.regions.reserve(ids.size());
    for (size_t index = 0; index < ids.size(); ++index) {
        const int column = static_cast<int>(index) % outLayout.columns;
        const int row = static_cast<int>(index) / outLayout.columns;
        outLayout.regions.push_back(ItemIconRegion{
                ids[index],
                Rectangle{
                        static_cast<float>(column * kItemIconCellPixels),
                        static_cast<float>(row * kItemIconCellPixels),
                        static_cast<float>(kItemIconCellPixels),
                        static_cast<float>(kItemIconCellPixels)}});
    }
    error.clear();
    return true;
}

ItemIconCameraFit BuildItemIconCameraFit(BoundingBox localBounds)
{
    ItemIconCameraFit fit;
    if (!Finite(localBounds.min) || !Finite(localBounds.max)
            || localBounds.max.x < localBounds.min.x
            || localBounds.max.y < localBounds.min.y
            || localBounds.max.z < localBounds.min.z) {
        return fit;
    }
    fit.target = Vector3{
            (localBounds.min.x + localBounds.max.x) * 0.5f,
            (localBounds.min.y + localBounds.max.y) * 0.5f,
            (localBounds.min.z + localBounds.max.z) * 0.5f};
    const Vector3 viewDirection = Normalize(Vector3{1.0f, 0.75f, 1.0f});
    const Vector3 right = Normalize(Cross(Vector3{0.0f, 1.0f, 0.0f}, viewDirection));
    fit.up = Normalize(Cross(viewDirection, right));
    float halfWidth = 0.0f;
    float halfHeight = 0.0f;
    float radius = 0.0f;
    for (int corner = 0; corner < 8; ++corner) {
        const Vector3 point{
                (corner & 1) != 0 ? localBounds.max.x : localBounds.min.x,
                (corner & 2) != 0 ? localBounds.max.y : localBounds.min.y,
                (corner & 4) != 0 ? localBounds.max.z : localBounds.min.z};
        const Vector3 offset = Subtract(point, fit.target);
        halfWidth = std::max(halfWidth, std::abs(Dot(offset, right)));
        halfHeight = std::max(halfHeight, std::abs(Dot(offset, fit.up)));
        radius = std::max(radius, std::sqrt(Dot(offset, offset)));
    }
    const float halfExtent = std::max({halfWidth, halfHeight, 0.001f});
    const float usableFraction = static_cast<float>(
            kItemIconCellPixels - kItemIconPaddingPixels * 2)
            / static_cast<float>(kItemIconCellPixels);
    fit.orthographicSize = (halfExtent * 2.0f) / usableFraction;
    const float distance = std::max(1.0f, radius * 3.0f);
    fit.position = Vector3{
            fit.target.x + viewDirection.x * distance,
            fit.target.y + viewDirection.y * distance,
            fit.target.z + viewDirection.z * distance};
    fit.farPlane = distance + std::max(1.0f, radius * 2.0f);
    fit.valid = true;
    return fit;
}

const ItemIconRegion* FindItemIconRegion(
        const ItemIconAtlasLayout& layout,
        std::string_view definitionId)
{
    const auto found = std::find_if(
            layout.regions.begin(), layout.regions.end(),
            [definitionId](const ItemIconRegion& region) {
                return region.definitionId == definitionId;
            });
    return found == layout.regions.end() ? nullptr : &*found;
}

} // namespace game
