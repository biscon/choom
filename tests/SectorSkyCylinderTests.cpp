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
    Check(game::FindDefaultSkyTexture(empty) == nullptr,
          "missing texture entry has no default sky texture");
    Check(!game::ShouldRenderDefaultSkyCylinder(empty),
          "missing texture entry disables default sky cylinder");

    game::SectorTopologyMap unrelated = MakeMap(true);
    AddTexture(unrelated, "wall");
    Check(game::FindDefaultSkyTexture(unrelated) == nullptr,
          "unrelated texture entry has no default sky texture");
    Check(!game::ShouldRenderDefaultSkyCylinder(unrelated),
          "unrelated texture entry disables default sky cylinder");

    game::SectorTopologyMap sky = MakeMap(true);
    AddTexture(sky, std::string(game::kDefaultSkyTextureId).c_str());
    const game::SectorTextureDefinition* texture = game::FindDefaultSkyTexture(sky);
    Check(texture != nullptr, "sky_cylinder texture entry is selected");
    Check(texture != nullptr && texture->id == game::kDefaultSkyTextureId,
          "selected sky texture preserves sky_cylinder id");
    Check(game::ShouldRenderDefaultSkyCylinder(sky),
          "sky sector plus sky_cylinder texture enables default sky cylinder");

    game::SectorTopologyMap noSkySector = MakeMap(false);
    AddTexture(noSkySector, std::string(game::kDefaultSkyTextureId).c_str());
    Check(!game::HasSkyCeilingSector(noSkySector),
          "map without ceilingSky sectors reports no sky ceiling");
    Check(!game::ShouldRenderDefaultSkyCylinder(noSkySector),
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

    bool validIndices = true;
    for (uint16_t index : data.indices) {
        if (index >= data.positions.size()) {
            validIndices = false;
            break;
        }
    }
    Check(validIndices, "sky cylinder indices are valid for vertex count");
}

} // namespace

int main()
{
    TestDefaultSkyTextureLookup();
    TestSkyCylinderMeshData();

    if (failures == 0) {
        std::puts("Sector sky cylinder tests passed");
    }
    return failures == 0 ? 0 : 1;
}
