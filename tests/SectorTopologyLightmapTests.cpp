#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorTopologyGeometry.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
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

std::filesystem::path ObjectProbePhase01aSandboxDir()
{
    return std::filesystem::temp_directory_path() / "sector_baked_object_light_probes_phase_01a";
}

void PatchByte(const std::filesystem::path& path, std::streamoff offset, unsigned char value)
{
    std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);
    Check(file.is_open(), "binary patch test file opens");
    file.seekp(offset);
    file.put(static_cast<char>(value));
}

void TruncateFileByOneByte(const std::filesystem::path& path)
{
    const auto size = std::filesystem::file_size(path);
    Check(size > 0, "binary truncate test file has data");
    std::filesystem::resize_file(path, size - 1);
}

bool Near(float a, float b, float tolerance = 0.001f)
{
    return std::fabs(a - b) <= tolerance;
}

bool SameVector(Vector3 a, Vector3 b)
{
    return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z);
}

bool FiniteVector(Vector3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float Brightness(Vector3 value)
{
    return value.x + value.y + value.z;
}

Vector3 WorldToAuthoring(Vector3 value)
{
    return game::SectorWorldToAuthoringPosition(value);
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

game::SectorTopologyMap MakeProbeRectangle(game::SectorCoord width, game::SectorCoord height)
{
    game::SectorTopologyMap map;
    AddTextureDefaults(map);
    map.vertices = {{1, 0, 0}, {2, width, 0}, {3, width, height}, {4, 0, height}};
    for (int i = 1; i <= 4; ++i) {
        const int end = i == 4 ? 1 : i + 1;
        map.lineDefs.push_back({i, i, end, i, -1});
        AddSide(map, i, i, game::SectorTopologySideKind::Front, 10);
    }
    map.sectors.push_back(Sector(10));
    return map;
}

game::SectorTopologyMap MakeProbeConcaveSector()
{
    game::SectorTopologyMap map;
    AddTextureDefaults(map);
    map.vertices = {
            {1, 0, 0},
            {2, 1024, 0},
            {3, 1024, 512},
            {4, 512, 512},
            {5, 512, 1024},
            {6, 0, 1024}};
    for (int i = 1; i <= 6; ++i) {
        const int end = i == 6 ? 1 : i + 1;
        map.lineDefs.push_back({i, i, end, i, -1});
        AddSide(map, i, i, game::SectorTopologySideKind::Front, 10);
    }
    map.sectors.push_back(Sector(10));
    return map;
}

game::SectorTopologyMap MakeProbeHoleSector()
{
    game::SectorTopologyMap map = MakeProbeRectangle(1536, 1536);
    map.vertices.insert(map.vertices.end(), {
            {5, 512, 512},
            {6, 1024, 512},
            {7, 1024, 1024},
            {8, 512, 1024}});
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

    game::SectorTopologyMap changedProbeSettings = base;
    changedProbeSettings.lightmapSettings.objectProbeSpacingWorld = 3.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedProbeSettings) != hash,
          "hash changes when object probe spacing changes");
    changedProbeSettings = base;
    changedProbeSettings.lightmapSettings.objectProbeHeightWorld = 1.6f;
    Check(game::ComputeSectorLightmapSourceHash(changedProbeSettings) != hash,
          "hash changes when object probe height changes");
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
    Check(game::kSectorLightmapBakeVersion == 10,
          "lightmap bake version is bumped for baked object lighting probes");

    const std::filesystem::path lightmapPath = Phase01bSandboxDir() / "phase06a_status_lightmap.png";
    WriteSolidAlphaTestTexture(lightmapPath, 255);

    game::SectorTopologyMap map = MakeSquare();
    map.bakedLightmap.path = lightmapPath.string();
    map.bakedLightmap.width = 2;
    map.bakedLightmap.height = 2;
    map.bakedLightmap.sourceHash = game::ComputeSectorLightmapSourceHash(map);
    Check(game::GetSectorLightmapStatus(map) == game::SectorLightmapStatus::Valid,
          "current bake version source hash keeps existing lightmap valid");

    const std::filesystem::path objectProbePath =
            game::MakeSectorObjectProbeSidecarPathForLightmapPath(lightmapPath.string());
    std::string probeError;
    Check(game::WriteSectorBakedObjectLightProbeSidecar(objectProbePath.string(), {}, 4.0f, 1.2f, probeError),
          "object probe version invalidation sidecar fixture writes");
    map.bakedLightmap.objectProbes.path = objectProbePath.string();
    map.bakedLightmap.objectProbes.version = game::kSectorBakedObjectLightProbeSidecarVersion;
    map.bakedLightmap.objectProbes.sourceHash = map.bakedLightmap.sourceHash;
    map.bakedLightmap.objectProbes.count = 0;
    map.bakedLightmap.objectProbes.probeSpacingWorld = 4.0f;
    map.bakedLightmap.objectProbes.probeHeightWorld = 1.2f;
    map.bakedLightmap.objectProbes.format = game::kSectorBakedObjectLightProbeSidecarFormat;
    Check(game::GetSectorBakedObjectLightProbeStatus(map) == game::SectorLightmapStatus::Valid,
          "current bake version source hash keeps object probe metadata valid");

    map.bakedLightmap.sourceHash = "pre-object-probe-source-hash";
    Check(game::GetSectorLightmapStatus(map) == game::SectorLightmapStatus::Stale,
          "old bake version source hash is stale after object probe bake output change");
    map.bakedLightmap.objectProbes.sourceHash = "pre-object-probe-source-hash";
    Check(game::GetSectorBakedObjectLightProbeStatus(map) == game::SectorLightmapStatus::Stale,
          "old bake version source hash is stale for object probe metadata");

    std::error_code removeError;
    std::filesystem::remove(objectProbePath, removeError);
    std::filesystem::remove(lightmapPath, removeError);
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

std::vector<game::SectorBakedObjectLightProbe> MakeObjectLightProbesForSidecarTest()
{
    std::vector<game::SectorBakedObjectLightProbe> probes(2);
    probes[0].sectorId = 10;
    probes[0].position = Vector3{1.0f, 2.0f, 3.0f};
    probes[1].sectorId = -5;
    probes[1].position = Vector3{-4.0f, 5.0f, 6.5f};

    for (size_t probeIndex = 0; probeIndex < probes.size(); ++probeIndex) {
        for (int face = 0; face < 6; ++face) {
            const float base = static_cast<float>(probeIndex * 10 + static_cast<size_t>(face));
            probes[probeIndex].ambientCube[face] = Vector3{
                    base + 0.1f,
                    base + 0.2f,
                    base + 0.3f};
        }
    }
    return probes;
}

void TestObjectLightProbeSidecarRoundTrip()
{
    const std::filesystem::path sandbox = ObjectProbePhase01aSandboxDir();
    std::filesystem::create_directories(sandbox);
    const std::filesystem::path path = sandbox / "round_trip.object_probes.bin";
    const std::vector<game::SectorBakedObjectLightProbe> probes = MakeObjectLightProbesForSidecarTest();

    std::string error;
    Check(game::WriteSectorBakedObjectLightProbeSidecar(path.string(), probes, 4.0f, 1.2f, error),
          "object light probe sidecar writes");

    game::SectorBakedObjectLightProbeMetadata expected;
    expected.version = game::kSectorBakedObjectLightProbeSidecarVersion;
    expected.count = static_cast<int>(probes.size());
    expected.format = game::kSectorBakedObjectLightProbeSidecarFormat;

    std::vector<game::SectorBakedObjectLightProbe> loaded;
    game::SectorBakedObjectLightProbeMetadata metadata;
    Check(game::ReadSectorBakedObjectLightProbeSidecar(path.string(), &expected, loaded, metadata, error),
          "object light probe sidecar reads");
    Check(metadata.path == path.string()
                  && metadata.version == game::kSectorBakedObjectLightProbeSidecarVersion
                  && metadata.count == static_cast<int>(probes.size())
                  && Near(metadata.probeSpacingWorld, 4.0f)
                  && Near(metadata.probeHeightWorld, 1.2f)
                  && metadata.format == game::kSectorBakedObjectLightProbeSidecarFormat,
          "object light probe sidecar metadata is populated");
    Check(loaded.size() == probes.size(), "object light probe sidecar preserves probe count");
    for (size_t probeIndex = 0; probeIndex < probes.size() && probeIndex < loaded.size(); ++probeIndex) {
        Check(loaded[probeIndex].sectorId == probes[probeIndex].sectorId,
              "object light probe sidecar preserves sector id");
        Check(SameVector(loaded[probeIndex].position, probes[probeIndex].position),
              "object light probe sidecar preserves position");
        for (int face = 0; face < 6; ++face) {
            Check(SameVector(loaded[probeIndex].ambientCube[face], probes[probeIndex].ambientCube[face]),
                  "object light probe sidecar preserves ambient cube face");
        }
    }
}

void TestObjectLightProbeSidecarRejectsInvalidFiles()
{
    const std::filesystem::path sandbox = ObjectProbePhase01aSandboxDir();
    std::filesystem::create_directories(sandbox);
    const std::vector<game::SectorBakedObjectLightProbe> probes = MakeObjectLightProbesForSidecarTest();

    auto writeFixture = [&](const char* name) {
        const std::filesystem::path path = sandbox / name;
        std::string error;
        Check(game::WriteSectorBakedObjectLightProbeSidecar(path.string(), probes, 4.0f, 1.2f, error),
              "object light probe invalid fixture writes");
        return path;
    };

    auto readRejected = [](const std::filesystem::path& path, const game::SectorBakedObjectLightProbeMetadata* expected) {
        std::vector<game::SectorBakedObjectLightProbe> loaded;
        game::SectorBakedObjectLightProbeMetadata metadata;
        std::string error;
        return !game::ReadSectorBakedObjectLightProbeSidecar(path.string(), expected, loaded, metadata, error)
                && !error.empty()
                && loaded.empty();
    };

    const std::filesystem::path badMagic = writeFixture("bad_magic.object_probes.bin");
    PatchByte(badMagic, 0, static_cast<unsigned char>('X'));
    Check(readRejected(badMagic, nullptr), "object light probe sidecar rejects bad magic");

    const std::filesystem::path badVersion = writeFixture("bad_version.object_probes.bin");
    PatchByte(badVersion, 4, 2);
    Check(readRejected(badVersion, nullptr), "object light probe sidecar rejects bad version");

    const std::filesystem::path truncated = writeFixture("truncated.object_probes.bin");
    TruncateFileByOneByte(truncated);
    Check(readRejected(truncated, nullptr), "object light probe sidecar rejects truncated file");

    const std::filesystem::path nonFinite = writeFixture("non_finite.object_probes.bin");
    PatchByte(nonFinite, 32, 0x00);
    PatchByte(nonFinite, 33, 0x00);
    PatchByte(nonFinite, 34, 0x80);
    PatchByte(nonFinite, 35, 0x7f);
    Check(readRejected(nonFinite, nullptr), "object light probe sidecar rejects non-finite floats");

    const std::filesystem::path mismatch = writeFixture("metadata_mismatch.object_probes.bin");
    game::SectorBakedObjectLightProbeMetadata expected;
    expected.version = game::kSectorBakedObjectLightProbeSidecarVersion;
    expected.count = static_cast<int>(probes.size() + 1);
    expected.format = game::kSectorBakedObjectLightProbeSidecarFormat;
    Check(readRejected(mismatch, &expected), "object light probe sidecar detects metadata count mismatch");

    std::vector<game::SectorBakedObjectLightProbe> invalid = probes;
    invalid[0].position.x = std::numeric_limits<float>::infinity();
    std::string error;
    Check(!game::WriteSectorBakedObjectLightProbeSidecar(
                  (sandbox / "write_non_finite.object_probes.bin").string(),
                  invalid,
                  4.0f,
                  1.2f,
                  error)
                  && !error.empty(),
          "object light probe sidecar refuses non-finite values on write");
}

void TestObjectLightProbeRuntimeDataLoadsAndBuildsSectorRanges()
{
    const std::filesystem::path sandbox = ObjectProbePhase01aSandboxDir();
    std::filesystem::create_directories(sandbox);
    const std::filesystem::path path = sandbox / "runtime_ranges.object_probes.bin";

    std::vector<game::SectorBakedObjectLightProbe> probes = MakeObjectLightProbesForSidecarTest();
    game::SectorBakedObjectLightProbe thirdProbe;
    thirdProbe.sectorId = 10;
    thirdProbe.position = Vector3{7.0f, 8.0f, 9.0f};
    for (int face = 0; face < 6; ++face) {
        thirdProbe.ambientCube[face] = Vector3{20.0f + static_cast<float>(face), 0.5f, 0.25f};
    }
    probes.push_back(thirdProbe);

    std::string error;
    Check(game::WriteSectorBakedObjectLightProbeSidecar(path.string(), probes, 4.0f, 1.2f, error),
          "runtime object probe sidecar fixture writes");

    game::SectorTopologyMap map = MakeProbeRectangle(1024, 1024);
    map.bakedLightmap.objectProbes.path = path.string();
    map.bakedLightmap.objectProbes.version = game::kSectorBakedObjectLightProbeSidecarVersion;
    map.bakedLightmap.objectProbes.sourceHash = game::ComputeSectorLightmapSourceHash(map);
    map.bakedLightmap.objectProbes.count = static_cast<int>(probes.size());
    map.bakedLightmap.objectProbes.probeSpacingWorld = 4.0f;
    map.bakedLightmap.objectProbes.probeHeightWorld = 1.2f;
    map.bakedLightmap.objectProbes.format = game::kSectorBakedObjectLightProbeSidecarFormat;

    game::SectorBakedObjectLightProbeRuntimeData runtimeData;
    Check(game::LoadSectorBakedObjectLightProbeRuntimeData(map, runtimeData, error),
          "valid runtime object probe sidecar loads");
    Check(runtimeData.probes.size() == probes.size(), "runtime object probe load preserves probe count");
    Check(runtimeData.metadata.path == path.string()
                  && runtimeData.metadata.sourceHash == map.bakedLightmap.objectProbes.sourceHash
                  && runtimeData.metadata.count == static_cast<int>(probes.size()),
          "runtime object probe load preserves metadata contract");
    Check(runtimeData.sectorRanges.size() == 2, "runtime object probe load builds one range per sector");
    Check(runtimeData.sectorRanges[0].sectorId == -5
                  && runtimeData.sectorRanges[0].begin == 0
                  && runtimeData.sectorRanges[0].count == 1,
          "runtime object probe sector range for first sorted sector is correct");
    Check(runtimeData.sectorRanges[1].sectorId == 10
                  && runtimeData.sectorRanges[1].begin == 1
                  && runtimeData.sectorRanges[1].count == 2,
          "runtime object probe sector range for repeated sector is correct");
    for (const game::SectorBakedObjectLightProbeSectorRange& range : runtimeData.sectorRanges) {
        for (int index = range.begin; index < range.begin + range.count; ++index) {
            Check(runtimeData.probes[static_cast<size_t>(index)].sectorId == range.sectorId,
                  "runtime object probe range covers matching sorted probes");
        }
    }
}

void TestObjectLightProbeRuntimeDataRejectsUnavailableInputs()
{
    const std::filesystem::path sandbox = ObjectProbePhase01aSandboxDir();
    std::filesystem::create_directories(sandbox);
    const std::filesystem::path path = sandbox / "runtime_unavailable.object_probes.bin";
    const std::vector<game::SectorBakedObjectLightProbe> probes = MakeObjectLightProbesForSidecarTest();

    std::string error;
    Check(game::WriteSectorBakedObjectLightProbeSidecar(path.string(), probes, 4.0f, 1.2f, error),
          "runtime unavailable object probe fixture writes");

    game::SectorTopologyMap map = MakeProbeRectangle(1024, 1024);
    map.bakedLightmap.objectProbes.path = path.string();
    map.bakedLightmap.objectProbes.version = game::kSectorBakedObjectLightProbeSidecarVersion;
    map.bakedLightmap.objectProbes.sourceHash = game::ComputeSectorLightmapSourceHash(map);
    map.bakedLightmap.objectProbes.count = static_cast<int>(probes.size());
    map.bakedLightmap.objectProbes.probeSpacingWorld = 4.0f;
    map.bakedLightmap.objectProbes.probeHeightWorld = 1.2f;
    map.bakedLightmap.objectProbes.format = game::kSectorBakedObjectLightProbeSidecarFormat;

    auto loadRejected = [](const game::SectorTopologyMap& candidate) {
        game::SectorBakedObjectLightProbeRuntimeData runtimeData;
        std::string loadError;
        return !game::LoadSectorBakedObjectLightProbeRuntimeData(candidate, runtimeData, loadError)
                && !loadError.empty()
                && runtimeData.probes.empty()
                && runtimeData.sectorRanges.empty();
    };

    game::SectorTopologyMap stale = map;
    stale.bakedLightmap.objectProbes.sourceHash = "stale-probe-source-hash";
    Check(loadRejected(stale), "runtime object probe load rejects stale source hash");

    game::SectorTopologyMap missing = map;
    missing.bakedLightmap.objectProbes.path = (sandbox / "missing_runtime.object_probes.bin").string();
    Check(loadRejected(missing), "runtime object probe load handles missing sidecar");

    const std::filesystem::path malformedPath = sandbox / "runtime_malformed.object_probes.bin";
    Check(game::WriteSectorBakedObjectLightProbeSidecar(malformedPath.string(), probes, 4.0f, 1.2f, error),
          "runtime malformed object probe fixture writes");
    PatchByte(malformedPath, 0, static_cast<unsigned char>('X'));
    game::SectorTopologyMap malformed = map;
    malformed.bakedLightmap.objectProbes.path = malformedPath.string();
    Check(loadRejected(malformed), "runtime object probe load rejects malformed binary safely");
}

game::SectorBakedObjectLightProbe SamplingProbe(
        int sectorId,
        Vector3 position,
        Vector3 ambient)
{
    game::SectorBakedObjectLightProbe probe;
    probe.sectorId = sectorId;
    probe.position = position;
    for (Vector3& face : probe.ambientCube) {
        face = ambient;
    }
    return probe;
}

game::SectorBakedObjectLightProbeRuntimeData MakeSamplingRuntimeData(
        std::vector<game::SectorBakedObjectLightProbe> probes)
{
    std::sort(probes.begin(), probes.end(), [](const auto& a, const auto& b) {
        return a.sectorId < b.sectorId;
    });

    game::SectorBakedObjectLightProbeRuntimeData data;
    data.probes = std::move(probes);
    for (size_t begin = 0; begin < data.probes.size();) {
        const int sectorId = data.probes[begin].sectorId;
        size_t end = begin + 1;
        while (end < data.probes.size() && data.probes[end].sectorId == sectorId) {
            ++end;
        }

        game::SectorBakedObjectLightProbeSectorRange range;
        range.sectorId = sectorId;
        range.begin = static_cast<int>(begin);
        range.count = static_cast<int>(end - begin);
        data.sectorRanges.push_back(range);
        begin = end;
    }
    return data;
}

void TestObjectLightProbeSamplingInterpolatesAndPrefersSector()
{
    game::SectorBakedObjectLightProbeRuntimeData data = MakeSamplingRuntimeData({
            SamplingProbe(10, Vector3{0.0f, 0.0f, 0.0f}, Vector3{1.0f, 0.0f, 0.0f}),
            SamplingProbe(10, Vector3{10.0f, 0.0f, 0.0f}, Vector3{0.0f, 0.0f, 1.0f}),
            SamplingProbe(20, Vector3{5.0f, 0.0f, 0.0f}, Vector3{0.0f, 1.0f, 0.0f}),
    });

    const game::BakedObjectLightingSample sameSector =
            game::SampleBakedObjectLighting(data, Vector3{5.0f, 0.0f, 0.0f}, 10, nullptr);
    Check(sameSector.valid, "object light probe sampling returns valid sample for loaded sector probes");
    Check(SameVector(sameSector.ambientCube[0], Vector3{0.5f, 0.0f, 0.5f}),
          "object light probe sampling interpolates nearest same-sector probes");

    const game::BakedObjectLightingSample preferred =
            game::SampleBakedObjectLighting(data, Vector3{5.0f, 0.0f, 0.0f}, 20, nullptr);
    Check(preferred.valid, "object light probe sampling returns valid exact preferred-sector sample");
    Check(SameVector(preferred.ambientCube[0], Vector3{0.0f, 1.0f, 0.0f}),
          "object light probe sampling prefers same-sector probes over nearer or coincident other-sector probes");

    const game::BakedObjectLightingSample anyProbe =
            game::SampleBakedObjectLighting(data, Vector3{10.0f, 0.0f, 0.0f}, 999, nullptr);
    Check(anyProbe.valid, "object light probe sampling falls back to any loaded probe when preferred sector has no probes");
    Check(SameVector(anyProbe.ambientCube[0], Vector3{0.0f, 0.0f, 1.0f}),
          "object light probe sampling any-probe fallback is deterministic nearest-probe lighting");
}

void TestObjectLightProbeSamplingFallbacksAndFiniteOutput()
{
    const game::SectorBakedObjectLightProbeRuntimeData emptyData;
    game::SectorTopologyMap map = MakeProbeRectangle(1024, 1024);
    game::SectorTopologySector* sector = game::FindSectorTopologySector(map, 10);
    Check(sector != nullptr, "object light probe sampling fallback test sector exists");
    if (sector != nullptr) {
        sector->ambientColor = Color{64, 128, 255, 255};
        sector->ambientIntensity = 0.5f;
    }

    const game::BakedObjectLightingSample sectorAmbient =
            game::SampleBakedObjectLighting(emptyData, Vector3{}, 10, &map);
    Check(!sectorAmbient.valid, "object light probe sampling sector-ambient fallback is marked fallback");
    Check(SameVector(sectorAmbient.ambientCube[0], Vector3{0.125490f, 0.250980f, 0.5f}),
          "object light probe sampling falls back to sector ambient when map and sector are available");

    const game::BakedObjectLightingSample neutral =
            game::SampleBakedObjectLighting(emptyData, Vector3{}, 999, nullptr);
    Check(!neutral.valid, "object light probe sampling neutral result is marked fallback");
    for (int face = 0; face < 6; ++face) {
        Check(SameVector(neutral.ambientCube[face], Vector3{0.15f, 0.15f, 0.15f}),
              "object light probe sampling neutral fallback uses dim neutral cube");
        Check(FiniteVector(sectorAmbient.ambientCube[face]) && FiniteVector(neutral.ambientCube[face]),
              "object light probe sampling fallback outputs are finite");
    }
}

void TestObjectLightProbeBakeWritesSidecarAndStats()
{
    const std::filesystem::path sandbox = ObjectProbePhase01aSandboxDir();
    std::filesystem::create_directories(sandbox);
    const std::filesystem::path outputPath = sandbox / "phase_03b_success.lightmap.png";
    const std::filesystem::path sidecarPath =
            game::MakeSectorObjectProbeSidecarPathForLightmapPath(outputPath.string());
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);
    std::filesystem::remove(sidecarPath, removeError);

    game::SectorTopologyMap map = MakeProbeRectangle(1024, 1024);
    map.lightmapSettings.objectProbeSpacingWorld = 4.0f;
    map.lightmapSettings.objectProbeHeightWorld = 1.2f;

    game::SectorLightmapLayout layout;
    std::string error;
    Check(game::BuildSectorLightmapLayout(map, layout, error), "phase 3b lightmap layout builds");

    game::SectorLightmapBakeResult result;
    Check(game::BakeSectorLightmap(map, layout, outputPath.string().c_str(), result, error),
          "phase 3b bake writes atlas and object probe sidecar");
    Check(std::filesystem::exists(outputPath), "phase 3b bake writes atlas file");
    Check(std::filesystem::exists(sidecarPath), "phase 3b bake writes object probe sidecar file");
    Check(result.objectProbes.path == sidecarPath.string(),
          "phase 3b bake result reports object probe sidecar path");
    Check(result.objectProbes.version == game::kSectorBakedObjectLightProbeSidecarVersion
                  && result.objectProbes.sourceHash == result.sourceHash
                  && result.objectProbes.count > 0
                  && Near(result.objectProbes.probeSpacingWorld, 4.0f)
                  && Near(result.objectProbes.probeHeightWorld, 1.2f)
                  && result.objectProbes.format == game::kSectorBakedObjectLightProbeSidecarFormat,
          "phase 3b bake result reports compact object probe metadata");
    Check(result.objectProbeBakeSeconds >= 0.0 && result.objectProbeSidecarWriteSeconds >= 0.0,
          "phase 3b bake result reports object probe timings");

    std::vector<game::SectorBakedObjectLightProbe> loaded;
    game::SectorBakedObjectLightProbeMetadata metadata;
    Check(game::ReadSectorBakedObjectLightProbeSidecar(
                  sidecarPath.string(),
                  &result.objectProbes,
                  loaded,
                  metadata,
                  error),
          "phase 3b written object probe sidecar reads with result metadata");
    Check(static_cast<int>(loaded.size()) == result.objectProbes.count
                  && metadata.count == result.objectProbes.count,
          "phase 3b sidecar probe count matches metadata");

    const std::string report = game::FormatSectorLightmapBakeReport(result);
    Check(report.find("Object light probes:") != std::string::npos
                  && report.find("Object probe sidecar:") != std::string::npos
                  && report.find("Object probe bake:") != std::string::npos,
          "phase 3b bake report includes object probe stats");

    std::filesystem::remove(outputPath, removeError);
    std::filesystem::remove(sidecarPath, removeError);
}

void TestObjectLightProbeBakeCancellationDoesNotMarkValid()
{
    const std::filesystem::path sandbox = ObjectProbePhase01aSandboxDir();
    std::filesystem::create_directories(sandbox);
    const std::filesystem::path outputPath = sandbox / "phase_03b_cancelled.lightmap.png";
    const std::filesystem::path sidecarPath =
            game::MakeSectorObjectProbeSidecarPathForLightmapPath(outputPath.string());
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);
    std::filesystem::remove(sidecarPath, removeError);

    const game::SectorTopologyMap map = MakeProbeRectangle(1024, 1024);
    game::SectorLightmapLayout layout;
    std::string error;
    Check(game::BuildSectorLightmapLayout(map, layout, error), "phase 3b cancellation layout builds");

    game::SectorLightmapBakeCallbacks callbacks;
    callbacks.isCancellationRequested = []() { return true; };

    game::SectorLightmapBakeResult result;
    Check(!game::BakeSectorLightmap(map, layout, outputPath.string().c_str(), callbacks, result, error),
          "phase 3b cancelled bake fails without installing probe metadata");
    Check(result.objectProbes.path.empty() && result.objectProbes.count == 0,
          "phase 3b cancelled bake leaves object probe metadata empty");
    Check(!std::filesystem::exists(sidecarPath),
          "phase 3b cancelled bake does not leave an object probe sidecar");

    std::filesystem::remove(outputPath, removeError);
    std::filesystem::remove(sidecarPath, removeError);
}

std::vector<game::SectorBakedObjectLightProbe> BuildObjectProbePlacementsForTest(
        const game::SectorTopologyMap& map,
        std::vector<game::SectorBakedObjectLightProbePlacementDiagnostic>* diagnostics = nullptr)
{
    std::vector<game::SectorBakedObjectLightProbe> probes;
    std::string error;
    const game::SectorBakedObjectLightProbePlacementSettings settings;
    Check(game::BuildSectorBakedObjectLightProbePlacements(map, settings, probes, diagnostics, error),
          "object light probe placement builds");
    Check(error.empty(), "object light probe placement has no error on success");
    return probes;
}

int CountProbesForSector(const std::vector<game::SectorBakedObjectLightProbe>& probes, int sectorId)
{
    int count = 0;
    for (const game::SectorBakedObjectLightProbe& probe : probes) {
        if (probe.sectorId == sectorId) {
            ++count;
        }
    }
    return count;
}

bool HasProbeNear(
        const std::vector<game::SectorBakedObjectLightProbe>& probes,
        int sectorId,
        float x,
        float z)
{
    for (const game::SectorBakedObjectLightProbe& probe : probes) {
        if (probe.sectorId == sectorId && Near(probe.position.x, x) && Near(probe.position.z, z)) {
            return true;
        }
    }
    return false;
}

void TestObjectLightProbePlacementGridCounts()
{
    const std::vector<game::SectorBakedObjectLightProbe> corridor =
            BuildObjectProbePlacementsForTest(MakeProbeRectangle(2048, 512));
    Check(CountProbesForSector(corridor, 10) == 4,
          "long corridor receives multiple object light probes");

    const std::vector<game::SectorBakedObjectLightProbe> room =
            BuildObjectProbePlacementsForTest(MakeProbeRectangle(1024, 1024));
    Check(CountProbesForSector(room, 10) == 4,
          "large room receives multiple object light probes");
    Check(HasProbeNear(room, 10, 2.0f, 2.0f)
                  && HasProbeNear(room, 10, 6.0f, 2.0f)
                  && HasProbeNear(room, 10, 2.0f, 6.0f)
                  && HasProbeNear(room, 10, 6.0f, 6.0f),
          "object light probe placement converts topology coordinates to world positions");
    for (const game::SectorBakedObjectLightProbe& probe : room) {
        Check(Near(probe.position.y, 1.2f), "object light probe uses floor plus default torso height");
    }
}

void TestObjectLightProbePlacementRejectsConcaveVoid()
{
    const std::vector<game::SectorBakedObjectLightProbe> probes =
            BuildObjectProbePlacementsForTest(MakeProbeConcaveSector());
    Check(CountProbesForSector(probes, 10) == 3,
          "concave sector object light probe placement keeps only interior grid points");
    Check(!HasProbeNear(probes, 10, 6.0f, 6.0f),
          "concave sector object light probe placement rejects AABB points outside the polygon");
}

void TestObjectLightProbePlacementRejectsHoles()
{
    const std::vector<game::SectorBakedObjectLightProbe> probes =
            BuildObjectProbePlacementsForTest(MakeProbeHoleSector());
    Check(!HasProbeNear(probes, 10, 6.0f, 6.0f),
          "object light probe placement rejects parent-sector hole points");
    Check(HasProbeNear(probes, 20, 6.0f, 6.0f),
          "object light probe placement still places probes in the sector inside the hole");
}

void TestObjectLightProbePlacementFallbackAndLowCeiling()
{
    game::SectorTopologyMap small = MakeSquare();
    game::FindSectorTopologySector(small, 10)->ceilingZ = game::SectorWorldToAuthoringDistance(0.8f);

    std::vector<game::SectorBakedObjectLightProbePlacementDiagnostic> diagnostics;
    const std::vector<game::SectorBakedObjectLightProbe> probes =
            BuildObjectProbePlacementsForTest(small, &diagnostics);
    Check(CountProbesForSector(probes, 10) == 1,
          "small sector receives one fallback object light probe");
    Check(!probes.empty() && Near(probes.front().position.y, 0.4f),
          "low ceiling object light probe height is clamped to sector midpoint");

    bool sawFallback = false;
    bool sawHeightClamp = false;
    for (const game::SectorBakedObjectLightProbePlacementDiagnostic& diagnostic : diagnostics) {
        sawFallback = sawFallback || diagnostic.message.find("fallback") != std::string::npos;
        sawHeightClamp = sawHeightClamp || diagnostic.message.find("clamped") != std::string::npos;
    }
    Check(sawFallback, "object light probe placement records small-sector fallback diagnostic");
    Check(sawHeightClamp, "object light probe placement records low-ceiling height diagnostic");
}

game::SectorBakedObjectLightProbe MakeProbeAt(Vector3 position, int sectorId = 10)
{
    game::SectorBakedObjectLightProbe probe;
    probe.sectorId = sectorId;
    probe.position = position;
    return probe;
}

game::SectorTopologyMap MakeObjectProbeLightingMap()
{
    game::SectorTopologyMap map = MakeProbeRectangle(1024, 1024);
    map.staticLights.clear();
    map.staticSpotLights.clear();
    map.directionalLight.enabled = false;
    for (game::SectorTopologySector& sector : map.sectors) {
        sector.ambientIntensity = 0.0f;
    }
    return map;
}

std::vector<game::SectorBakedObjectLightProbe> BakeObjectProbeLighting(
        game::SectorTopologyMap map,
        std::vector<game::SectorBakedObjectLightProbe> probes)
{
    std::string error;
    Check(game::BakeSectorBakedObjectLightProbeAmbientCubes(map, probes, error),
          "object light probe ambient cube bake succeeds");
    Check(error.empty(), "object light probe ambient cube bake has no error on success");
    return probes;
}

void TestObjectLightProbePointAndDirectionalLighting()
{
    game::SectorTopologyMap pointMap = MakeObjectProbeLightingMap();
    pointMap.staticLights.push_back(game::SectorTopologyStaticPointLight{
            200,
            WorldToAuthoring(Vector3{6.0f, 1.2f, 4.0f}),
            Color{255, 64, 32, 255},
            2.0f,
            game::SectorWorldToAuthoringDistance(6.0f),
            0.0f
    });
    const std::vector<game::SectorBakedObjectLightProbe> pointProbes =
            BakeObjectProbeLighting(pointMap, {MakeProbeAt(Vector3{4.0f, 1.2f, 4.0f})});
    Check(!pointProbes.empty() && Brightness(pointProbes.front().ambientCube[0]) > 0.05f,
          "static point light contributes to facing object probe cube side");
    Check(Brightness(pointProbes.front().ambientCube[0])
                  > Brightness(pointProbes.front().ambientCube[1]) + 0.05f,
          "static point light is strongest on the object probe side facing the light");

    game::SectorTopologyMap directionalMap = MakeObjectProbeLightingMap();
    directionalMap.sectors[0].ceilingSky = true;
    directionalMap.directionalLight.enabled = true;
    directionalMap.directionalLight.directionToLight = Vector3{0.0f, 1.0f, 0.0f};
    directionalMap.directionalLight.color = Color{64, 128, 255, 255};
    directionalMap.directionalLight.intensity = 0.75f;
    const std::vector<game::SectorBakedObjectLightProbe> directionalProbes =
            BakeObjectProbeLighting(directionalMap, {MakeProbeAt(Vector3{4.0f, 1.2f, 4.0f})});
    Check(Brightness(directionalProbes.front().ambientCube[2]) > 0.05f,
          "static directional light contributes to object probes when unoccluded");
    Check(Brightness(directionalProbes.front().ambientCube[2])
                  > Brightness(directionalProbes.front().ambientCube[3]) + 0.05f,
          "static directional light follows ambient cube face direction");
}

void TestObjectLightProbeSpotlightCone()
{
    game::SectorTopologyMap insideCone = MakeObjectProbeLightingMap();
    insideCone.staticSpotLights.push_back(game::SectorTopologyStaticSpotLight{
            201,
            WorldToAuthoring(Vector3{6.0f, 1.2f, 4.0f}),
            WorldToAuthoring(Vector3{4.0f, 1.2f, 4.0f}),
            WHITE,
            4.0f,
            game::SectorWorldToAuthoringDistance(8.0f),
            12.0f,
            24.0f,
            0.0f
    });
    const std::vector<game::SectorBakedObjectLightProbe> lit =
            BakeObjectProbeLighting(insideCone, {MakeProbeAt(Vector3{4.0f, 1.2f, 4.0f})});

    game::SectorTopologyMap outsideCone = MakeObjectProbeLightingMap();
    outsideCone.staticSpotLights.push_back(game::SectorTopologyStaticSpotLight{
            202,
            WorldToAuthoring(Vector3{6.0f, 1.2f, 4.0f}),
            WorldToAuthoring(Vector3{8.0f, 1.2f, 4.0f}),
            WHITE,
            4.0f,
            game::SectorWorldToAuthoringDistance(8.0f),
            12.0f,
            24.0f,
            0.0f
    });
    const std::vector<game::SectorBakedObjectLightProbe> unlit =
            BakeObjectProbeLighting(outsideCone, {MakeProbeAt(Vector3{4.0f, 1.2f, 4.0f})});

    Check(Brightness(lit.front().ambientCube[0]) > Brightness(unlit.front().ambientCube[0]) + 0.2f,
          "static spotlight cone affects object probe cube contribution");
    Check(Brightness(lit.front().ambientCube[0]) > Brightness(lit.front().ambientCube[1]) + 0.2f,
          "static spotlight contribution follows object probe face direction");
}

void TestObjectLightProbeOcclusionAndAlphaOcclusion()
{
    game::SectorTopologyMap solidWall = MakeObjectProbeLightingMap();
    solidWall.staticLights.push_back(game::SectorTopologyStaticPointLight{
            203,
            WorldToAuthoring(Vector3{10.0f, 1.2f, 4.0f}),
            WHITE,
            8.0f,
            game::SectorWorldToAuthoringDistance(8.0f),
            0.0f
    });
    const std::vector<game::SectorBakedObjectLightProbe> blocked =
            BakeObjectProbeLighting(solidWall, {MakeProbeAt(Vector3{6.0f, 1.2f, 4.0f})});
    Check(Brightness(blocked.front().ambientCube[0]) < 0.01f,
          "solid sector wall blocks object probe direct point light contribution");

    const std::filesystem::path transparentPath = Phase01bSandboxDir() / "phase03a_probe_transparent.png";
    const std::filesystem::path opaquePath = Phase01bSandboxDir() / "phase03a_probe_opaque.png";
    WriteSolidAlphaTestTexture(transparentPath, 0);
    WriteSolidAlphaTestTexture(opaquePath, 255);

    auto makeAlphaProbeMap = [](const std::filesystem::path& texturePath) {
        game::SectorTopologyMap map = MakeAlphaMiddleOcclusionBakeMap(texturePath);
        map.staticLights.clear();
        map.staticSpotLights.clear();
        map.directionalLight.enabled = false;
        for (game::SectorTopologySector& sector : map.sectors) {
            sector.ambientIntensity = 0.0f;
        }
        map.staticLights.push_back(game::SectorTopologyStaticPointLight{
                204,
                WorldToAuthoring(Vector3{0.75f, 0.25f, 0.25f}),
                WHITE,
                8.0f,
                game::SectorWorldToAuthoringDistance(2.0f),
                0.0f
        });
        return map;
    };

    const std::vector<game::SectorBakedObjectLightProbe> transparent =
            BakeObjectProbeLighting(
                    makeAlphaProbeMap(transparentPath),
                    {MakeProbeAt(Vector3{0.25f, 0.25f, 0.25f})});
    const std::vector<game::SectorBakedObjectLightProbe> opaque =
            BakeObjectProbeLighting(
                    makeAlphaProbeMap(opaquePath),
                    {MakeProbeAt(Vector3{0.25f, 0.25f, 0.25f})});
    Check(Brightness(transparent.front().ambientCube[0])
                  > Brightness(opaque.front().ambientCube[0]) + 0.2f,
          "alpha-tested transparent middle texels let object probe direct lighting pass");
}

void TestObjectLightProbeAmbientAndDegenerateFiniteOutput()
{
    game::SectorTopologyMap map = MakeObjectProbeLightingMap();
    game::SectorTopologySector* sector = game::FindSectorTopologySector(map, 10);
    Check(sector != nullptr, "object probe ambient test sector exists");
    if (sector != nullptr) {
        sector->ambientColor = Color{64, 128, 255, 255};
        sector->ambientIntensity = 0.5f;
    }
    map.staticLights.push_back(game::SectorTopologyStaticPointLight{
            205,
            WorldToAuthoring(Vector3{4.0f, 1.2f, 4.0f}),
            WHITE,
            8.0f,
            game::SectorWorldToAuthoringDistance(8.0f),
            0.0f
    });
    map.staticSpotLights.push_back(game::SectorTopologyStaticSpotLight{
            206,
            WorldToAuthoring(Vector3{4.0f, 1.2f, 4.0f}),
            WorldToAuthoring(Vector3{4.0f, 1.2f, 4.0f}),
            WHITE,
            8.0f,
            game::SectorWorldToAuthoringDistance(8.0f),
            12.0f,
            24.0f,
            0.0f
    });

    const std::vector<game::SectorBakedObjectLightProbe> probes =
            BakeObjectProbeLighting(map, {MakeProbeAt(Vector3{4.0f, 1.2f, 4.0f})});
    Check(!probes.empty(), "object probe ambient and degenerate test produced a probe");
    for (int face = 0; face < 6 && !probes.empty(); ++face) {
        Check(FiniteVector(probes.front().ambientCube[face]),
              "object probe degenerate direct light cases produce finite ambient cube output");
        Check(probes.front().ambientCube[face].x > 0.12f
                      && probes.front().ambientCube[face].y > 0.24f
                      && probes.front().ambientCube[face].z > 0.49f,
              "sector ambient baseline appears on every object probe cube face");
    }
}

} // namespace

int main()
{
    TestObjectLightProbeSidecarRoundTrip();
    TestObjectLightProbeSidecarRejectsInvalidFiles();
    TestObjectLightProbeRuntimeDataLoadsAndBuildsSectorRanges();
    TestObjectLightProbeRuntimeDataRejectsUnavailableInputs();
    TestObjectLightProbeSamplingInterpolatesAndPrefersSector();
    TestObjectLightProbeSamplingFallbacksAndFiniteOutput();
    TestObjectLightProbeBakeWritesSidecarAndStats();
    TestObjectLightProbeBakeCancellationDoesNotMarkValid();
    TestObjectLightProbePlacementGridCounts();
    TestObjectLightProbePlacementRejectsConcaveVoid();
    TestObjectLightProbePlacementRejectsHoles();
    TestObjectLightProbePlacementFallbackAndLowCeiling();
    TestObjectLightProbePointAndDirectionalLighting();
    TestObjectLightProbeSpotlightCone();
    TestObjectLightProbeOcclusionAndAlphaOcclusion();
    TestObjectLightProbeAmbientAndDegenerateFiniteOutput();
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
