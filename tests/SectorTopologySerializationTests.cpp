#include "sector_demo/SectorTopologySerialization.h"
#include "util/json.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <cmath>
#include <limits>
#include <string>

namespace {

using game::SectorTextureDefinition;
using game::SectorTextureFilter;
using game::SectorTopologyLineDef;
using game::SectorTopologyLoopSet;
using game::SectorTopologyMap;
using game::SectorTopologySector;
using game::SectorTopologySideDef;
using game::SectorTopologySideKind;
using game::SectorTopologyStaticPointLight;
using game::SectorTopologyVertex;
using Json = nlohmann::ordered_json;

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        ++failures;
    }
}

game::SectorTopologyWallPartSettings MakePart(
        const char* textureId,
        float scaleX,
        float scaleY,
        float offsetX,
        float offsetY)
{
    game::SectorTopologyWallPartSettings part;
    part.textureId = textureId;
    part.uv.scale = {scaleX, scaleY};
    part.uv.offset = {offsetX, offsetY};
    return part;
}

game::SectorTopologyDecalLayer MakeDecal(
        const char* textureId,
        float scaleX,
        float scaleY,
        float offsetX,
        float offsetY,
        float opacity,
        bool emissive = false,
        Vector3 tint = {1.0f, 1.0f, 1.0f},
        float bloomIntensity = 1.0f)
{
    game::SectorTopologyDecalLayer decal;
    decal.textureId = textureId;
    decal.uv.scale = {scaleX, scaleY};
    decal.uv.offset = {offsetX, offsetY};
    decal.opacity = opacity;
    decal.emissive = emissive;
    decal.tint = tint;
    decal.bloomIntensity = bloomIntensity;
    return decal;
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

SectorTopologyMap MakeSquare()
{
    SectorTopologyMap map;
    map.texturesById.emplace("wall", SectorTextureDefinition{
            "wall", "textures/wall.png", SectorTextureFilter::Point});
    map.texturesById.emplace("floor", SectorTextureDefinition{
            "floor", "textures/floor.png", SectorTextureFilter::Bilinear});
    map.texturesById.emplace("ceiling", SectorTextureDefinition{
            "ceiling", "textures/ceiling.png", SectorTextureFilter::Bilinear});

    map.vertices = {
            {1, 0, 0},
            {2, 64, 0},
            {3, 64, 64},
            {4, 0, 64}
    };
    map.lineDefs = {
            {1, 1, 2, 1, -1},
            {2, 2, 3, 2, -1},
            {3, 3, 4, 3, -1},
            {4, 4, 1, 4, -1}
    };
    for (int i = 1; i <= 4; ++i) {
        SectorTopologySideDef sideDef;
        sideDef.id = i;
        sideDef.lineDefId = i;
        sideDef.side = SectorTopologySideKind::Front;
        sideDef.sectorId = 1;
        sideDef.wall = MakePart("wall", 1.0f + i, 2.0f, 0.25f * i, 0.5f);
        sideDef.lower = MakePart("wall", 0.5f, 0.75f, 1.0f, 2.0f);
        sideDef.upper = MakePart("wall", 3.0f, 4.0f, 5.0f, 6.0f);
        map.sideDefs.push_back(sideDef);
    }

    SectorTopologySector sector;
    sector.id = 1;
    sector.name = "sector_001";
    sector.floorZ = -2.5f;
    sector.ceilingZ = 24.25f;
    sector.floorTextureId = "floor";
    sector.ceilingTextureId = "ceiling";
    sector.floorUv.scale = {2.0f, 3.0f};
    sector.floorUv.offset = {4.0f, 5.0f};
    sector.ceilingUv.scale = {6.0f, 7.0f};
    sector.ceilingUv.offset = {8.0f, 9.0f};
    sector.ambientColor = Color{10, 20, 30, 40};
    sector.ambientIntensity = 0.625f;
    sector.defaultWall = MakePart("wall", 1.0f, 2.0f, 3.0f, 4.0f);
    sector.defaultLower = MakePart("wall", 5.0f, 6.0f, 7.0f, 8.0f);
    sector.defaultUpper = MakePart("wall", 9.0f, 10.0f, 11.0f, 12.0f);
    map.sectors.push_back(sector);
    return map;
}

SectorTopologyMap MakeAdjacentSquares()
{
    SectorTopologyMap map;
    map.texturesById.emplace("wall", SectorTextureDefinition{
            "wall", "textures/wall.png", SectorTextureFilter::Point});
    map.texturesById.emplace("front_wall", SectorTextureDefinition{
            "front_wall", "textures/front.png", SectorTextureFilter::Point});
    map.texturesById.emplace("back_wall", SectorTextureDefinition{
            "back_wall", "textures/back.png", SectorTextureFilter::Point});
    map.texturesById.emplace("floor", SectorTextureDefinition{
            "floor", "textures/floor.png", SectorTextureFilter::Bilinear});
    map.texturesById.emplace("ceiling", SectorTextureDefinition{
            "ceiling", "textures/ceiling.png", SectorTextureFilter::Bilinear});

    map.vertices = {
            {1, 0, 0},
            {2, 64, 0},
            {3, 64, 64},
            {4, 0, 64},
            {5, 128, 0},
            {6, 128, 64}
    };
    map.lineDefs = {
            {1, 1, 2, 1, -1},
            {2, 2, 3, 2, 8},
            {3, 3, 4, 3, -1},
            {4, 4, 1, 4, -1},
            {5, 2, 5, 5, -1},
            {6, 5, 6, 6, -1},
            {7, 6, 3, 7, -1}
    };

    const auto addSide = [&map](int id, int lineId, SectorTopologySideKind side, int sectorId) {
        SectorTopologySideDef sideDef;
        sideDef.id = id;
        sideDef.lineDefId = lineId;
        sideDef.side = side;
        sideDef.sectorId = sectorId;
        sideDef.wall = MakePart("wall", 1.0f, 1.0f, 0.0f, 0.0f);
        sideDef.lower = MakePart("wall", 1.0f, 1.0f, 0.0f, 0.0f);
        sideDef.upper = MakePart("wall", 1.0f, 1.0f, 0.0f, 0.0f);
        map.sideDefs.push_back(sideDef);
    };
    addSide(1, 1, SectorTopologySideKind::Front, 1);
    addSide(2, 2, SectorTopologySideKind::Front, 1);
    addSide(3, 3, SectorTopologySideKind::Front, 1);
    addSide(4, 4, SectorTopologySideKind::Front, 1);
    addSide(5, 5, SectorTopologySideKind::Front, 2);
    addSide(6, 6, SectorTopologySideKind::Front, 2);
    addSide(7, 7, SectorTopologySideKind::Front, 2);
    addSide(8, 2, SectorTopologySideKind::Back, 2);

    SectorTopologySector left;
    left.id = 1;
    left.name = "left";
    left.floorTextureId = "floor";
    left.ceilingTextureId = "ceiling";
    left.defaultWall = MakePart("wall", 1.0f, 1.0f, 0.0f, 0.0f);
    left.defaultLower = MakePart("wall", 1.0f, 1.0f, 0.0f, 0.0f);
    left.defaultUpper = MakePart("wall", 1.0f, 1.0f, 0.0f, 0.0f);
    map.sectors.push_back(left);

    SectorTopologySector right = left;
    right.id = 2;
    right.name = "right";
    map.sectors.push_back(right);
    return map;
}

bool LoadText(const std::string& text, SectorTopologyMap& map, std::string& error)
{
    error.clear();
    return game::LoadSectorTopologyMapFromJsonString(text, map, &error);
}

std::string SaveText(const SectorTopologyMap& map)
{
    std::string text;
    std::string error;
    Check(game::SaveSectorTopologyMapToJsonString(map, text, &error), "test map serializes");
    Check(error.empty(), "successful serialization clears error");
    return text;
}

void ExpectRejected(const Json& json, const char* description)
{
    SectorTopologyMap output = MakeSquare();
    std::string error;
    Check(!LoadText(json.dump(), output, error), description);
    Check(!error.empty(), "rejected JSON reports an error");
}

void ExpectRejectedText(const std::string& text, const char* description)
{
    SectorTopologyMap output = MakeSquare();
    std::string error;
    Check(!LoadText(text, output, error), description);
    Check(!error.empty(), "rejected JSON text reports an error");
}

void TestRoundTrip()
{
    const SectorTopologyMap original = MakeSquare();
    const std::string text = SaveText(original);
    const Json saved = Json::parse(text);
    Check(saved["vertices"][1]["x"].is_number_integer(), "vertex x is written as an integer");
    Check(saved["vertices"][2]["y"].get<int>() == 64, "integer coordinate is exact");

    SectorTopologyMap loaded;
    std::string error = "stale";
    Check(LoadText(text, loaded, error), "serialized topology loads");
    Check(error.empty(), "successful load clears error");
    Check(loaded.vertices.size() == 4 && loaded.vertices[2].x == 64,
          "vertex coordinates round-trip without drift");
    Check(loaded.sideDefs[2].wall.uv.offset.x == 0.75f,
          "concrete sidedef settings round-trip");
    Check(loaded.sectors[0].ambientColor.a == 40,
          "ambient color alpha round-trips");
    Check(loaded.sectors[0].defaultUpper.uv.offset.y == 12.0f,
          "sector defaults round-trip");
    Check(loaded.texturesById.at("wall").filter == SectorTextureFilter::Point,
          "texture definition round-trips");
    Check(loaded.texturesById.at("floor").filter == SectorTextureFilter::Bilinear,
          "linear texture definition round-trips");
    Check(!game::HasSectorTopologyValidationErrors(game::ValidateSectorTopologyMap(loaded)),
          "round-tripped map validates");
    SectorTopologyLoopSet loops;
    Check(game::ExtractSectorTopologyLoops(loaded, 1, loops)
                  && loops.outer.signedAreaTwice > 0,
          "round-tripped map extracts a CCW loop");
}

void TestTextureFilterSerialization()
{
    SectorTopologyMap map = MakeSquare();
    map.texturesById.emplace("tri", SectorTextureDefinition{
            "tri", "textures/tri.png", SectorTextureFilter::Trilinear});
    map.texturesById.emplace("aniso", SectorTextureDefinition{
            "aniso", "textures/aniso.png", SectorTextureFilter::Anisotropic8x});

    const Json saved = Json::parse(SaveText(map));
    Check(saved["textures"]["wall"]["filter"] == "point",
          "point texture filter serializes");
    Check(saved["textures"]["floor"]["filter"] == "linear",
          "linear texture filter serializes");
    Check(saved["textures"]["tri"]["filter"] == "trilinear",
          "trilinear texture filter serializes");
    Check(saved["textures"]["aniso"]["filter"] == "anisotropic8x",
          "anisotropic texture filter serializes");

    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(saved.dump(), loaded, error), "all texture filter presets load");
    Check(loaded.texturesById.at("wall").filter == SectorTextureFilter::Point,
          "point texture filter loads");
    Check(loaded.texturesById.at("floor").filter == SectorTextureFilter::Bilinear,
          "linear texture filter loads");
    Check(loaded.texturesById.at("tri").filter == SectorTextureFilter::Trilinear,
          "trilinear texture filter loads");
    Check(loaded.texturesById.at("aniso").filter == SectorTextureFilter::Anisotropic8x,
          "anisotropic texture filter loads");

    Json legacy = saved;
    legacy["textures"]["floor"]["filter"] = "bilinear";
    Check(LoadText(legacy.dump(), loaded, error), "legacy bilinear texture filter loads");
    Check(loaded.texturesById.at("floor").filter == SectorTextureFilter::Anisotropic8x,
          "legacy bilinear upgrades to anisotropic filtering");
}

void TestCeilingSkySerialization()
{
    SectorTopologyMap original = MakeSquare();
    original.sectors[0].ceilingSky = true;

    const Json saved = Json::parse(SaveText(original));
    Check(saved["sectors"][0]["ceilingSky"] == true, "ceilingSky true is serialized");

    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(saved.dump(), loaded, error), "ceilingSky true JSON loads");
    Check(loaded.sectors[0].ceilingSky, "ceilingSky true round-trips");

    Json explicitFalse = saved;
    explicitFalse["sectors"][0]["ceilingSky"] = false;
    Check(LoadText(explicitFalse.dump(), loaded, error), "ceilingSky false JSON loads");
    Check(!loaded.sectors[0].ceilingSky, "ceilingSky false remains false");

    Json missing = saved;
    missing["sectors"][0].erase("ceilingSky");
    Check(LoadText(missing.dump(), loaded, error), "missing ceilingSky JSON loads");
    Check(!loaded.sectors[0].ceilingSky, "missing ceilingSky loads false");

    SectorTopologyMap defaultFalse = MakeSquare();
    const Json defaultSaved = Json::parse(SaveText(defaultFalse));
    Check(defaultSaved["sectors"][0].find("ceilingSky") == defaultSaved["sectors"][0].end(),
          "default false ceilingSky is omitted");

    Json invalid = saved;
    invalid["sectors"][0]["ceilingSky"] = "yes";
    ExpectRejected(invalid, "non-boolean ceilingSky is rejected");
}

void TestStaticLightRoundTrip()
{
    SectorTopologyMap original = MakeSquare();
    original.staticLights.push_back(SectorTopologyStaticPointLight{
            7,
            Vector3{32.5f, 14.0f, -9.25f},
            Color{255, 220, 180, 255},
            2.5f,
            64.0f,
            2.0f
    });
    original.staticLights.push_back(SectorTopologyStaticPointLight{
            3,
            Vector3{-4.0f, 8.0f, 12.0f},
            Color{40, 80, 120, 255},
            0.75f,
            16.0f,
            0.0f
    });

    const std::string text = SaveText(original);
    const Json saved = Json::parse(text);
    Check(saved["staticLights"].is_array(), "static light array is written");
    Check(saved["staticLights"][0]["id"].get<int>() == 3
                  && saved["staticLights"][1]["id"].get<int>() == 7,
          "static lights serialize sorted by stable ID");
    Check(saved["staticLights"][1]["position"][0].get<float>() == 32.5f
                  && saved["staticLights"][1]["position"][1].get<float>() == 14.0f
                  && saved["staticLights"][1]["position"][2].get<float>() == -9.25f,
          "static light position is saved in authoring coordinates");

    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(text, loaded, error), "static light topology JSON loads");
    Check(loaded.staticLights.size() == 2, "static lights round-trip");
    const SectorTopologyStaticPointLight* light = game::FindSectorTopologyStaticLight(loaded, 7);
    Check(light != nullptr, "round-tripped light can be found by stable ID");
    if (light != nullptr) {
        Check(std::fabs(light->position.x - 32.5f) <= 0.0001f
                      && std::fabs(light->position.y - 14.0f) <= 0.0001f
                      && std::fabs(light->position.z + 9.25f) <= 0.0001f,
              "round-tripped light preserves authored position values");
        Check(light->color.r == 255 && light->color.g == 220 && light->color.b == 180,
              "round-tripped light preserves color");
        Check(std::fabs(light->intensity - 2.5f) <= 0.0001f
                      && std::fabs(light->radius - 64.0f) <= 0.0001f
                      && std::fabs(light->sourceRadius - 2.0f) <= 0.0001f,
              "round-tripped light preserves numeric properties");
    }
    Check(!game::HasSectorTopologyValidationErrors(game::ValidateSectorTopologyMap(loaded)),
          "topology with static lights validates");

    Json withoutLights = saved;
    withoutLights.erase("staticLights");
    SectorTopologyMap oldStyle;
    Check(LoadText(withoutLights.dump(), oldStyle, error), "omitted staticLights field is accepted");
    Check(oldStyle.staticLights.empty(), "omitted staticLights field loads empty");
}

void TestLightmapMetadataRoundTrip()
{
    SectorTopologyMap original = MakeSquare();
    original.lightmapSettings.ambientOcclusionRadius = 3.5f;
    original.lightmapSettings.ambientOcclusionStrength = 0.25f;
    original.lightmapSettings.indirectBounceRadius = 9.0f;
    original.lightmapSettings.indirectBounceStrength = 0.35f;
    original.bakedLightmap.path = "assets/levels/test/test.lightmap.png";
    original.bakedLightmap.width = 2048;
    original.bakedLightmap.height = 2048;
    original.bakedLightmap.sourceHash = "abc123";

    const std::string text = SaveText(original);
    const Json saved = Json::parse(text);
    Check(saved["lightmapSettings"].is_object(), "topology lightmap settings are written");
    Check(saved["bakedLightmap"].is_object(), "topology baked lightmap metadata is written");
    Check(saved["bakedLightmap"]["path"].get<std::string>() == original.bakedLightmap.path,
          "topology baked lightmap path is serialized");

    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(text, loaded, error), "topology lightmap metadata JSON loads");
    Check(std::fabs(loaded.lightmapSettings.ambientOcclusionRadius - 3.5f) <= 0.0001f
                  && std::fabs(loaded.lightmapSettings.ambientOcclusionStrength - 0.25f) <= 0.0001f
                  && std::fabs(loaded.lightmapSettings.indirectBounceRadius - 9.0f) <= 0.0001f
                  && std::fabs(loaded.lightmapSettings.indirectBounceStrength - 0.35f) <= 0.0001f,
          "topology lightmap settings round-trip");
    Check(loaded.bakedLightmap.path == original.bakedLightmap.path
                  && loaded.bakedLightmap.width == 2048
                  && loaded.bakedLightmap.height == 2048
                  && loaded.bakedLightmap.sourceHash == "abc123",
          "topology baked lightmap metadata round-trips");

    Json withoutLightmap = saved;
    withoutLightmap.erase("lightmapSettings");
    withoutLightmap.erase("bakedLightmap");
    SectorTopologyMap oldStyle;
    Check(LoadText(withoutLightmap.dump(), oldStyle, error), "omitted topology lightmap fields are accepted");
    Check(oldStyle.bakedLightmap.path.empty()
                  && oldStyle.bakedLightmap.width == 0
                  && oldStyle.bakedLightmap.height == 0
                  && oldStyle.bakedLightmap.sourceHash.empty(),
          "omitted baked lightmap metadata loads empty");
}

void TestPreviewSettingsRoundTripAndValidation()
{
    SectorTopologyMap original = MakeSquare();
    original.previewSettings.walkSpeed = 7.25f;
    original.previewSettings.runSpeed = 15.5f;
    original.previewSettings.mouseSensitivity = 2.75f;
    original.previewSettings.eyeHeight = 1.25f;
    original.previewSettings.gravity = 38.5f;
    original.previewSettings.playerRadius = 0.35f;
    original.previewSettings.playerHeight = 1.75f;
    original.previewSettings.stepHeight = 0.5f;
    original.previewSettings.jumpHeight = 0.75f;
    original.previewSettings.headBobStrength = 0.08f;
    original.previewSettings.headBobFrequency = 10.5f;

    const std::string text = SaveText(original);
    const Json saved = Json::parse(text);
    Check(saved["previewSettings"].is_object(), "preview settings are written");
    Check(Near(saved["previewSettings"]["walkSpeed"].get<float>(), 7.25f)
                  && Near(saved["previewSettings"]["runSpeed"].get<float>(), 15.5f)
                  && Near(saved["previewSettings"]["mouseSensitivity"].get<float>(), 2.75f)
                  && Near(saved["previewSettings"]["eyeHeight"].get<float>(), 1.25f)
                  && Near(saved["previewSettings"]["gravity"].get<float>(), 38.5f)
                  && Near(saved["previewSettings"]["playerRadius"].get<float>(), 0.35f)
                  && Near(saved["previewSettings"]["playerHeight"].get<float>(), 1.75f)
                  && Near(saved["previewSettings"]["stepHeight"].get<float>(), 0.5f)
                  && Near(saved["previewSettings"]["jumpHeight"].get<float>(), 0.75f)
                  && Near(saved["previewSettings"]["headBobStrength"].get<float>(), 0.08f)
                  && Near(saved["previewSettings"]["headBobFrequency"].get<float>(), 10.5f),
          "preview settings values are serialized");

    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(text, loaded, error), "preview settings JSON loads");
    Check(Near(loaded.previewSettings.walkSpeed, 7.25f)
                  && Near(loaded.previewSettings.runSpeed, 15.5f)
                  && Near(loaded.previewSettings.mouseSensitivity, 2.75f)
                  && Near(loaded.previewSettings.eyeHeight, 1.25f)
                  && Near(loaded.previewSettings.gravity, 38.5f)
                  && Near(loaded.previewSettings.playerRadius, 0.35f)
                  && Near(loaded.previewSettings.playerHeight, 1.75f)
                  && Near(loaded.previewSettings.stepHeight, 0.5f)
                  && Near(loaded.previewSettings.jumpHeight, 0.75f)
                  && Near(loaded.previewSettings.headBobStrength, 0.08f)
                  && Near(loaded.previewSettings.headBobFrequency, 10.5f),
          "preview settings round-trip");

    Json withoutGravity = saved;
    withoutGravity["previewSettings"].erase("gravity");
    Check(LoadText(withoutGravity.dump(), loaded, error), "omitted gravity field is accepted");
    Check(Near(loaded.previewSettings.gravity, game::DefaultSectorPreviewSettings().gravity),
          "omitted gravity loads default");

    Json withoutCollisionSettings = saved;
    withoutCollisionSettings["previewSettings"].erase("playerRadius");
    withoutCollisionSettings["previewSettings"].erase("playerHeight");
    withoutCollisionSettings["previewSettings"].erase("stepHeight");
    withoutCollisionSettings["previewSettings"].erase("jumpHeight");
    Check(LoadText(withoutCollisionSettings.dump(), loaded, error),
          "omitted collision preview fields are accepted");
    Check(Near(loaded.previewSettings.playerRadius, game::DefaultSectorPreviewSettings().playerRadius)
                  && Near(loaded.previewSettings.playerHeight, game::DefaultSectorPreviewSettings().playerHeight)
                  && Near(loaded.previewSettings.stepHeight, game::DefaultSectorPreviewSettings().stepHeight)
                  && Near(loaded.previewSettings.jumpHeight, game::DefaultSectorPreviewSettings().jumpHeight),
          "omitted collision preview fields load defaults");

    Json withoutJumpHeight = saved;
    withoutJumpHeight["previewSettings"].erase("jumpHeight");
    Check(LoadText(withoutJumpHeight.dump(), loaded, error), "omitted jump height field is accepted");
    Check(Near(loaded.previewSettings.jumpHeight, game::DefaultSectorPreviewSettings().jumpHeight),
          "omitted jump height loads default");

    Json withoutHeadBob = saved;
    withoutHeadBob["previewSettings"].erase("headBobStrength");
    withoutHeadBob["previewSettings"].erase("headBobFrequency");
    Check(LoadText(withoutHeadBob.dump(), loaded, error), "omitted headbob fields are accepted");
    Check(Near(loaded.previewSettings.headBobStrength, game::DefaultSectorPreviewSettings().headBobStrength)
                  && Near(loaded.previewSettings.headBobFrequency, game::DefaultSectorPreviewSettings().headBobFrequency),
          "omitted headbob fields load defaults");

    Json withoutPreviewSettings = saved;
    withoutPreviewSettings.erase("previewSettings");
    SectorTopologyMap oldStyle;
    Check(LoadText(withoutPreviewSettings.dump(), oldStyle, error),
          "omitted preview settings field is accepted");
    const game::SectorPreviewSettings defaults = game::DefaultSectorPreviewSettings();
    Check(Near(oldStyle.previewSettings.walkSpeed, defaults.walkSpeed)
                  && Near(oldStyle.previewSettings.runSpeed, defaults.runSpeed)
                  && Near(oldStyle.previewSettings.mouseSensitivity, defaults.mouseSensitivity)
                  && Near(oldStyle.previewSettings.eyeHeight, defaults.eyeHeight)
                  && Near(oldStyle.previewSettings.gravity, defaults.gravity)
                  && Near(oldStyle.previewSettings.playerRadius, defaults.playerRadius)
                  && Near(oldStyle.previewSettings.playerHeight, defaults.playerHeight)
                  && Near(oldStyle.previewSettings.stepHeight, defaults.stepHeight)
                  && Near(oldStyle.previewSettings.jumpHeight, defaults.jumpHeight)
                  && Near(oldStyle.previewSettings.headBobStrength, defaults.headBobStrength)
                  && Near(oldStyle.previewSettings.headBobFrequency, defaults.headBobFrequency),
          "omitted preview settings load defaults");

    Json invalid = saved;
    invalid["previewSettings"] = 4;
    ExpectRejected(invalid, "non-object preview settings are rejected");

    const std::array<const char*, 11> fields{
            "walkSpeed",
            "runSpeed",
            "mouseSensitivity",
            "eyeHeight",
            "gravity",
            "playerRadius",
            "playerHeight",
            "stepHeight",
            "jumpHeight",
            "headBobStrength",
            "headBobFrequency"
    };
    for (const char* field : fields) {
        invalid = saved;
        invalid["previewSettings"][field] = "invalid";
        ExpectRejected(invalid, "wrong-type preview settings field is rejected");
    }

    invalid = saved;
    invalid["previewSettings"]["gravity"] = "__NONFINITE__";
    std::string nonFiniteText = invalid.dump();
    const std::string marker = "\"__NONFINITE__\"";
    const size_t markerPos = nonFiniteText.find(marker);
    Check(markerPos != std::string::npos, "non-finite preview settings marker exists");
    if (markerPos != std::string::npos) {
        nonFiniteText.replace(markerPos, marker.size(), "1e999");
        ExpectRejectedText(nonFiniteText, "non-finite preview settings field is rejected");
    }

    invalid = saved;
    invalid["previewSettings"]["jumpHeight"] = "__NONFINITE__";
    nonFiniteText = invalid.dump();
    const size_t jumpMarkerPos = nonFiniteText.find(marker);
    Check(jumpMarkerPos != std::string::npos, "non-finite jump height marker exists");
    if (jumpMarkerPos != std::string::npos) {
        nonFiniteText.replace(jumpMarkerPos, marker.size(), "1e999");
        ExpectRejectedText(nonFiniteText, "non-finite jump height is rejected");
    }

    invalid = saved;
    invalid["previewSettings"]["headBobStrength"] = "__NONFINITE__";
    nonFiniteText = invalid.dump();
    const size_t headBobStrengthMarkerPos = nonFiniteText.find(marker);
    Check(headBobStrengthMarkerPos != std::string::npos, "non-finite headbob strength marker exists");
    if (headBobStrengthMarkerPos != std::string::npos) {
        nonFiniteText.replace(headBobStrengthMarkerPos, marker.size(), "1e999");
        ExpectRejectedText(nonFiniteText, "non-finite headbob strength is rejected");
    }

    invalid = saved;
    invalid["previewSettings"]["headBobFrequency"] = "__NONFINITE__";
    nonFiniteText = invalid.dump();
    const size_t headBobFrequencyMarkerPos = nonFiniteText.find(marker);
    Check(headBobFrequencyMarkerPos != std::string::npos, "non-finite headbob frequency marker exists");
    if (headBobFrequencyMarkerPos != std::string::npos) {
        nonFiniteText.replace(headBobFrequencyMarkerPos, marker.size(), "1e999");
        ExpectRejectedText(nonFiniteText, "non-finite headbob frequency is rejected");
    }

    Json clamped = saved;
    clamped["previewSettings"]["walkSpeed"] = -5.0f;
    clamped["previewSettings"]["runSpeed"] = 500.0f;
    clamped["previewSettings"]["mouseSensitivity"] = 0.001f;
    clamped["previewSettings"]["eyeHeight"] = 40.0f;
    clamped["previewSettings"]["gravity"] = -5.0f;
    clamped["previewSettings"]["playerRadius"] = -1.0f;
    clamped["previewSettings"]["playerHeight"] = 0.1f;
    clamped["previewSettings"]["stepHeight"] = 9.0f;
    clamped["previewSettings"]["jumpHeight"] = 9.0f;
    clamped["previewSettings"]["headBobStrength"] = 9.0f;
    clamped["previewSettings"]["headBobFrequency"] = 99.0f;
    Check(LoadText(clamped.dump(), loaded, error), "out-of-range preview settings load");
    Check(Near(loaded.previewSettings.walkSpeed, 0.1f)
                  && Near(loaded.previewSettings.runSpeed, 200.0f)
                  && Near(loaded.previewSettings.mouseSensitivity, 0.01f)
                  && Near(loaded.previewSettings.eyeHeight, 3.0f)
                  && Near(loaded.previewSettings.gravity, 0.0f)
                  && Near(loaded.previewSettings.playerRadius, 0.05f)
                  && Near(loaded.previewSettings.playerHeight, 3.0f)
                  && Near(loaded.previewSettings.stepHeight, 2.0f)
                  && Near(loaded.previewSettings.jumpHeight, 3.0f)
                  && Near(loaded.previewSettings.headBobStrength, 0.25f)
                  && Near(loaded.previewSettings.headBobFrequency, 20.0f),
          "out-of-range preview settings clamp");

    clamped["previewSettings"]["gravity"] = 500.0f;
    clamped["previewSettings"]["jumpHeight"] = -5.0f;
    clamped["previewSettings"]["headBobStrength"] = -5.0f;
    clamped["previewSettings"]["headBobFrequency"] = -5.0f;
    Check(LoadText(clamped.dump(), loaded, error), "high gravity preview settings load");
    Check(Near(loaded.previewSettings.gravity, 200.0f), "high gravity clamps");
    Check(Near(loaded.previewSettings.jumpHeight, 0.0f), "low jump height clamps");
    Check(Near(loaded.previewSettings.headBobStrength, 0.0f), "low headbob strength clamps");
    Check(Near(loaded.previewSettings.headBobFrequency, 0.0f), "low headbob frequency clamps");
}

void TestSkySettingsRoundTripAndValidation()
{
    SectorTopologyMap original = MakeSquare();
    original.skySettings.textureId = "storm_panorama";
    original.skySettings.yawOffsetDegrees = 45.5f;
    original.skySettings.verticalOffset = -0.125f;
    original.skySettings.verticalScale = 1.75f;
    original.skySettings.topColor = Color{12, 34, 56, 255};

    const std::string text = SaveText(original);
    const Json saved = Json::parse(text);
    Check(saved["skySettings"].is_object(), "non-default sky settings are written");
    Check(saved["skySettings"]["textureId"].get<std::string>() == "storm_panorama"
                  && Near(saved["skySettings"]["yawOffsetDegrees"].get<float>(), 45.5f)
                  && Near(saved["skySettings"]["verticalOffset"].get<float>(), -0.125f)
                  && Near(saved["skySettings"]["verticalScale"].get<float>(), 1.75f)
                  && saved["skySettings"]["topColor"]["r"].get<int>() == 12
                  && saved["skySettings"]["topColor"]["g"].get<int>() == 34
                  && saved["skySettings"]["topColor"]["b"].get<int>() == 56
                  && saved["skySettings"]["topColor"]["a"].get<int>() == 255,
          "sky settings values are serialized");

    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(text, loaded, error), "sky settings JSON loads");
    Check(loaded.skySettings.textureId == "storm_panorama"
                  && Near(loaded.skySettings.yawOffsetDegrees, 45.5f)
                  && Near(loaded.skySettings.verticalOffset, -0.125f)
                  && Near(loaded.skySettings.verticalScale, 1.75f)
                  && loaded.skySettings.topColor.r == 12
                  && loaded.skySettings.topColor.g == 34
                  && loaded.skySettings.topColor.b == 56
                  && loaded.skySettings.topColor.a == 255,
          "sky settings round-trip");

    Json missingFields = saved;
    missingFields["skySettings"].erase("textureId");
    missingFields["skySettings"].erase("yawOffsetDegrees");
    missingFields["skySettings"].erase("verticalOffset");
    missingFields["skySettings"].erase("verticalScale");
    Check(LoadText(missingFields.dump(), loaded, error), "omitted sky setting fields are accepted");
    const game::SectorTopologySkySettings defaults = game::DefaultSectorTopologySkySettings();
    Check(loaded.skySettings.textureId == defaults.textureId
                  && Near(loaded.skySettings.yawOffsetDegrees, defaults.yawOffsetDegrees)
                  && Near(loaded.skySettings.verticalOffset, defaults.verticalOffset)
                  && Near(loaded.skySettings.verticalScale, defaults.verticalScale)
                  && loaded.skySettings.topColor.r == 12,
          "omitted sky setting fields load defaults while present color remains");

    Json withoutSkySettings = saved;
    withoutSkySettings.erase("skySettings");
    Check(LoadText(withoutSkySettings.dump(), loaded, error),
          "omitted sky settings field is accepted");
    Check(loaded.skySettings.textureId == defaults.textureId
                  && Near(loaded.skySettings.yawOffsetDegrees, defaults.yawOffsetDegrees)
                  && Near(loaded.skySettings.verticalOffset, defaults.verticalOffset)
                  && Near(loaded.skySettings.verticalScale, defaults.verticalScale)
                  && loaded.skySettings.topColor.r == defaults.topColor.r
                  && loaded.skySettings.topColor.g == defaults.topColor.g
                  && loaded.skySettings.topColor.b == defaults.topColor.b
                  && loaded.skySettings.topColor.a == 255,
          "omitted sky settings load defaults");

    SectorTopologyMap defaultMap = MakeSquare();
    const Json defaultSaved = Json::parse(SaveText(defaultMap));
    Check(!defaultSaved.contains("skySettings"), "default sky settings are omitted");

    Json clamped = saved;
    clamped["skySettings"]["verticalScale"] = 0.0f;
    Check(LoadText(clamped.dump(), loaded, error), "zero sky vertical scale loads");
    Check(Near(loaded.skySettings.verticalScale, 0.01f), "zero sky vertical scale clamps to positive minimum");
    clamped["skySettings"]["verticalScale"] = -4.0f;
    Check(LoadText(clamped.dump(), loaded, error), "negative sky vertical scale loads");
    Check(Near(loaded.skySettings.verticalScale, 0.01f), "negative sky vertical scale clamps to positive minimum");

    Json invalid = saved;
    invalid["skySettings"] = 4;
    ExpectRejected(invalid, "non-object sky settings are rejected");

    const std::array<const char*, 4> fields{
            "textureId",
            "yawOffsetDegrees",
            "verticalOffset",
            "verticalScale"
    };
    for (const char* field : fields) {
        invalid = saved;
        invalid["skySettings"][field] = field == std::string("textureId") ? Json(42) : Json("invalid");
        ExpectRejected(invalid, "wrong-type sky settings field is rejected");
    }
    invalid = saved;
    invalid["skySettings"]["topColor"] = Json::array({1, 2, 3});
    ExpectRejected(invalid, "wrong-type sky top color is rejected");
}

void TestDecalDefaultsAndOmission()
{
    SectorTopologyMap map = MakeSquare();
    const std::string text = SaveText(map);
    const Json saved = Json::parse(text);
    Check(!saved["sectors"][0].contains("floorDecal"),
          "default floor decal is omitted");
    Check(!saved["sectors"][0].contains("ceilingDecal"),
          "default ceiling decal is omitted");
    Check(!saved["sidedefs"][0]["wall"].contains("decal"),
          "default wall decal is omitted");
    Check(!saved["sidedefs"][0]["lower"].contains("decal"),
          "default lower decal is omitted");
    Check(!saved["sidedefs"][0]["upper"].contains("decal"),
          "default upper decal is omitted");

    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(text, loaded, error), "topology without decal fields loads");
    Check(loaded.sectors[0].floorDecal.textureId.empty()
                  && loaded.sectors[0].floorDecal.uv.scale.x == 1.0f
                  && loaded.sectors[0].floorDecal.uv.offset.y == 0.0f
                  && loaded.sectors[0].floorDecal.opacity == 1.0f
                  && !loaded.sectors[0].floorDecal.emissive
                  && Near(loaded.sectors[0].floorDecal.tint, Vector3{1.0f, 1.0f, 1.0f})
                  && Near(loaded.sectors[0].floorDecal.bloomIntensity, 1.0f),
          "omitted floor decal loads default no-decal state");
    Check(loaded.sideDefs[0].wall.decal.textureId.empty()
                  && loaded.sideDefs[0].wall.decal.uv.scale.y == 1.0f
                  && loaded.sideDefs[0].wall.decal.opacity == 1.0f
                  && !loaded.sideDefs[0].wall.decal.emissive
                  && Near(loaded.sideDefs[0].wall.decal.tint, Vector3{1.0f, 1.0f, 1.0f})
                  && Near(loaded.sideDefs[0].wall.decal.bloomIntensity, 1.0f),
          "omitted sidedef decal loads default no-decal state");

    map.sectors[0].floorDecal.uv.scale = {2.0f, 3.0f};
    map.sectors[0].floorDecal.uv.offset = {4.0f, 5.0f};
    map.sectors[0].floorDecal.opacity = 0.25f;
    map.sectors[0].floorDecal.emissive = true;
    map.sectors[0].floorDecal.tint = Vector3{0.5f, 0.25f, 0.75f};
    map.sectors[0].floorDecal.bloomIntensity = 4.0f;
    map.sideDefs[0].wall.decal.uv.scale = {6.0f, 7.0f};
    map.sideDefs[0].wall.decal.opacity = 0.5f;
    map.sideDefs[0].wall.decal.emissive = true;
    map.sideDefs[0].wall.decal.tint = Vector3{0.25f, 0.5f, 0.75f};
    map.sideDefs[0].wall.decal.bloomIntensity = 6.0f;
    const Json normalized = Json::parse(SaveText(map));
    Check(!normalized["sectors"][0].contains("floorDecal"),
          "empty texture sector decal with stray data is omitted");
    Check(!normalized["sidedefs"][0]["wall"].contains("decal"),
          "empty texture wall decal with stray data is omitted");
}

void TestMiddleDefaultsAndOmission()
{
    SectorTopologyMap map = MakeSquare();
    const Json savedDefault = Json::parse(SaveText(map));
    Check(!savedDefault["sidedefs"][0].contains("middle"),
          "default middle settings are omitted");

    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(savedDefault.dump(), loaded, error), "topology without middle fields loads");
    Check(loaded.sideDefs[0].middle.textureId.empty()
                  && loaded.sideDefs[0].middle.uv.scale.x == 1.0f
                  && loaded.sideDefs[0].middle.uv.scale.y == 1.0f
                  && loaded.sideDefs[0].middle.uv.offset.x == 0.0f
                  && loaded.sideDefs[0].middle.uv.offset.y == 0.0f
                  && loaded.sideDefs[0].middle.decal.textureId.empty()
                  && loaded.sideDefs[0].middle.decal.opacity == 1.0f,
          "omitted middle loads default empty settings");

    map.sideDefs[0].middle.uv.scale = {2.0f, 3.0f};
    const Json savedUvOnly = Json::parse(SaveText(map));
    Check(savedUvOnly["sidedefs"][0].contains("middle")
                  && savedUvOnly["sidedefs"][0]["middle"]["textureId"].get<std::string>().empty()
                  && savedUvOnly["sidedefs"][0]["middle"]["uv"]["scale"][0].get<float>() == 2.0f,
          "non-default middle UV is saved even without a texture");

    map = MakeSquare();
    map.sideDefs[0].middle.decal.opacity = 0.5f;
    const Json normalized = Json::parse(SaveText(map));
    Check(!normalized["sidedefs"][0].contains("middle"),
          "empty middle decal texture with stray data is omitted");
}

void TestMiddleRoundTrip()
{
    SectorTopologyMap original = MakeSquare();
    original.texturesById.emplace("bars", SectorTextureDefinition{
            "bars", "textures/bars.png", SectorTextureFilter::Point});
    original.sideDefs[0].middle = MakePart("bars", 2.0f, 3.0f, 4.0f, 5.0f);

    const std::string text = SaveText(original);
    const Json saved = Json::parse(text);
    Check(saved["sidedefs"][0]["middle"]["textureId"].get<std::string>() == "bars",
          "middle texture ID is saved");
    Check(saved["sidedefs"][0]["middle"]["uv"]["scale"][0].get<float>() == 2.0f
                  && saved["sidedefs"][0]["middle"]["uv"]["offset"][1].get<float>() == 5.0f,
          "middle UV is saved");
    Check(!saved["sidedefs"][1].contains("middle"),
          "default middle is omitted on other sidedefs");

    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(text, loaded, error), "topology with middle field loads");
    Check(loaded.sideDefs[0].middle.textureId == "bars"
                  && loaded.sideDefs[0].middle.uv.scale.x == 2.0f
                  && loaded.sideDefs[0].middle.uv.scale.y == 3.0f
                  && loaded.sideDefs[0].middle.uv.offset.x == 4.0f
                  && loaded.sideDefs[0].middle.uv.offset.y == 5.0f,
          "middle texture settings round-trip");
}

void TestLineDefFlagsRoundTripAndDefaults()
{
    SectorTopologyMap original = MakeAdjacentSquares();
    original.lineDefs[1].flags.blocksPlayer = true;

    const std::string text = SaveText(original);
    const Json saved = Json::parse(text);
    Check(!saved["linedefs"][0].contains("flags"),
          "default linedef flags are omitted");
    Check(saved["linedefs"][1]["flags"]["blocksPlayer"].get<bool>(),
          "blocksPlayer true is saved under linedef flags");

    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(text, loaded, error), "topology with linedef flags loads");
    Check(!loaded.lineDefs[0].flags.blocksPlayer,
          "missing linedef flags load blocksPlayer false");
    Check(loaded.lineDefs[1].flags.blocksPlayer,
          "blocksPlayer true round-trips");

    Json withoutBlocksPlayer = saved;
    withoutBlocksPlayer["linedefs"][1]["flags"].erase("blocksPlayer");
    Check(LoadText(withoutBlocksPlayer.dump(), loaded, error),
          "linedef flags without blocksPlayer load");
    Check(!loaded.lineDefs[1].flags.blocksPlayer,
          "missing blocksPlayer defaults false");
}

void TestStrictLineDefFlagsValidation()
{
    const Json valid = Json::parse(SaveText(MakeAdjacentSquares()));
    Json changed = valid;
    changed["linedefs"][1]["flags"] = "not an object";
    ExpectRejected(changed, "linedef flags wrong type is rejected");

    changed = valid;
    changed["linedefs"][1]["flags"] = Json{{"blocksPlayer", "yes"}};
    ExpectRejected(changed, "blocksPlayer wrong type is rejected");

    SectorTopologyMap output = MakeAdjacentSquares();
    output.lineDefs[1].flags.blocksPlayer = true;
    std::string error;
    Check(!LoadText(changed.dump(), output, error), "invalid linedef flags load fails");
    Check(output.lineDefs[1].flags.blocksPlayer,
          "failed linedef flags load leaves output map unchanged");
}

void TestDecalRoundTrip()
{
    SectorTopologyMap original = MakeSquare();
    original.sectors[0].floorDecal = MakeDecal("floor_arrow", 0.5f, 0.75f, 0.125f, 0.25f, 0.8f, true, Vector3{1.0f, 0.25f, 0.5f}, 2.5f);
    original.sectors[0].ceilingDecal = MakeDecal("ceiling_grime", 2.0f, 3.0f, 4.0f, 5.0f, 0.35f, false, Vector3{0.2f, 0.3f, 0.4f});
    original.sideDefs[0].wall.decal = MakeDecal("painting_01", 0.25f, 0.5f, 0.75f, 1.0f, 1.0f, true, Vector3{0.5f, 0.6f, 0.7f}, 7.0f);
    original.sideDefs[0].lower.decal = MakeDecal("lower_sign", 1.25f, 1.5f, 1.75f, 2.0f, 0.65f);
    original.sideDefs[0].upper.decal = MakeDecal("upper_text", 2.25f, 2.5f, 2.75f, 3.0f, 0.9f);

    const std::string text = SaveText(original);
    const Json saved = Json::parse(text);
    Check(saved["sectors"][0]["floorDecal"]["textureId"].get<std::string>() == "floor_arrow",
          "floor decal texture ID is saved");
    Check(saved["sectors"][0]["floorDecal"]["uv"]["scale"][0].get<float>() == 0.5f,
          "floor decal UV scale is saved");
    Check(saved["sectors"][0]["floorDecal"]["opacity"].get<float>() == 0.8f,
          "floor decal opacity is saved");
    Check(saved["sectors"][0]["floorDecal"]["emissive"].get<bool>(),
          "floor decal emissive flag is saved");
    Check(saved["sectors"][0]["floorDecal"]["tint"][1].get<float>() == 0.25f,
          "floor decal tint is saved");
    Check(saved["sectors"][0]["floorDecal"]["bloomIntensity"].get<float>() == 2.5f,
          "floor decal bloom intensity is saved");
    Check(saved["sidedefs"][0]["wall"]["decal"]["textureId"].get<std::string>() == "painting_01",
          "wall decal texture ID is saved");
    Check(saved["sidedefs"][0]["lower"]["decal"]["uv"]["offset"][1].get<float>() == 2.0f,
          "lower decal UV offset is saved");
    Check(saved["sidedefs"][0]["upper"]["decal"]["opacity"].get<float>() == 0.9f,
          "upper decal opacity is saved");

    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(text, loaded, error), "topology with decal fields loads");
    Check(loaded.sectors[0].floorDecal.textureId == "floor_arrow"
                  && loaded.sectors[0].floorDecal.uv.scale.x == 0.5f
                  && loaded.sectors[0].floorDecal.uv.offset.y == 0.25f
                  && loaded.sectors[0].floorDecal.opacity == 0.8f
                  && loaded.sectors[0].floorDecal.emissive
                  && Near(loaded.sectors[0].floorDecal.tint, Vector3{1.0f, 0.25f, 0.5f})
                  && Near(loaded.sectors[0].floorDecal.bloomIntensity, 2.5f),
          "floor decal round-trips");
    Check(loaded.sectors[0].ceilingDecal.textureId == "ceiling_grime"
                  && loaded.sectors[0].ceilingDecal.uv.scale.y == 3.0f
                  && loaded.sectors[0].ceilingDecal.uv.offset.x == 4.0f
                  && loaded.sectors[0].ceilingDecal.opacity == 0.35f
                  && !loaded.sectors[0].ceilingDecal.emissive
                  && Near(loaded.sectors[0].ceilingDecal.tint, Vector3{0.2f, 0.3f, 0.4f}),
          "ceiling decal round-trips");
    Check(loaded.sideDefs[0].wall.decal.textureId == "painting_01"
                  && loaded.sideDefs[0].wall.decal.uv.offset.x == 0.75f
                  && loaded.sideDefs[0].wall.decal.opacity == 1.0f
                  && loaded.sideDefs[0].wall.decal.emissive
                  && Near(loaded.sideDefs[0].wall.decal.tint, Vector3{0.5f, 0.6f, 0.7f})
                  && Near(loaded.sideDefs[0].wall.decal.bloomIntensity, 7.0f),
          "wall decal round-trips");
    Check(loaded.sideDefs[0].lower.decal.textureId == "lower_sign"
                  && loaded.sideDefs[0].lower.decal.uv.scale.x == 1.25f
                  && loaded.sideDefs[0].lower.decal.opacity == 0.65f,
          "lower decal round-trips");
    Check(loaded.sideDefs[0].upper.decal.textureId == "upper_text"
                  && loaded.sideDefs[0].upper.decal.uv.offset.y == 3.0f
                  && loaded.sideDefs[0].upper.decal.opacity == 0.9f,
          "upper decal round-trips");

    Json withoutOpacity = saved;
    withoutOpacity["sidedefs"][0]["wall"]["decal"].erase("opacity");
    Check(LoadText(withoutOpacity.dump(), loaded, error), "decal opacity is optional on load");
    Check(loaded.sideDefs[0].wall.decal.opacity == 1.0f,
          "omitted decal opacity defaults to 1");

    Json withoutBloomIntensity = saved;
    withoutBloomIntensity["sidedefs"][0]["wall"]["decal"].erase("bloomIntensity");
    Check(LoadText(withoutBloomIntensity.dump(), loaded, error), "decal bloom intensity is optional on load");
    Check(loaded.sideDefs[0].wall.decal.bloomIntensity == 1.0f,
          "omitted decal bloom intensity defaults to 1");

    Json oldDecal = saved;
    oldDecal["sectors"][0]["floorDecal"].erase("emissive");
    oldDecal["sectors"][0]["floorDecal"].erase("tint");
    oldDecal["sectors"][0]["floorDecal"].erase("bloomIntensity");
    Check(LoadText(oldDecal.dump(), loaded, error), "old decal JSON without emissive and tint loads");
    Check(!loaded.sectors[0].floorDecal.emissive
                  && Near(loaded.sectors[0].floorDecal.tint, Vector3{1.0f, 1.0f, 1.0f})
                  && Near(loaded.sectors[0].floorDecal.bloomIntensity, 1.0f),
          "old decal JSON defaults emissive tint and bloom intensity");
}

void TestStrictDecalValidation()
{
    const Json valid = Json::parse(SaveText(MakeSquare()));
    Json changed = valid;
    changed["sectors"][0]["floorDecal"] = "not an object";
    ExpectRejected(changed, "decal field wrong type is rejected");
    changed = valid;
    changed["sectors"][0]["floorDecal"] = Json{
            {"textureId", ""},
            {"uv", {{"scale", Json::array({1, 1})}, {"offset", Json::array({0, 0})}}},
            {"opacity", 1.0f}
    };
    ExpectRejected(changed, "empty present decal texture ID is rejected");
    changed["sectors"][0]["floorDecal"]["textureId"] = 17;
    ExpectRejected(changed, "decal texture ID wrong type is rejected");
    changed = valid;
    changed["sectors"][0]["floorDecal"] = Json{
            {"textureId", "arrow"},
            {"opacity", 1.0f}
    };
    ExpectRejected(changed, "decal missing UV is rejected");
    changed = valid;
    changed["sectors"][0]["floorDecal"] = Json{
            {"textureId", "arrow"},
            {"uv", {{"scale", Json::array({1})}, {"offset", Json::array({0, 0})}}},
            {"opacity", 1.0f}
    };
    ExpectRejected(changed, "decal UV scale wrong shape is rejected");
    changed["sectors"][0]["floorDecal"]["uv"]["scale"] = Json::array({1, 1});
    changed["sectors"][0]["floorDecal"]["uv"]["offset"] = Json::array({0, "bad"});
    ExpectRejected(changed, "decal UV offset wrong type is rejected");
    changed["sectors"][0]["floorDecal"]["uv"]["offset"] = Json::array({0, 0});
    changed["sectors"][0]["floorDecal"]["uv"]["scale"] = Json::array({1.0e39, 1});
    ExpectRejected(changed, "decal UV outside float range is rejected");
    changed = valid;
    changed["sidedefs"][0]["wall"]["decal"] = Json{
            {"textureId", "painting"},
            {"uv", {{"scale", Json::array({1, 1})}, {"offset", Json::array({0, 0})}}},
            {"opacity", "solid"}
    };
    ExpectRejected(changed, "decal opacity wrong type is rejected");
    changed["sidedefs"][0]["wall"]["decal"]["opacity"] = -0.01f;
    ExpectRejected(changed, "negative decal opacity is rejected");
    changed["sidedefs"][0]["wall"]["decal"]["opacity"] = 1.01f;
    ExpectRejected(changed, "oversized decal opacity is rejected");
    changed["sidedefs"][0]["wall"]["decal"]["opacity"] = 1.0f;
    changed["sidedefs"][0]["wall"]["decal"]["emissive"] = "yes";
    ExpectRejected(changed, "decal emissive wrong type is rejected");
    changed["sidedefs"][0]["wall"]["decal"]["emissive"] = true;
    changed["sidedefs"][0]["wall"]["decal"]["tint"] = Json::array({1.0f, 0.5f});
    ExpectRejected(changed, "decal tint wrong shape is rejected");
    changed["sidedefs"][0]["wall"]["decal"]["tint"] = Json::array({1.0f, "green", 0.5f});
    ExpectRejected(changed, "decal tint wrong type is rejected");
    changed["sidedefs"][0]["wall"]["decal"]["tint"] = Json::array({1.0e39, 0.5f, 0.5f});
    ExpectRejected(changed, "decal tint outside float range is rejected");
    changed["sidedefs"][0]["wall"]["decal"]["tint"] = Json::array({1.01f, 0.5f, 0.5f});
    ExpectRejected(changed, "oversized decal tint is rejected");
    changed["sidedefs"][0]["wall"]["decal"]["tint"] = Json::array({-0.01f, 0.5f, 0.5f});
    ExpectRejected(changed, "negative decal tint is rejected");
    changed["sidedefs"][0]["wall"]["decal"]["tint"] = Json::array({1.0f, 0.5f, 0.5f});
    changed["sidedefs"][0]["wall"]["decal"]["bloomIntensity"] = "bright";
    ExpectRejected(changed, "decal bloom intensity wrong type is rejected");
    changed["sidedefs"][0]["wall"]["decal"]["bloomIntensity"] = -0.01f;
    ExpectRejected(changed, "negative decal bloom intensity is rejected");
    changed["sidedefs"][0]["wall"]["decal"]["bloomIntensity"] = 10.01f;
    ExpectRejected(changed, "oversized decal bloom intensity is rejected");

    SectorTopologyMap invalid = MakeSquare();
    invalid.sideDefs[0].wall.decal = MakeDecal("painting", 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);
    invalid.sideDefs[0].wall.decal.uv.scale.x = std::numeric_limits<float>::infinity();
    std::string jsonOutput = "sentinel";
    std::string error;
    Check(!game::SaveSectorTopologyMapToJsonString(invalid, jsonOutput, &error),
          "non-finite decal UV is rejected on save");
    Check(jsonOutput == "sentinel", "failed decal save leaves JSON output unchanged");

    invalid = MakeSquare();
    invalid.sectors[0].floorDecal = MakeDecal("arrow", 1.0f, 1.0f, 0.0f, 0.0f, 1.5f);
    jsonOutput = "sentinel";
    error.clear();
    Check(!game::SaveSectorTopologyMapToJsonString(invalid, jsonOutput, &error),
          "invalid decal opacity is rejected on save");
    Check(jsonOutput == "sentinel", "failed invalid opacity save leaves JSON output unchanged");

    invalid = MakeSquare();
    invalid.sectors[0].floorDecal = MakeDecal("arrow", 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, false, Vector3{1.0f, 0.5f, 1.5f});
    jsonOutput = "sentinel";
    error.clear();
    Check(!game::SaveSectorTopologyMapToJsonString(invalid, jsonOutput, &error),
          "invalid decal tint is rejected on save");
    Check(jsonOutput == "sentinel", "failed invalid tint save leaves JSON output unchanged");

    invalid.sectors[0].floorDecal.tint = Vector3{1.0f, std::numeric_limits<float>::infinity(), 1.0f};
    jsonOutput = "sentinel";
    error.clear();
    Check(!game::SaveSectorTopologyMapToJsonString(invalid, jsonOutput, &error),
          "non-finite decal tint is rejected on save");
    Check(jsonOutput == "sentinel", "failed non-finite tint save leaves JSON output unchanged");

    invalid = MakeSquare();
    invalid.sectors[0].floorDecal = MakeDecal("arrow", 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, true, Vector3{1.0f, 1.0f, 1.0f}, 10.5f);
    jsonOutput = "sentinel";
    error.clear();
    Check(!game::SaveSectorTopologyMapToJsonString(invalid, jsonOutput, &error),
          "invalid decal bloom intensity is rejected on save");
    Check(jsonOutput == "sentinel", "failed invalid bloom intensity save leaves JSON output unchanged");

    SectorTopologyMap output = MakeSquare();
    output.sectors[0].floorDecal = MakeDecal("existing", 2.0f, 2.0f, 3.0f, 3.0f, 0.5f);
    changed = valid;
    changed["sectors"][0]["floorDecal"] = Json{
            {"textureId", ""},
            {"uv", {{"scale", Json::array({1, 1})}, {"offset", Json::array({0, 0})}}},
            {"opacity", 1.0f}
    };
    error.clear();
    Check(!LoadText(changed.dump(), output, error), "invalid decal load fails");
    Check(output.sectors[0].floorDecal.textureId == "existing"
                  && output.sectors[0].floorDecal.opacity == 0.5f,
          "failed decal load leaves output map unchanged");
}

void TestStrictMiddleValidation()
{
    const Json valid = Json::parse(SaveText(MakeSquare()));
    Json changed = valid;
    changed["sidedefs"][0]["middle"] = "not an object";
    ExpectRejected(changed, "middle wrong type is rejected");
    changed = valid;
    changed["sidedefs"][0]["middle"] = Json{
            {"textureId", 17},
            {"uv", {{"scale", Json::array({1, 1})}, {"offset", Json::array({0, 0})}}}
    };
    ExpectRejected(changed, "middle texture ID wrong type is rejected");
    changed["sidedefs"][0]["middle"]["textureId"] = "bars";
    changed["sidedefs"][0]["middle"]["uv"]["scale"] = Json::array({1});
    ExpectRejected(changed, "middle UV scale wrong shape is rejected");
    changed["sidedefs"][0]["middle"]["uv"]["scale"] = Json::array({1, 1});
    changed["sidedefs"][0]["middle"]["uv"]["offset"] = Json::array({0, "bad"});
    ExpectRejected(changed, "middle UV offset wrong type is rejected");
    changed["sidedefs"][0]["middle"]["uv"]["offset"] = Json::array({0, 0});
    changed["sidedefs"][0]["middle"]["uv"]["scale"] = Json::array({1.0e39, 1});
    ExpectRejected(changed, "middle UV outside float range is rejected");
    changed = valid;
    changed["sidedefs"][0]["middle"] = Json{
            {"textureId", "bars"},
            {"uv", {{"scale", Json::array({1, 1})}, {"offset", Json::array({0, 0})}}},
            {"decal", "not an object"}
    };
    ExpectRejected(changed, "middle nested decal wrong type is rejected");

    SectorTopologyMap output = MakeSquare();
    output.sideDefs[0].middle = MakePart("existing_middle", 2.0f, 3.0f, 4.0f, 5.0f);
    changed = valid;
    changed["sidedefs"][0]["middle"] = Json{
            {"textureId", 17},
            {"uv", {{"scale", Json::array({1, 1})}, {"offset", Json::array({0, 0})}}}
    };
    std::string error;
    Check(!LoadText(changed.dump(), output, error), "invalid middle load fails");
    Check(output.sideDefs[0].middle.textureId == "existing_middle"
                  && output.sideDefs[0].middle.uv.offset.y == 5.0f,
          "failed middle load leaves output map unchanged");

    SectorTopologyMap invalid = MakeSquare();
    invalid.sideDefs[0].middle = MakePart("bars", 1.0f, 1.0f, 0.0f, 0.0f);
    invalid.sideDefs[0].middle.uv.scale.x = std::numeric_limits<float>::infinity();
    std::string jsonOutput = "sentinel";
    error.clear();
    Check(!game::SaveSectorTopologyMapToJsonString(invalid, jsonOutput, &error),
          "non-finite middle UV is rejected on save");
    Check(jsonOutput == "sentinel", "failed middle save leaves JSON output unchanged");
}

void TestHandAuthoredJson()
{
    const std::string text = R"json({
  "formatVersion": 2,
  "topology": "linedef",
  "coordSubdivisions": 16,
  "textures": {},
  "vertices": [
    {"id":1,"x":0,"y":0}, {"id":2,"x":32,"y":0},
    {"id":3,"x":32,"y":32}, {"id":4,"x":0,"y":32}
  ],
  "linedefs": [
    {"id":1,"startVertexId":1,"endVertexId":2,"frontSideDefId":1,"backSideDefId":-1},
    {"id":2,"startVertexId":2,"endVertexId":3,"frontSideDefId":2,"backSideDefId":-1},
    {"id":3,"startVertexId":3,"endVertexId":4,"frontSideDefId":3,"backSideDefId":-1},
    {"id":4,"startVertexId":4,"endVertexId":1,"frontSideDefId":4,"backSideDefId":-1}
  ],
  "sidedefs": [
    {"id":1,"lineDefId":1,"side":"front","sectorId":1,"wall":{"textureId":"","uv":{"scale":[1,1],"offset":[0,0]}},"lower":{"textureId":"","uv":{"scale":[1,1],"offset":[0,0]}},"upper":{"textureId":"","uv":{"scale":[1,1],"offset":[0,0]}}},
    {"id":2,"lineDefId":2,"side":"front","sectorId":1,"wall":{"textureId":"","uv":{"scale":[1,1],"offset":[0,0]}},"lower":{"textureId":"","uv":{"scale":[1,1],"offset":[0,0]}},"upper":{"textureId":"","uv":{"scale":[1,1],"offset":[0,0]}}},
    {"id":3,"lineDefId":3,"side":"front","sectorId":1,"wall":{"textureId":"","uv":{"scale":[1,1],"offset":[0,0]}},"lower":{"textureId":"","uv":{"scale":[1,1],"offset":[0,0]}},"upper":{"textureId":"","uv":{"scale":[1,1],"offset":[0,0]}}},
    {"id":4,"lineDefId":4,"side":"front","sectorId":1,"wall":{"textureId":"","uv":{"scale":[1,1],"offset":[0,0]}},"lower":{"textureId":"","uv":{"scale":[1,1],"offset":[0,0]}},"upper":{"textureId":"","uv":{"scale":[1,1],"offset":[0,0]}}}
  ],
  "sectors": [{
    "id":1,"name":"sector_001","floorZ":0,"ceilingZ":24,
    "floorTextureId":"","ceilingTextureId":"",
    "floorUv":{"scale":[1,1],"offset":[0,0]},
    "ceilingUv":{"scale":[1,1],"offset":[0,0]},
    "ambientColor":{"r":255,"g":255,"b":255,"a":255},"ambientIntensity":1,
    "defaultWall":{"textureId":"","uv":{"scale":[1,1],"offset":[0,0]}},
    "defaultLower":{"textureId":"","uv":{"scale":[1,1],"offset":[0,0]}},
    "defaultUpper":{"textureId":"","uv":{"scale":[1,1],"offset":[0,0]}}
  }],
  "futureMetadata": {"allowed": true}
})json";

    SectorTopologyMap map;
    std::string error;
    Check(LoadText(text, map, error), "hand-authored topology JSON loads");
    Check(!game::HasSectorTopologyValidationErrors(game::ValidateSectorTopologyMap(map)),
          "hand-authored map validates");
    SectorTopologyLoopSet loops;
    Check(game::ExtractSectorTopologyLoops(map, 1, loops)
                  && loops.outer.signedAreaTwice > 0,
          "hand-authored map extracts a CCW outer loop");
}

void TestStrictMarkersAndShapes()
{
    Json valid = Json::parse(SaveText(MakeSquare()));
    for (const char* field : {"formatVersion", "topology", "coordSubdivisions"}) {
        Json changed = valid;
        changed.erase(field);
        ExpectRejected(changed, "missing marker is rejected");
    }
    Json changed = valid;
    changed["formatVersion"] = 1;
    ExpectRejected(changed, "format version 1 is rejected");
    changed = valid;
    changed["topology"] = "polygon";
    ExpectRejected(changed, "polygon topology marker is rejected");
    changed = valid;
    changed["coordSubdivisions"] = 8;
    ExpectRejected(changed, "wrong coordinate subdivisions are rejected");
    changed = valid;
    changed["coordSubdivisions"] = 16.0;
    ExpectRejected(changed, "floating coordinate subdivisions are rejected");

    for (const char* field : {"textures", "vertices", "linedefs", "sidedefs", "sectors"}) {
        changed = valid;
        changed.erase(field);
        ExpectRejected(changed, "missing required collection is rejected");
    }
    changed = valid;
    changed["vertices"] = Json::object();
    ExpectRejected(changed, "malformed topology array is rejected");
}

void TestStrictValuesAndValidation()
{
    const Json valid = Json::parse(SaveText(MakeSquare()));
    Json changed = valid;
    changed["vertices"][0]["x"] = 0.0;
    ExpectRejected(changed, "floating vertex x is rejected");
    changed = valid;
    changed["sidedefs"][0]["side"] = "outside";
    ExpectRejected(changed, "invalid side string is rejected");
    changed = valid;
    changed["sidedefs"][0].erase("wall");
    ExpectRejected(changed, "missing required sidedef field is rejected");
    changed = valid;
    changed["sectors"][0]["floorUv"]["scale"] = Json::array({1});
    ExpectRejected(changed, "malformed UV is rejected");
    changed = valid;
    changed["sectors"][0]["ambientColor"]["r"] = 256;
    ExpectRejected(changed, "out-of-range color is rejected");
    changed = valid;
    changed["textures"]["wall"]["filter"] = "nearest";
    ExpectRejected(changed, "invalid texture filter is rejected");
    changed = valid;
    changed["linedefs"][0]["startVertexId"] = 999;
    ExpectRejected(changed, "dangling vertex reference is rejected");
    changed = valid;
    changed["vertices"][1]["id"] = 1;
    ExpectRejected(changed, "duplicate IDs are rejected");

    changed = valid;
    changed["staticLights"] = Json::array({
            {
                    {"id", 1},
                    {"position", Json::array({1.0f, 2.0f, 3.0f})},
                    {"radius", 8.0f},
                    {"sourceRadius", 1.0f},
                    {"intensity", 1.0f},
                    {"color", {{"r", 255}, {"g", 220}, {"b", 180}, {"a", 255}}}
            },
            {
                    {"id", 1},
                    {"position", Json::array({4.0f, 5.0f, 6.0f})},
                    {"radius", 8.0f},
                    {"sourceRadius", 1.0f},
                    {"intensity", 1.0f},
                    {"color", {{"r", 255}, {"g", 220}, {"b", 180}, {"a", 255}}}
            }
    });
    ExpectRejected(changed, "duplicate static light IDs are rejected");
    changed["staticLights"][1]["id"] = 2;
    changed["staticLights"][0].erase("position");
    ExpectRejected(changed, "missing static light position is rejected");
    changed = valid;
    changed["staticLights"] = Json::array({
            {
                    {"id", 1},
                    {"position", Json::array({1.0f, 2.0f, 3.0f})},
                    {"radius", 0.0f},
                    {"sourceRadius", 0.0f},
                    {"intensity", 1.0f},
                    {"color", {{"r", 255}, {"g", 220}, {"b", 180}, {"a", 255}}}
            }
    });
    ExpectRejected(changed, "non-positive static light radius is rejected");
    changed["staticLights"][0]["radius"] = 4.0f;
    changed["staticLights"][0]["sourceRadius"] = 5.0f;
    ExpectRejected(changed, "oversized static light source radius is rejected");
    changed["staticLights"][0]["sourceRadius"] = 1.0f;
    changed["staticLights"][0]["color"]["r"] = 300;
    ExpectRejected(changed, "invalid static light color channel is rejected");
}

void TestTransactionalFailures()
{
    SectorTopologyMap output = MakeSquare();
    const size_t originalVertexCount = output.vertices.size();
    const int originalFirstX = output.vertices.front().x;
    std::string error;
    Check(!LoadText("{\"formatVersion\":2}", output, error), "invalid load fails");
    Check(!error.empty(), "failed load reports an error");
    Check(output.vertices.size() == originalVertexCount
                  && output.vertices.front().x == originalFirstX,
          "failed load leaves output map unchanged");

    SectorTopologyMap invalid = MakeSquare();
    invalid.lineDefs.front().startVertexId = 999;
    std::string jsonOutput = "sentinel";
    error.clear();
    Check(!game::SaveSectorTopologyMapToJsonString(invalid, jsonOutput, &error),
          "invalid map is rejected before save");
    Check(!error.empty(), "failed save reports an error");
    Check(jsonOutput == "sentinel", "failed save leaves JSON output unchanged");

    Json invalidLight = Json::parse(SaveText(MakeSquare()));
    invalidLight["staticLights"] = Json::array({
            {
                    {"id", 1},
                    {"position", Json::array({1.0f, 2.0f, 3.0f})},
                    {"radius", -1.0f},
                    {"sourceRadius", 0.0f},
                    {"intensity", 1.0f},
                    {"color", {{"r", 255}, {"g", 255}, {"b", 255}, {"a", 255}}}
            }
    });
    output = MakeSquare();
    output.staticLights.push_back(SectorTopologyStaticPointLight{
            42,
            Vector3{9.0f, 8.0f, 7.0f},
            WHITE,
            1.0f,
            12.0f,
            0.0f
    });
    error.clear();
    Check(!LoadText(invalidLight.dump(), output, error), "invalid static light load fails");
    Check(output.staticLights.size() == 1 && output.staticLights.front().id == 42,
          "failed static light load leaves output map unchanged");
}

void TestDeterministicOutput()
{
    SectorTopologyMap first = MakeSquare();
    SectorTopologyMap second = first;
    std::reverse(second.vertices.begin(), second.vertices.end());
    std::reverse(second.lineDefs.begin(), second.lineDefs.end());
    std::reverse(second.sideDefs.begin(), second.sideDefs.end());
    first.staticLights.push_back(SectorTopologyStaticPointLight{
            2, Vector3{2.0f, 3.0f, 4.0f}, WHITE, 1.0f, 8.0f, 0.0f});
    first.staticLights.push_back(SectorTopologyStaticPointLight{
            1, Vector3{1.0f, 2.0f, 3.0f}, Color{10, 20, 30, 255}, 2.0f, 16.0f, 1.0f});
    second.staticLights = first.staticLights;
    std::reverse(second.staticLights.begin(), second.staticLights.end());
    second.texturesById.clear();
    second.texturesById.emplace("ceiling", first.texturesById.at("ceiling"));
    second.texturesById.emplace("wall", first.texturesById.at("wall"));
    second.texturesById.emplace("floor", first.texturesById.at("floor"));
    Check(SaveText(first) == SaveText(second),
          "serialization is independent of vector and hash-map insertion order");
    Check(SaveText(first) == SaveText(first), "repeated serialization is identical");
}

void TestStaticLightHelpers()
{
    SectorTopologyMap map = MakeSquare();
    Check(game::AllocateSectorTopologyStaticLightId(map) == 1,
          "first topology static light ID is 1");
    map.staticLights.push_back(SectorTopologyStaticPointLight{
            4, Vector3{0.0f, 1.0f, 2.0f}, WHITE, 1.0f, 8.0f, 0.0f});
    map.staticLights.push_back(SectorTopologyStaticPointLight{
            2, Vector3{3.0f, 4.0f, 5.0f}, WHITE, 1.0f, 8.0f, 0.0f});
    Check(game::AllocateSectorTopologyStaticLightId(map) == 5,
          "topology static light allocation returns max plus one");
    Check(game::FindSectorTopologyStaticLight(map, 2) != nullptr,
          "topology static light lookup finds existing ID");
    Check(game::RemoveSectorTopologyStaticLight(map, 2),
          "topology static light delete succeeds for existing ID");
    Check(game::FindSectorTopologyStaticLight(map, 2) == nullptr
                  && game::FindSectorTopologyStaticLight(map, 4) != nullptr,
          "topology static light delete preserves remaining lights");
    Check(!game::RemoveSectorTopologyStaticLight(map, 99),
          "topology static light delete fails for unknown ID");
}

void TestIndependentFrontBackSidedefEdits()
{
    SectorTopologyMap map = MakeAdjacentSquares();
    SectorTopologySideDef* front = game::FindSectorTopologySideDef(map, 2);
    SectorTopologySideDef* back = game::FindSectorTopologySideDef(map, 8);
    Check(front != nullptr && back != nullptr, "shared linedef has editable front and back sidedefs");
    if (front == nullptr || back == nullptr) {
        return;
    }

    front->wall.textureId = "front_wall";
    front->wall.uv.scale = {2.0f, 3.0f};
    front->wall.uv.offset = {4.0f, 5.0f};
    back->wall.textureId = "back_wall";
    back->wall.uv.scale = {6.0f, 7.0f};
    back->wall.uv.offset = {8.0f, 9.0f};

    Check(front->wall.textureId != back->wall.textureId,
          "front and back sidedef textures can differ");
    Check(front->wall.uv.scale.x != back->wall.uv.scale.x
                  && front->wall.uv.offset.x != back->wall.uv.offset.x,
          "front and back sidedef UVs can differ");
    Check(!game::HasSectorTopologyValidationErrors(game::ValidateSectorTopologyMap(map)),
          "independently edited adjacent map validates");

    const std::string text = SaveText(map);
    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(text, loaded, error), "independently edited adjacent map reloads");
    const SectorTopologySideDef* loadedFront = game::FindSectorTopologySideDef(loaded, 2);
    const SectorTopologySideDef* loadedBack = game::FindSectorTopologySideDef(loaded, 8);
    Check(loadedFront != nullptr && loadedBack != nullptr,
          "round-tripped adjacent map retains shared sidedefs");
    if (loadedFront != nullptr && loadedBack != nullptr) {
        Check(loadedFront->wall.textureId == "front_wall"
                      && loadedFront->wall.uv.scale.x == 2.0f
                      && loadedFront->wall.uv.offset.y == 5.0f,
              "front sidedef edit round-trips");
        Check(loadedBack->wall.textureId == "back_wall"
                      && loadedBack->wall.uv.scale.y == 7.0f
                      && loadedBack->wall.uv.offset.x == 8.0f,
              "back sidedef edit round-trips");
    }
}

void TestOppositeSidedefLookup()
{
    const SectorTopologyMap map = MakeAdjacentSquares();
    const SectorTopologySideDef* frontOpposite = game::FindOppositeSectorTopologySideDef(map, 2);
    const SectorTopologySideDef* backOpposite = game::FindOppositeSectorTopologySideDef(map, 8);
    const SectorTopologySideDef* oneSidedOpposite = game::FindOppositeSectorTopologySideDef(map, 1);
    Check(frontOpposite != nullptr && frontOpposite->id == 8,
          "front sidedef finds back opposite");
    Check(backOpposite != nullptr && backOpposite->id == 2,
          "back sidedef finds front opposite");
    Check(oneSidedOpposite == nullptr,
          "one-sided linedef reports no opposite sidedef");
}

void TestFileApi()
{
    const std::filesystem::path path =
            std::filesystem::temp_directory_path() / "sector_topology_serialization_test.json";
    std::error_code removeError;
    std::filesystem::remove(path, removeError);

    std::string error;
    const SectorTopologyMap original = MakeSquare();
    Check(game::SaveSectorTopologyMap(path.string().c_str(), original, &error),
          "file save succeeds");
    SectorTopologyMap loaded;
    Check(game::LoadSectorTopologyMap(path.string().c_str(), loaded, &error),
          "file load succeeds");
    Check(loaded.vertices.size() == original.vertices.size(), "file API round-trips map");
    std::filesystem::remove(path, removeError);
}

} // namespace

int main()
{
    TestRoundTrip();
    TestTextureFilterSerialization();
    TestCeilingSkySerialization();
    TestStaticLightRoundTrip();
    TestLightmapMetadataRoundTrip();
    TestPreviewSettingsRoundTripAndValidation();
    TestSkySettingsRoundTripAndValidation();
    TestDecalDefaultsAndOmission();
    TestMiddleDefaultsAndOmission();
    TestMiddleRoundTrip();
    TestLineDefFlagsRoundTripAndDefaults();
    TestStrictLineDefFlagsValidation();
    TestDecalRoundTrip();
    TestStrictDecalValidation();
    TestStrictMiddleValidation();
    TestHandAuthoredJson();
    TestStrictMarkersAndShapes();
    TestStrictValuesAndValidation();
    TestTransactionalFailures();
    TestDeterministicOutput();
    TestStaticLightHelpers();
    TestIndependentFrontBackSidedefEdits();
    TestOppositeSidedefLookup();
    TestFileApi();

    if (failures != 0) {
        std::cerr << failures << " topology serialization test(s) failed\n";
        return 1;
    }
    std::cout << "Sector topology serialization tests passed\n";
    return 0;
}
