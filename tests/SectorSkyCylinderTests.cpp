#include "sector_demo/SectorSkyCylinder.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

bool Near(float actual, float expected, float epsilon = 0.00001f)
{
    return std::fabs(actual - expected) <= epsilon;
}

bool Near(Vector3 actual, Vector3 expected, float epsilon = 0.00001f)
{
    return Near(actual.x, expected.x, epsilon)
            && Near(actual.y, expected.y, epsilon)
            && Near(actual.z, expected.z, epsilon);
}

Vector3 Subtract(Vector3 a, Vector3 b)
{
    return Vector3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 Cross(Vector3 a, Vector3 b)
{
    return Vector3{
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

bool HasValidIndices(const game::SectorSkyCylinderMeshData& data)
{
    if (data.indices.empty() || data.indices.size() % 3u != 0u) {
        return false;
    }

    for (uint16_t index : data.indices) {
        if (index >= data.positions.size()) {
            return false;
        }
    }
    return true;
}

game::SectorTopologyMap MakeMap(bool skyCeiling)
{
    game::SectorTopologyMap map;
    game::SectorTopologySector sector;
    sector.id = 10;
    sector.ceilingSky = skyCeiling;
    map.sectors.push_back(sector);
    return map;
}

void AddTexture(game::SectorTopologyMap& map, const char* id)
{
    map.texturesById.emplace(id, game::SectorTextureDefinition{
            id,
            "assets/images/test.png",
            game::SectorTextureFilter::Anisotropic8x});
}

void TestDefaultSkyTextureLookup()
{
    game::SectorTopologyMap empty = MakeMap(true);
    Check(game::FindSkyTexture(empty) == nullptr,
          "missing configured texture entry has no sky texture");
    Check(!game::ShouldRenderSkyCylinder(empty),
          "missing configured texture entry disables sky cylinder");

    game::SectorTopologyMap unrelated = MakeMap(true);
    AddTexture(unrelated, "wall");
    Check(game::FindSkyTexture(unrelated) == nullptr,
          "unrelated texture entry has no sky texture");
    Check(!game::ShouldRenderSkyCylinder(unrelated),
          "unrelated texture entry disables sky cylinder");

    game::SectorTopologyMap sky = MakeMap(true);
    AddTexture(sky, std::string(game::kDefaultSkyTextureId).c_str());
    const game::SectorTextureDefinition* texture = game::FindSkyTexture(sky);
    Check(texture != nullptr, "sky_cylinder texture entry is selected");
    Check(texture != nullptr && texture->id == game::kDefaultSkyTextureId,
          "selected sky texture preserves sky_cylinder id");
    Check(game::ShouldRenderSkyCylinder(sky),
          "sky sector plus sky_cylinder texture enables default sky cylinder");

    game::SectorTopologyMap configured = MakeMap(true);
    configured.skySettings.textureId = "storm";
    AddTexture(configured, "sky_cylinder");
    AddTexture(configured, "storm");
    texture = game::FindSkyTexture(configured);
    Check(texture != nullptr && texture->id == "storm",
          "configured sky texture ID selects matching texture");

    game::SectorTopologyMap emptyConfigured = MakeMap(true);
    emptyConfigured.skySettings.textureId.clear();
    AddTexture(emptyConfigured, std::string(game::kDefaultSkyTextureId).c_str());
    Check(game::FindSkyTexture(emptyConfigured) == nullptr,
          "empty configured sky texture ID selects no texture");
    Check(!game::ShouldRenderSkyCylinder(emptyConfigured),
          "empty configured sky texture ID disables sky cylinder");

    game::SectorTopologyMap missingConfigured = MakeMap(true);
    missingConfigured.skySettings.textureId = "missing";
    AddTexture(missingConfigured, std::string(game::kDefaultSkyTextureId).c_str());
    Check(game::FindSkyTexture(missingConfigured) == nullptr,
          "missing configured sky texture ID ignores unrelated default texture");

    game::SectorTopologyMap noSkySector = MakeMap(false);
    AddTexture(noSkySector, std::string(game::kDefaultSkyTextureId).c_str());
    Check(!game::HasSkyCeilingSector(noSkySector),
          "map without ceilingSky sectors reports no sky ceiling");
    Check(!game::ShouldRenderSkyCylinder(noSkySector),
          "sky_cylinder texture alone does not enable default sky cylinder");
}

void TestSkyCylinderMeshData()
{
    constexpr int segments = 8;
    constexpr float radius = 12.0f;
    constexpr float height = 20.0f;
    const game::SectorSkyCylinderMeshData data = game::BuildSkyCylinderMeshData(segments, radius, height);

    Check(data.positions.size() == static_cast<size_t>((segments + 1) * 2),
          "sky cylinder duplicates seam ring vertices");
    Check(data.normals.size() == data.positions.size(),
          "sky cylinder normals match position count");
    Check(data.uvs.size() == data.positions.size(),
          "sky cylinder UVs match position count");
    Check(data.indices.size() == static_cast<size_t>(segments * 6),
          "sky cylinder creates two indexed triangles per segment");
    Check(!data.indices.empty(), "sky cylinder indices are non-empty");

    Check(Near(data.positions.front(), data.positions[data.positions.size() - 2]),
          "first and last bottom ring positions match at seam");
    Check(Near(data.positions[1], data.positions.back()),
          "first and last top ring positions match at seam");
    Check(Near(data.uvs.front().x, 0.0f), "first seam U is zero");
    Check(Near(data.uvs[data.uvs.size() - 2].x, 1.0f), "last seam U is one");
    Check(Near(data.uvs[1].y, 0.0f), "top V is zero");
    Check(Near(data.uvs.front().y, 1.0f), "bottom V is one");

    Check(HasValidIndices(data), "sky cylinder indices are valid for vertex count");
}

void TestSkyCylinderVerticalUvSettings()
{
    constexpr int segments = 8;
    constexpr float radius = 12.0f;
    constexpr float height = 20.0f;
    const game::SectorSkyCylinderMeshData data = game::BuildSkyCylinderMeshData(
            segments,
            radius,
            height,
            0.25f,
            2.0f);

    Check(Near(data.uvs[1].y, 0.25f), "vertical offset affects top V");
    Check(Near(data.uvs.front().y, 2.25f), "vertical scale and offset affect bottom V");
    Check(Near(data.uvs.front().x, 0.0f)
                  && Near(data.uvs[data.uvs.size() - 2].x, 1.0f),
          "yaw is not baked into sky cylinder U coordinates");

    const game::SectorSkyCylinderMeshData clamped = game::BuildSkyCylinderMeshData(
            segments,
            radius,
            height,
            0.0f,
            -1.0f);
    Check(Near(clamped.uvs.front().y, 0.01f),
          "non-positive vertical scale clamps to positive minimum in mesh data");
}

void TestSkyCylinderTopCapMeshData()
{
    constexpr int segments = 8;
    constexpr float radius = 12.0f;
    constexpr float height = 20.0f;
    constexpr float topY = height * 0.5f;
    const game::SectorSkyCylinderMeshData data = game::BuildSkyCylinderTopCapMeshData(segments, radius, height);

    Check(!data.positions.empty(), "sky cylinder top cap positions are non-empty");
    Check(!data.indices.empty(), "sky cylinder top cap indices are non-empty");
    Check(data.normals.size() == data.positions.size(),
          "sky cylinder top cap normals match position count");
    Check(data.uvs.size() == data.positions.size(),
          "sky cylinder top cap UVs match position count");
    Check(HasValidIndices(data), "sky cylinder top cap indices are valid for vertex count");

    Check(Near(data.positions.front(), Vector3{0.0f, topY, 0.0f}),
          "sky cylinder top cap includes center vertex at top origin");
    bool verticesAtTop = true;
    bool rimRadiusMatches = true;
    for (size_t i = 0; i < data.positions.size(); ++i) {
        const Vector3 position = data.positions[i];
        if (!Near(position.y, topY)) {
            verticesAtTop = false;
        }
        if (i > 0) {
            const float xzRadius = std::sqrt(position.x * position.x + position.z * position.z);
            if (!Near(xzRadius, radius)) {
                rimRadiusMatches = false;
            }
        }
    }
    Check(verticesAtTop, "sky cylinder top cap vertices are at the cylinder top");
    Check(rimRadiusMatches, "sky cylinder top cap rim matches cylinder radius");

    if (data.indices.size() >= 3u) {
        const Vector3 a = data.positions[data.indices[0]];
        const Vector3 b = data.positions[data.indices[1]];
        const Vector3 c = data.positions[data.indices[2]];
        const Vector3 windingNormal = Cross(Subtract(b, a), Subtract(c, a));
        Check(windingNormal.y < 0.0f,
              "sky cylinder top cap winding faces downward for inside visibility");
    } else {
        Check(false, "sky cylinder top cap has a first triangle");
    }
}

} // namespace

int main()
{
    TestDefaultSkyTextureLookup();
    TestSkyCylinderMeshData();
    TestSkyCylinderVerticalUvSettings();
    TestSkyCylinderTopCapMeshData();

    if (failures == 0) {
        std::puts("Sector sky cylinder tests passed");
    }
    return failures == 0 ? 0 : 1;
}
