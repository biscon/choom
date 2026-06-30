#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorLightmap.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

game::SectorTextureDefinition Texture(const char* id)
{
    game::SectorTextureDefinition texture;
    texture.id = id;
    texture.path = std::string("assets/textures/") + id + ".png";
    return texture;
}

std::filesystem::path Phase01bSandboxDir()
{
    return std::filesystem::temp_directory_path() / "sector_lightmap_alpha_occlusion_phase_01b";
}

void WriteAlphaMaskTestTexture(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path.parent_path());
    Image image = GenImageColor(2, 2, Color{255, 255, 255, 255});
    ImageDrawPixel(&image, 0, 0, Color{255, 255, 255, 0});
    ImageDrawPixel(&image, 1, 0, Color{255, 255, 255, 255});
    ImageDrawPixel(&image, 0, 1, Color{255, 255, 255, 96});
    ImageDrawPixel(&image, 1, 1, Color{255, 255, 255, 192});
    Check(ExportImage(image, path.string().c_str()), "alpha mask test texture exports");
    UnloadImage(image);
}

void WriteSolidAlphaTestTexture(const std::filesystem::path& path, unsigned char alpha)
{
    std::filesystem::create_directories(path.parent_path());
    Image image = GenImageColor(2, 2, Color{255, 255, 255, alpha});
    Check(ExportImage(image, path.string().c_str()), "solid alpha test texture exports");
    UnloadImage(image);
}

game::SectorTopologyWallPartSettings Part(const char* textureId)
{
    game::SectorTopologyWallPartSettings part;
    part.textureId = textureId;
    return part;
}

game::SectorTopologySector Sector(int id, float floorZ = 0.0f, float ceilingZ = 24.0f)
{
    game::SectorTopologySector sector;
    sector.id = id;
    sector.name = "sector-" + std::to_string(id);
    sector.floorZ = floorZ;
    sector.ceilingZ = ceilingZ;
    sector.floorTextureId = "floor";
    sector.ceilingTextureId = "ceiling";
    sector.ambientColor = Color{200, 180, 160, 255};
    sector.ambientIntensity = 0.5f;
    sector.defaultWall = Part("wall");
    sector.defaultLower = Part("lower");
    sector.defaultUpper = Part("upper");
    return sector;
}

void AddTextureDefaults(game::SectorTopologyMap& map)
{
    for (const char* id : {"floor", "ceiling", "wall", "lower", "upper", "alt"}) {
        map.texturesById.emplace(id, Texture(id));
    }
}

void AddSide(
        game::SectorTopologyMap& map,
        int sideId,
        int lineId,
        game::SectorTopologySideKind side,
        int sectorId,
        const char* wallTexture = "wall")
{
    game::SectorTopologySideDef sideDef;
    sideDef.id = sideId;
    sideDef.lineDefId = lineId;
    sideDef.side = side;
    sideDef.sectorId = sectorId;
    sideDef.wall = Part(wallTexture);
    sideDef.lower = Part("lower");
    sideDef.upper = Part("upper");
    map.sideDefs.push_back(sideDef);
}

game::SectorTopologyMap MakeSquare()
{
    game::SectorTopologyMap map;
    AddTextureDefaults(map);
    map.vertices = {{1, 0, 0}, {2, 64, 0}, {3, 64, 64}, {4, 0, 64}};
    for (int i = 1; i <= 4; ++i) {
        const int end = i == 4 ? 1 : i + 1;
        map.lineDefs.push_back({i, i, end, i, -1});
        AddSide(map, i, i, game::SectorTopologySideKind::Front, 10);
    }
    map.sectors.push_back(Sector(10));
    map.staticLights.push_back(game::SectorTopologyStaticPointLight{
            1,
            Vector3{16.0f, game::SectorWorldToAuthoringDistance(3.0f), 16.0f},
            WHITE,
            1.0f,
            game::SectorWorldToAuthoringDistance(8.0f),
            game::SectorWorldToAuthoringDistance(0.2f)
    });
    return map;
}

game::SectorTopologyMap MakeAdjacent(float leftFloor, float leftCeiling, float rightFloor, float rightCeiling)
{
    game::SectorTopologyMap map;
    AddTextureDefaults(map);
    map.vertices = {
            {1, 0, 0}, {2, 64, 0}, {3, 64, 64}, {4, 0, 64},
            {5, 128, 0}, {6, 128, 64}};
    map.lineDefs = {
            {1, 1, 2, 1, -1},
            {2, 2, 3, 2, 8},
            {3, 3, 4, 3, -1},
            {4, 4, 1, 4, -1},
            {5, 2, 5, 5, -1},
            {6, 5, 6, 6, -1},
            {7, 6, 3, 7, -1}};
    AddSide(map, 1, 1, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 2, 2, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 3, 3, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 4, 4, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 5, 5, game::SectorTopologySideKind::Front, 20);
    AddSide(map, 6, 6, game::SectorTopologySideKind::Front, 20);
    AddSide(map, 7, 7, game::SectorTopologySideKind::Front, 20);
    AddSide(map, 8, 2, game::SectorTopologySideKind::Back, 20);
    map.sectors.push_back(Sector(10, leftFloor, leftCeiling));
    map.sectors.push_back(Sector(20, rightFloor, rightCeiling));
    return map;
}

game::SectorTopologyMap MakePlatform()
{
    game::SectorTopologyMap map = MakeSquare();
    map.vertices.insert(map.vertices.end(), {
            {5, 16, 16}, {6, 48, 16}, {7, 48, 48}, {8, 16, 48}});
    for (int i = 5; i <= 8; ++i) {
        const int end = i == 8 ? 5 : i + 1;
        const int frontSideId = i;
        const int backSideId = i + 4;
        map.lineDefs.push_back({i, i, end, frontSideId, backSideId});
        AddSide(map, frontSideId, i, game::SectorTopologySideKind::Front, 20);
        AddSide(map, backSideId, i, game::SectorTopologySideKind::Back, 10);
    }
    map.sectors.push_back(Sector(20, 8.0f, 24.0f));
    return map;
}

int CountWallChartsForLine(const game::SectorTopologyMap& map, int lineDefId)
{
    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error), "geometry builds for chart counting");
    game::SectorLightmapLayout layout;
    Check(game::BuildSectorLightmapLayout(map, layout, error), "layout builds for chart counting");

    int count = 0;
    for (const game::SectorLightmapChart& chart : layout.charts) {
        const game::SectorGeneratedSurface& surface = geometry.surfaces[static_cast<size_t>(chart.surfaceIndex)];
        if (surface.ref.topologyLineDefId == lineDefId
                && (surface.ref.kind == game::SectorGeneratedSurfaceKind::Wall
                    || surface.ref.kind == game::SectorGeneratedSurfaceKind::LowerWall
                    || surface.ref.kind == game::SectorGeneratedSurfaceKind::UpperWall)) {
            ++count;
        }
    }
    return count;
}

int CountValidCharts(const game::SectorLightmapLayout& layout)
{
    int count = 0;
    for (const game::SectorLightmapChart& chart : layout.charts) {
        if (chart.surfaceIndex >= 0) {
            ++count;
        }
    }
    return count;
}

int CountGeneratedSurfaces(const game::SectorGeneratedGeometry& geometry, game::SectorGeneratedSurfaceKind kind)
{
    int count = 0;
    for (const game::SectorGeneratedSurface& surface : geometry.surfaces) {
        if (surface.ref.kind == kind) {
            ++count;
        }
    }
    return count;
}

int CountGeneratedTrianglesExceptMiddle(const game::SectorGeneratedGeometry& geometry)
{
    int count = 0;
    for (const game::SectorGeneratedSurface& surface : geometry.surfaces) {
        if (surface.ref.kind != game::SectorGeneratedSurfaceKind::Middle) {
            count += static_cast<int>(surface.vertices.size() / 3);
        }
    }
    return count;
}

int CountAlphaOccluderTrianglesForKind(
        const std::vector<game::SectorLightmapAlphaOccluderTriangle>& occluders,
        game::SectorGeneratedSurfaceKind kind)
{
    int count = 0;
    for (const game::SectorLightmapAlphaOccluderTriangle& occluder : occluders) {
        if (occluder.surfaceRef.kind == kind) {
            ++count;
        }
    }
    return count;
}

int CountValidChartsForKind(
        const game::SectorGeneratedGeometry& geometry,
        const game::SectorLightmapLayout& layout,
        game::SectorGeneratedSurfaceKind kind)
{
    int count = 0;
    for (const game::SectorLightmapChart& chart : layout.charts) {
        if (chart.surfaceIndex < 0 || chart.surfaceIndex >= static_cast<int>(geometry.surfaces.size())) {
            continue;
        }
        if (geometry.surfaces[static_cast<size_t>(chart.surfaceIndex)].ref.kind == kind) {
            ++count;
        }
    }
    return count;
}

int CountValidChartsForSurface(
        const game::SectorGeneratedGeometry& geometry,
        const game::SectorLightmapLayout& layout,
        game::SectorGeneratedSurfaceKind kind,
        int sectorId,
        int lineDefId = -2)
{
    int count = 0;
    for (const game::SectorLightmapChart& chart : layout.charts) {
        if (chart.surfaceIndex < 0 || chart.surfaceIndex >= static_cast<int>(geometry.surfaces.size())) {
            continue;
        }
        const game::SectorGeneratedSurface& surface = geometry.surfaces[static_cast<size_t>(chart.surfaceIndex)];
        if (surface.ref.kind == kind
                && surface.ref.topologySectorId == sectorId
                && (lineDefId == -2 || surface.ref.topologyLineDefId == lineDefId)) {
            ++count;
        }
    }
    return count;
}

struct LightmapImageMetrics {
    int maxRgb = 0;
    double averageRgb = 0.0;
    unsigned char minAlpha = 255;
    int staticLightCount = 0;
    int staticSpotLightCount = 0;
    long long directShadowRays = 0;
    long long softShadowSourceRays = 0;
    int ceilingCenterRgb = 0;
    int floorCenterRgb = 0;
};

LightmapImageMetrics BakeAndMeasure(game::SectorTopologyMap map, const char* fileName)
{
    game::SectorLightmapLayout layout;
    std::string error;
    Check(game::BuildSectorLightmapLayout(map, layout, error), "metric lightmap layout builds");
    game::SectorGeneratedGeometry geometry;
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error), "metric geometry builds");

    int ceilingCenterPixel = -1;
    int floorCenterPixel = -1;
    for (const game::SectorLightmapChart& chart : layout.charts) {
        if (chart.surfaceIndex < 0 || chart.surfaceIndex >= static_cast<int>(geometry.surfaces.size())) {
            continue;
        }
        const game::SectorGeneratedSurface& surface = geometry.surfaces[static_cast<size_t>(chart.surfaceIndex)];
        const int x = chart.usableX + chart.usableWidth / 2;
        const int y = chart.usableY + chart.usableHeight / 2;
        const int pixel = y * layout.atlasWidth + x;
        if (surface.ref.topologySectorId == 10 && surface.ref.kind == game::SectorGeneratedSurfaceKind::Ceiling) {
            ceilingCenterPixel = pixel;
        } else if (surface.ref.topologySectorId == 10 && surface.ref.kind == game::SectorGeneratedSurfaceKind::Floor) {
            floorCenterPixel = pixel;
        }
    }

    std::filesystem::create_directories(Phase01bSandboxDir());
    const std::filesystem::path path = Phase01bSandboxDir() / fileName;
    game::SectorLightmapBakeResult result;
    Check(game::BakeSectorLightmap(map, layout, path.string().c_str(), result, error),
          "metric lightmap bake succeeds");
    LightmapImageMetrics metrics;
    metrics.staticLightCount = result.staticLightCount;
    metrics.staticSpotLightCount = result.staticSpotLightCount;
    metrics.directShadowRays = result.directShadowRays;
    metrics.softShadowSourceRays = result.softShadowSourceRays;

    Image image = LoadImage(path.string().c_str());
    Check(image.data != nullptr, "metric lightmap image loads");
    if (image.data == nullptr || image.width <= 0 || image.height <= 0) {
        return metrics;
    }

    Color* colors = LoadImageColors(image);
    Check(colors != nullptr, "metric lightmap colors load");
    if (colors != nullptr) {
        uint64_t rgbSum = 0;
        const int pixelCount = image.width * image.height;
        for (int i = 0; i < pixelCount; ++i) {
            const Color color = colors[i];
            const int rgb = static_cast<int>(color.r) + static_cast<int>(color.g) + static_cast<int>(color.b);
            metrics.maxRgb = std::max(metrics.maxRgb, rgb);
            rgbSum += static_cast<uint64_t>(rgb);
            metrics.minAlpha = std::min(metrics.minAlpha, color.a);
        }
        if (ceilingCenterPixel >= 0 && ceilingCenterPixel < pixelCount) {
            const Color color = colors[ceilingCenterPixel];
            metrics.ceilingCenterRgb = static_cast<int>(color.r) + static_cast<int>(color.g) + static_cast<int>(color.b);
        }
        if (floorCenterPixel >= 0 && floorCenterPixel < pixelCount) {
            const Color color = colors[floorCenterPixel];
            metrics.floorCenterRgb = static_cast<int>(color.r) + static_cast<int>(color.g) + static_cast<int>(color.b);
        }
        metrics.averageRgb = static_cast<double>(rgbSum) / static_cast<double>(pixelCount);
        UnloadImageColors(colors);
    }
    UnloadImage(image);
    std::error_code removeError;
    std::filesystem::remove(path, removeError);
    return metrics;
}

game::SectorTopologyMap MakeAlphaMiddleOcclusionBakeMap(const std::filesystem::path& texturePath)
{
    game::SectorTopologyMap map = MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f);
    game::SectorTextureDefinition texture;
    texture.id = "bars";
    texture.path = texturePath.string();
    map.texturesById["bars"] = texture;

    game::SectorTopologySideDef* frontMiddle = game::FindSectorTopologySideDef(map, 2);
    game::SectorTopologySideDef* backMiddle = game::FindSectorTopologySideDef(map, 8);
    Check(frontMiddle != nullptr && backMiddle != nullptr, "alpha middle occlusion map has portal sidedefs");
    if (frontMiddle != nullptr) {
        frontMiddle->middle = Part("bars");
    }
    if (backMiddle != nullptr) {
        backMiddle->middle = Part("bars");
    }
    return map;
}

LightmapImageMetrics BakeAlphaMiddlePointLight(const std::filesystem::path& texturePath, const char* fileName)
{
    game::SectorTopologyMap map = MakeAlphaMiddleOcclusionBakeMap(texturePath);
    map.staticLights.clear();
    map.staticSpotLights.clear();
    map.directionalLight.enabled = false;
    map.staticLights.push_back(game::SectorTopologyStaticPointLight{
            20,
            Vector3{6.0f, game::SectorWorldToAuthoringDistance(2.0f), 2.0f},
            WHITE,
            8.0f,
            game::SectorWorldToAuthoringDistance(4.0f),
            0.0f
    });
    return BakeAndMeasure(map, fileName);
}

game::SectorTopologyStaticSpotLight MakeStaticSpotlight(
        Vector3 position,
        Vector3 target,
        float innerConeDegrees,
        float outerConeDegrees,
        float sourceRadius = 0.0f);

LightmapImageMetrics BakeAlphaMiddleSpotLight(const std::filesystem::path& texturePath, const char* fileName)
{
    game::SectorTopologyMap map = MakeAlphaMiddleOcclusionBakeMap(texturePath);
    map.staticLights.clear();
    map.staticSpotLights.clear();
    map.directionalLight.enabled = false;
    map.staticSpotLights.push_back(MakeStaticSpotlight(
            Vector3{6.0f, game::SectorWorldToAuthoringDistance(2.0f), 2.0f},
            Vector3{2.0f, 0.0f, 2.0f},
            12.0f,
            24.0f));
    return BakeAndMeasure(map, fileName);
}

LightmapImageMetrics BakeAlphaMiddleDirectionalLight(const std::filesystem::path& texturePath, const char* fileName)
{
    game::SectorTopologyMap map = MakeAlphaMiddleOcclusionBakeMap(texturePath);
    map.staticLights.clear();
    map.staticSpotLights.clear();
    for (game::SectorTopologySector& sector : map.sectors) {
        sector.ceilingSky = true;
    }
    map.directionalLight.enabled = true;
    map.directionalLight.directionToLight = Vector3Normalize(Vector3{0.95f, 0.3f, 0.0f});
    map.directionalLight.color = WHITE;
    map.directionalLight.intensity = 4.0f;
    return BakeAndMeasure(map, fileName);
}

void TestSourceHashChanges()
{
    const game::SectorTopologyMap base = MakeSquare();
    const std::string hash = game::ComputeSectorLightmapSourceHash(base);

    game::SectorTopologyMap movedVertex = base;
    movedVertex.vertices[0].x += 1;
    Check(game::ComputeSectorLightmapSourceHash(movedVertex) != hash, "hash changes when vertex coordinate changes");

    game::SectorTopologyMap changedSector = base;
    changedSector.sectors[0].floorZ += 1.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedSector) != hash, "hash changes when sector floor changes");
    changedSector = base;
    changedSector.sectors[0].ceilingTextureId = "alt";
    Check(game::ComputeSectorLightmapSourceHash(changedSector) != hash, "hash changes when sector texture changes");
    changedSector = base;
    changedSector.sectors[0].ceilingSky = true;
    Check(game::ComputeSectorLightmapSourceHash(changedSector) != hash, "hash changes when sector ceiling sky changes");

    game::SectorTopologyMap changedSideDef = base;
    changedSideDef.sideDefs[0].wall.textureId = "alt";
    Check(game::ComputeSectorLightmapSourceHash(changedSideDef) != hash, "hash changes when sidedef wall texture changes");

    game::SectorTopologyMap changedLight = base;
    changedLight.staticLights[0].position.x += 1.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedLight) != hash, "hash changes when light position changes");
    changedLight = base;
    changedLight.staticLights[0].radius += 1.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedLight) != hash, "hash changes when light radius changes");
    changedLight = base;
    changedLight.staticLights[0].sourceRadius += 1.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedLight) != hash, "hash changes when light source radius changes");
    changedLight = base;
    changedLight.staticLights[0].intensity += 0.25f;
    Check(game::ComputeSectorLightmapSourceHash(changedLight) != hash, "hash changes when light intensity changes");
    changedLight = base;
    changedLight.staticLights[0].color.r = 64;
    Check(game::ComputeSectorLightmapSourceHash(changedLight) != hash, "hash changes when light color changes");

    game::SectorTopologyMap changedStaticSpotLight = base;
    changedStaticSpotLight.staticSpotLights.push_back(game::SectorTopologyStaticSpotLight{
            3,
            Vector3{16.0f, game::SectorWorldToAuthoringDistance(4.0f), 16.0f},
            Vector3{48.0f, game::SectorWorldToAuthoringDistance(1.0f), 16.0f},
            WHITE,
            1.0f,
            game::SectorWorldToAuthoringDistance(8.0f),
            20.0f,
            35.0f,
            game::SectorWorldToAuthoringDistance(0.25f)
    });
    const std::string staticSpotHash = game::ComputeSectorLightmapSourceHash(changedStaticSpotLight);
    Check(staticSpotHash != hash, "hash changes when static spot light is added");
    changedStaticSpotLight.staticSpotLights.front().position.x += 1.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedStaticSpotLight) != staticSpotHash,
          "hash changes when static spot light position changes");
    changedStaticSpotLight.staticSpotLights.front().position.x -= 1.0f;
    changedStaticSpotLight.staticSpotLights.front().target.z += 1.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedStaticSpotLight) != staticSpotHash,
          "hash changes when static spot light target changes");
    changedStaticSpotLight.staticSpotLights.front().target.z -= 1.0f;
    changedStaticSpotLight.staticSpotLights.front().range += game::SectorWorldToAuthoringDistance(1.0f);
    Check(game::ComputeSectorLightmapSourceHash(changedStaticSpotLight) != staticSpotHash,
          "hash changes when static spot light range changes");
    changedStaticSpotLight.staticSpotLights.front().range -= game::SectorWorldToAuthoringDistance(1.0f);
    changedStaticSpotLight.staticSpotLights.front().sourceRadius += game::SectorWorldToAuthoringDistance(0.25f);
    Check(game::ComputeSectorLightmapSourceHash(changedStaticSpotLight) != staticSpotHash,
          "hash changes when static spot light source radius changes");
    changedStaticSpotLight.staticSpotLights.front().sourceRadius -= game::SectorWorldToAuthoringDistance(0.25f);
    changedStaticSpotLight.staticSpotLights.front().innerConeDegrees = 12.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedStaticSpotLight) != staticSpotHash,
          "hash changes when static spot light inner cone changes");
    changedStaticSpotLight.staticSpotLights.front().innerConeDegrees = 20.0f;
    changedStaticSpotLight.staticSpotLights.front().outerConeDegrees = 48.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedStaticSpotLight) != staticSpotHash,
          "hash changes when static spot light outer cone changes");
    changedStaticSpotLight.staticSpotLights.front().outerConeDegrees = 35.0f;
    changedStaticSpotLight.staticSpotLights.front().intensity += 0.25f;
    Check(game::ComputeSectorLightmapSourceHash(changedStaticSpotLight) != staticSpotHash,
          "hash changes when static spot light intensity changes");
    changedStaticSpotLight.staticSpotLights.front().intensity -= 0.25f;
    changedStaticSpotLight.staticSpotLights.front().color.r = 64;
    Check(game::ComputeSectorLightmapSourceHash(changedStaticSpotLight) != staticSpotHash,
          "hash changes when static spot light color changes");

    game::SectorTopologyMap changedDynamicLight = base;
    changedDynamicLight.dynamicPointLights.push_back(game::SectorTopologyDynamicPointLight{
            1, Vector3{1.0f, 2.0f, 3.0f}, WHITE, 1.0f, 8.0f, true});
    Check(game::ComputeSectorLightmapSourceHash(changedDynamicLight) == hash,
          "hash ignores added dynamic point lights");
    changedDynamicLight.dynamicPointLights.front().position.x += 1.0f;
    changedDynamicLight.dynamicPointLights.front().radius += 1.0f;
    changedDynamicLight.dynamicPointLights.front().intensity += 0.25f;
    changedDynamicLight.dynamicPointLights.front().color.r = 64;
    changedDynamicLight.dynamicPointLights.front().enabled = false;
    changedDynamicLight.dynamicPointLights.front().flicker = true;
    changedDynamicLight.dynamicPointLights.front().flickerSpeed = 4.0f;
    changedDynamicLight.dynamicPointLights.front().flickerAmount = 0.8f;
    Check(game::ComputeSectorLightmapSourceHash(changedDynamicLight) == hash,
          "hash ignores dynamic point light edits");

    game::SectorTopologyMap changedDynamicSpotLight = base;
    changedDynamicSpotLight.dynamicSpotLights.push_back(game::SectorTopologyDynamicSpotLight{
            2,
            Vector3{1.0f, 2.0f, 3.0f},
            Vector3{4.0f, 5.0f, 6.0f},
            WHITE,
            1.0f,
            8.0f,
            20.0f,
            35.0f,
            true
    });
    Check(game::ComputeSectorLightmapSourceHash(changedDynamicSpotLight) == hash,
          "hash ignores added dynamic spot lights");
    changedDynamicSpotLight.dynamicSpotLights.front().position.x += 1.0f;
    changedDynamicSpotLight.dynamicSpotLights.front().target.z += 1.0f;
    changedDynamicSpotLight.dynamicSpotLights.front().range += 1.0f;
    changedDynamicSpotLight.dynamicSpotLights.front().intensity += 0.25f;
    changedDynamicSpotLight.dynamicSpotLights.front().color.r = 64;
    changedDynamicSpotLight.dynamicSpotLights.front().innerConeDegrees = 12.0f;
    changedDynamicSpotLight.dynamicSpotLights.front().outerConeDegrees = 48.0f;
    changedDynamicSpotLight.dynamicSpotLights.front().enabled = false;
    changedDynamicSpotLight.dynamicSpotLights.front().flicker = true;
    changedDynamicSpotLight.dynamicSpotLights.front().flickerSpeed = 4.0f;
    changedDynamicSpotLight.dynamicSpotLights.front().flickerAmount = 0.8f;
    changedDynamicSpotLight.dynamicSpotLights.front().castsShadow = true;
    changedDynamicSpotLight.dynamicSpotLights.front().shadowPriority = 42;
    changedDynamicSpotLight.dynamicSpotLights.front().shadowBias = 0.01f;
    changedDynamicSpotLight.dynamicSpotLights.front().shadowStrength = 0.5f;
    changedDynamicSpotLight.dynamicSpotLights.front().shadowSoftness = 4.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedDynamicSpotLight) == hash,
          "hash ignores dynamic spot light edits including shadow settings");

    game::SectorTopologyMap changedDirectional = base;
    changedDirectional.directionalLight.enabled = true;
    Check(game::ComputeSectorLightmapSourceHash(changedDirectional) != hash,
          "hash changes when directional light enabled changes");
    const std::string directionalHash = game::ComputeSectorLightmapSourceHash(changedDirectional);
    changedDirectional = base;
    changedDirectional.directionalLight.enabled = true;
    changedDirectional.directionalLight.directionToLight = Vector3{0.0f, 1.0f, 0.0f};
    Check(game::ComputeSectorLightmapSourceHash(changedDirectional) != directionalHash,
          "hash changes when directional light direction changes");
    changedDirectional = base;
    changedDirectional.directionalLight.enabled = true;
    changedDirectional.directionalLight.color.r = 128;
    Check(game::ComputeSectorLightmapSourceHash(changedDirectional) != directionalHash,
          "hash changes when directional light color changes");
    changedDirectional = base;
    changedDirectional.directionalLight.enabled = true;
    changedDirectional.directionalLight.intensity = 2.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedDirectional) != directionalHash,
          "hash changes when directional light intensity changes");

    game::SectorTopologyMap changedPreview = base;
    changedPreview.previewSettings.gravity = 99.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedPreview) == hash,
          "hash ignores preview gravity");
    changedPreview = base;
    changedPreview.previewSettings.jumpHeight = 2.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedPreview) == hash,
          "hash ignores preview jump height");
    changedPreview = base;
    changedPreview.previewSettings.headBobStrength = 0.2f;
    Check(game::ComputeSectorLightmapSourceHash(changedPreview) == hash,
          "hash ignores preview headbob strength");
    changedPreview = base;
    changedPreview.previewSettings.headBobFrequency = 12.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedPreview) == hash,
          "hash ignores preview headbob frequency");

    game::SectorTopologyMap changedSky = base;
    changedSky.skySettings.textureId = "storm_panorama";
    Check(game::ComputeSectorLightmapSourceHash(changedSky) == hash,
          "hash ignores sky texture ID");
    changedSky = base;
    changedSky.skySettings.yawOffsetDegrees = 90.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedSky) == hash,
          "hash ignores sky yaw offset");
    changedSky = base;
    changedSky.skySettings.verticalOffset = 0.25f;
    Check(game::ComputeSectorLightmapSourceHash(changedSky) == hash,
          "hash ignores sky vertical offset");
    changedSky = base;
    changedSky.skySettings.verticalScale = 2.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedSky) == hash,
          "hash ignores sky vertical scale");
    changedSky = base;
    changedSky.skySettings.topColor = Color{10, 20, 30, 255};
    Check(game::ComputeSectorLightmapSourceHash(changedSky) == hash,
          "hash ignores sky top color");
}

void TestSourceHashIncludesMiddleTextureData()
{
    const game::SectorTopologyMap base = MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f);
    const std::string hash = game::ComputeSectorLightmapSourceHash(base);

    game::SectorTopologyMap withMiddle = base;
    withMiddle.texturesById.emplace("bars", Texture("bars"));
    game::FindSectorTopologySideDef(withMiddle, 2)->middle = Part("bars");
    const std::string middleHash = game::ComputeSectorLightmapSourceHash(withMiddle);
    Check(middleHash != hash, "hash changes when middle receiver texture is added");

    game::SectorTopologyMap changedUv = withMiddle;
    game::FindSectorTopologySideDef(changedUv, 2)->middle.uv.offset.x += 1.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedUv) != middleHash,
          "hash changes when middle receiver UV changes");

    game::SectorTopologyMap changedTextureDefinition = withMiddle;
    changedTextureDefinition.texturesById["bars"].path = "assets/images/alternate_bars.png";
    Check(game::ComputeSectorLightmapSourceHash(changedTextureDefinition) != middleHash,
          "hash changes when a middle-only referenced texture definition changes");

    game::SectorTopologyMap unreferencedTexture = base;
    unreferencedTexture.texturesById.emplace("bars", Texture("bars"));
    Check(game::ComputeSectorLightmapSourceHash(unreferencedTexture) == hash,
          "hash ignores unreferenced middle texture table entries");
}

void TestSourceHashStableWhenVectorsReordered()
{
    game::SectorTopologyMap reordered = MakeAdjacent(0.0f, 24.0f, 8.0f, 24.0f);
    const std::string hash = game::ComputeSectorLightmapSourceHash(reordered);
    std::reverse(reordered.vertices.begin(), reordered.vertices.end());
    std::reverse(reordered.lineDefs.begin(), reordered.lineDefs.end());
    std::reverse(reordered.sideDefs.begin(), reordered.sideDefs.end());
    std::reverse(reordered.sectors.begin(), reordered.sectors.end());
    Check(game::ComputeSectorLightmapSourceHash(reordered) == hash, "hash is stable when topology vectors are reordered");

    reordered.staticSpotLights.push_back(game::SectorTopologyStaticSpotLight{
            7,
            Vector3{16.0f, game::SectorWorldToAuthoringDistance(3.0f), 16.0f},
            Vector3{32.0f, game::SectorWorldToAuthoringDistance(1.0f), 16.0f},
            WHITE,
            2.0f,
            game::SectorWorldToAuthoringDistance(6.0f),
            18.0f,
            36.0f,
            game::SectorWorldToAuthoringDistance(0.25f)
    });
    reordered.staticSpotLights.push_back(game::SectorTopologyStaticSpotLight{
            3,
            Vector3{48.0f, game::SectorWorldToAuthoringDistance(3.0f), 48.0f},
            Vector3{32.0f, game::SectorWorldToAuthoringDistance(1.0f), 48.0f},
            Color{255, 160, 96, 255},
            1.5f,
            game::SectorWorldToAuthoringDistance(5.0f),
            12.0f,
            30.0f,
            0.0f
    });
    const std::string spotHash = game::ComputeSectorLightmapSourceHash(reordered);
    std::reverse(reordered.staticSpotLights.begin(), reordered.staticSpotLights.end());
    Check(game::ComputeSectorLightmapSourceHash(reordered) == spotHash,
          "hash is stable when static spot light vector is reordered");
}

void TestBakeVersionInvalidatesOldLightmaps()
{
    Check(game::kSectorLightmapBakeVersion == 9,
          "lightmap bake version is bumped for alpha-tested static occlusion");

    const std::filesystem::path lightmapPath = Phase01bSandboxDir() / "phase03a_status_lightmap.png";
    WriteSolidAlphaTestTexture(lightmapPath, 255);

    game::SectorTopologyMap map = MakeSquare();
    map.bakedLightmap.path = lightmapPath.string();
    map.bakedLightmap.width = 2;
    map.bakedLightmap.height = 2;
    map.bakedLightmap.sourceHash = game::ComputeSectorLightmapSourceHash(map);
    Check(game::GetSectorLightmapStatus(map) == game::SectorLightmapStatus::Valid,
          "current bake version source hash keeps existing lightmap valid");

    map.bakedLightmap.sourceHash = "pre-alpha-occlusion-source-hash";
    Check(game::GetSectorLightmapStatus(map) == game::SectorLightmapStatus::Stale,
          "old bake version source hash is stale after alpha-tested occlusion change");
}

void TestLogicalSelfComparison()
{
    game::SectorGeneratedSurfaceRef floorA;
    floorA.kind = game::SectorGeneratedSurfaceKind::Floor;
    floorA.topologySectorId = 10;
    game::SectorGeneratedSurfaceRef floorB = floorA;
    Check(game::IsSameLogicalSectorLightmapSurface(floorA, floorB), "same floor sector is same logical surface");
    floorB.topologySectorId = 20;
    Check(!game::IsSameLogicalSectorLightmapSurface(floorA, floorB), "different floor sector is different");

    game::SectorGeneratedSurfaceRef wallA;
    wallA.kind = game::SectorGeneratedSurfaceKind::Wall;
    wallA.topologySectorId = 10;
    wallA.topologyLineDefId = 2;
    wallA.topologySideDefId = 8;
    wallA.topologySide = game::SectorTopologySideKind::Back;
    game::SectorGeneratedSurfaceRef wallB = wallA;
    Check(game::IsSameLogicalSectorLightmapSurface(wallA, wallB), "same wall topology refs are same logical surface");
    wallB.topologySideDefId = 9;
    Check(!game::IsSameLogicalSectorLightmapSurface(wallA, wallB), "different sidedef is different wall surface");
    wallB = wallA;
    wallB.kind = game::SectorGeneratedSurfaceKind::LowerWall;
    Check(!game::IsSameLogicalSectorLightmapSurface(wallA, wallB), "different wall kind is different surface");
}

void TestLayoutSmoke()
{
    std::string error;
    game::SectorLightmapLayout layout;
    Check(game::BuildSectorLightmapLayout(MakeSquare(), layout, error) && !layout.charts.empty(),
          "simple sector topology layout succeeds");

    const game::SectorTopologyMap equalPortal = MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f);
    Check(game::BuildSectorLightmapLayout(equalPortal, layout, error) && !layout.charts.empty(),
          "adjacent equal-height topology layout succeeds");
    Check(CountWallChartsForLine(equalPortal, 2) == 0, "equal-height portal produces no wall chart");

    const game::SectorTopologyMap raisedPlatform = MakePlatform();
    Check(game::BuildSectorLightmapLayout(raisedPlatform, layout, error) && !layout.charts.empty(),
          "inserted platform topology layout succeeds");
    Check(CountWallChartsForLine(raisedPlatform, 5) > 0, "platform riser receives a chart");

    game::SectorTopologyMap skyCeiling = MakeSquare();
    skyCeiling.sectors[0].ceilingSky = true;
    game::SectorGeneratedGeometry geometry;
    Check(game::BuildSectorGeneratedGeometry(skyCeiling, geometry, &error), "sky ceiling chart test geometry builds");
    Check(game::BuildSectorLightmapLayout(skyCeiling, layout, error), "sky ceiling layout succeeds");
    Check(CountValidChartsForSurface(
                  geometry, layout, game::SectorGeneratedSurfaceKind::Ceiling, 10) == 0,
          "sky ceiling allocates no ceiling chart");

    game::SectorTopologyMap bothSkyPortal = MakeAdjacent(0.0f, 24.0f, 0.0f, 16.0f);
    bothSkyPortal.sectors[0].ceilingSky = true;
    bothSkyPortal.sectors[1].ceilingSky = true;
    Check(game::BuildSectorGeneratedGeometry(bothSkyPortal, geometry, &error), "sky-sky portal chart test geometry builds");
    Check(game::BuildSectorLightmapLayout(bothSkyPortal, layout, error), "sky-sky portal layout succeeds");
    Check(CountValidChartsForSurface(
                  geometry, layout, game::SectorGeneratedSurfaceKind::UpperWall, 10, 2) == 0,
          "sky-sky portal allocates no upper-wall chart");

    game::SectorTopologyMap oneSkyPortal = MakeAdjacent(0.0f, 24.0f, 0.0f, 16.0f);
    oneSkyPortal.sectors[0].ceilingSky = true;
    Check(game::BuildSectorGeneratedGeometry(oneSkyPortal, geometry, &error), "one-sky portal chart test geometry builds");
    Check(game::BuildSectorLightmapLayout(oneSkyPortal, layout, error), "one-sky portal layout succeeds");
    Check(CountValidChartsForSurface(
                  geometry, layout, game::SectorGeneratedSurfaceKind::UpperWall, 10, 2) == 1,
          "one-sky portal keeps upper-wall chart");
}

void TestMiddleSurfacesReceiveLightmapsWithoutOccluding()
{
    game::SectorTopologyMap map = MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f);
    map.texturesById.emplace("bars", Texture("bars"));
    game::FindSectorTopologySideDef(map, 2)->middle = Part("bars");

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error), "middle lightmap test geometry builds");
    const int middleSurfaceCount = CountGeneratedSurfaces(geometry, game::SectorGeneratedSurfaceKind::Middle);
    Check(middleSurfaceCount == 2, "middle lightmap test generated middle surfaces");

    game::SectorLightmapLayout layout;
    Check(game::BuildSectorLightmapLayout(map, layout, error), "middle lightmap layout builds");
    Check(static_cast<int>(layout.charts.size()) == static_cast<int>(geometry.surfaces.size()),
          "layout keeps surface-index slots for generated geometry");
    Check(CountValidCharts(layout) == static_cast<int>(geometry.surfaces.size()),
          "middle surfaces allocate valid lightmap charts");
    Check(CountValidChartsForKind(geometry, layout, game::SectorGeneratedSurfaceKind::Middle) == middleSurfaceCount,
          "all generated middle surfaces receive charts");
    for (const game::SectorLightmapChart& chart : layout.charts) {
        if (chart.surfaceIndex < 0 || chart.surfaceIndex >= static_cast<int>(geometry.surfaces.size())) {
            continue;
        }
        const game::SectorGeneratedSurface& surface = geometry.surfaces[static_cast<size_t>(chart.surfaceIndex)];
        if (surface.ref.kind != game::SectorGeneratedSurfaceKind::Middle) {
            continue;
        }
        Check(chart.vertexUvs.size() == surface.vertices.size(),
              "middle chart stores one lightmap UV per generated vertex");
        for (Vector2 uv : chart.vertexUvs) {
            Check(uv.x > 0.0f && uv.x < 1.0f && uv.y > 0.0f && uv.y < 1.0f,
                  "middle chart lightmap UV is inside the atlas");
        }
    }

    game::SectorLightmapBakeResult result;
    const std::filesystem::path outputPath = Phase01bSandboxDir() / "sector_middle_lightmap_test.png";
    std::filesystem::create_directories(outputPath.parent_path());
    Check(game::BakeSectorLightmap(map, layout, outputPath.string().c_str(), result, error),
          "middle lightmap bake succeeds with middle receivers");
    Check(result.staticGeometryTriangles == CountGeneratedTrianglesExceptMiddle(geometry),
          "bake triangle/BVH input ignores middle surfaces");
    Check(result.validChartTexels > 0,
          "middle receiver charts contribute to baked lightmap texels");
}

void TestAlphaTestMiddleOccluderCollection()
{
    game::SectorTopologyMap opaqueMap = MakeSquare();
    game::SectorGeneratedGeometry opaqueGeometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(opaqueMap, opaqueGeometry, &error),
          "alpha occluder opaque geometry builds");
    const std::vector<game::SectorLightmapAlphaOccluderTriangle> opaqueOccluders =
            game::CollectSectorLightmapAlphaOccluders(opaqueGeometry);
    Check(opaqueOccluders.empty(),
          "opaque wall floor and ceiling surfaces do not become alpha-test occluders");

    game::SectorTopologyMap skyMap = MakeSquare();
    skyMap.sectors[0].ceilingSky = true;
    game::SectorGeneratedGeometry skyGeometry;
    Check(game::BuildSectorGeneratedGeometry(skyMap, skyGeometry, &error),
          "alpha occluder sky geometry builds");
    const std::vector<game::SectorLightmapAlphaOccluderTriangle> skyOccluders =
            game::CollectSectorLightmapAlphaOccluders(skyGeometry);
    Check(skyOccluders.empty(), "sky surfaces do not become alpha-test occluders");

    game::SectorTopologyMap middleMap = MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f);
    middleMap.texturesById.emplace("bars", Texture("bars"));
    game::SectorTopologySideDef* middleSide = game::FindSectorTopologySideDef(middleMap, 2);
    Check(middleSide != nullptr, "alpha occluder middle sidedef exists");
    if (middleSide != nullptr) {
        middleSide->middle = Part("bars");
        middleSide->middle.uv.scale = Vector2{2.0f, 3.0f};
        middleSide->middle.uv.offset = Vector2{0.25f, 0.5f};
    }

    game::SectorGeneratedGeometry middleGeometry;
    Check(game::BuildSectorGeneratedGeometry(middleMap, middleGeometry, &error),
          "alpha occluder middle geometry builds");
    const int middleSurfaceCount = CountGeneratedSurfaces(middleGeometry, game::SectorGeneratedSurfaceKind::Middle);
    const std::vector<game::SectorLightmapAlphaOccluderTriangle> middleOccluders =
            game::CollectSectorLightmapAlphaOccluders(middleGeometry);
    Check(middleSurfaceCount == 2, "alpha occluder test generated middle surfaces");
    Check(static_cast<int>(middleOccluders.size()) == middleSurfaceCount * 2,
          "alpha-tested middle surfaces produce alpha occluder triangles");
    Check(CountAlphaOccluderTrianglesForKind(middleOccluders, game::SectorGeneratedSurfaceKind::Middle)
                  == static_cast<int>(middleOccluders.size()),
          "only middle surfaces produce alpha occluder triangles");

    if (!middleOccluders.empty()) {
        const game::SectorLightmapAlphaOccluderTriangle& occluder = middleOccluders.front();
        Check(occluder.textureId == "bars", "alpha occluder preserves texture id");
        Check(std::fabs(occluder.alphaCutoff - 0.5f) < 0.0001f, "alpha occluder preserves alpha cutoff");
        Check(occluder.sourceSurfaceIndex >= 0, "alpha occluder preserves source surface index");
        Check(occluder.triangleIndex >= 0, "alpha occluder preserves triangle index");
        Check(occluder.uv0.x != 0.0f || occluder.uv0.y != 0.0f,
              "alpha occluder preserves visible texture UVs");
        Check(Vector3LengthSqr(occluder.normal) > 0.9f, "alpha occluder preserves usable normal");
    }

    game::SectorGeneratedGeometry decalGeometry = opaqueGeometry;
    if (!decalGeometry.surfaces.empty()) {
        decalGeometry.surfaces.front().decalTextureId = "bars";
        decalGeometry.surfaces.front().alphaTest = true;
        decalGeometry.surfaces.front().textureId = "wall";
    }
    const std::vector<game::SectorLightmapAlphaOccluderTriangle> decalOccluders =
            game::CollectSectorLightmapAlphaOccluders(decalGeometry);
    Check(decalOccluders.empty(), "decals are not collected as alpha-test occluders");
}

void TestAlphaMaskCacheSampling()
{
    const std::filesystem::path texturePath = Phase01bSandboxDir() / "alpha_mask.png";
    WriteAlphaMaskTestTexture(texturePath);

    game::SectorTopologyMap map;
    game::SectorTextureDefinition texture;
    texture.id = "mask";
    texture.path = texturePath.string();
    map.texturesById.emplace("mask", texture);

    game::SectorLightmapAlphaMaskCache cache;
    const game::SectorLightmapAlphaSample transparent =
            cache.Sample(map, "mask", Vector2{0.25f, 0.25f}, 0.5f);
    Check(transparent.valid, "alpha mask transparent sample is valid");
    Check(transparent.width == 2 && transparent.height == 2, "alpha mask preserves dimensions");
    Check(transparent.alpha == 0 && !transparent.opaque, "alpha sample below cutoff is transparent");

    const game::SectorLightmapAlphaSample opaque =
            cache.Sample(map, "mask", Vector2{0.75f, 0.25f}, 0.5f);
    Check(opaque.valid, "alpha mask opaque sample is valid");
    Check(opaque.alpha == 255 && opaque.opaque, "alpha sample above cutoff is opaque");

    const game::SectorLightmapAlphaSample tiled =
            cache.Sample(map, "mask", Vector2{1.25f, -0.75f}, 0.5f);
    Check(tiled.valid && tiled.alpha == transparent.alpha && !tiled.opaque,
          "alpha mask tiled UVs repeat consistently");

    const game::SectorLightmapAlphaSample cutoffBelow =
            cache.Sample(map, "mask", Vector2{0.25f, 0.75f}, 0.5f);
    const game::SectorLightmapAlphaSample cutoffAbove =
            cache.Sample(map, "mask", Vector2{0.75f, 0.75f}, 0.5f);
    Check(!cutoffBelow.opaque && cutoffAbove.opaque, "alpha cutoff comparison matches alpha-test semantics");
    Check(cache.LoadAttemptCount(map, "mask") == 1, "alpha mask cache loads each texture once");
    Check(cache.CachedTextureCount() == 1, "alpha mask cache stores one loaded texture entry");

    game::SectorTopologyMap missingMap;
    game::SectorTextureDefinition missingTexture;
    missingTexture.id = "missing";
    missingTexture.path = (Phase01bSandboxDir() / "missing.png").string();
    missingMap.texturesById.emplace("missing", missingTexture);

    game::SectorLightmapAlphaMaskCache missingCache;
    const game::SectorLightmapAlphaSample missing =
            missingCache.Sample(missingMap, "missing", Vector2{0.25f, 0.25f}, 0.5f);
    const game::SectorLightmapAlphaSample missingAgain =
            missingCache.Sample(missingMap, "missing", Vector2{0.75f, 0.75f}, 0.5f);
    Check(!missing.valid && missing.opaque && missing.alpha == 255,
          "missing alpha texture behaves conservatively as opaque");
    Check(!missingAgain.valid && missingAgain.opaque,
          "cached missing alpha texture remains conservative");
    Check(missingCache.LoadAttemptCount(missingMap, "missing") == 1,
          "alpha mask cache does not repeatedly reload missing textures");
}

void TestAlphaAwareStaticRayOcclusion()
{
    const std::filesystem::path transparentPath = Phase01bSandboxDir() / "phase02a_transparent.png";
    const std::filesystem::path opaquePath = Phase01bSandboxDir() / "phase02a_opaque.png";
    WriteSolidAlphaTestTexture(transparentPath, 0);
    WriteSolidAlphaTestTexture(opaquePath, 255);

    auto makeMap = [](const std::filesystem::path& texturePath) {
        game::SectorTopologyMap map;
        game::SectorTextureDefinition texture;
        texture.id = "bars";
        texture.path = texturePath.string();
        map.texturesById["bars"] = texture;
        map.texturesById.emplace("wall", Texture("wall"));
        return map;
    };

    auto makeTriangleSurface = [](game::SectorGeneratedSurfaceKind kind, const char* textureId, float x) {
        game::SectorGeneratedSurface surface;
        surface.ref.kind = kind;
        surface.textureId = textureId;
        surface.alphaTest = kind == game::SectorGeneratedSurfaceKind::Middle;
        surface.alphaCutoff = 0.5f;
        surface.normal = Vector3{-1.0f, 0.0f, 0.0f};
        surface.vertices = {
                game::SectorGeneratedVertex{Vector3{x, 0.0f, 0.0f}, surface.normal, Vector2{0.0f, 0.0f}},
                game::SectorGeneratedVertex{Vector3{x, 1.0f, 0.0f}, surface.normal, Vector2{1.0f, 0.0f}},
                game::SectorGeneratedVertex{Vector3{x, 0.0f, 1.0f}, surface.normal, Vector2{0.0f, 1.0f}}
        };
        return surface;
    };

    auto makeLayout = [](const game::SectorGeneratedGeometry& geometry) {
        game::SectorLightmapLayout layout;
        layout.charts.resize(geometry.surfaces.size());
        for (size_t i = 0; i < geometry.surfaces.size(); ++i) {
            game::SectorLightmapChart chart;
            chart.surfaceIndex = static_cast<int>(i);
            chart.vertexUvs.resize(geometry.surfaces[i].vertices.size(), Vector2{});
            layout.charts[i] = chart;
        }
        return layout;
    };

    game::SectorTopologyMap transparentMap = makeMap(transparentPath);
    game::SectorGeneratedGeometry transparentGeometry;
    transparentGeometry.surfaces.push_back(makeTriangleSurface(game::SectorGeneratedSurfaceKind::Middle, "bars", 0.0f));
    transparentGeometry.surfaces.push_back(makeTriangleSurface(game::SectorGeneratedSurfaceKind::Middle, "bars", 0.01f));
    const game::SectorLightmapLayout transparentLayout = makeLayout(transparentGeometry);
    const Ray transparentRay{Vector3{-1.0f, 0.25f, 0.25f}, Vector3{1.0f, 0.0f, 0.0f}};
    Check(!game::IsSectorLightmapStaticRayOccludedForTests(
                  transparentMap,
                  transparentGeometry,
                  transparentLayout,
                  transparentRay,
                  2.0f),
          "static ray through transparent alpha texels reaches endpoint");

    transparentGeometry.surfaces.push_back(makeTriangleSurface(game::SectorGeneratedSurfaceKind::Wall, "wall", 1.0f));
    const game::SectorLightmapLayout transparentWithWallLayout = makeLayout(transparentGeometry);
    Check(game::IsSectorLightmapStaticRayOccludedForTests(
                  transparentMap,
                  transparentGeometry,
                  transparentWithWallLayout,
                  transparentRay,
                  8.0f),
          "static ray through transparent alpha texels can still hit farther opaque wall");

    game::SectorTopologyMap opaqueMap = makeMap(opaquePath);
    game::SectorGeneratedGeometry opaqueGeometry;
    opaqueGeometry.surfaces.push_back(makeTriangleSurface(game::SectorGeneratedSurfaceKind::Middle, "bars", 0.0f));
    const game::SectorLightmapLayout opaqueLayout = makeLayout(opaqueGeometry);
    Check(game::IsSectorLightmapStaticRayOccludedForTests(
                  opaqueMap,
                  opaqueGeometry,
                  opaqueLayout,
                  transparentRay,
                  2.0f),
          "static ray through opaque alpha texels is blocked");

    game::SectorTopologyMap wallMap = makeMap(transparentPath);
    game::SectorGeneratedGeometry wallGeometry;
    wallGeometry.surfaces.push_back(makeTriangleSurface(game::SectorGeneratedSurfaceKind::Wall, "wall", 0.0f));
    const game::SectorLightmapLayout wallLayout = makeLayout(wallGeometry);
    Check(game::IsSectorLightmapStaticRayOccludedForTests(
                  wallMap,
                  wallGeometry,
                  wallLayout,
                  transparentRay,
                  2.0f),
          "opaque geometry static ray behavior remains blocked");
}

void TestAlphaAwareStaticLightBakePaths()
{
    const std::filesystem::path transparentPath = Phase01bSandboxDir() / "phase02b_transparent.png";
    const std::filesystem::path opaquePath = Phase01bSandboxDir() / "phase02b_opaque.png";
    WriteSolidAlphaTestTexture(transparentPath, 0);
    WriteSolidAlphaTestTexture(opaquePath, 255);

    const LightmapImageMetrics pointTransparent =
            BakeAlphaMiddlePointLight(transparentPath, "phase02b_point_transparent.png");
    const LightmapImageMetrics pointOpaque =
            BakeAlphaMiddlePointLight(opaquePath, "phase02b_point_opaque.png");
    Check(pointTransparent.floorCenterRgb > pointOpaque.floorCenterRgb + 100,
          "static point light direct bake passes through transparent alpha middle texels");
    Check(pointOpaque.directShadowRays > 0,
          "static point light direct bake tests alpha-aware occlusion rays");

    const LightmapImageMetrics spotTransparent =
            BakeAlphaMiddleSpotLight(transparentPath, "phase02b_spot_transparent.png");
    const LightmapImageMetrics spotOpaque =
            BakeAlphaMiddleSpotLight(opaquePath, "phase02b_spot_opaque.png");
    Check(spotTransparent.floorCenterRgb > spotOpaque.floorCenterRgb + 100,
          "static spotlight direct bake passes through transparent alpha middle texels");
    Check(spotOpaque.directShadowRays > 0,
          "static spotlight direct bake tests alpha-aware occlusion rays");

    const LightmapImageMetrics directionalOpaque =
            BakeAlphaMiddleDirectionalLight(opaquePath, "phase02b_directional_opaque.png");
    Check(directionalOpaque.directShadowRays > 0,
          "static directional light direct bake tests alpha-aware occlusion rays");
}

void TestDirectionalLightBakeBehavior()
{
    game::SectorTopologyMap disabled = MakeSquare();
    disabled.staticLights.clear();
    disabled.sectors[0].ceilingSky = true;
    disabled.directionalLight.enabled = false;
    const LightmapImageMetrics disabledMetrics =
            BakeAndMeasure(disabled, "sector_directional_disabled_lightmap_test.png");
    Check(disabledMetrics.maxRgb == 0, "disabled directional light preserves black no-light baseline");

    game::SectorTopologyMap outdoor = disabled;
    outdoor.directionalLight.enabled = true;
    outdoor.directionalLight.directionToLight = Vector3{0.0f, 1.0f, 0.0f};
    outdoor.directionalLight.color = WHITE;
    outdoor.directionalLight.intensity = 1.0f;
    const LightmapImageMetrics outdoorMetrics =
            BakeAndMeasure(outdoor, "sector_directional_outdoor_lightmap_test.png");
    Check(outdoorMetrics.maxRgb > disabledMetrics.maxRgb + 100,
          "unoccluded outdoor sky-sector sample facing directional light is brighter");

    game::SectorTopologyMap indoor = outdoor;
    indoor.sectors[0].ceilingSky = false;
    const LightmapImageMetrics indoorMetrics =
            BakeAndMeasure(indoor, "sector_directional_indoor_lightmap_test.png");
    Check(indoorMetrics.maxRgb == 0, "indoor non-sky sample receives no directional contribution");

    game::SectorTopologyMap backFacing = outdoor;
    backFacing.directionalLight.directionToLight = Vector3{0.0f, -1.0f, 0.0f};
    const LightmapImageMetrics backFacingMetrics =
            BakeAndMeasure(backFacing, "sector_directional_back_facing_lightmap_test.png");
    Check(backFacingMetrics.maxRgb == 0, "back-facing sample receives no directional contribution");

    game::SectorTopologyMap shadowed = MakePlatform();
    shadowed.staticLights.clear();
    for (game::SectorTopologySector& sector : shadowed.sectors) {
        sector.ceilingSky = true;
    }
    shadowed.directionalLight.enabled = true;
    shadowed.directionalLight.directionToLight = Vector3{0.45f, 0.75f, 0.0f};
    shadowed.directionalLight.color = WHITE;
    shadowed.directionalLight.intensity = 1.0f;
    const LightmapImageMetrics shadowedMetrics =
            BakeAndMeasure(shadowed, "sector_directional_shadowed_lightmap_test.png");
    Check(shadowedMetrics.averageRgb < outdoorMetrics.averageRgb,
          "outdoor sample shadowed by solid geometry is darker on average than unshadowed outdoor sample");

    game::SectorTopologyMap pointLight = MakeSquare();
    pointLight.staticLights.clear();
    const Vector3 lightPositions[] = {
            Vector3{32.0f, 12.0f, 32.0f},
            Vector3{32.0f, -24.0f, 32.0f},
            Vector3{32.0f, 48.0f, 32.0f},
            Vector3{-24.0f, 12.0f, 32.0f},
            Vector3{88.0f, 12.0f, 32.0f},
            Vector3{32.0f, 12.0f, -24.0f},
            Vector3{32.0f, 12.0f, 88.0f}
    };
    int lightId = 1;
    for (Vector3 position : lightPositions) {
        pointLight.staticLights.push_back(game::SectorTopologyStaticPointLight{
                lightId++,
                position,
                WHITE,
                8.0f,
                256.0f,
                0.0f
        });
    }
    pointLight.directionalLight.enabled = true;
    pointLight.directionalLight.intensity = 0.0f;
    pointLight.directionalLight.directionToLight = Vector3{0.0f, -1.0f, 0.0f};
    const LightmapImageMetrics pointMetrics =
            BakeAndMeasure(pointLight, "sector_directional_point_light_lightmap_test.png");
    Check(pointMetrics.staticLightCount == static_cast<int>(pointLight.staticLights.size())
                  && pointMetrics.staticSpotLightCount == 0
                  && pointMetrics.directShadowRays > 0,
          "point lights are still evaluated with directional settings present");
    Check(pointMetrics.minAlpha < 255, "ambient occlusion alpha behavior remains present");
}

game::SectorTopologyStaticSpotLight MakeStaticSpotlight(
        Vector3 position,
        Vector3 target,
        float innerConeDegrees,
        float outerConeDegrees,
        float sourceRadius)
{
    return game::SectorTopologyStaticSpotLight{
            100,
            position,
            target,
            WHITE,
            8.0f,
            game::SectorWorldToAuthoringDistance(16.0f),
            innerConeDegrees,
            outerConeDegrees,
            sourceRadius
    };
}

void TestStaticSpotlightBakeBehavior()
{
    game::SectorTopologyMap baseline = MakeSquare();
    baseline.staticLights.clear();
    const LightmapImageMetrics baselineMetrics =
            BakeAndMeasure(baseline, "sector_static_spotlight_baseline_lightmap_test.png");
    Check(baselineMetrics.maxRgb == 0, "no-light baseline remains black before static spotlight bake");

    game::SectorTopologyMap insideCone = baseline;
    insideCone.staticSpotLights.push_back(MakeStaticSpotlight(
            Vector3{2.0f, game::SectorWorldToAuthoringDistance(1.0f), 2.0f},
            Vector3{2.0f, 24.0f, 2.0f},
            20.0f,
            35.0f,
            game::SectorWorldToAuthoringDistance(0.2f)));
    const LightmapImageMetrics insideMetrics =
            BakeAndMeasure(insideCone, "sector_static_spotlight_inside_lightmap_test.png");
    Check(insideMetrics.ceilingCenterRgb > baselineMetrics.ceilingCenterRgb + 100,
          "sample inside static spotlight cone receives baked light");
    Check(insideMetrics.staticLightCount == 1
                  && insideMetrics.staticSpotLightCount == 1
                  && insideMetrics.softShadowSourceRays > 0,
          "static spotlight is counted and uses the soft source-radius shadow ray path");

    game::SectorTopologyMap outsideCone = baseline;
    outsideCone.staticSpotLights.push_back(MakeStaticSpotlight(
            Vector3{2.0f, game::SectorWorldToAuthoringDistance(4.0f), 2.0f},
            Vector3{2.0f, game::SectorWorldToAuthoringDistance(5.0f), 2.0f},
            20.0f,
            35.0f));
    const LightmapImageMetrics outsideMetrics =
            BakeAndMeasure(outsideCone, "sector_static_spotlight_outside_lightmap_test.png");
    Check(outsideMetrics.ceilingCenterRgb == baselineMetrics.ceilingCenterRgb,
          "sample outside static spotlight outer cone receives no baked light");

    game::SectorTopologyMap partialCone = baseline;
    partialCone.staticSpotLights.push_back(MakeStaticSpotlight(
            Vector3{2.0f, game::SectorWorldToAuthoringDistance(1.0f), 2.0f},
            Vector3{34.0f, 24.0f, 2.0f},
            20.0f,
            70.0f));
    const LightmapImageMetrics partialMetrics =
            BakeAndMeasure(partialCone, "sector_static_spotlight_partial_lightmap_test.png");
    Check(partialMetrics.ceilingCenterRgb > outsideMetrics.ceilingCenterRgb,
          "sample between static spotlight inner and outer cones receives partial baked light");
    Check(partialMetrics.ceilingCenterRgb < insideMetrics.ceilingCenterRgb,
          "partial static spotlight cone sample is dimmer than an inner-cone sample");

    game::SectorTopologyMap degenerateTarget = baseline;
    degenerateTarget.staticSpotLights.push_back(MakeStaticSpotlight(
            Vector3{2.0f, game::SectorWorldToAuthoringDistance(1.0f), 2.0f},
            Vector3{2.0f, game::SectorWorldToAuthoringDistance(1.0f), 2.0f},
            20.0f,
            35.0f));
    const LightmapImageMetrics degenerateMetrics =
            BakeAndMeasure(degenerateTarget, "sector_static_spotlight_degenerate_lightmap_test.png");
    Check(degenerateMetrics.floorCenterRgb > baselineMetrics.floorCenterRgb,
          "degenerate static spotlight target safely falls back to downward baked direction");

    game::SectorTopologyMap occlusionRayPath = baseline;
    occlusionRayPath.staticSpotLights.push_back(MakeStaticSpotlight(
            Vector3{2.0f, game::SectorWorldToAuthoringDistance(1.0f), 0.5f},
            Vector3{2.0f, 24.0f, 3.5f},
            25.0f,
            65.0f));

    game::SectorTopologyMap occluded = MakePlatform();
    occluded.staticLights.clear();
    occluded.staticSpotLights = occlusionRayPath.staticSpotLights;
    const LightmapImageMetrics occludedMetrics =
            BakeAndMeasure(occluded, "sector_static_spotlight_occluded_lightmap_test.png");
    Check(occludedMetrics.directShadowRays > 0,
          "solid geometry is traversed by the static spotlight occlusion path");
}

} // namespace

int main()
{
    TestSourceHashChanges();
    TestSourceHashIncludesMiddleTextureData();
    TestSourceHashStableWhenVectorsReordered();
    TestBakeVersionInvalidatesOldLightmaps();
    TestLogicalSelfComparison();
    TestLayoutSmoke();
    TestMiddleSurfacesReceiveLightmapsWithoutOccluding();
    TestAlphaTestMiddleOccluderCollection();
    TestAlphaMaskCacheSampling();
    TestAlphaAwareStaticRayOcclusion();
    TestAlphaAwareStaticLightBakePaths();
    TestDirectionalLightBakeBehavior();
    TestStaticSpotlightBakeBehavior();

    if (failures != 0) {
        std::fprintf(stderr, "%d sector topology lightmap test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
