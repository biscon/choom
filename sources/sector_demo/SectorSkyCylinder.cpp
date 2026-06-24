#include "sector_demo/SectorSkyCylinder.h"

#include <raymath.h>

#include <cmath>
#include <limits>

namespace game {

namespace {

bool IsValidSkyCylinderMeshInput(int segments, float radius, float height)
{
    return segments >= 3
            && segments <= static_cast<int>((std::numeric_limits<uint16_t>::max() - 1) / 2)
            && std::isfinite(radius)
            && std::isfinite(height)
            && radius > 0.0f
            && height > 0.0f;
}

} // namespace

bool HasSkyCeilingSector(const SectorTopologyMap& map)
{
    for (const SectorTopologySector& sector : map.sectors) {
        if (sector.ceilingSky) {
            return true;
        }
    }
    return false;
}

const SectorTextureDefinition* FindDefaultSkyTexture(const SectorTopologyMap& map)
{
    const auto it = map.texturesById.find(DefaultSectorTopologySkySettings().textureId);
    return it == map.texturesById.end() ? nullptr : &it->second;
}

const SectorTextureDefinition* FindSkyTexture(const SectorTopologyMap& map)
{
    const SectorTopologySkySettings settings = NormalizeSectorTopologySkySettings(map.skySettings);
    if (settings.textureId.empty()) {
        return nullptr;
    }
    const auto it = map.texturesById.find(settings.textureId);
    return it == map.texturesById.end() ? nullptr : &it->second;
}

bool ShouldRenderSkyCylinder(const SectorTopologyMap& map)
{
    return HasSkyCeilingSector(map) && FindSkyTexture(map) != nullptr;
}

bool ShouldRenderDefaultSkyCylinder(const SectorTopologyMap& map)
{
    return ShouldRenderSkyCylinder(map);
}

SectorSkyCylinderMeshData BuildSkyCylinderMeshData(
        int segments,
        float radius,
        float height,
        float verticalOffset,
        float verticalScale)
{
    SectorSkyCylinderMeshData data;
    if (!IsValidSkyCylinderMeshInput(segments, radius, height)) {
        return data;
    }
    SectorTopologySkySettings settings;
    settings.verticalOffset = verticalOffset;
    settings.verticalScale = verticalScale;
    settings = NormalizeSectorTopologySkySettings(settings);

    const int ringVertexCount = segments + 1;
    data.positions.reserve(static_cast<size_t>(ringVertexCount) * 2u);
    data.normals.reserve(static_cast<size_t>(ringVertexCount) * 2u);
    data.uvs.reserve(static_cast<size_t>(ringVertexCount) * 2u);
    data.indices.reserve(static_cast<size_t>(segments) * 6u);

    const float halfHeight = height * 0.5f;
    constexpr float twoPi = PI * 2.0f;
    for (int i = 0; i <= segments; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(segments);
        const float angle = twoPi * u;
        const float x = std::cos(angle) * radius;
        const float z = std::sin(angle) * radius;
        const Vector3 normal = Vector3Normalize(Vector3{-x, 0.0f, -z});

        data.positions.push_back(Vector3{x, -halfHeight, z});
        data.normals.push_back(normal);
        data.uvs.push_back(Vector2{u, 1.0f * settings.verticalScale + settings.verticalOffset});

        data.positions.push_back(Vector3{x, halfHeight, z});
        data.normals.push_back(normal);
        data.uvs.push_back(Vector2{u, settings.verticalOffset});
    }

    for (int i = 0; i < segments; ++i) {
        const uint16_t bottom0 = static_cast<uint16_t>(i * 2);
        const uint16_t top0 = static_cast<uint16_t>(i * 2 + 1);
        const uint16_t bottom1 = static_cast<uint16_t>((i + 1) * 2);
        const uint16_t top1 = static_cast<uint16_t>((i + 1) * 2 + 1);

        data.indices.push_back(bottom0);
        data.indices.push_back(bottom1);
        data.indices.push_back(top1);

        data.indices.push_back(bottom0);
        data.indices.push_back(top1);
        data.indices.push_back(top0);
    }

    return data;
}

SectorSkyCylinderMeshData BuildSkyCylinderTopCapMeshData(int segments, float radius, float height)
{
    SectorSkyCylinderMeshData data;
    if (!IsValidSkyCylinderMeshInput(segments, radius, height)) {
        return data;
    }

    const int rimVertexCount = segments + 1;
    data.positions.reserve(static_cast<size_t>(rimVertexCount) + 1u);
    data.normals.reserve(static_cast<size_t>(rimVertexCount) + 1u);
    data.uvs.reserve(static_cast<size_t>(rimVertexCount) + 1u);
    data.indices.reserve(static_cast<size_t>(segments) * 3u);

    const float topY = height * 0.5f;
    constexpr float twoPi = PI * 2.0f;

    data.positions.push_back(Vector3{0.0f, topY, 0.0f});
    data.normals.push_back(Vector3{0.0f, -1.0f, 0.0f});
    data.uvs.push_back(Vector2{0.5f, 0.5f});

    for (int i = 0; i <= segments; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(segments);
        const float angle = twoPi * u;
        const float cosAngle = std::cos(angle);
        const float sinAngle = std::sin(angle);
        const float x = cosAngle * radius;
        const float z = sinAngle * radius;

        data.positions.push_back(Vector3{x, topY, z});
        data.normals.push_back(Vector3{0.0f, -1.0f, 0.0f});
        data.uvs.push_back(Vector2{cosAngle * 0.5f + 0.5f, sinAngle * 0.5f + 0.5f});
    }

    for (int i = 0; i < segments; ++i) {
        data.indices.push_back(0);
        data.indices.push_back(static_cast<uint16_t>(i + 1));
        data.indices.push_back(static_cast<uint16_t>(i + 2));
    }

    return data;
}

} // namespace game
