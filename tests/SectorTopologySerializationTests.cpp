#include "sector_demo/SectorTopologySerialization.h"
#include "sector_demo/SectorLightmap.h"
#include "game/SectorLevelLoader.h"
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
#include <vector>

namespace {

using game::SectorTextureDefinition;
using game::SectorTextureFilter;
using game::SectorSoundDefinition;
using game::SectorSoundType;
using game::SectorTopologyLineDef;
using game::SectorTopologyLoopSet;
using game::SectorTopologyMap;
using game::SectorTopologySector;
using game::SectorTopologySideDef;
using game::SectorTopologySideKind;
using game::SectorTopologyDynamicPointLight;
using game::SectorTopologyDynamicSpotLight;
using game::SectorPlacedRuntimeObject;
using game::SectorTopologyStaticPointLight;
using game::SectorTopologyStaticSpotLight;
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

bool Near(Vector2 actual, Vector2 expected, float epsilon = 0.00001f)
{
    return Near(actual.x, expected.x, epsilon)
            && Near(actual.y, expected.y, epsilon);
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

SectorPlacedRuntimeObject MakeBillboardRuntimeObject(
        int id,
        const char* spritePath,
        Vector3 position,
        float yawRadians)
{
    SectorPlacedRuntimeObject object;
    object.id = id;
    object.kind = "billboard";
    object.position = position;
    object.yawRadians = yawRadians;
    object.billboard.spriteAnimationPath = spritePath;
    return object;
}

SectorPlacedRuntimeObject MakeDoorRuntimeObject(int id)
{
    SectorPlacedRuntimeObject object;
    object.id = id;
    object.kind = "door";
    object.door.anchor.lineDefId = 2;
    object.door.anchor.frontSectorId = 1;
    object.door.anchor.backSectorId = 2;
    object.door.anchor.frontSideDefId = 2;
    object.door.anchor.backSideDefId = 8;
    object.door.anchor.endpointAX = 64;
    object.door.anchor.endpointAY = 0;
    object.door.anchor.endpointBX = 64;
    object.door.anchor.endpointBY = 64;
    object.door.width = 4.0f;
    object.door.height = 2.5f;
    object.door.thickness = 0.375f;
    object.door.normalOffset = 0.125f;
    object.door.heightOffsetWorld = -0.25f;
    object.door.motion = game::SectorDoorMotionType::SlideRight;
    object.door.openDistance = 3.0f;
    object.door.speed = 2.25f;
    object.door.initialOpenFraction = 0.5f;
    object.door.autoOpen = true;
    object.door.interactionDistance = 1.75f;
    object.door.autoOpenDistance = 2.5f;
    object.door.textureId = "industrial_door";
    object.door.openSoundId = "door_open";
    object.door.closeSoundId = "door_close";
    return object;
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

bool LoadAuthoringText(
        const std::string& text,
        game::SectorAuthoringDocument& document,
        std::string& error)
{
    error.clear();
    return game::LoadSectorAuthoringDocumentFromJsonString(text, document, &error);
}

std::string SaveText(const SectorTopologyMap& map)
{
    std::string text;
    std::string error;
    Check(game::SaveSectorTopologyMapToJsonString(map, text, &error), "test map serializes");
    Check(error.empty(), "successful serialization clears error");
    return text;
}

std::string SaveAuthoringText(const game::SectorAuthoringDocument& document)
{
    std::string text;
    std::string error;
    Check(game::SaveSectorAuthoringDocumentToJsonString(document, text, &error),
          "test authoring document serializes");
    Check(error.empty(), "successful authoring serialization clears error");
    return text;
}

game::SectorAuthoringDocument MakeAuthoringDocumentFromMap(const SectorTopologyMap& map)
{
    game::SectorAuthoringDocument document;
    document.graph = game::ImportSectorTopologyMapToAuthoringGraph(map);
    document.mapData.audioSettings = map.audioSettings;
    document.mapData.texturesById = map.texturesById;
    document.mapData.staticLights = map.staticLights;
    document.mapData.staticSpotLights = map.staticSpotLights;
    document.mapData.dynamicPointLights = map.dynamicPointLights;
    document.mapData.dynamicSpotLights = map.dynamicSpotLights;
    document.mapData.runtimeObjects = map.runtimeObjects;
    document.mapData.previewSettings = map.previewSettings;
    document.mapData.skySettings = map.skySettings;
    document.mapData.directionalLight = map.directionalLight;
    document.mapData.fogSettings = map.fogSettings;
    document.mapData.lightmapSettings = map.lightmapSettings;
    document.mapData.bakedLightmap = map.bakedLightmap;
    document.derivation = game::DeriveSectorTopologyMapFromAuthoringGraph(document.graph);
    return document;
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

void ExpectSaveRejected(const SectorTopologyMap& map, const char* description)
{
    std::string text;
    std::string error;
    Check(!game::SaveSectorTopologyMapToJsonString(map, text, &error), description);
    Check(!error.empty(), "rejected save reports an error");
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
    original.staticLights.back().castsShadow = false;
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
    Check(!saved["staticLights"][0].contains("castsShadow")
                  && saved["staticLights"][1]["castsShadow"] == false,
          "static light shadow defaults are omitted and disabled shadows are written");

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
                      && std::fabs(light->sourceRadius - 2.0f) <= 0.0001f
                      && !light->castsShadow,
              "round-tripped light preserves numeric and shadow properties");
    }
    const SectorTopologyStaticPointLight* defaultShadowLight =
            game::FindSectorTopologyStaticLight(loaded, 3);
    Check(defaultShadowLight != nullptr && defaultShadowLight->castsShadow,
          "missing static light shadow field loads enabled");
    Check(!game::HasSectorTopologyValidationErrors(game::ValidateSectorTopologyMap(loaded)),
          "topology with static lights validates");

    Json withoutLights = saved;
    withoutLights.erase("staticLights");
    SectorTopologyMap oldStyle;
    Check(LoadText(withoutLights.dump(), oldStyle, error), "omitted staticLights field is accepted");
    Check(oldStyle.staticLights.empty(), "omitted staticLights field loads empty");
}

void TestStaticSpotLightRoundTrip()
{
    SectorTopologyMap original = MakeSquare();
    original.staticSpotLights.push_back(SectorTopologyStaticSpotLight{
            17,
            Vector3{2.5f, 18.0f, -4.25f},
            Vector3{6.5f, 12.0f, -1.25f},
            Color{255, 120, 64, 255},
            3.0f,
            24.0f,
            15.0f,
            42.0f,
            1.25f
    });
    original.staticSpotLights.back().castsShadow = false;
    original.staticSpotLights.push_back(SectorTopologyStaticSpotLight{
            9,
            Vector3{-1.0f, 9.5f, 6.0f},
            Vector3{-1.0f, 9.5f, 6.0f},
            Color{40, 80, 200, 255},
            0.5f,
            12.0f,
            20.0f,
            35.0f,
            0.0f
    });

    const std::string text = SaveText(original);
    const Json saved = Json::parse(text);
    Check(saved["staticSpotLights"].is_array(), "static spot light array is written");
    Check(saved["staticSpotLights"][0]["id"].get<int>() == 9
                  && saved["staticSpotLights"][1]["id"].get<int>() == 17,
          "static spot lights serialize sorted by stable ID");
    Check(!saved["staticSpotLights"][0].contains("innerConeDegrees")
                  && !saved["staticSpotLights"][0].contains("outerConeDegrees"),
          "default static spot light cone fields are omitted");
    Check(Near(saved["staticSpotLights"][1]["innerConeDegrees"].get<float>(), 15.0f)
                  && Near(saved["staticSpotLights"][1]["outerConeDegrees"].get<float>(), 42.0f),
          "non-default static spot light cone fields are written");
    Check(!saved["staticSpotLights"][0].contains("castsShadow")
                  && saved["staticSpotLights"][1]["castsShadow"] == false,
          "static spot shadow defaults are omitted and disabled shadows are written");
    Check(!saved["staticSpotLights"][0].contains("enabled")
                  && !saved["staticSpotLights"][0].contains("flicker")
                  && !saved["staticSpotLights"][0].contains("flickerSpeed")
                  && !saved["staticSpotLights"][0].contains("flickerAmount"),
          "static spot light does not write dynamic runtime fields");

    SectorTopologyMap defaultMap = MakeSquare();
    SectorTopologyStaticSpotLight defaultLight;
    defaultLight.id = 21;
    defaultMap.staticSpotLights.push_back(defaultLight);
    const Json savedDefaults = Json::parse(SaveText(defaultMap));
    Check(Near(savedDefaults["staticSpotLights"][0]["position"][1].get<float>(),
               game::SectorWorldToAuthoringDistance(1.8f))
                  && Near(savedDefaults["staticSpotLights"][0]["target"][0].get<float>(),
                          game::SectorWorldToAuthoringDistance(4.0f))
                  && Near(savedDefaults["staticSpotLights"][0]["target"][1].get<float>(),
                          game::SectorWorldToAuthoringDistance(1.0f))
                  && Near(savedDefaults["staticSpotLights"][0]["range"].get<float>(),
                          game::SectorWorldToAuthoringDistance(8.0f))
                  && Near(savedDefaults["staticSpotLights"][0]["sourceRadius"].get<float>(), 0.0f)
                  && Near(savedDefaults["staticSpotLights"][0]["intensity"].get<float>(), 1.0f),
          "default static spot light authoring position target range source radius and intensity are stable");
    Check(!savedDefaults["staticSpotLights"][0].contains("innerConeDegrees")
                  && !savedDefaults["staticSpotLights"][0].contains("outerConeDegrees")
                  && !savedDefaults["staticSpotLights"][0].contains("castsShadow"),
          "default static spot light optional fields remain omitted");

    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(text, loaded, error), "static spot light topology JSON loads");
    Check(loaded.staticSpotLights.size() == 2, "static spot lights round-trip");
    const SectorTopologyStaticSpotLight* light = game::FindSectorTopologyStaticSpotLight(loaded, 17);
    Check(light != nullptr, "round-tripped static spot light can be found by stable ID");
    if (light != nullptr) {
        Check(Near(light->position, Vector3{2.5f, 18.0f, -4.25f})
                      && Near(light->target, Vector3{6.5f, 12.0f, -1.25f}),
              "round-tripped static spot light preserves position and target values");
        Check(light->color.r == 255 && light->color.g == 120 && light->color.b == 64,
              "round-tripped static spot light preserves color");
        Check(Near(light->intensity, 3.0f)
                      && Near(light->range, 24.0f)
                      && Near(light->innerConeDegrees, 15.0f)
                      && Near(light->outerConeDegrees, 42.0f)
                      && Near(light->sourceRadius, 1.25f)
                      && !light->castsShadow,
              "round-tripped static spot light preserves numeric and shadow properties");
    }
    const SectorTopologyStaticSpotLight* coincident =
            game::FindSectorTopologyStaticSpotLight(loaded, 9);
    Check(coincident != nullptr && Near(coincident->position, coincident->target)
                  && coincident->castsShadow,
          "round-tripped static spot light preserves coincident target and missing shadow default");
    Check(!game::HasSectorTopologyValidationErrors(game::ValidateSectorTopologyMap(loaded)),
          "topology with static spot lights validates");

    Json withoutLights = saved;
    withoutLights.erase("staticSpotLights");
    SectorTopologyMap oldStyle;
    Check(LoadText(withoutLights.dump(), oldStyle, error),
          "omitted staticSpotLights field is accepted");
    Check(oldStyle.staticSpotLights.empty(), "omitted staticSpotLights field loads empty");

    Json clamped = saved;
    clamped["staticSpotLights"][0]["innerConeDegrees"] = -20.0f;
    clamped["staticSpotLights"][0]["outerConeDegrees"] = 240.0f;
    Check(LoadText(clamped.dump(), loaded, error), "out-of-range static spot light cones load");
    const SectorTopologyStaticSpotLight* clampedLight =
            game::FindSectorTopologyStaticSpotLight(loaded, 9);
    Check(clampedLight != nullptr
                  && Near(clampedLight->innerConeDegrees, 0.0f)
                  && Near(clampedLight->outerConeDegrees, 179.0f),
          "out-of-range static spot light cones clamp on load");

    Json widened = saved;
    widened["staticSpotLights"][0]["innerConeDegrees"] = 80.0f;
    widened["staticSpotLights"][0]["outerConeDegrees"] = 20.0f;
    Check(LoadText(widened.dump(), loaded, error), "narrow outer static spot cone loads");
    const SectorTopologyStaticSpotLight* widenedLight =
            game::FindSectorTopologyStaticSpotLight(loaded, 9);
    Check(widenedLight != nullptr
                  && Near(widenedLight->innerConeDegrees, 80.0f)
                  && Near(widenedLight->outerConeDegrees, 80.0f),
          "outer static spot cone widens to inner cone on load");
}

void TestDynamicPointLightRoundTrip()
{
    SectorTopologyMap original = MakeSquare();
    original.dynamicPointLights.push_back(SectorTopologyDynamicPointLight{
            11,
            Vector3{2.5f, 18.0f, -4.25f},
            Color{255, 120, 64, 255},
            3.0f,
            24.0f,
            false
    });
    original.dynamicPointLights.push_back(SectorTopologyDynamicPointLight{
            5,
            Vector3{-1.0f, 9.5f, 6.0f},
            Color{40, 80, 200, 255},
            0.5f,
            12.0f,
            true,
            true,
            2.5f,
            0.65f
    });
    original.dynamicPointLights.back().castsShadow = true;
    original.dynamicPointLights.back().shadowPriority = 7;
    original.dynamicPointLights.back().shadowBias = 0.003f;
    original.dynamicPointLights.back().shadowStrength = 0.75f;
    original.dynamicPointLights.back().shadowSoftness = 2.0f;

    const std::string text = SaveText(original);
    const Json saved = Json::parse(text);
    Check(saved["dynamicPointLights"].is_array(), "dynamic point light array is written");
    Check(saved["dynamicPointLights"][0]["id"].get<int>() == 5
                  && saved["dynamicPointLights"][1]["id"].get<int>() == 11,
          "dynamic point lights serialize sorted by stable ID");
    Check(saved["dynamicPointLights"][0].find("enabled") == saved["dynamicPointLights"][0].end(),
          "default enabled dynamic point light omits enabled field");
    Check(saved["dynamicPointLights"][1]["enabled"] == false,
          "disabled dynamic point light writes enabled field");
    Check(saved["dynamicPointLights"][0]["flicker"] == true
                  && Near(saved["dynamicPointLights"][0]["flickerSpeed"].get<float>(), 2.5f)
                  && Near(saved["dynamicPointLights"][0]["flickerAmount"].get<float>(), 0.65f),
          "non-default dynamic point light flicker fields are written");
    Check(saved["dynamicPointLights"][0]["castsShadow"] == true
                  && saved["dynamicPointLights"][0]["shadowPriority"] == 7
                  && Near(saved["dynamicPointLights"][0]["shadowBias"].get<float>(), 0.003f)
                  && Near(saved["dynamicPointLights"][0]["shadowStrength"].get<float>(), 0.75f)
                  && Near(saved["dynamicPointLights"][0]["shadowSoftness"].get<float>(), 2.0f),
          "dynamic point shadow settings are written");
    Check(!saved["dynamicPointLights"][1].contains("flicker")
                  && !saved["dynamicPointLights"][1].contains("flickerSpeed")
                  && !saved["dynamicPointLights"][1].contains("flickerAmount"),
          "default dynamic point light flicker fields are omitted");

    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(text, loaded, error), "dynamic point light topology JSON loads");
    Check(loaded.dynamicPointLights.size() == 2, "dynamic point lights round-trip");
    const SectorTopologyDynamicPointLight* light = game::FindSectorTopologyDynamicLight(loaded, 11);
    Check(light != nullptr, "round-tripped dynamic point light can be found by stable ID");
    if (light != nullptr) {
        Check(std::fabs(light->position.x - 2.5f) <= 0.0001f
                      && std::fabs(light->position.y - 18.0f) <= 0.0001f
                      && std::fabs(light->position.z + 4.25f) <= 0.0001f,
              "round-tripped dynamic point light preserves authored position values");
        Check(light->color.r == 255 && light->color.g == 120 && light->color.b == 64,
              "round-tripped dynamic point light preserves color");
        Check(std::fabs(light->intensity - 3.0f) <= 0.0001f
                      && std::fabs(light->radius - 24.0f) <= 0.0001f
                      && !light->enabled
                      && !light->flicker
                      && Near(light->flickerSpeed, game::DynamicLightFlickerDefaultSpeed)
                      && Near(light->flickerAmount, game::DynamicLightFlickerDefaultAmount),
              "round-tripped dynamic point light preserves properties and missing flicker defaults");
    }
    const SectorTopologyDynamicPointLight* flickerLight = game::FindSectorTopologyDynamicLight(loaded, 5);
    Check(flickerLight != nullptr
                  && flickerLight->flicker
                  && Near(flickerLight->flickerSpeed, 2.5f)
                  && Near(flickerLight->flickerAmount, 0.65f)
                  && flickerLight->castsShadow
                  && flickerLight->shadowPriority == 7
                  && Near(flickerLight->shadowBias, 0.003f)
                  && Near(flickerLight->shadowStrength, 0.75f)
                  && Near(flickerLight->shadowSoftness, 2.0f),
          "round-tripped dynamic point light preserves flicker and shadow settings");
    Check(!game::HasSectorTopologyValidationErrors(game::ValidateSectorTopologyMap(loaded)),
          "topology with dynamic point lights validates");

    Json withoutLights = saved;
    withoutLights.erase("dynamicPointLights");
    SectorTopologyMap oldStyle;
    Check(LoadText(withoutLights.dump(), oldStyle, error),
          "omitted dynamicPointLights field is accepted");
    Check(oldStyle.dynamicPointLights.empty(), "omitted dynamicPointLights field loads empty");
}

void TestDynamicSpotLightRoundTrip()
{
    SectorTopologyMap original = MakeSquare();
    original.dynamicSpotLights.push_back(SectorTopologyDynamicSpotLight{
            13,
            Vector3{2.5f, 18.0f, -4.25f},
            Vector3{6.5f, 12.0f, -1.25f},
            Color{255, 120, 64, 255},
            3.0f,
            24.0f,
            15.0f,
            42.0f,
            false
    });
    original.dynamicSpotLights.push_back(SectorTopologyDynamicSpotLight{
            7,
            Vector3{-1.0f, 9.5f, 6.0f},
            Vector3{-1.0f, 9.5f, 6.0f},
            Color{40, 80, 200, 255},
            0.5f,
            12.0f,
            20.0f,
            35.0f,
            true,
            true,
            2.5f,
            0.65f,
            true,
            17,
            0.015f,
            0.75f,
            2.5f
    });

    const std::string text = SaveText(original);
    const Json saved = Json::parse(text);
    Check(saved["dynamicSpotLights"].is_array(), "dynamic spot light array is written");
    Check(saved["dynamicSpotLights"][0]["id"].get<int>() == 7
                  && saved["dynamicSpotLights"][1]["id"].get<int>() == 13,
          "dynamic spot lights serialize sorted by stable ID");
    Check(saved["dynamicSpotLights"][0].find("enabled") == saved["dynamicSpotLights"][0].end(),
          "default enabled dynamic spot light omits enabled field");
    Check(saved["dynamicSpotLights"][1]["enabled"] == false,
          "disabled dynamic spot light writes enabled field");
    Check(!saved["dynamicSpotLights"][0].contains("innerConeDegrees")
                  && !saved["dynamicSpotLights"][0].contains("outerConeDegrees"),
          "default dynamic spot light cone fields are omitted");
    Check(Near(saved["dynamicSpotLights"][1]["innerConeDegrees"].get<float>(), 15.0f)
                  && Near(saved["dynamicSpotLights"][1]["outerConeDegrees"].get<float>(), 42.0f),
          "non-default dynamic spot light cone fields are written");
    Check(saved["dynamicSpotLights"][0]["flicker"] == true
                  && Near(saved["dynamicSpotLights"][0]["flickerSpeed"].get<float>(), 2.5f)
                  && Near(saved["dynamicSpotLights"][0]["flickerAmount"].get<float>(), 0.65f),
          "non-default dynamic spot light flicker fields are written");
    Check(!saved["dynamicSpotLights"][1].contains("flicker")
                  && !saved["dynamicSpotLights"][1].contains("flickerSpeed")
                  && !saved["dynamicSpotLights"][1].contains("flickerAmount"),
          "default dynamic spot light flicker fields are omitted");
    Check(saved["dynamicSpotLights"][0]["castsShadow"] == true
                  && saved["dynamicSpotLights"][0]["shadowPriority"] == 17
                  && Near(saved["dynamicSpotLights"][0]["shadowBias"].get<float>(), 0.015f)
                  && Near(saved["dynamicSpotLights"][0]["shadowStrength"].get<float>(), 0.75f)
                  && Near(saved["dynamicSpotLights"][0]["shadowSoftness"].get<float>(), 2.5f),
          "non-default dynamic spot light shadow fields are written");
    Check(!saved["dynamicSpotLights"][1].contains("castsShadow")
                  && !saved["dynamicSpotLights"][1].contains("shadowPriority")
                  && !saved["dynamicSpotLights"][1].contains("shadowBias")
                  && !saved["dynamicSpotLights"][1].contains("shadowStrength")
                  && !saved["dynamicSpotLights"][1].contains("shadowSoftness"),
          "default dynamic spot light shadow fields are omitted");

    SectorTopologyMap defaultMap = MakeSquare();
    SectorTopologyDynamicSpotLight defaultLight;
    defaultLight.id = 21;
    defaultMap.dynamicSpotLights.push_back(defaultLight);
    const Json savedDefaults = Json::parse(SaveText(defaultMap));
    Check(Near(savedDefaults["dynamicSpotLights"][0]["position"][1].get<float>(),
               game::SectorWorldToAuthoringDistance(1.8f))
                  && Near(savedDefaults["dynamicSpotLights"][0]["target"][0].get<float>(),
                          game::SectorWorldToAuthoringDistance(4.0f))
                  && Near(savedDefaults["dynamicSpotLights"][0]["target"][1].get<float>(),
                          game::SectorWorldToAuthoringDistance(1.0f))
                  && Near(savedDefaults["dynamicSpotLights"][0]["range"].get<float>(),
                          game::SectorWorldToAuthoringDistance(8.0f))
                  && Near(savedDefaults["dynamicSpotLights"][0]["intensity"].get<float>(), 1.0f),
          "default dynamic spot light authoring position target range and intensity are stable");
    Check(!savedDefaults["dynamicSpotLights"][0].contains("enabled")
                  && !savedDefaults["dynamicSpotLights"][0].contains("innerConeDegrees")
                  && !savedDefaults["dynamicSpotLights"][0].contains("outerConeDegrees")
                  && !savedDefaults["dynamicSpotLights"][0].contains("flicker")
                  && !savedDefaults["dynamicSpotLights"][0].contains("castsShadow")
                  && !savedDefaults["dynamicSpotLights"][0].contains("shadowPriority")
                  && !savedDefaults["dynamicSpotLights"][0].contains("shadowBias")
                  && !savedDefaults["dynamicSpotLights"][0].contains("shadowStrength")
                  && !savedDefaults["dynamicSpotLights"][0].contains("shadowSoftness"),
          "default dynamic spot light optional fields remain omitted");

    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(text, loaded, error), "dynamic spot light topology JSON loads");
    Check(loaded.dynamicSpotLights.size() == 2, "dynamic spot lights round-trip");
    const SectorTopologyDynamicSpotLight* light = game::FindSectorTopologyDynamicSpotLight(loaded, 13);
    Check(light != nullptr, "round-tripped dynamic spot light can be found by stable ID");
    if (light != nullptr) {
        Check(Near(light->position, Vector3{2.5f, 18.0f, -4.25f})
                      && Near(light->target, Vector3{6.5f, 12.0f, -1.25f}),
              "round-tripped dynamic spot light preserves position and target values");
        Check(light->color.r == 255 && light->color.g == 120 && light->color.b == 64,
              "round-tripped dynamic spot light preserves color");
        Check(Near(light->intensity, 3.0f)
                      && Near(light->range, 24.0f)
                      && Near(light->innerConeDegrees, 15.0f)
                      && Near(light->outerConeDegrees, 42.0f)
                      && !light->enabled
                      && !light->flicker
                      && Near(light->flickerSpeed, game::DynamicLightFlickerDefaultSpeed)
                      && Near(light->flickerAmount, game::DynamicLightFlickerDefaultAmount)
                      && !light->castsShadow
                      && light->shadowPriority == game::DynamicSpotLightDefaultShadowPriority
                      && Near(light->shadowBias, game::DynamicSpotLightDefaultShadowBias)
                      && Near(light->shadowStrength, game::DynamicSpotLightDefaultShadowStrength)
                      && Near(light->shadowSoftness, game::DynamicSpotLightDefaultShadowSoftness),
              "round-tripped dynamic spot light preserves properties and missing optional defaults");
    }
    const SectorTopologyDynamicSpotLight* flickerLight =
            game::FindSectorTopologyDynamicSpotLight(loaded, 7);
    Check(flickerLight != nullptr
                  && Near(flickerLight->position, flickerLight->target)
                  && flickerLight->flicker
                  && Near(flickerLight->flickerSpeed, 2.5f)
                  && Near(flickerLight->flickerAmount, 0.65f)
                  && flickerLight->castsShadow
                  && flickerLight->shadowPriority == 17
                  && Near(flickerLight->shadowBias, 0.015f)
                  && Near(flickerLight->shadowStrength, 0.75f)
                  && Near(flickerLight->shadowSoftness, 2.5f),
          "round-tripped dynamic spot light preserves flicker shadow settings and coincident target");
    Check(!game::HasSectorTopologyValidationErrors(game::ValidateSectorTopologyMap(loaded)),
          "topology with dynamic spot lights validates");

    Json withoutLights = saved;
    withoutLights.erase("dynamicSpotLights");
    SectorTopologyMap oldStyle;
    Check(LoadText(withoutLights.dump(), oldStyle, error),
          "omitted dynamicSpotLights field is accepted");
    Check(oldStyle.dynamicSpotLights.empty(), "omitted dynamicSpotLights field loads empty");

    Json clamped = saved;
    clamped["dynamicSpotLights"][0]["innerConeDegrees"] = -20.0f;
    clamped["dynamicSpotLights"][0]["outerConeDegrees"] = 240.0f;
    clamped["dynamicSpotLights"][0]["flickerSpeed"] = 500.0f;
    clamped["dynamicSpotLights"][0]["flickerAmount"] = -2.0f;
    clamped["dynamicSpotLights"][0]["shadowPriority"] = 5000;
    clamped["dynamicSpotLights"][0]["shadowBias"] = 1.0f;
    clamped["dynamicSpotLights"][0]["shadowStrength"] = -1.0f;
    clamped["dynamicSpotLights"][0]["shadowSoftness"] = 100.0f;
    Check(LoadText(clamped.dump(), loaded, error), "out-of-range dynamic spot light fields load");
    const SectorTopologyDynamicSpotLight* clampedLight =
            game::FindSectorTopologyDynamicSpotLight(loaded, 7);
    Check(clampedLight != nullptr
                  && Near(clampedLight->innerConeDegrees, 0.0f)
                  && Near(clampedLight->outerConeDegrees, 179.0f)
                  && Near(clampedLight->flickerSpeed, game::DynamicLightFlickerMaxSpeed)
                  && Near(clampedLight->flickerAmount, game::DynamicLightFlickerMinAmount)
                  && clampedLight->shadowPriority == game::DynamicSpotLightMaxShadowPriority
                  && Near(clampedLight->shadowBias, game::DynamicSpotLightMaxShadowBias)
                  && Near(clampedLight->shadowStrength, game::DynamicSpotLightMinShadowStrength)
                  && Near(clampedLight->shadowSoftness, game::DynamicSpotLightMaxShadowSoftness),
          "out-of-range dynamic spot light fields clamp on load");

    clamped["dynamicSpotLights"][0]["shadowSoftness"] = -5.0f;
    Check(LoadText(clamped.dump(), loaded, error), "negative dynamic spot light shadow softness loads");
    clampedLight = game::FindSectorTopologyDynamicSpotLight(loaded, 7);
    Check(clampedLight != nullptr
                  && Near(clampedLight->shadowSoftness, game::DynamicSpotLightMinShadowSoftness),
          "negative dynamic spot light shadow softness clamps on load");

    Json widened = saved;
    widened["dynamicSpotLights"][0]["innerConeDegrees"] = 80.0f;
    widened["dynamicSpotLights"][0]["outerConeDegrees"] = 20.0f;
    Check(LoadText(widened.dump(), loaded, error), "narrow outer dynamic spot cone loads");
    const SectorTopologyDynamicSpotLight* widenedLight =
            game::FindSectorTopologyDynamicSpotLight(loaded, 7);
    Check(widenedLight != nullptr
                  && Near(widenedLight->innerConeDegrees, 80.0f)
                  && Near(widenedLight->outerConeDegrees, 80.0f),
          "outer cone widens to inner cone on load");
}

void TestLightAtmosphereRoundTripAndDefaultOmission()
{
    const game::SectorLightHazeSettings hazeDefaults;
    Check(Near(hazeDefaults.noiseAmount, 0.65f)
                  && Near(hazeDefaults.noiseScaleWorld, 0.5f)
                  && Near(hazeDefaults.heightOffsetWorld, 0.0f)
                  && Near(hazeDefaults.flowSpeedWorld, 0.20f),
          "light haze defaults provide readable coherent movement");

    SectorTopologyMap defaults = MakeSquare();
    defaults.staticLights.push_back(SectorTopologyStaticPointLight{});
    defaults.staticLights.back().id = 1;
    defaults.staticSpotLights.push_back(SectorTopologyStaticSpotLight{});
    defaults.staticSpotLights.back().id = 2;
    defaults.dynamicPointLights.push_back(SectorTopologyDynamicPointLight{});
    defaults.dynamicPointLights.back().id = 3;
    defaults.dynamicSpotLights.push_back(SectorTopologyDynamicSpotLight{});
    defaults.dynamicSpotLights.back().id = 4;
    const Json defaultJson = Json::parse(SaveText(defaults));
    Check(!defaultJson["staticLights"][0].contains("atmosphere")
                  && !defaultJson["staticSpotLights"][0].contains("atmosphere")
                  && !defaultJson["dynamicPointLights"][0].contains("atmosphere")
                  && !defaultJson["dynamicSpotLights"][0].contains("atmosphere"),
          "default-disabled light atmosphere is omitted for every light variant");
    SectorTopologyMap defaultOffset = MakeSquare();
    defaultOffset.staticLights.push_back(SectorTopologyStaticPointLight{});
    defaultOffset.staticLights.back().id = 5;
    defaultOffset.staticLights.back().atmosphere.haze.enabled = true;
    const Json defaultOffsetJson = Json::parse(SaveText(defaultOffset));
    Check(!defaultOffsetJson["staticLights"][0]["atmosphere"]["haze"].contains(
                  "heightOffsetWorld"),
          "default zero haze height offset is omitted");
    SectorTopologyMap defaultOpacity = MakeSquare();
    defaultOpacity.staticSpotLights.push_back(SectorTopologyStaticSpotLight{});
    defaultOpacity.staticSpotLights.back().id = 6;
    defaultOpacity.staticSpotLights.back().atmosphere.proxy.halo.enabled = true;
    defaultOpacity.staticSpotLights.back().atmosphere.proxy.shaft.enabled = true;
    const Json defaultOpacityJson = Json::parse(SaveText(defaultOpacity));
    Check(!defaultOpacityJson["staticSpotLights"][0]["atmosphere"]["proxy"]["halo"].contains(
                      "maxExtinction")
                  && !defaultOpacityJson["staticSpotLights"][0]["atmosphere"]["proxy"]["halo"].contains(
                          "centerOffsetWorld")
                  && !defaultOpacityJson["staticSpotLights"][0]["atmosphere"]["proxy"]["shaft"].contains(
                          "maxExtinction"),
          "default proxy maximum extinction and halo offset are omitted");

    game::SectorLightAtmosphereSettings atmosphere;
    atmosphere.haze.enabled = true;
    atmosphere.haze.extentScale = 0.75f;
    atmosphere.haze.heightOffsetWorld = 0.65f;
    atmosphere.haze.density = 0.125f;
    atmosphere.haze.scatteringTint = Color{210, 225, 240, 255};
    atmosphere.haze.edgeSoftness = 0.45f;
    atmosphere.haze.noiseAmount = 0.6f;
    atmosphere.haze.noiseScaleWorld = 2.5f;
    atmosphere.haze.flowDirectionDegrees = 35.0f;
    atmosphere.haze.flowSpeedWorld = 0.08f;
    atmosphere.proxy.halo.enabled = true;
    atmosphere.proxy.halo.radiusWorld = 1.25f;
    atmosphere.proxy.halo.centerOffsetWorld = Vector3{0.25f, -0.5f, 0.75f};
    atmosphere.proxy.halo.brightness = 0.4f;
    atmosphere.proxy.halo.maxExtinction = 0.6f;
    atmosphere.proxy.halo.scatteringTint = Color{180, 220, 255, 255};
    atmosphere.proxy.shaft.enabled = true;
    atmosphere.proxy.shaft.lengthScale = 0.8f;
    atmosphere.proxy.shaft.widthScale = 0.6f;
    atmosphere.proxy.shaft.maxExtinction = 0.45f;
    atmosphere.proxy.shaft.scatteringTint = Color{255, 190, 140, 255};
    atmosphere.dust.enabled = true;
    atmosphere.dust.amount = 47;
    atmosphere.dust.extentScale = 0.9f;
    atmosphere.dust.minimumSizeWorld = 0.01f;
    atmosphere.dust.maximumSizeWorld = 0.04f;
    atmosphere.dust.opacity = 0.3f;
    atmosphere.dust.driftSpeedWorld = 0.035f;
    atmosphere.dust.turbulenceWorld = 0.02f;
    atmosphere.dust.scatteringTint = Color{255, 235, 200, 255};
    defaults.staticLights[0].atmosphere = atmosphere;
    defaults.staticSpotLights[0].atmosphere = atmosphere;
    defaults.dynamicPointLights[0].atmosphere = atmosphere;
    defaults.dynamicSpotLights[0].atmosphere = atmosphere;

    const std::string text = SaveText(defaults);
    const Json saved = Json::parse(text);
    const bool allVariantsHaveAtmosphere = saved["staticLights"][0].contains("atmosphere")
            && saved["staticSpotLights"][0].contains("atmosphere")
            && saved["dynamicPointLights"][0].contains("atmosphere")
            && saved["dynamicSpotLights"][0].contains("atmosphere");
    Check(allVariantsHaveAtmosphere,
          "enabled haze and dust serialize for every light variant");
    if (allVariantsHaveAtmosphere) {
        const Json& staticAtmosphere = saved["staticLights"][0]["atmosphere"];
        Check(staticAtmosphere.contains("haze") && staticAtmosphere.contains("proxy")
                      && staticAtmosphere.contains("dust"),
              "enabled atmosphere serializes haze, proxy, and dust blocks");
        if (staticAtmosphere.contains("haze") && staticAtmosphere.contains("proxy")
                && staticAtmosphere.contains("dust")) {
            Check(staticAtmosphere["haze"].value("enabled", false)
                          && Near(staticAtmosphere["haze"].value("heightOffsetWorld", 0.0f), 0.65f)
                          && staticAtmosphere["proxy"]["halo"].value("enabled", false)
                          && staticAtmosphere["proxy"]["halo"].contains("centerOffsetWorld")
                          && Near(staticAtmosphere["proxy"]["halo"].value("maxExtinction", 0.0f), 0.6f)
                          && staticAtmosphere["proxy"]["halo"].contains("scatteringTint")
                          && staticAtmosphere["proxy"]["shaft"].value("enabled", false)
                          && Near(staticAtmosphere["proxy"]["shaft"].value("maxExtinction", 0.0f), 0.45f)
                          && staticAtmosphere["proxy"]["shaft"].contains("scatteringTint")
                          && !staticAtmosphere["proxy"].contains("tint")
                          && staticAtmosphere["dust"].value("enabled", false),
                  "enabled atmosphere flags are serialized");
        }
    }

    SectorTopologyMap loaded;
    std::string error;
    Json omittedNoise = defaultJson;
    omittedNoise["staticLights"][0]["atmosphere"]["haze"] = Json{{"enabled", true}};
    Check(LoadText(omittedNoise.dump(), loaded, error),
          "light haze with omitted noise settings loads");
    Check(loaded.staticLights.size() == 1
                  && Near(loaded.staticLights[0].atmosphere.haze.noiseAmount, 0.65f)
                  && Near(loaded.staticLights[0].atmosphere.haze.noiseScaleWorld, 0.5f)
                  && Near(loaded.staticLights[0].atmosphere.haze.heightOffsetWorld, 0.0f)
                  && Near(loaded.staticLights[0].atmosphere.haze.flowSpeedWorld, 0.20f),
          "omitted light haze noise settings use the new global defaults");
    Check(LoadText(text, loaded, error), "light atmosphere JSON loads");
    const auto checkAtmosphere = [](const game::SectorLightAtmosphereSettings& value) {
        return value.haze.enabled
                && Near(value.haze.extentScale, 0.75f)
                && Near(value.haze.heightOffsetWorld, 0.65f)
                && Near(value.haze.density, 0.125f)
                && value.haze.scatteringTint.r == 210
                && Near(value.haze.flowDirectionDegrees, 35.0f)
                && Near(value.haze.flowSpeedWorld, 0.08f)
                && value.proxy.halo.enabled
                && Near(value.proxy.halo.radiusWorld, 1.25f)
                && Near(value.proxy.halo.centerOffsetWorld.x, 0.25f)
                && Near(value.proxy.halo.centerOffsetWorld.y, -0.5f)
                && Near(value.proxy.halo.centerOffsetWorld.z, 0.75f)
                && Near(value.proxy.halo.maxExtinction, 0.6f)
                && value.proxy.halo.scatteringTint.g == 220
                && value.proxy.shaft.enabled
                && Near(value.proxy.shaft.lengthScale, 0.8f)
                && Near(value.proxy.shaft.maxExtinction, 0.45f)
                && value.proxy.shaft.scatteringTint.g == 190
                && value.dust.enabled
                && value.dust.amount == 47
                && Near(value.dust.extentScale, 0.9f)
                && Near(value.dust.minimumSizeWorld, 0.01f)
                && Near(value.dust.maximumSizeWorld, 0.04f)
                && Near(value.dust.opacity, 0.3f)
                && value.dust.scatteringTint.g == 235;
    };
    Check(loaded.staticLights.size() == 1 && checkAtmosphere(loaded.staticLights[0].atmosphere),
          "static point atmosphere round-trips");
    Check(loaded.staticSpotLights.size() == 1 && checkAtmosphere(loaded.staticSpotLights[0].atmosphere),
          "static spot atmosphere round-trips");
    Check(loaded.dynamicPointLights.size() == 1 && checkAtmosphere(loaded.dynamicPointLights[0].atmosphere),
          "dynamic point atmosphere round-trips");
    Check(loaded.dynamicSpotLights.size() == 1 && checkAtmosphere(loaded.dynamicSpotLights[0].atmosphere),
          "dynamic spot atmosphere round-trips");

    Json omittedExtinction = defaultJson;
    omittedExtinction["staticSpotLights"][0]["atmosphere"]["proxy"] = Json{
            {"halo", {{"enabled", true}}},
            {"shaft", {{"enabled", true}}}};
    Check(LoadText(omittedExtinction.dump(), loaded, error),
          "proxy settings with omitted maximum extinction load");
    Check(loaded.staticSpotLights.size() == 1
                  && Near(loaded.staticSpotLights[0].atmosphere.proxy.halo.maxExtinction, 0.03f)
                  && Near(loaded.staticSpotLights[0].atmosphere.proxy.shaft.maxExtinction, 0.08f),
          "omitted proxy maximum extinction uses restrained defaults");

    game::SectorLightProxySettings invalidExtinction;
    invalidExtinction.halo.maxExtinction = -2.0f;
    invalidExtinction.halo.centerOffsetWorld = Vector3{
            std::numeric_limits<float>::infinity(), -200000.0f, 200000.0f};
    invalidExtinction.shaft.maxExtinction = 3.0f;
    invalidExtinction = game::NormalizeSectorLightProxySettings(invalidExtinction);
    Check(Near(invalidExtinction.halo.maxExtinction, 0.0f)
                  && Near(invalidExtinction.shaft.maxExtinction, 1.0f)
                  && Near(invalidExtinction.halo.centerOffsetWorld.x, 0.0f)
                  && Near(invalidExtinction.halo.centerOffsetWorld.y, -100000.0f)
                  && Near(invalidExtinction.halo.centerOffsetWorld.z, 100000.0f),
          "proxy extinction and halo center offset are normalized to safe ranges");

    Json legacyProxy = defaultJson;
    legacyProxy["staticSpotLights"][0]["atmosphere"]["proxy"] = Json{
            {"tint", {{"r", 170}, {"g", 200}, {"b", 230}, {"a", 255}}},
            {"halo", {{"enabled", true}, {"maxOpacity", 0.7f}}},
            {"shaft", {
                    {"enabled", true},
                    {"maxOpacity", 0.6f},
                    {"maxExtinction", 0.2f},
                    {"scatteringTint", {{"r", 240}, {"g", 180}, {"b", 120}, {"a", 255}}}}}};
    Check(LoadText(legacyProxy.dump(), loaded, error),
          "legacy proxy tint and maximum opacity load");
    const auto& migrated = loaded.staticSpotLights[0].atmosphere.proxy;
    Check(migrated.halo.scatteringTint.r == 170
                  && migrated.shaft.scatteringTint.r == 240
                  && Near(migrated.halo.maxExtinction, 0.7f)
                  && Near(migrated.shaft.maxExtinction, 0.2f),
          "legacy proxy values migrate to both effects and new fields take precedence");
    const Json migratedJson = Json::parse(SaveText(loaded));
    const Json& migratedProxy = migratedJson["staticSpotLights"][0]["atmosphere"]["proxy"];
    Check(!migratedProxy.contains("tint")
                  && !migratedProxy["halo"].contains("maxOpacity")
                  && !migratedProxy["shaft"].contains("maxOpacity")
                  && migratedProxy["halo"].contains("scatteringTint")
                  && migratedProxy["shaft"].contains("scatteringTint"),
          "saving migrated proxies emits only the new schema");
}

void TestRuntimeObjectsRoundTripAndValidation()
{
    SectorTopologyMap empty = MakeSquare();
    const Json emptySaved = Json::parse(SaveText(empty));
    Check(!emptySaved.contains("runtimeObjects"), "empty runtime object list is omitted");

    SectorTopologyMap missingLoaded;
    std::string error;
    Check(LoadText(emptySaved.dump(), missingLoaded, error), "missing runtimeObjects field loads");
    Check(missingLoaded.runtimeObjects.empty(), "missing runtimeObjects loads empty");

    SectorTopologyMap original = MakeSquare();
    original.runtimeObjects.push_back(MakeBillboardRuntimeObject(
            12,
            "assets/sprites/torch/torch.json",
            Vector3{4.5f, 1.25f, -8.0f},
            1.57079632679f));
    original.runtimeObjects.push_back(MakeBillboardRuntimeObject(
            3,
            "assets/sprites/crate/crate.json",
            Vector3{-2.0f, 0.0f, 5.5f},
            -0.78539816339f));

    const Json saved = Json::parse(SaveText(original));
    Check(saved["runtimeObjects"].is_array(), "runtime object array is written");
    Check(saved["runtimeObjects"][0]["id"].get<int>() == 3
                  && saved["runtimeObjects"][1]["id"].get<int>() == 12,
          "runtime objects serialize sorted by stable ID");
    Check(saved["runtimeObjects"][1]["kind"].get<std::string>() == "billboard"
                  && saved["runtimeObjects"][1]["billboard"]["spriteAnimationPath"]
                             .get<std::string>() == "assets/sprites/torch/torch.json"
                  && Near(saved["runtimeObjects"][1]["position"][0].get<float>(), 4.5f)
                  && Near(saved["runtimeObjects"][1]["position"][1].get<float>(), 1.25f)
                  && Near(saved["runtimeObjects"][1]["position"][2].get<float>(), -8.0f)
                  && Near(saved["runtimeObjects"][1]["yawDegrees"].get<float>(), 90.0f),
          "runtime object fields are serialized");

    SectorTopologyMap loaded;
    Check(LoadText(saved.dump(), loaded, error), "runtime object JSON loads");
    Check(loaded.runtimeObjects.size() == 2, "runtime objects round-trip");
    const SectorPlacedRuntimeObject* billboard = game::FindSectorPlacedRuntimeObject(loaded, 12);
    Check(billboard != nullptr, "round-tripped runtime object can be found by stable ID");
    if (billboard != nullptr) {
        Check(billboard->kind == "billboard"
                      && billboard->definitionId.empty()
                      && billboard->billboard.spriteAnimationPath == "assets/sprites/torch/torch.json"
                      && Near(billboard->position, Vector3{4.5f, 1.25f, -8.0f})
                      && Near(billboard->yawRadians, 1.57079632679f),
              "round-tripped runtime object preserves billboard payload position and yaw");
    }
    Check(game::AllocateSectorPlacedRuntimeObjectId(loaded) == 13,
          "runtime object allocator returns next stable ID");

    SectorTopologyMap placeholderMap = MakeSquare();
    placeholderMap.runtimeObjects.push_back(MakeBillboardRuntimeObject(
            14,
            "",
            Vector3{1.0f, 0.0f, 1.0f},
            0.0f));
    const Json placeholderSaved = Json::parse(SaveText(placeholderMap));
    Check(placeholderSaved["runtimeObjects"][0]["billboard"]["spriteAnimationPath"]
                  .get<std::string>().empty(),
          "empty billboard sprite path placeholder is serialized");
    SectorTopologyMap placeholderLoaded;
    Check(LoadText(placeholderSaved.dump(), placeholderLoaded, error),
          "empty billboard sprite path placeholder loads");
    const SectorPlacedRuntimeObject* placeholder =
            game::FindSectorPlacedRuntimeObject(placeholderLoaded, 14);
    Check(placeholder != nullptr
                  && placeholder->kind == "billboard"
                  && placeholder->billboard.spriteAnimationPath.empty(),
          "empty billboard sprite path placeholder round-trips");

    const game::SectorAuthoringDocument placeholderDocument =
            MakeAuthoringDocumentFromMap(placeholderMap);
    const Json placeholderAuthoringSaved = Json::parse(SaveAuthoringText(placeholderDocument));
    Check(placeholderAuthoringSaved["runtimeObjects"][0]["billboard"]["spriteAnimationPath"]
                  .get<std::string>().empty(),
          "graph-native save writes empty billboard sprite path placeholder");
    game::SectorAuthoringDocument placeholderAuthoringLoaded;
    Check(LoadAuthoringText(placeholderAuthoringSaved.dump(), placeholderAuthoringLoaded, error),
          "graph-native empty billboard sprite path placeholder loads");
    Check(placeholderAuthoringLoaded.mapData.runtimeObjects.size() == 1
                  && placeholderAuthoringLoaded.mapData.runtimeObjects[0].billboard.spriteAnimationPath.empty(),
          "graph-native empty billboard sprite path placeholder survives load");

    SectorTopologyMap staticModelMap = MakeSquare();
    SectorPlacedRuntimeObject staticModel;
    staticModel.id = 18;
    staticModel.kind = "static_model";
    staticModel.position = Vector3{12.0f, -2.5f, 20.0f};
    staticModel.yawRadians = 0.5f;
    staticModel.staticModel.modelPath = "assets/models/props/nested/crate.glb";
    staticModel.staticModel.rotationXRadians = 0.25f;
    staticModel.staticModel.rotationZRadians = -0.75f;
    staticModel.staticModel.heightOffsetWorld = 0.75f;
    staticModel.staticModel.scale = 1.5f;
    staticModel.staticModel.collision = true;
    staticModelMap.runtimeObjects.push_back(staticModel);
    const Json staticModelSaved = Json::parse(SaveText(staticModelMap));
    Check(staticModelSaved["runtimeObjects"][0]["kind"] == "static_model"
                  && staticModelSaved["runtimeObjects"][0]["staticModel"]["modelPath"]
                             == "assets/models/props/nested/crate.glb"
                  && Near(
                          staticModelSaved["runtimeObjects"][0]["staticModel"]["heightOffsetWorld"]
                                  .get<float>(),
                          0.75f)
                  && Near(
                          staticModelSaved["runtimeObjects"][0]["staticModel"]["scale"]
                                  .get<float>(),
                          1.5f)
                  && Near(
                          staticModelSaved["runtimeObjects"][0]["staticModel"]["rotationXDegrees"]
                                  .get<float>(),
                          0.25f * 180.0f / PI)
                  && Near(
                          staticModelSaved["runtimeObjects"][0]["staticModel"]["rotationZDegrees"]
                                  .get<float>(),
                          -0.75f * 180.0f / PI)
                  && staticModelSaved["runtimeObjects"][0]["staticModel"]["collision"]
                             .get<bool>()
                  && Near(
                          staticModelSaved["runtimeObjects"][0]["position"][1].get<float>(),
                          -2.5f),
          "static prop save writes model path, height offset, scale, and authored floor height");
    SectorTopologyMap staticModelLoaded;
    Check(LoadText(staticModelSaved.dump(), staticModelLoaded, error),
          "static prop JSON loads");
    const SectorPlacedRuntimeObject* loadedStaticModel =
            game::FindSectorPlacedRuntimeObject(staticModelLoaded, 18);
    Check(loadedStaticModel != nullptr
                  && loadedStaticModel->kind == "static_model"
                  && loadedStaticModel->staticModel.modelPath
                          == "assets/models/props/nested/crate.glb"
                  && Near(loadedStaticModel->staticModel.heightOffsetWorld, 0.75f)
                  && Near(loadedStaticModel->staticModel.scale, 1.5f)
                  && Near(loadedStaticModel->staticModel.rotationXRadians, 0.25f)
                  && Near(loadedStaticModel->staticModel.rotationZRadians, -0.75f)
                  && loadedStaticModel->staticModel.collision
                  && Near(loadedStaticModel->position, Vector3{12.0f, -2.5f, 20.0f})
                  && Near(loadedStaticModel->yawRadians, 0.5f),
          "static prop JSON round-trip preserves payload and common transform");

    staticModelMap.runtimeObjects[0].staticModel = game::SectorPlacedStaticModel{};
    const Json unassignedStaticModelSaved = Json::parse(SaveText(staticModelMap));
    Check(unassignedStaticModelSaved["runtimeObjects"][0]["staticModel"].is_object()
                  && unassignedStaticModelSaved["runtimeObjects"][0]["staticModel"].empty(),
          "unassigned static prop saves an empty payload and omits default fields");
    SectorTopologyMap unassignedStaticModelLoaded;
    Check(LoadText(unassignedStaticModelSaved.dump(), unassignedStaticModelLoaded, error),
          "unassigned static prop with missing optional fields loads");
    const SectorPlacedRuntimeObject* unassignedStaticModel =
            game::FindSectorPlacedRuntimeObject(unassignedStaticModelLoaded, 18);
    Check(unassignedStaticModel != nullptr
                  && unassignedStaticModel->staticModel.modelPath.empty()
                  && Near(unassignedStaticModel->staticModel.rotationXRadians, 0.0f)
                  && Near(unassignedStaticModel->staticModel.rotationZRadians, 0.0f)
                  && Near(unassignedStaticModel->staticModel.heightOffsetWorld, 0.0f)
                  && Near(unassignedStaticModel->staticModel.scale, 1.0f)
                  && !unassignedStaticModel->staticModel.collision,
          "missing static prop payload fields use backward-compatible defaults");

    Json missingModelPath = staticModelSaved;
    missingModelPath["runtimeObjects"][0]["staticModel"].erase("modelPath");
    SectorTopologyMap missingModelPathLoaded;
    Check(LoadText(missingModelPath.dump(), missingModelPathLoaded, error),
          "static prop payload without model path loads");
    loadedStaticModel = game::FindSectorPlacedRuntimeObject(missingModelPathLoaded, 18);
    Check(loadedStaticModel != nullptr
                  && loadedStaticModel->staticModel.modelPath.empty()
                  && Near(loadedStaticModel->staticModel.heightOffsetWorld, 0.75f),
          "missing static prop model path defaults empty without losing height offset");

    Json missingRotations = staticModelSaved;
    missingRotations["runtimeObjects"][0]["staticModel"].erase(
            "rotationXDegrees");
    missingRotations["runtimeObjects"][0]["staticModel"].erase(
            "rotationZDegrees");
    SectorTopologyMap missingRotationsLoaded;
    Check(LoadText(missingRotations.dump(), missingRotationsLoaded, error),
          "legacy static prop payload without X/Z rotations loads");
    loadedStaticModel = game::FindSectorPlacedRuntimeObject(
            missingRotationsLoaded,
            18);
    Check(loadedStaticModel != nullptr
                  && Near(loadedStaticModel->staticModel.rotationXRadians, 0.0f)
                  && Near(loadedStaticModel->staticModel.rotationZRadians, 0.0f),
          "missing static prop X/Z rotations default to zero");

    Json missingScale = staticModelSaved;
    missingScale["runtimeObjects"][0]["staticModel"].erase("scale");
    SectorTopologyMap missingScaleLoaded;
    Check(LoadText(missingScale.dump(), missingScaleLoaded, error),
          "static prop payload without scale loads");
    loadedStaticModel = game::FindSectorPlacedRuntimeObject(missingScaleLoaded, 18);
    Check(loadedStaticModel != nullptr
                  && Near(loadedStaticModel->staticModel.scale, 1.0f),
          "missing static prop scale defaults to one");

    Json missingCollision = staticModelSaved;
    missingCollision["runtimeObjects"][0]["staticModel"].erase("collision");
    SectorTopologyMap missingCollisionLoaded;
    Check(LoadText(missingCollision.dump(), missingCollisionLoaded, error),
          "static prop payload without collision loads");
    loadedStaticModel = game::FindSectorPlacedRuntimeObject(
            missingCollisionLoaded,
            18);
    Check(loadedStaticModel != nullptr
                  && !loadedStaticModel->staticModel.collision,
          "missing static prop collision defaults off");

    SectorTopologyMap invalidStaticModel = staticModelMap;
    invalidStaticModel.runtimeObjects[0].staticModel.heightOffsetWorld =
            std::numeric_limits<float>::infinity();
    ExpectSaveRejected(
            invalidStaticModel,
            "non-finite static prop height offset is rejected on save");
    invalidStaticModel = staticModelMap;
    invalidStaticModel.runtimeObjects[0].staticModel.scale = 0.0f;
    ExpectSaveRejected(
            invalidStaticModel,
            "non-positive static prop scale is rejected on save");
    invalidStaticModel = staticModelMap;
    invalidStaticModel.runtimeObjects[0].staticModel.rotationXRadians =
            std::numeric_limits<float>::infinity();
    ExpectSaveRejected(
            invalidStaticModel,
            "non-finite static prop X rotation is rejected on save");
    invalidStaticModel = staticModelMap;
    invalidStaticModel.runtimeObjects[0].staticModel.rotationZRadians =
            std::numeric_limits<float>::max();
    ExpectSaveRejected(
            invalidStaticModel,
            "static prop Z rotation that overflows degrees is rejected on save");

    Json invalidStaticModelRotation = staticModelSaved;
    invalidStaticModelRotation["runtimeObjects"][0]["staticModel"]
            ["rotationXDegrees"] = "ninety";
    ExpectRejected(
            invalidStaticModelRotation,
            "non-numeric static prop X rotation is rejected on load");

    SectorTopologyMap authoringSource = original;
    game::SectorAuthoringDocument document = MakeAuthoringDocumentFromMap(authoringSource);
    const Json graphSaved = Json::parse(SaveAuthoringText(document));
    Check(graphSaved["runtimeObjects"].is_array(), "graph-native save writes runtime objects");
    game::SectorAuthoringDocument graphLoaded;
    Check(LoadAuthoringText(graphSaved.dump(), graphLoaded, error), "graph-native runtime object JSON loads");
    Check(graphLoaded.mapData.runtimeObjects.size() == 2
                  && graphLoaded.mapData.runtimeObjects[0].id == 3
                  && graphLoaded.derivation.success
                  && graphLoaded.derivation.topology.runtimeObjects.size() == 2
                  && graphLoaded.derivation.topology.runtimeObjects[1].id == 12,
          "graph-native save/load preserves runtime objects in map data and derived topology");

    Json invalid = saved;
    invalid["runtimeObjects"] = 7;
    ExpectRejected(invalid, "non-array runtimeObjects is rejected");

    invalid = saved;
    invalid["runtimeObjects"][0]["id"] = 0;
    ExpectRejected(invalid, "non-positive runtime object ID is rejected");

    invalid = saved;
    invalid["runtimeObjects"][1]["id"] = 3;
    ExpectRejected(invalid, "duplicate runtime object ID is rejected");

    Json legacyObject = saved;
    legacyObject["runtimeObjects"][0].erase("kind");
    legacyObject["runtimeObjects"][0].erase("billboard");
    legacyObject["runtimeObjects"][0]["definitionId"] = "goblin";
    ExpectRejected(legacyObject, "legacy definitionId-only runtime object is rejected");

    invalid = saved;
    invalid["runtimeObjects"][0].erase("kind");
    ExpectRejected(invalid, "missing runtime object kind is rejected");

    invalid = saved;
    invalid["runtimeObjects"][0]["position"] = Json::array({1.0f, 2.0f});
    ExpectRejected(invalid, "wrong-size runtime object position is rejected");

    invalid = saved;
    invalid["runtimeObjects"][0].erase("yawDegrees");
    ExpectRejected(invalid, "missing runtime object yaw is rejected");

    invalid = saved;
    invalid["runtimeObjects"][0]["yawDegrees"] = "__NONFINITE__";
    std::string nonFiniteText = invalid.dump();
    const std::string marker = "\"__NONFINITE__\"";
    const size_t markerPos = nonFiniteText.find(marker);
    Check(markerPos != std::string::npos, "non-finite runtime object yaw marker exists");
    if (markerPos != std::string::npos) {
        nonFiniteText.replace(markerPos, marker.size(), "1e999");
        ExpectRejectedText(nonFiniteText, "non-finite runtime object yaw is rejected");
    }

    SectorTopologyMap largeYaw = original;
    largeYaw.runtimeObjects[0].yawRadians = std::numeric_limits<float>::max();
    ExpectSaveRejected(largeYaw, "finite runtime object yaw that overflows degrees is rejected on save");

    SectorTopologyMap billboardMap = MakeSquare();
    SectorPlacedRuntimeObject single;
    single.id = 20;
    single.kind = "billboard";
    single.position = Vector3{8.0f, 0.0f, 6.0f};
    single.yawRadians = 0.25f;
    single.billboard.spriteAnimationPath = "assets/sprites/torch/torch.json";
    single.billboard.sizeWorld = Vector2{1.5f, 2.25f};
    single.billboard.clip = "Idle";
    billboardMap.runtimeObjects.push_back(single);

    SectorPlacedRuntimeObject directional;
    directional.id = 21;
    directional.kind = "billboard";
    directional.position = Vector3{10.0f, 0.5f, 7.0f};
    directional.yawRadians = 1.0f;
    directional.billboard.spriteAnimationPath = "assets/sprites/guard/guard.json";
    directional.billboard.sizeWorld = Vector2{0.8f, 1.2f};
    directional.billboard.keepAspectRatio = false;
    directional.billboard.originNormalized = Vector2{0.25f, 0.75f};
    directional.billboard.directional = true;
    directional.billboard.frontClip = "Face";
    directional.billboard.backClip = "Away";
    directional.billboard.leftClip = "Port";
    directional.billboard.rightClip = "Starboard";
    directional.billboard.playing = false;
    billboardMap.runtimeObjects.push_back(directional);

    const Json billboardSaved = Json::parse(SaveText(billboardMap));
    Check(billboardSaved["runtimeObjects"][0]["kind"].get<std::string>() == "billboard"
                  && !billboardSaved["runtimeObjects"][0].contains("definitionId"),
          "generic billboard runtime object writes kind without definition ID");
    Check(billboardSaved["runtimeObjects"][0]["billboard"]["spriteAnimationPath"]
                          .get<std::string>() == "assets/sprites/torch/torch.json"
                  && Near(billboardSaved["runtimeObjects"][0]["billboard"]["width"].get<float>(), 1.5f)
                  && Near(billboardSaved["runtimeObjects"][0]["billboard"]["height"].get<float>(), 2.25f)
                  && billboardSaved["runtimeObjects"][0]["billboard"]["clip"].get<std::string>() == "Idle",
          "single-clip billboard payload writes sprite path size and clip");
    Check(!billboardSaved["runtimeObjects"][0]["billboard"].contains("keepAspectRatio")
                  && !billboardSaved["runtimeObjects"][0]["billboard"].contains("originNormalized")
                  && !billboardSaved["runtimeObjects"][0]["billboard"].contains("directional")
                  && !billboardSaved["runtimeObjects"][0]["billboard"].contains("playing"),
          "default billboard payload fields are omitted");
    Check(billboardSaved["runtimeObjects"][1]["billboard"]["directional"].get<bool>()
                  && !billboardSaved["runtimeObjects"][1]["billboard"]["keepAspectRatio"].get<bool>()
                  && !billboardSaved["runtimeObjects"][1]["billboard"]["playing"].get<bool>()
                  && billboardSaved["runtimeObjects"][1]["billboard"]["frontClip"].get<std::string>() == "Face"
                  && billboardSaved["runtimeObjects"][1]["billboard"]["rightClip"].get<std::string>() == "Starboard",
          "directional billboard payload writes non-default fields");

    SectorTopologyMap billboardLoaded;
    Check(LoadText(billboardSaved.dump(), billboardLoaded, error), "generic billboard JSON loads");
    const SectorPlacedRuntimeObject* loadedSingle =
            game::FindSectorPlacedRuntimeObject(billboardLoaded, 20);
    const SectorPlacedRuntimeObject* loadedDirectional =
            game::FindSectorPlacedRuntimeObject(billboardLoaded, 21);
    Check(loadedSingle != nullptr
                  && loadedSingle->kind == "billboard"
                  && loadedSingle->definitionId.empty()
                  && loadedSingle->billboard.spriteAnimationPath == "assets/sprites/torch/torch.json"
                  && Near(loadedSingle->billboard.sizeWorld, Vector2{1.5f, 2.25f})
                  && loadedSingle->billboard.keepAspectRatio
                  && Near(loadedSingle->billboard.originNormalized, Vector2{0.5f, 1.0f})
                  && !loadedSingle->billboard.directional
                  && loadedSingle->billboard.clip == "Idle"
                  && loadedSingle->billboard.playing,
          "single-clip billboard payload restores explicit and default fields");
    Check(loadedDirectional != nullptr
                  && loadedDirectional->kind == "billboard"
                  && loadedDirectional->billboard.directional
                  && loadedDirectional->billboard.frontClip == "Face"
                  && loadedDirectional->billboard.backClip == "Away"
                  && loadedDirectional->billboard.leftClip == "Port"
                  && loadedDirectional->billboard.rightClip == "Starboard"
                  && !loadedDirectional->billboard.keepAspectRatio
                  && !loadedDirectional->billboard.playing
                  && Near(loadedDirectional->billboard.originNormalized, Vector2{0.25f, 0.75f}),
          "directional billboard payload round-trips");

    Json invalidBillboard = billboardSaved;
    invalidBillboard["runtimeObjects"][0]["billboard"]["width"] = 0.0f;
    ExpectRejected(invalidBillboard, "non-positive billboard width is rejected");

    invalidBillboard = billboardSaved;
    invalidBillboard["runtimeObjects"][0]["billboard"]["originNormalized"] =
            Json::array({1.25f, 0.5f});
    ExpectRejected(invalidBillboard, "out-of-range billboard origin is rejected");

    invalidBillboard = billboardSaved;
    invalidBillboard["runtimeObjects"][0]["kind"] = "unknown";
    ExpectRejected(invalidBillboard, "unknown runtime object kind is rejected");

    invalidBillboard = billboardSaved;
    invalidBillboard["runtimeObjects"][0].erase("billboard");
    ExpectRejected(invalidBillboard, "missing billboard payload is rejected");

    SectorTopologyMap doorMap = MakeSquare();
    doorMap.runtimeObjects.push_back(MakeDoorRuntimeObject(30));
    doorMap.runtimeObjects[0].door.faceUvs.faces[static_cast<int>(game::SectorDoorFace::Front)].scale = {2.0f, 3.0f};
    doorMap.runtimeObjects[0].door.faceUvs.faces[static_cast<int>(game::SectorDoorFace::Front)].offset = {0.25f, 0.5f};
    doorMap.runtimeObjects[0].door.faceUvs.faces[static_cast<int>(game::SectorDoorFace::Left)].scale = {4.0f, 5.0f};
    doorMap.runtimeObjects[0].door.faceUvs.faces[static_cast<int>(game::SectorDoorFace::Left)].offset = {1.25f, 1.5f};
    const Json doorSaved = Json::parse(SaveText(doorMap));
    Check(doorSaved["runtimeObjects"][0]["kind"].get<std::string>() == "door"
                  && doorSaved["runtimeObjects"][0]["door"].is_object()
                  && !doorSaved["runtimeObjects"][0].contains("billboard"),
          "door runtime object writes kind and nested door payload");
    const Json& savedDoor = doorSaved["runtimeObjects"][0]["door"];
    Check(savedDoor["anchor"]["lineDefId"].get<int>() == 2
                  && savedDoor["anchor"]["frontSectorId"].get<int>() == 1
                  && savedDoor["anchor"]["backSectorId"].get<int>() == 2
                  && savedDoor["anchor"]["frontSideDefId"].get<int>() == 2
                  && savedDoor["anchor"]["backSideDefId"].get<int>() == 8
                  && savedDoor["anchor"]["endpointA"][0].get<int>() == 64
                  && savedDoor["anchor"]["endpointA"][1].get<int>() == 0
                  && savedDoor["anchor"]["endpointB"][0].get<int>() == 64
                  && savedDoor["anchor"]["endpointB"][1].get<int>() == 64,
          "door anchor writes IDs and exact topology endpoints");
    Check(Near(savedDoor["width"].get<float>(), 4.0f)
                  && Near(savedDoor["height"].get<float>(), 2.5f)
                  && Near(savedDoor["thickness"].get<float>(), 0.375f)
                  && Near(savedDoor["normalOffset"].get<float>(), 0.125f)
                  && Near(savedDoor["heightOffsetWorld"].get<float>(), -0.25f)
                  && savedDoor["motion"].get<std::string>() == "slide_right"
                  && Near(savedDoor["openDistance"].get<float>(), 3.0f)
                  && Near(savedDoor["speed"].get<float>(), 2.25f)
                  && Near(savedDoor["initialOpenFraction"].get<float>(), 0.5f)
                  && savedDoor["autoOpen"].get<bool>()
                  && Near(savedDoor["interactionDistance"].get<float>(), 1.75f)
                  && Near(savedDoor["autoOpenDistance"].get<float>(), 2.5f)
                  && savedDoor["textureId"].get<std::string>() == "industrial_door"
                  && savedDoor["openSoundId"].get<std::string>() == "door_open"
                  && savedDoor["closeSoundId"].get<std::string>() == "door_close",
          "door payload writes dimensions motion interaction and asset IDs");
    Check(savedDoor["faceUvs"]["front"]["scale"][0].get<float>() == 2.0f
                  && savedDoor["faceUvs"]["front"]["scale"][1].get<float>() == 3.0f
                  && savedDoor["faceUvs"]["front"]["offset"][0].get<float>() == 0.25f
                  && savedDoor["faceUvs"]["front"]["offset"][1].get<float>() == 0.5f
                  && savedDoor["faceUvs"]["left"]["scale"][0].get<float>() == 4.0f
                  && !savedDoor["faceUvs"].contains("back")
                  && !savedDoor["faceUvs"].contains("right")
                  && !savedDoor["faceUvs"].contains("top")
                  && !savedDoor["faceUvs"].contains("bottom"),
          "door face UVs write non-default faces and omit default faces");

    SectorTopologyMap doorLoaded;
    Check(LoadText(doorSaved.dump(), doorLoaded, error), "door runtime object JSON loads");
    const SectorPlacedRuntimeObject* loadedDoor =
            game::FindSectorPlacedRuntimeObject(doorLoaded, 30);
    Check(loadedDoor != nullptr
                  && loadedDoor->kind == "door"
                  && loadedDoor->door.anchor.lineDefId == 2
                  && loadedDoor->door.anchor.frontSectorId == 1
                  && loadedDoor->door.anchor.backSectorId == 2
                  && loadedDoor->door.anchor.frontSideDefId == 2
                  && loadedDoor->door.anchor.backSideDefId == 8
                  && loadedDoor->door.anchor.endpointAX == 64
                  && loadedDoor->door.anchor.endpointAY == 0
                  && loadedDoor->door.anchor.endpointBX == 64
                  && loadedDoor->door.anchor.endpointBY == 64
                  && Near(loadedDoor->door.width, 4.0f)
                  && Near(loadedDoor->door.height, 2.5f)
                  && Near(loadedDoor->door.thickness, 0.375f)
                  && Near(loadedDoor->door.normalOffset, 0.125f)
                  && Near(loadedDoor->door.heightOffsetWorld, -0.25f)
                  && loadedDoor->door.motion == game::SectorDoorMotionType::SlideRight
                  && Near(loadedDoor->door.openDistance, 3.0f)
                  && Near(loadedDoor->door.speed, 2.25f)
                  && Near(loadedDoor->door.initialOpenFraction, 0.5f)
                  && loadedDoor->door.autoOpen
                  && Near(loadedDoor->door.interactionDistance, 1.75f)
                  && Near(loadedDoor->door.autoOpenDistance, 2.5f)
                  && loadedDoor->door.textureId == "industrial_door"
                  && loadedDoor->door.openSoundId == "door_open"
                  && loadedDoor->door.closeSoundId == "door_close"
                  && Near(loadedDoor->door.faceUvs.faces[static_cast<int>(game::SectorDoorFace::Front)].scale, Vector2{2.0f, 3.0f})
                  && Near(loadedDoor->door.faceUvs.faces[static_cast<int>(game::SectorDoorFace::Front)].offset, Vector2{0.25f, 0.5f})
                  && Near(loadedDoor->door.faceUvs.faces[static_cast<int>(game::SectorDoorFace::Left)].scale, Vector2{4.0f, 5.0f})
                  && Near(loadedDoor->door.faceUvs.faces[static_cast<int>(game::SectorDoorFace::Back)].scale, Vector2{1.0f, 1.0f})
                  && Near(loadedDoor->door.faceUvs.faces[static_cast<int>(game::SectorDoorFace::Back)].offset, Vector2{0.0f, 0.0f}),
          "door payload round-trips authored fields and face UVs");

    Json oldDoorJson = doorSaved;
    oldDoorJson["runtimeObjects"][0]["door"].erase("faceUvs");
    oldDoorJson["runtimeObjects"][0]["door"].erase("heightOffsetWorld");
    SectorTopologyMap oldDoorLoaded;
    Check(LoadText(oldDoorJson.dump(), oldDoorLoaded, error),
          "old door JSON without face UVs loads");
    const SectorPlacedRuntimeObject* loadedOldDoor =
            game::FindSectorPlacedRuntimeObject(oldDoorLoaded, 30);
    Check(loadedOldDoor != nullptr
                  && Near(loadedOldDoor->door.faceUvs.faces[static_cast<int>(game::SectorDoorFace::Front)].scale, Vector2{1.0f, 1.0f})
                  && Near(loadedOldDoor->door.faceUvs.faces[static_cast<int>(game::SectorDoorFace::Bottom)].offset, Vector2{0.0f, 0.0f})
                  && loadedOldDoor->door.visual == game::SectorDoorVisualType::Procedural
                  && loadedOldDoor->door.modelAssetId.empty()
                  && loadedOldDoor->door.modelFit == game::SectorDoorModelFit::FitInside
                  && Near(loadedOldDoor->door.modelScale, 1.0f)
                  && Near(loadedOldDoor->door.heightOffsetWorld, 0.0f)
                  && loadedOldDoor->door.hinge == game::SectorDoorHinge::Start
                  && loadedOldDoor->door.swingSide == game::SectorDoorSwingSide::Front
                  && Near(loadedOldDoor->door.openAngleDegrees, 90.0f)
                  && Near(loadedOldDoor->door.angularSpeedDegrees, 90.0f),
          "old door JSON without new fields loads exact procedural defaults");

    SectorTopologyMap swingDoorMap = MakeSquare();
    SectorPlacedRuntimeObject swingDoor = MakeDoorRuntimeObject(34);
    swingDoor.door.visual = game::SectorDoorVisualType::Model;
    swingDoor.door.modelAssetId = "future_catalog_style";
    swingDoor.door.modelFit = game::SectorDoorModelFit::FitWidth;
    swingDoor.door.modelScale = 1.25f;
    swingDoor.door.motion = game::SectorDoorMotionType::Swing;
    swingDoor.door.hinge = game::SectorDoorHinge::End;
    swingDoor.door.swingSide = game::SectorDoorSwingSide::Back;
    swingDoor.door.openAngleDegrees = 135.0f;
    swingDoor.door.angularSpeedDegrees = 72.5f;
    swingDoorMap.runtimeObjects.push_back(swingDoor);
    const Json swingDoorSaved = Json::parse(SaveText(swingDoorMap));
    const Json& savedSwingDoor = swingDoorSaved["runtimeObjects"][0]["door"];
    Check(savedSwingDoor["visual"] == "model"
                  && savedSwingDoor["modelAssetId"] == "future_catalog_style"
                  && savedSwingDoor["modelFit"] == "fit_width"
                  && Near(savedSwingDoor["modelScale"].get<float>(), 1.25f)
                  && savedSwingDoor["motion"] == "swing"
                  && savedSwingDoor["hinge"] == "end"
                  && savedSwingDoor["swingSide"] == "back"
                  && Near(savedSwingDoor["openAngleDegrees"].get<float>(), 135.0f)
                  && Near(savedSwingDoor["angularSpeedDegrees"].get<float>(), 72.5f),
          "model swing door writes every non-default authored field");
    SectorTopologyMap swingDoorLoaded;
    Check(LoadText(swingDoorSaved.dump(), swingDoorLoaded, error),
          "model swing door with an unknown catalog ID remains valid map data");
    const SectorPlacedRuntimeObject* loadedSwingDoor =
            game::FindSectorPlacedRuntimeObject(swingDoorLoaded, 34);
    Check(loadedSwingDoor != nullptr
                  && loadedSwingDoor->door.visual == game::SectorDoorVisualType::Model
                  && loadedSwingDoor->door.modelAssetId == "future_catalog_style"
                  && loadedSwingDoor->door.modelFit == game::SectorDoorModelFit::FitWidth
                  && Near(loadedSwingDoor->door.modelScale, 1.25f)
                  && loadedSwingDoor->door.motion == game::SectorDoorMotionType::Swing
                  && loadedSwingDoor->door.hinge == game::SectorDoorHinge::End
                  && loadedSwingDoor->door.swingSide == game::SectorDoorSwingSide::Back
                  && Near(loadedSwingDoor->door.openAngleDegrees, 135.0f)
                  && Near(loadedSwingDoor->door.angularSpeedDegrees, 72.5f),
          "model swing door authored fields round-trip without catalog lookup");

    swingDoorMap.runtimeObjects[0].door.modelFit = game::SectorDoorModelFit::Manual;
    swingDoorMap.runtimeObjects[0].door.angularSpeedDegrees = 0.0f;
    const Json manualSwingDoorSaved = Json::parse(SaveText(swingDoorMap));
    Check(manualSwingDoorSaved["runtimeObjects"][0]["door"]["modelFit"] == "manual"
                  && Near(manualSwingDoorSaved["runtimeObjects"][0]["door"]
                                  ["angularSpeedDegrees"].get<float>(),
                          0.0f),
          "manual fit and stationary angular speed serialize as valid values");
    swingDoorMap.runtimeObjects[0].door.modelFit = game::SectorDoorModelFit::FitInside;
    swingDoorMap.runtimeObjects[0].door.modelScale = 1.0f;
    swingDoorMap.runtimeObjects[0].door.hinge = game::SectorDoorHinge::Start;
    swingDoorMap.runtimeObjects[0].door.swingSide = game::SectorDoorSwingSide::Front;
    swingDoorMap.runtimeObjects[0].door.openAngleDegrees = 90.0f;
    swingDoorMap.runtimeObjects[0].door.angularSpeedDegrees = 90.0f;
    const Json defaultSwingFieldsSaved = Json::parse(SaveText(swingDoorMap));
    const Json& defaultSwingDoor = defaultSwingFieldsSaved["runtimeObjects"][0]["door"];
    Check(!defaultSwingDoor.contains("modelFit")
                  && !defaultSwingDoor.contains("modelScale")
                  && !defaultSwingDoor.contains("hinge")
                  && !defaultSwingDoor.contains("swingSide")
                  && !defaultSwingDoor.contains("openAngleDegrees")
                  && !defaultSwingDoor.contains("angularSpeedDegrees"),
          "default model and swing tuning fields remain omitted");

    SectorTopologyMap proceduralSwingMap = MakeSquare();
    SectorPlacedRuntimeObject proceduralSwing = MakeDoorRuntimeObject(35);
    proceduralSwing.door.motion = game::SectorDoorMotionType::Swing;
    proceduralSwingMap.runtimeObjects.push_back(proceduralSwing);
    const Json proceduralSwingSaved = Json::parse(SaveText(proceduralSwingMap));
    Check(proceduralSwingSaved["runtimeObjects"][0]["door"]["motion"] == "swing"
                  && !proceduralSwingSaved["runtimeObjects"][0]["door"].contains("visual")
                  && !proceduralSwingSaved["runtimeObjects"][0]["door"].contains("modelAssetId"),
          "procedural swing is valid without model fields");

    SectorTopologyMap mixedDoorMap = MakeSquare();
    SectorPlacedRuntimeObject mixedVertical = MakeDoorRuntimeObject(40);
    mixedVertical.door.motion = game::SectorDoorMotionType::SlideVertical;
    mixedVertical.door.initialOpenFraction = 0.1f;
    mixedDoorMap.runtimeObjects.push_back(mixedVertical);
    SectorPlacedRuntimeObject mixedLeft = MakeDoorRuntimeObject(41);
    mixedLeft.door.motion = game::SectorDoorMotionType::SlideLeft;
    mixedLeft.door.initialOpenFraction = 0.2f;
    mixedDoorMap.runtimeObjects.push_back(mixedLeft);
    SectorPlacedRuntimeObject mixedRight = MakeDoorRuntimeObject(42);
    mixedRight.door.motion = game::SectorDoorMotionType::SlideRight;
    mixedRight.door.initialOpenFraction = 0.3f;
    mixedDoorMap.runtimeObjects.push_back(mixedRight);
    SectorPlacedRuntimeObject mixedSwing = MakeDoorRuntimeObject(43);
    mixedSwing.door.visual = game::SectorDoorVisualType::Model;
    mixedSwing.door.modelAssetId = "wooden_interior_001";
    mixedSwing.door.motion = game::SectorDoorMotionType::Swing;
    mixedSwing.door.initialOpenFraction = 0.4f;
    mixedDoorMap.runtimeObjects.push_back(mixedSwing);

    const Json mixedDoorSaved = Json::parse(SaveText(mixedDoorMap));
    Check(!mixedDoorSaved["runtimeObjects"][0]["door"].contains("motion")
                  && mixedDoorSaved["runtimeObjects"][1]["door"]["motion"] == "slide_left"
                  && mixedDoorSaved["runtimeObjects"][2]["door"]["motion"] == "slide_right"
                  && mixedDoorSaved["runtimeObjects"][3]["door"]["visual"] == "model"
                  && mixedDoorSaved["runtimeObjects"][3]["door"]["modelAssetId"]
                             == "wooden_interior_001"
                  && mixedDoorSaved["runtimeObjects"][3]["door"]["motion"] == "swing",
          "mixed map preserves procedural defaults and explicit slide/model-swing values");
    SectorTopologyMap mixedDoorLoaded;
    Check(LoadText(mixedDoorSaved.dump(), mixedDoorLoaded, error)
                  && mixedDoorLoaded.runtimeObjects.size() == 4
                  && mixedDoorLoaded.runtimeObjects[0].door.motion
                             == game::SectorDoorMotionType::SlideVertical
                  && mixedDoorLoaded.runtimeObjects[1].door.motion
                             == game::SectorDoorMotionType::SlideLeft
                  && mixedDoorLoaded.runtimeObjects[2].door.motion
                             == game::SectorDoorMotionType::SlideRight
                  && mixedDoorLoaded.runtimeObjects[3].door.visual
                             == game::SectorDoorVisualType::Model
                  && mixedDoorLoaded.runtimeObjects[3].door.motion
                             == game::SectorDoorMotionType::Swing
                  && Near(mixedDoorLoaded.runtimeObjects[3].door.initialOpenFraction, 0.4f),
          "mixed procedural and model door map round-trips all motion types and partial state");

    SectorTopologyMap defaultDoorMap = MakeSquare();
    SectorPlacedRuntimeObject defaultDoor = MakeDoorRuntimeObject(31);
    defaultDoor.door.width = 0.0f;
    defaultDoor.door.height = 0.0f;
    defaultDoor.door.thickness = 0.25f;
    defaultDoor.door.normalOffset = 0.0f;
    defaultDoor.door.heightOffsetWorld = 0.0f;
    defaultDoor.door.motion = game::SectorDoorMotionType::SlideVertical;
    defaultDoor.door.openDistance = 0.0f;
    defaultDoor.door.speed = 1.5f;
    defaultDoor.door.initialOpenFraction = 0.0f;
    defaultDoor.door.autoOpen = false;
    defaultDoor.door.interactionDistance = 1.5f;
    defaultDoor.door.autoOpenDistance = 2.0f;
    defaultDoor.door.textureId.clear();
    defaultDoor.door.openSoundId.clear();
    defaultDoor.door.closeSoundId.clear();
    defaultDoorMap.runtimeObjects.push_back(defaultDoor);
    const Json defaultDoorSaved = Json::parse(SaveText(defaultDoorMap));
    const Json& savedDefaultDoor = defaultDoorSaved["runtimeObjects"][0]["door"];
    Check(!savedDefaultDoor.contains("width")
                  && !savedDefaultDoor.contains("height")
                  && !savedDefaultDoor.contains("thickness")
                  && !savedDefaultDoor.contains("normalOffset")
                  && !savedDefaultDoor.contains("heightOffsetWorld")
                  && !savedDefaultDoor.contains("visual")
                  && !savedDefaultDoor.contains("modelAssetId")
                  && !savedDefaultDoor.contains("modelFit")
                  && !savedDefaultDoor.contains("modelScale")
                  && !savedDefaultDoor.contains("motion")
                  && !savedDefaultDoor.contains("hinge")
                  && !savedDefaultDoor.contains("swingSide")
                  && !savedDefaultDoor.contains("openAngleDegrees")
                  && !savedDefaultDoor.contains("angularSpeedDegrees")
                  && !savedDefaultDoor.contains("openDistance")
                  && !savedDefaultDoor.contains("speed")
                  && !savedDefaultDoor.contains("initialOpenFraction")
                  && !savedDefaultDoor.contains("autoOpen")
                  && !savedDefaultDoor.contains("interactionDistance")
                  && !savedDefaultDoor.contains("autoOpenDistance")
                  && !savedDefaultDoor.contains("textureId")
                  && !savedDefaultDoor.contains("openSoundId")
                  && !savedDefaultDoor.contains("closeSoundId")
                  && !savedDefaultDoor.contains("faceUvs"),
          "default door payload fields are omitted");

    SectorTopologyMap defaultDoorLoaded;
    Check(LoadText(defaultDoorSaved.dump(), defaultDoorLoaded, error),
          "default door payload JSON loads");
    const SectorPlacedRuntimeObject* loadedDefaultDoor =
            game::FindSectorPlacedRuntimeObject(defaultDoorLoaded, 31);
    Check(loadedDefaultDoor != nullptr
                  && loadedDefaultDoor->kind == "door"
                  && Near(loadedDefaultDoor->door.width, 0.0f)
                  && Near(loadedDefaultDoor->door.height, 0.0f)
                  && Near(loadedDefaultDoor->door.thickness, 0.25f)
                  && Near(loadedDefaultDoor->door.normalOffset, 0.0f)
                  && Near(loadedDefaultDoor->door.heightOffsetWorld, 0.0f)
                  && loadedDefaultDoor->door.visual == game::SectorDoorVisualType::Procedural
                  && loadedDefaultDoor->door.modelAssetId.empty()
                  && loadedDefaultDoor->door.modelFit == game::SectorDoorModelFit::FitInside
                  && Near(loadedDefaultDoor->door.modelScale, 1.0f)
                  && loadedDefaultDoor->door.motion == game::SectorDoorMotionType::SlideVertical
                  && loadedDefaultDoor->door.hinge == game::SectorDoorHinge::Start
                  && loadedDefaultDoor->door.swingSide == game::SectorDoorSwingSide::Front
                  && Near(loadedDefaultDoor->door.openAngleDegrees, 90.0f)
                  && Near(loadedDefaultDoor->door.angularSpeedDegrees, 90.0f)
                  && Near(loadedDefaultDoor->door.openDistance, 0.0f)
                  && Near(loadedDefaultDoor->door.speed, 1.5f)
                  && Near(loadedDefaultDoor->door.initialOpenFraction, 0.0f)
                  && !loadedDefaultDoor->door.autoOpen
                  && Near(loadedDefaultDoor->door.interactionDistance, 1.5f)
                  && Near(loadedDefaultDoor->door.autoOpenDistance, 2.0f)
                  && loadedDefaultDoor->door.textureId.empty()
                  && loadedDefaultDoor->door.openSoundId.empty()
                  && loadedDefaultDoor->door.closeSoundId.empty(),
          "default door payload restores default fields");

    SectorTopologyMap mixedMap = MakeSquare();
    mixedMap.runtimeObjects.push_back(MakeBillboardRuntimeObject(
            32,
            "assets/sprites/torch/torch.json",
            Vector3{1.0f, 0.0f, 2.0f},
            0.0f));
    mixedMap.runtimeObjects.push_back(MakeDoorRuntimeObject(33));
    const Json mixedSaved = Json::parse(SaveText(mixedMap));
    SectorTopologyMap mixedLoaded;
    Check(LoadText(mixedSaved.dump(), mixedLoaded, error),
          "mixed billboard and door runtime object JSON loads");
    Check(mixedLoaded.runtimeObjects.size() == 2
                  && game::FindSectorPlacedRuntimeObject(mixedLoaded, 32) != nullptr
                  && game::FindSectorPlacedRuntimeObject(mixedLoaded, 33) != nullptr
                  && game::FindSectorPlacedRuntimeObject(mixedLoaded, 32)->kind == "billboard"
                  && game::FindSectorPlacedRuntimeObject(mixedLoaded, 33)->kind == "door",
          "mixed billboard and door runtime objects round-trip");

    game::SectorAuthoringDocument doorDocument = MakeAuthoringDocumentFromMap(doorMap);
    const Json doorAuthoringSaved = Json::parse(SaveAuthoringText(doorDocument));
    Check(doorAuthoringSaved["runtimeObjects"][0]["kind"].get<std::string>() == "door",
          "graph-native save writes door runtime objects");
    game::SectorAuthoringDocument doorAuthoringLoaded;
    Check(LoadAuthoringText(doorAuthoringSaved.dump(), doorAuthoringLoaded, error),
          "graph-native door runtime object JSON loads");
    Check(doorAuthoringLoaded.mapData.runtimeObjects.size() == 1
                  && doorAuthoringLoaded.mapData.runtimeObjects[0].kind == "door"
                  && doorAuthoringLoaded.derivation.topology.runtimeObjects.size() == 1
                  && doorAuthoringLoaded.derivation.topology.runtimeObjects[0].kind == "door",
          "graph-native door runtime object survives load and derivation copy");

    Json invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["motion"] = "spin";
    ExpectRejected(invalidDoor, "unknown door motion is rejected");

    SectorTopologyMap nonFiniteDoor = doorMap;
    nonFiniteDoor.runtimeObjects[0].door.heightOffsetWorld =
            std::numeric_limits<float>::infinity();
    ExpectSaveRejected(
            nonFiniteDoor,
            "non-finite door height offset is rejected on save");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["visual"] = "hologram";
    ExpectRejected(invalidDoor, "unknown door visual is rejected");

    invalidDoor = swingDoorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["modelFit"] = "stretch";
    ExpectRejected(invalidDoor, "unknown door model fit is rejected");

    invalidDoor = swingDoorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["hinge"] = "middle";
    ExpectRejected(invalidDoor, "unknown door hinge is rejected");

    invalidDoor = swingDoorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["swingSide"] = "sideways";
    ExpectRejected(invalidDoor, "unknown door swing side is rejected");

    invalidDoor = swingDoorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["modelScale"] = 0.0f;
    ExpectRejected(invalidDoor, "non-positive door model scale is rejected");

    invalidDoor = swingDoorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["openAngleDegrees"] = 0.0f;
    ExpectRejected(invalidDoor, "zero door open angle is rejected");

    invalidDoor = swingDoorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["openAngleDegrees"] = 170.01f;
    ExpectRejected(invalidDoor, "door open angle above 170 degrees is rejected");

    invalidDoor = swingDoorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["angularSpeedDegrees"] = -0.01f;
    ExpectRejected(invalidDoor, "negative door angular speed is rejected");

    invalidDoor = swingDoorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["modelAssetId"] = "";
    ExpectRejected(invalidDoor, "model door with empty catalog ID is rejected");

    invalidDoor = swingDoorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["motion"] = "slide_left";
    ExpectRejected(invalidDoor, "model visual combined with slide motion is rejected");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["anchor"]["lineDefId"] = 0;
    ExpectRejected(invalidDoor, "non-positive door anchor linedef ID is rejected");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["anchor"]["frontSectorId"] = -1;
    ExpectRejected(invalidDoor, "non-positive door anchor sector ID is rejected");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["anchor"]["frontSideDefId"] = 0;
    ExpectRejected(invalidDoor, "non-positive door anchor sidedef ID is rejected");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["anchor"]["endpointA"] = Json::array({64.0f, 0});
    ExpectRejected(invalidDoor, "floating door anchor endpoint coordinate is rejected");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["anchor"]["endpointB"] = Json::array({64});
    ExpectRejected(invalidDoor, "wrong-size door anchor endpoint is rejected");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["width"] = -0.01f;
    ExpectRejected(invalidDoor, "negative door width is rejected");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["height"] = -0.01f;
    ExpectRejected(invalidDoor, "negative door height is rejected");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["thickness"] = 0.0f;
    ExpectRejected(invalidDoor, "non-positive door thickness is rejected");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["openDistance"] = -0.01f;
    ExpectRejected(invalidDoor, "negative door open distance is rejected");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["speed"] = -0.01f;
    ExpectRejected(invalidDoor, "negative door speed is rejected");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["initialOpenFraction"] = 1.01f;
    ExpectRejected(invalidDoor, "out-of-range door initial open fraction is rejected");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["interactionDistance"] = 0.0f;
    ExpectRejected(invalidDoor, "non-positive door interaction distance is rejected");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["autoOpenDistance"] = 0.0f;
    ExpectRejected(invalidDoor, "non-positive door auto-open distance is rejected");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["textureId"] = 7;
    ExpectRejected(invalidDoor, "wrong-type door texture ID is rejected");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["faceUvs"] = "bad";
    ExpectRejected(invalidDoor, "wrong-type door face UV container is rejected");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["faceUvs"]["front"]["scale"] = Json::array({0.0f, 1.0f});
    ExpectRejected(invalidDoor, "zero door face UV scale is rejected");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["faceUvs"]["front"]["offset"] = Json::array({0.0f, "bad"});
    ExpectRejected(invalidDoor, "wrong-type door face UV offset is rejected");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0]["door"]["faceUvs"]["front"]["scale"] = Json::array({65.0f, 1.0f});
    ExpectRejected(invalidDoor, "out-of-range door face UV scale is rejected");

    invalidDoor = doorSaved;
    invalidDoor["runtimeObjects"][0].erase("door");
    ExpectRejected(invalidDoor, "missing door payload is rejected");
}

void TestDynamicModelRoundTripAndDefaultOmission()
{
    SectorTopologyMap map = MakeSquare();
    SectorPlacedRuntimeObject object;
    object.id = 41;
    object.kind = "dynamic_model";
    object.position = Vector3{12.0f, 16.0f, 20.0f};
    object.yawRadians = 0.5f;
    object.dynamicModel.modelPath = "assets/models/characters/test.glb";
    object.dynamicModel.rotationXRadians = 0.25f;
    object.dynamicModel.rotationZRadians = -0.5f;
    object.dynamicModel.heightOffsetWorld = 0.75f;
    object.dynamicModel.scale = 1.5f;
    object.dynamicModel.collision = true;
    object.dynamicModel.animation = "Standard Walk";
    object.dynamicModel.loop = false;
    object.dynamicModel.animationSpeed = 1.25f;
    object.dynamicModel.shadowMode = game::SectorDynamicModelShadowMode::Dynamic;
    map.runtimeObjects.push_back(object);

    const Json saved = Json::parse(SaveText(map));
    const Json& payload = saved["runtimeObjects"][0]["dynamicModel"];
    Check(saved["runtimeObjects"][0]["kind"] == "dynamic_model"
                  && payload["modelPath"] == object.dynamicModel.modelPath
                  && payload["animation"] == "Standard Walk"
                  && !payload["loop"].get<bool>()
                  && Near(payload["animationSpeed"].get<float>(), 1.25f)
                  && payload["shadowMode"] == "dynamic"
                  && payload["collision"].get<bool>(),
          "dynamic prop writes model, animation, playback, shadow, and collision fields");

    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(saved.dump(), loaded, error), "dynamic prop JSON loads");
    const SectorPlacedRuntimeObject* roundTripped =
            game::FindSectorPlacedRuntimeObject(loaded, 41);
    Check(roundTripped != nullptr
                  && roundTripped->kind == "dynamic_model"
                  && roundTripped->dynamicModel.modelPath == object.dynamicModel.modelPath
                  && roundTripped->dynamicModel.animation == "Standard Walk"
                  && !roundTripped->dynamicModel.loop
                  && roundTripped->dynamicModel.shadowMode
                          == game::SectorDynamicModelShadowMode::Dynamic
                  && Near(roundTripped->dynamicModel.animationSpeed, 1.25f)
                  && Near(roundTripped->dynamicModel.rotationXRadians, 0.25f)
                  && Near(roundTripped->dynamicModel.rotationZRadians, -0.5f),
          "dynamic prop fields round-trip");

    map.runtimeObjects[0].dynamicModel = game::SectorPlacedDynamicModel{};
    const Json defaults = Json::parse(SaveText(map))["runtimeObjects"][0]["dynamicModel"];
    Check(!defaults.contains("animation")
                  && !defaults.contains("loop")
                  && !defaults.contains("animationSpeed")
                  && !defaults.contains("scale")
                  && !defaults.contains("shadowMode")
                  && !defaults.contains("collision"),
          "dynamic prop default playback, shadow, and transform fields are omitted");

    map.runtimeObjects[0].dynamicModel.shadowMode =
            game::SectorDynamicModelShadowMode::None;
    const Json none = Json::parse(SaveText(map))["runtimeObjects"][0]["dynamicModel"];
    Check(none["shadowMode"] == "none", "dynamic prop writes an explicit disabled shadow mode");

    Json invalid = saved;
    invalid["runtimeObjects"][0]["dynamicModel"]["shadowMode"] = "cinematic";
    Check(!LoadText(invalid.dump(), loaded, error),
          "dynamic prop rejects unknown shadow modes");

    Json legacy = saved;
    legacy["runtimeObjects"][0]["dynamicModel"]["shadowMode"] =
            "projected_silhouette";
    Check(LoadText(legacy.dump(), loaded, error),
          "legacy projected dynamic prop shadow mode loads");
    const SectorPlacedRuntimeObject* migrated =
            game::FindSectorPlacedRuntimeObject(loaded, 41);
    Check(migrated != nullptr
                  && migrated->dynamicModel.shadowMode
                          == game::SectorDynamicModelShadowMode::Dynamic,
          "legacy projected dynamic prop shadow mode migrates to dynamic");
    Check(Json::parse(SaveText(loaded))["runtimeObjects"][0]["dynamicModel"]["shadowMode"]
                          == "dynamic",
          "migrated dynamic prop shadow mode saves with the new value");
}

void TestNpcRoundTripDefaultsAndValidation()
{
    SectorTopologyMap map = MakeSquare();
    SectorPlacedRuntimeObject object;
    object.id = 42;
    object.kind = "npc";
    object.position = Vector3{12.0f, 16.0f, 20.0f};
    object.yawRadians = 0.75f;
    object.npc.definitionId = "zombie_guard";
    object.npc.instanceId = "guard_at_gate";
    object.npc.scale = 1.25f;
    object.npc.shadowMode =
            game::SectorDynamicModelShadowMode::Dynamic;
    map.runtimeObjects.push_back(object);

    const Json saved = Json::parse(SaveText(map));
    const Json& payload = saved["runtimeObjects"][0]["npc"];
    Check(saved["runtimeObjects"][0]["kind"] == "npc"
                  && payload["definitionId"] == "zombie_guard"
                  && payload["instanceId"] == "guard_at_gate"
                  && Near(payload["scale"].get<float>(), 1.25f)
                  && payload["shadowMode"] == "dynamic",
          "NPC placement writes definition, instance, scale, and shadow fields");

    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(saved.dump(), loaded, error),
          "NPC placement JSON loads without consulting the external catalog");
    const SectorPlacedRuntimeObject* roundTripped =
            game::FindSectorPlacedRuntimeObject(loaded, 42);
    Check(roundTripped != nullptr
                  && roundTripped->kind == "npc"
                  && roundTripped->npc.definitionId == "zombie_guard"
                  && roundTripped->npc.instanceId == "guard_at_gate"
                  && Near(roundTripped->npc.scale, 1.25f)
                  && roundTripped->npc.shadowMode
                          == game::SectorDynamicModelShadowMode::Dynamic,
          "NPC placement fields round-trip");

    map.runtimeObjects[0].npc.instanceId.clear();
    map.runtimeObjects[0].npc.scale = 1.0f;
    map.runtimeObjects[0].npc.shadowMode =
            game::SectorDynamicModelShadowMode::Contact;
    const Json defaults = Json::parse(SaveText(map))["runtimeObjects"][0]["npc"];
    Check(defaults["definitionId"] == "zombie_guard"
                  && !defaults.contains("instanceId")
                  && !defaults.contains("scale")
                  && !defaults.contains("shadowMode"),
          "NPC placement omits optional and default payload fields");

    Json invalid = saved;
    invalid["runtimeObjects"][0]["npc"]["instanceId"] = "bad instance";
    ExpectRejected(invalid, "NPC placement rejects an invalid instance ID");
    invalid = saved;
    invalid["runtimeObjects"][0]["npc"]["scale"] = 0.0f;
    ExpectRejected(invalid, "NPC placement rejects a non-positive scale");
    invalid = saved;
    invalid["runtimeObjects"][0]["npc"]["shadowMode"] = "blob";
    ExpectRejected(invalid, "NPC placement rejects an unknown shadow mode");

    Json legacy = saved;
    legacy["runtimeObjects"][0]["npc"]["shadowMode"] =
            "projected_silhouette";
    Check(LoadText(legacy.dump(), loaded, error),
          "legacy projected NPC shadow mode loads");
    const SectorPlacedRuntimeObject* migrated =
            game::FindSectorPlacedRuntimeObject(loaded, 42);
    Check(migrated != nullptr
                  && migrated->npc.shadowMode
                          == game::SectorDynamicModelShadowMode::Dynamic,
          "legacy projected NPC shadow mode migrates to dynamic");

    Json duplicate = saved;
    Json second = duplicate["runtimeObjects"][0];
    second["id"] = 43;
    duplicate["runtimeObjects"].push_back(second);
    ExpectRejected(duplicate, "NPC instance IDs must be unique within a map");
}

void TestRuntimeObjectEditAndDeleteHelpers()
{
    SectorTopologyMap map = MakeSquare();
    map.runtimeObjects.push_back(MakeBillboardRuntimeObject(
            4,
            "assets/sprites/torch/torch.json",
            Vector3{1.0f, 0.5f, 1.0f},
            0.0f));
    map.runtimeObjects.push_back(MakeBillboardRuntimeObject(
            9,
            "assets/sprites/crate/crate.json",
            Vector3{2.0f, 0.5f, 2.0f},
            0.25f));

    SectorPlacedRuntimeObject* edited = game::FindSectorPlacedRuntimeObject(map, 4);
    Check(edited != nullptr, "runtime object edit target can be found by stable ID");
    if (edited != nullptr) {
        edited->position = Vector3{3.0f, 1.0f, 5.0f};
        edited->yawRadians = 1.0f;
    }

    const SectorPlacedRuntimeObject* preserved = game::FindSectorPlacedRuntimeObject(map, 4);
    Check(preserved != nullptr
                  && Near(preserved->position, Vector3{3.0f, 1.0f, 5.0f})
                  && Near(preserved->yawRadians, 1.0f),
          "runtime object edit mutates authored placement data");
    Check(game::RemoveSectorPlacedRuntimeObject(map, 9),
          "runtime object delete removes authored placement data");
    Check(game::FindSectorPlacedRuntimeObject(map, 9) == nullptr,
          "runtime object delete clears removed stable ID lookup");
    Check(!game::RemoveSectorPlacedRuntimeObject(map, 9),
          "runtime object delete reports missing object safely");
    Check(game::AllocateSectorPlacedRuntimeObjectId(map) == 5,
          "runtime object ID allocation reuses the next stable positive ID after delete");
}

void TestLightmapMetadataRoundTrip()
{
    SectorTopologyMap original = MakeSquare();
    original.lightmapSettings.ambientOcclusionRadius = 3.5f;
    original.lightmapSettings.ambientOcclusionStrength = 0.25f;
    original.lightmapSettings.indirectBounceRadius = 9.0f;
    original.lightmapSettings.indirectBounceStrength = 0.35f;
    original.lightmapSettings.objectProbeSpacingWorld = 5.5f;
    original.lightmapSettings.objectProbeLowerHeightWorld = 0.7f;
    original.lightmapSettings.objectProbeUpperHeightWorld = 1.4f;
    original.bakedLightmap.path = "assets/levels/test/test.lightmap.png";
    original.bakedLightmap.width = 2048;
    original.bakedLightmap.height = 2048;
    original.bakedLightmap.version = game::kSectorLightmapArtifactVersion;
    original.bakedLightmap.format = game::kSectorLightmapArtifactFormat;
    original.bakedLightmap.sourceHash = "abc123";
    original.bakedLightmap.additionalAtlases = {
            game::SectorLightmapAtlasMetadata{
                    "assets/levels/test/test.lightmap.1.png",
                    2048,
                    2048},
            game::SectorLightmapAtlasMetadata{
                    "assets/levels/test/test.lightmap.2.png",
                    2048,
                    2048}};

    const std::string text = SaveText(original);
    const Json saved = Json::parse(text);
    Check(saved["lightmapSettings"].is_object(), "topology lightmap settings are written");
    Check(saved["bakedLightmap"].is_object(), "topology baked lightmap metadata is written");
    Check(saved["bakedLightmap"]["path"].get<std::string>() == original.bakedLightmap.path,
          "topology baked lightmap path is serialized");
    Check(saved["bakedLightmap"]["additionalAtlases"].is_array()
                  && saved["bakedLightmap"]["additionalAtlases"].size() == 2
                  && saved["bakedLightmap"]["additionalAtlases"][1]["path"]
                          == "assets/levels/test/test.lightmap.2.png",
          "topology additional lightmap atlases are serialized in index order");

    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(text, loaded, error), "topology lightmap metadata JSON loads");
    Check(std::fabs(loaded.lightmapSettings.ambientOcclusionRadius - 3.5f) <= 0.0001f
                  && std::fabs(loaded.lightmapSettings.ambientOcclusionStrength - 0.25f) <= 0.0001f
                  && std::fabs(loaded.lightmapSettings.indirectBounceRadius - 9.0f) <= 0.0001f
                  && std::fabs(loaded.lightmapSettings.indirectBounceStrength - 0.35f) <= 0.0001f
                  && Near(loaded.lightmapSettings.objectProbeSpacingWorld, 5.5f)
                  && Near(loaded.lightmapSettings.objectProbeLowerHeightWorld, 0.7f)
                  && Near(loaded.lightmapSettings.objectProbeUpperHeightWorld, 1.4f),
          "topology lightmap settings round-trip");
    Check(loaded.bakedLightmap.path == original.bakedLightmap.path
                  && loaded.bakedLightmap.width == 2048
                  && loaded.bakedLightmap.height == 2048
                  && loaded.bakedLightmap.version == game::kSectorLightmapArtifactVersion
                  && loaded.bakedLightmap.format == game::kSectorLightmapArtifactFormat
                  && loaded.bakedLightmap.sourceHash == "abc123"
                  && loaded.bakedLightmap.additionalAtlases.size() == 2
                  && loaded.bakedLightmap.additionalAtlases[0].width == 2048,
          "topology baked lightmap metadata round-trips");

    SectorTopologyMap singleAtlas = original;
    singleAtlas.bakedLightmap.additionalAtlases.clear();
    const Json singleAtlasSaved = Json::parse(SaveText(singleAtlas));
    Check(!singleAtlasSaved["bakedLightmap"].contains("additionalAtlases"),
          "single-atlas metadata omits the optional additional atlas array");

    Json duplicateAtlas = saved;
    duplicateAtlas["bakedLightmap"]["additionalAtlases"][0]["path"] =
            original.bakedLightmap.path;
    SectorTopologyMap rejectedDuplicate;
    Check(!LoadText(duplicateAtlas.dump(), rejectedDuplicate, error),
          "duplicate multi-atlas paths are rejected transactionally");

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

    Json oldSettings = saved;
    oldSettings["lightmapSettings"].erase("objectProbeSpacingWorld");
    oldSettings["lightmapSettings"].erase("objectProbeLowerHeightWorld");
    oldSettings["lightmapSettings"].erase("objectProbeUpperHeightWorld");
    SectorTopologyMap oldSettingsStyle;
    Check(LoadText(oldSettings.dump(), oldSettingsStyle, error), "old topology lightmap settings load");
    Check(Near(oldSettingsStyle.lightmapSettings.objectProbeSpacingWorld, 4.0f)
                  && Near(oldSettingsStyle.lightmapSettings.objectProbeLowerHeightWorld, 0.6f)
                  && Near(oldSettingsStyle.lightmapSettings.objectProbeUpperHeightWorld, 1.5f),
          "old topology lightmap settings default object probe settings");

    Json legacyProbeHeight = saved;
    legacyProbeHeight["lightmapSettings"].erase("objectProbeLowerHeightWorld");
    legacyProbeHeight["lightmapSettings"].erase("objectProbeUpperHeightWorld");
    legacyProbeHeight["lightmapSettings"]["objectProbeHeightWorld"] = 1.4f;
    SectorTopologyMap migratedLegacyHeight;
    Check(LoadText(legacyProbeHeight.dump(), migratedLegacyHeight, error),
          "legacy object probe height setting loads");
    Check(Near(migratedLegacyHeight.lightmapSettings.objectProbeLowerHeightWorld, 0.6f)
                  && Near(migratedLegacyHeight.lightmapSettings.objectProbeUpperHeightWorld, 1.4f),
          "legacy object probe height migrates to upper layer with the new lower default");

    legacyProbeHeight["lightmapSettings"]["objectProbeHeightWorld"] = 0.7f;
    SectorTopologyMap migratedCloseLegacyHeight;
    Check(LoadText(legacyProbeHeight.dump(), migratedCloseLegacyHeight, error),
          "legacy close object probe height setting loads");
    Check(Near(migratedCloseLegacyHeight.lightmapSettings.objectProbeLowerHeightWorld, 0.7f)
                  && Near(migratedCloseLegacyHeight.lightmapSettings.objectProbeUpperHeightWorld, 0.7f),
          "legacy object probe height too close to the lower default preserves one layer");
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
    original.previewSettings.objectProbeDebugDrawMaxDistanceWorld = 96.0f;

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
                  && Near(saved["previewSettings"]["headBobFrequency"].get<float>(), 10.5f)
                  && Near(saved["previewSettings"]["objectProbeDebugDrawMaxDistanceWorld"].get<float>(), 96.0f),
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
                  && Near(loaded.previewSettings.headBobFrequency, 10.5f)
                  && Near(loaded.previewSettings.objectProbeDebugDrawMaxDistanceWorld, 96.0f),
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

    Json withoutObjectProbeDebugDistance = saved;
    withoutObjectProbeDebugDistance["previewSettings"].erase("objectProbeDebugDrawMaxDistanceWorld");
    Check(LoadText(withoutObjectProbeDebugDistance.dump(), loaded, error),
          "omitted object probe debug draw distance field is accepted");
    Check(Near(
                  loaded.previewSettings.objectProbeDebugDrawMaxDistanceWorld,
                  game::DefaultSectorPreviewSettings().objectProbeDebugDrawMaxDistanceWorld),
          "omitted object probe debug draw distance loads default");

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
                  && Near(oldStyle.previewSettings.headBobFrequency, defaults.headBobFrequency)
                  && Near(
                          oldStyle.previewSettings.objectProbeDebugDrawMaxDistanceWorld,
                          defaults.objectProbeDebugDrawMaxDistanceWorld),
          "omitted preview settings load defaults");

    Json invalid = saved;
    invalid["previewSettings"] = 4;
    ExpectRejected(invalid, "non-object preview settings are rejected");

    const std::array<const char*, 12> fields{
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
            "headBobFrequency",
            "objectProbeDebugDrawMaxDistanceWorld"
    };
    for (const char* field : fields) {
        invalid = saved;
        invalid["previewSettings"][field] = "invalid";
        ExpectRejected(invalid, "wrong-type preview settings field is rejected");
    }

    const std::string marker = "\"__NONFINITE__\"";

    invalid = saved;
    invalid["previewSettings"]["gravity"] = "__NONFINITE__";
    std::string nonFiniteText = invalid.dump();
    const size_t markerPos = nonFiniteText.find(marker);
    Check(markerPos != std::string::npos, "non-finite preview settings marker exists");
    if (markerPos != std::string::npos) {
        nonFiniteText.replace(markerPos, marker.size(), "1e999");
        ExpectRejectedText(nonFiniteText, "non-finite preview settings field is rejected");
    }

    invalid = saved;
    invalid["previewSettings"]["objectProbeDebugDrawMaxDistanceWorld"] = "__NONFINITE__";
    nonFiniteText = invalid.dump();
    const size_t objectProbeDebugDistanceMarkerPos = nonFiniteText.find(marker);
    Check(objectProbeDebugDistanceMarkerPos != std::string::npos,
          "non-finite object probe debug draw distance marker exists");
    if (objectProbeDebugDistanceMarkerPos != std::string::npos) {
        nonFiniteText.replace(objectProbeDebugDistanceMarkerPos, marker.size(), "1e999");
        ExpectRejectedText(nonFiniteText, "non-finite object probe debug draw distance is rejected");
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
    clamped["previewSettings"]["objectProbeDebugDrawMaxDistanceWorld"] = 999.0f;
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
                  && Near(loaded.previewSettings.headBobFrequency, 20.0f)
                  && Near(loaded.previewSettings.objectProbeDebugDrawMaxDistanceWorld, 512.0f),
          "out-of-range preview settings clamp");

    clamped["previewSettings"]["gravity"] = 500.0f;
    clamped["previewSettings"]["jumpHeight"] = -5.0f;
    clamped["previewSettings"]["headBobStrength"] = -5.0f;
    clamped["previewSettings"]["headBobFrequency"] = -5.0f;
    clamped["previewSettings"]["objectProbeDebugDrawMaxDistanceWorld"] = -5.0f;
    Check(LoadText(clamped.dump(), loaded, error), "high gravity preview settings load");
    Check(Near(loaded.previewSettings.gravity, 200.0f), "high gravity clamps");
    Check(Near(loaded.previewSettings.jumpHeight, 0.0f), "low jump height clamps");
    Check(Near(loaded.previewSettings.headBobStrength, 0.0f), "low headbob strength clamps");
    Check(Near(loaded.previewSettings.headBobFrequency, 0.0f), "low headbob frequency clamps");
    Check(Near(loaded.previewSettings.objectProbeDebugDrawMaxDistanceWorld, 0.0f),
          "low object probe debug draw distance clamps");
}

void TestAudioSettingsRoundTripAndValidation()
{
    SectorTopologyMap original = MakeSquare();
    original.audioSettings.musicPath = "music/level_theme.ogg";
    original.audioSettings.musicVolume = 0.35f;
    original.audioSettings.soundsById.emplace(
            "door_open", SectorSoundDefinition{
                    "door_open", "shared/doors/open.wav", SectorSoundType::Sound});
    original.audioSettings.soundsById.emplace(
            "alarm", SectorSoundDefinition{
                    "alarm", "ambience/alarm.mp3", SectorSoundType::Music});

    const Json saved = Json::parse(SaveText(original));
    Check(saved["audio"]["music"] == "music/level_theme.ogg",
          "level music path is serialized relative to assets/audio");
    Check(Near(saved["audio"]["musicVolume"].get<float>(), 0.35f),
          "non-default level music volume is serialized");
    Check(saved["audio"]["sounds"]["door_open"]["path"]
                  == "shared/doors/open.wav"
                  && saved["audio"]["sounds"]["door_open"]["type"] == "sound"
                  && saved["audio"]["sounds"]["alarm"]["path"]
                  == "ambience/alarm.mp3"
                  && saved["audio"]["sounds"]["alarm"]["type"] == "music",
          "typed level sounds are serialized");

    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(saved.dump(), loaded, error), "level audio JSON loads");
    Check(loaded.audioSettings.musicPath == "music/level_theme.ogg"
                  && Near(loaded.audioSettings.musicVolume, 0.35f)
                  && loaded.audioSettings.soundsById.at("door_open").path
                  == "shared/doors/open.wav"
                  && loaded.audioSettings.soundsById.at("door_open").type
                  == SectorSoundType::Sound
                  && loaded.audioSettings.soundsById.at("alarm").path
                  == "ambience/alarm.mp3"
                  && loaded.audioSettings.soundsById.at("alarm").type
                  == SectorSoundType::Music,
          "level audio settings round-trip");

    Json legacySounds = saved;
    legacySounds["audio"]["sounds"]["door_open"] = "shared/doors/open.wav";
    Check(LoadText(legacySounds.dump(), loaded, error),
          "legacy string sound registry entry remains compatible");
    Check(loaded.audioSettings.soundsById.at("door_open").type
                  == SectorSoundType::Sound,
          "legacy string sound registry entry defaults to sound type");

    Json legacy = saved;
    legacy["audio"].erase("musicVolume");
    Check(LoadText(legacy.dump(), loaded, error),
          "level audio without music volume remains compatible");
    Check(Near(
                  loaded.audioSettings.musicVolume,
                  game::SectorLevelAudioSettings::DefaultMusicVolume),
          "omitted level music volume uses the default");

    SectorTopologyMap defaultVolume = MakeSquare();
    defaultVolume.audioSettings.musicPath = "music/default_volume.ogg";
    const Json defaultVolumeSaved = Json::parse(SaveText(defaultVolume));
    Check(defaultVolumeSaved["audio"].find("musicVolume")
                  == defaultVolumeSaved["audio"].end(),
          "default level music volume is omitted on save");

    const Json defaults = Json::parse(SaveText(MakeSquare()));
    Check(defaults.find("audio") == defaults.end(),
          "empty level audio settings are omitted");

    Json invalid = saved;
    invalid["audio"] = "music/level_theme.ogg";
    ExpectRejected(invalid, "non-object level audio is rejected");
    invalid = saved;
    invalid["audio"]["music"] = "../outside.ogg";
    ExpectRejected(invalid, "traversing level music path is rejected");
    invalid = saved;
    invalid["audio"]["sounds"]["door_open"] = "shared/door.flac";
    ExpectRejected(invalid, "unsupported level sound format is rejected");
    invalid = saved;
    invalid["audio"]["sounds"]["door_open"]["type"] = "voice";
    ExpectRejected(invalid, "unknown level sound registry type is rejected");
    invalid = saved;
    invalid["audio"]["musicVolume"] = "loud";
    ExpectRejected(invalid, "non-numeric level music volume is rejected");
    invalid = saved;
    invalid["audio"]["musicVolume"] = -0.01f;
    ExpectRejected(invalid, "negative level music volume is rejected");
    invalid = saved;
    invalid["audio"]["musicVolume"] = 1.01f;
    ExpectRejected(invalid, "level music volume above one is rejected");

    SectorTopologyMap invalidSave = original;
    invalidSave.audioSettings.musicPath = "/absolute/theme.ogg";
    ExpectSaveRejected(invalidSave, "absolute level music path is rejected on save");
    invalidSave = original;
    invalidSave.audioSettings.musicVolume = -0.01f;
    ExpectSaveRejected(invalidSave, "invalid level music volume is rejected on save");
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

void TestDirectionalLightRoundTripAndValidation()
{
    SectorTopologyMap original = MakeSquare();
    original.directionalLight.enabled = true;
    original.directionalLight.directionToLight = Vector3{0.0f, 2.0f, 0.0f};
    original.directionalLight.color = Color{12, 34, 56, 128};
    original.directionalLight.intensity = 1.75f;

    const std::string text = SaveText(original);
    const Json saved = Json::parse(text);
    Check(saved["directionalLight"].is_object(), "non-default directional light is written");
    Check(saved["directionalLight"]["enabled"].get<bool>()
                  && Near(saved["directionalLight"]["directionToLight"][0].get<float>(), 0.0f)
                  && Near(saved["directionalLight"]["directionToLight"][1].get<float>(), 1.0f)
                  && Near(saved["directionalLight"]["directionToLight"][2].get<float>(), 0.0f)
                  && saved["directionalLight"]["color"]["r"].get<int>() == 12
                  && saved["directionalLight"]["color"]["g"].get<int>() == 34
                  && saved["directionalLight"]["color"]["b"].get<int>() == 56
                  && saved["directionalLight"]["color"]["a"].get<int>() == 255
                  && Near(saved["directionalLight"]["intensity"].get<float>(), 1.75f),
          "directional light values are serialized normalized");

    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(text, loaded, error), "directional light JSON loads");
    Check(loaded.directionalLight.enabled
                  && Near(loaded.directionalLight.directionToLight, Vector3{0.0f, 1.0f, 0.0f})
                  && loaded.directionalLight.color.r == 12
                  && loaded.directionalLight.color.g == 34
                  && loaded.directionalLight.color.b == 56
                  && loaded.directionalLight.color.a == 255
                  && Near(loaded.directionalLight.intensity, 1.75f),
          "directional light round-trips");

    Json missingFields = saved;
    missingFields["directionalLight"].erase("enabled");
    missingFields["directionalLight"].erase("directionToLight");
    missingFields["directionalLight"].erase("color");
    missingFields["directionalLight"].erase("intensity");
    Check(LoadText(missingFields.dump(), loaded, error), "omitted directional light fields are accepted");
    const game::SectorTopologyDirectionalLightSettings defaults =
            game::DefaultSectorTopologyDirectionalLightSettings();
    Check(loaded.directionalLight.enabled == defaults.enabled
                  && Near(loaded.directionalLight.directionToLight, defaults.directionToLight)
                  && loaded.directionalLight.color.r == defaults.color.r
                  && Near(loaded.directionalLight.intensity, defaults.intensity),
          "omitted directional light fields load defaults");

    Json partialColor = saved;
    partialColor["directionalLight"]["color"].erase("g");
    Check(LoadText(partialColor.dump(), loaded, error), "omitted directional color channel is accepted");
    Check(loaded.directionalLight.color.r == 12
                  && loaded.directionalLight.color.g == defaults.color.g
                  && loaded.directionalLight.color.a == 255,
          "omitted directional color channel loads default and alpha normalizes");

    Json withoutDirectionalLight = saved;
    withoutDirectionalLight.erase("directionalLight");
    Check(LoadText(withoutDirectionalLight.dump(), loaded, error),
          "omitted directional light field is accepted");
    Check(!loaded.directionalLight.enabled
                  && Near(loaded.directionalLight.directionToLight, defaults.directionToLight)
                  && loaded.directionalLight.color.a == 255,
          "omitted directional light loads defaults");

    SectorTopologyMap defaultMap = MakeSquare();
    const Json defaultSaved = Json::parse(SaveText(defaultMap));
    Check(!defaultSaved.contains("directionalLight"), "default directional light is omitted");

    Json zeroDirection = saved;
    zeroDirection["directionalLight"]["directionToLight"] = Json::array({0.0f, 0.0f, 0.0f});
    Check(LoadText(zeroDirection.dump(), loaded, error), "zero directional light vector loads");
    Check(Near(loaded.directionalLight.directionToLight, defaults.directionToLight),
          "zero directional light vector falls back to default");

    Json negativeIntensity = saved;
    negativeIntensity["directionalLight"]["intensity"] = -2.0f;
    Check(LoadText(negativeIntensity.dump(), loaded, error), "negative directional intensity loads");
    Check(Near(loaded.directionalLight.intensity, 0.0f), "negative directional intensity clamps to zero");

    Json invalid = saved;
    invalid["directionalLight"] = 4;
    ExpectRejected(invalid, "non-object directional light is rejected");
    invalid = saved;
    invalid["directionalLight"]["enabled"] = "yes";
    ExpectRejected(invalid, "wrong-type directional enabled is rejected");
    invalid = saved;
    invalid["directionalLight"]["directionToLight"] = Json::array({1, 2});
    ExpectRejected(invalid, "wrong-type directional vector is rejected");
    invalid = saved;
    invalid["directionalLight"]["color"] = Json::array({1, 2, 3});
    ExpectRejected(invalid, "wrong-type directional color is rejected");
    invalid = saved;
    invalid["directionalLight"]["color"]["r"] = "red";
    ExpectRejected(invalid, "wrong-type directional color channel is rejected");
    invalid = saved;
    invalid["directionalLight"]["intensity"] = "bright";
    ExpectRejected(invalid, "wrong-type directional intensity is rejected");
}

void TestFogSettingsRoundTripAndValidation()
{
    SectorTopologyMap original = MakeSquare();
    original.fogSettings.enabled = true;
    original.fogSettings.mode = game::SectorTopologyFogMode::Distance;
    original.fogSettings.color = Color{12, 34, 56, 128};
    original.fogSettings.startDistanceWorld = 3.5f;
    original.fogSettings.endDistanceWorld = 48.0f;
    original.fogSettings.falloffExponent = 1.75f;
    original.fogSettings.brightness = 1.4f;
    original.fogSettings.density = 0.075f;
    original.fogSettings.maxOpacity = 0.8f;
    original.fogSettings.referenceHeightWorld = -2.25f;
    original.fogSettings.heightFalloff = 1.5f;

    const Json saved = Json::parse(SaveText(original));
    Check(saved["fogSettings"].is_object(), "non-default fog settings are written");
    Check(saved["fogSettings"]["enabled"].get<bool>()
                  && saved["fogSettings"]["mode"].get<std::string>() == "distance"
                  && saved["fogSettings"]["color"]["r"].get<int>() == 12
                  && saved["fogSettings"]["color"]["g"].get<int>() == 34
                  && saved["fogSettings"]["color"]["b"].get<int>() == 56
                  && saved["fogSettings"]["color"]["a"].get<int>() == 255
                  && Near(saved["fogSettings"]["startDistanceWorld"].get<float>(), 3.5f)
                  && Near(saved["fogSettings"]["endDistanceWorld"].get<float>(), 48.0f)
                  && Near(saved["fogSettings"]["falloffExponent"].get<float>(), 1.75f)
                  && Near(saved["fogSettings"]["brightness"].get<float>(), 1.4f)
                  && Near(saved["fogSettings"]["density"].get<float>(), 0.075f)
                  && Near(saved["fogSettings"]["maxOpacity"].get<float>(), 0.8f)
                  && Near(saved["fogSettings"]["referenceHeightWorld"].get<float>(), -2.25f)
                  && Near(saved["fogSettings"]["heightFalloff"].get<float>(), 1.5f),
          "fog settings serialize normalized values");
    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(saved.dump(), loaded, error), "fog settings JSON loads");
    Check(loaded.fogSettings.enabled
                  && loaded.fogSettings.mode == game::SectorTopologyFogMode::Distance
                  && loaded.fogSettings.color.r == 12
                  && loaded.fogSettings.color.g == 34
                  && loaded.fogSettings.color.b == 56
                  && loaded.fogSettings.color.a == 255
                  && Near(loaded.fogSettings.startDistanceWorld, 3.5f)
                  && Near(loaded.fogSettings.endDistanceWorld, 48.0f)
                  && Near(loaded.fogSettings.falloffExponent, 1.75f)
                  && Near(loaded.fogSettings.brightness, 1.4f)
                  && Near(loaded.fogSettings.density, 0.075f)
                  && Near(loaded.fogSettings.maxOpacity, 0.8f)
                  && Near(loaded.fogSettings.referenceHeightWorld, -2.25f)
                  && Near(loaded.fogSettings.heightFalloff, 1.5f),
          "fog settings round-trip");
    Json legacyQuality = saved;
    legacyQuality["fogSettings"]["localVolumeQuality"] = "low";
    Check(LoadText(legacyQuality.dump(), loaded, error),
          "obsolete map-local fog quality is ignored on load");
    Check(!Json::parse(SaveText(loaded))["fogSettings"].contains(
                    "localVolumeQuality"),
          "obsolete map-local fog quality is removed on save");

    Json missingFields = saved;
    for (const char* field : {"enabled", "startDistanceWorld", "density", "maxOpacity",
                              "referenceHeightWorld", "heightFalloff"}) {
        missingFields["fogSettings"].erase(field);
    }
    Check(LoadText(missingFields.dump(), loaded, error), "omitted fog fields are accepted");
    const game::SectorTopologyFogSettings defaults = game::DefaultSectorTopologyFogSettings();
    Check(loaded.fogSettings.enabled == defaults.enabled
                  && loaded.fogSettings.color.r == 12
                  && Near(loaded.fogSettings.startDistanceWorld, defaults.startDistanceWorld)
                  && Near(loaded.fogSettings.density, defaults.density)
                  && Near(loaded.fogSettings.maxOpacity, defaults.maxOpacity)
                  && Near(loaded.fogSettings.referenceHeightWorld, defaults.referenceHeightWorld)
                  && Near(loaded.fogSettings.heightFalloff, defaults.heightFalloff),
          "omitted fog fields load defaults while present color remains");

    Json withoutFog = saved;
    withoutFog.erase("fogSettings");
    Check(LoadText(withoutFog.dump(), loaded, error), "omitted fog settings are accepted");
    Check(!loaded.fogSettings.enabled
                  && loaded.fogSettings.color.r == defaults.color.r
                  && Near(loaded.fogSettings.density, defaults.density),
          "omitted fog settings load defaults");

    Check(!Json::parse(SaveText(MakeSquare())).contains("fogSettings"),
          "default fog settings are omitted");

    Json clamped = saved;
    clamped["fogSettings"]["startDistanceWorld"] = -1.0f;
    clamped["fogSettings"]["density"] = 2.0f;
    clamped["fogSettings"]["maxOpacity"] = -1.0f;
    clamped["fogSettings"]["referenceHeightWorld"] = 999.0f;
    clamped["fogSettings"]["heightFalloff"] = 99.0f;
    Check(LoadText(clamped.dump(), loaded, error), "out-of-range fog settings load");
    Check(Near(loaded.fogSettings.startDistanceWorld, 0.0f)
                  && Near(loaded.fogSettings.density, 1.0f)
                  && Near(loaded.fogSettings.maxOpacity, 0.0f)
                  && Near(loaded.fogSettings.referenceHeightWorld, 512.0f)
                  && Near(loaded.fogSettings.heightFalloff, 16.0f),
          "out-of-range fog settings clamp");

    Json invalid = saved;
    invalid["fogSettings"] = 4;
    ExpectRejected(invalid, "non-object fog settings are rejected");
    invalid = saved;
    invalid["fogSettings"]["enabled"] = "yes";
    ExpectRejected(invalid, "wrong-type fog enabled is rejected");
    invalid = saved;
    invalid["fogSettings"]["color"] = Json::array({1, 2, 3});
    ExpectRejected(invalid, "wrong-type fog color is rejected");
    invalid = saved;
    invalid["fogSettings"]["color"]["r"] = "red";
    ExpectRejected(invalid, "wrong-type fog color channel is rejected");
    invalid = saved;
    invalid["fogSettings"]["mode"] = "volumetric";
    ExpectRejected(invalid, "unknown fog mode is rejected");
    for (const char* field : {"startDistanceWorld", "endDistanceWorld",
                              "falloffExponent", "brightness", "density", "maxOpacity",
                              "referenceHeightWorld", "heightFalloff"}) {
        invalid = saved;
        invalid["fogSettings"][field] = "invalid";
        ExpectRejected(invalid, "wrong-type fog numeric field is rejected");
    }

    SectorTopologyMap nonFinite = original;
    nonFinite.fogSettings.density = std::numeric_limits<float>::infinity();
    ExpectSaveRejected(nonFinite, "non-finite fog settings are rejected on save");
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
    changed["staticLights"][0]["castsShadow"] = "yes";
    ExpectRejected(changed, "non-boolean static light castsShadow is rejected");
    changed["staticLights"][0]["castsShadow"] = true;
    changed["staticLights"][0]["color"]["r"] = 300;
    ExpectRejected(changed, "invalid static light color channel is rejected");

    changed = valid;
    changed["staticSpotLights"] = Json::array({
            {
                    {"id", 1},
                    {"position", Json::array({1.0f, 2.0f, 3.0f})},
                    {"target", Json::array({4.0f, 5.0f, 6.0f})},
                    {"range", 8.0f},
                    {"sourceRadius", 1.0f},
                    {"intensity", 1.0f},
                    {"color", {{"r", 255}, {"g", 220}, {"b", 180}, {"a", 255}}}
            },
            {
                    {"id", 1},
                    {"position", Json::array({7.0f, 8.0f, 9.0f})},
                    {"target", Json::array({10.0f, 11.0f, 12.0f})},
                    {"range", 8.0f},
                    {"sourceRadius", 1.0f},
                    {"intensity", 1.0f},
                    {"color", {{"r", 255}, {"g", 220}, {"b", 180}, {"a", 255}}}
            }
    });
    ExpectRejected(changed, "duplicate static spot light IDs are rejected");
    changed["staticSpotLights"][1]["id"] = 2;
    changed["staticSpotLights"][0].erase("position");
    ExpectRejected(changed, "missing static spot light position is rejected");
    changed = valid;
    changed["staticSpotLights"] = Json::array({
            {
                    {"id", 1},
                    {"position", Json::array({1.0f, 2.0f, 3.0f})},
                    {"target", Json::array({4.0f, 5.0f, 6.0f})},
                    {"range", 0.0f},
                    {"sourceRadius", 0.0f},
                    {"intensity", 1.0f},
                    {"color", {{"r", 255}, {"g", 220}, {"b", 180}, {"a", 255}}}
            }
    });
    ExpectRejected(changed, "non-positive static spot light range is rejected");
    changed["staticSpotLights"][0]["range"] = 4.0f;
    changed["staticSpotLights"][0]["sourceRadius"] = 5.0f;
    ExpectRejected(changed, "oversized static spot light source radius is rejected");
    changed["staticSpotLights"][0]["sourceRadius"] = 1.0f;
    changed["staticSpotLights"][0]["intensity"] = -1.0f;
    ExpectRejected(changed, "negative static spot light intensity is rejected");
    changed["staticSpotLights"][0]["intensity"] = 1.0f;
    changed["staticSpotLights"][0]["innerConeDegrees"] = "wide";
    ExpectRejected(changed, "non-number static spot light inner cone is rejected");
    changed["staticSpotLights"][0]["innerConeDegrees"] = 20.0f;
    changed["staticSpotLights"][0]["outerConeDegrees"] = "wider";
    ExpectRejected(changed, "non-number static spot light outer cone is rejected");
    changed["staticSpotLights"][0]["outerConeDegrees"] = 35.0f;
    changed["staticSpotLights"][0]["castsShadow"] = "yes";
    ExpectRejected(changed, "non-boolean static spot light castsShadow is rejected");
    changed["staticSpotLights"][0]["castsShadow"] = true;
    changed["staticSpotLights"][0]["color"]["r"] = 300;
    ExpectRejected(changed, "invalid static spot light color channel is rejected");

    changed = valid;
    changed["dynamicPointLights"] = Json::array({
            {
                    {"id", 1},
                    {"position", Json::array({1.0f, 2.0f, 3.0f})},
                    {"radius", 8.0f},
                    {"intensity", 1.0f},
                    {"color", {{"r", 255}, {"g", 220}, {"b", 180}, {"a", 255}}}
            },
            {
                    {"id", 1},
                    {"position", Json::array({4.0f, 5.0f, 6.0f})},
                    {"radius", 8.0f},
                    {"intensity", 1.0f},
                    {"color", {{"r", 255}, {"g", 220}, {"b", 180}, {"a", 255}}}
            }
    });
    ExpectRejected(changed, "duplicate dynamic point light IDs are rejected");
    changed["dynamicPointLights"][1]["id"] = 2;
    changed["dynamicPointLights"][0].erase("position");
    ExpectRejected(changed, "missing dynamic point light position is rejected");
    changed = valid;
    changed["dynamicPointLights"] = Json::array({
            {
                    {"id", 1},
                    {"position", Json::array({1.0f, 2.0f, 3.0f})},
                    {"radius", 0.0f},
                    {"intensity", 1.0f},
                    {"color", {{"r", 255}, {"g", 220}, {"b", 180}, {"a", 255}}}
            }
    });
    ExpectRejected(changed, "non-positive dynamic point light radius is rejected");
    changed["dynamicPointLights"][0]["radius"] = 4.0f;
    changed["dynamicPointLights"][0]["enabled"] = "yes";
    ExpectRejected(changed, "non-boolean dynamic point light enabled is rejected");
    changed["dynamicPointLights"][0]["enabled"] = true;
    changed["dynamicPointLights"][0]["flicker"] = "yes";
    ExpectRejected(changed, "non-boolean dynamic point light flicker is rejected");
    changed["dynamicPointLights"][0]["flicker"] = true;
    changed["dynamicPointLights"][0]["flickerSpeed"] = "fast";
    ExpectRejected(changed, "non-number dynamic point light flicker speed is rejected");
    changed["dynamicPointLights"][0]["flickerSpeed"] = 1.0f;
    changed["dynamicPointLights"][0]["flickerAmount"] = "deep";
    ExpectRejected(changed, "non-number dynamic point light flicker amount is rejected");
    changed["dynamicPointLights"][0]["flickerAmount"] = 0.5f;
    changed["dynamicPointLights"][0]["color"]["r"] = 300;
    ExpectRejected(changed, "invalid dynamic point light color channel is rejected");

    changed = valid;
    changed["dynamicSpotLights"] = Json::array({
            {
                    {"id", 1},
                    {"position", Json::array({1.0f, 2.0f, 3.0f})},
                    {"target", Json::array({4.0f, 5.0f, 6.0f})},
                    {"range", 8.0f},
                    {"intensity", 1.0f},
                    {"color", {{"r", 255}, {"g", 220}, {"b", 180}, {"a", 255}}}
            },
            {
                    {"id", 1},
                    {"position", Json::array({7.0f, 8.0f, 9.0f})},
                    {"target", Json::array({10.0f, 11.0f, 12.0f})},
                    {"range", 8.0f},
                    {"intensity", 1.0f},
                    {"color", {{"r", 255}, {"g", 220}, {"b", 180}, {"a", 255}}}
            }
    });
    ExpectRejected(changed, "duplicate dynamic spot light IDs are rejected");
    changed["dynamicSpotLights"][1]["id"] = 2;
    changed["dynamicSpotLights"][0].erase("position");
    ExpectRejected(changed, "missing dynamic spot light position is rejected");
    changed = valid;
    changed["dynamicSpotLights"] = Json::array({
            {
                    {"id", 1},
                    {"position", Json::array({1.0f, 2.0f, 3.0f})},
                    {"target", Json::array({4.0f, 5.0f, 6.0f})},
                    {"range", 0.0f},
                    {"intensity", 1.0f},
                    {"color", {{"r", 255}, {"g", 220}, {"b", 180}, {"a", 255}}}
            }
    });
    ExpectRejected(changed, "non-positive dynamic spot light range is rejected");
    changed["dynamicSpotLights"][0]["range"] = 4.0f;
    changed["dynamicSpotLights"][0]["intensity"] = -1.0f;
    ExpectRejected(changed, "negative dynamic spot light intensity is rejected");
    changed["dynamicSpotLights"][0]["intensity"] = 1.0f;
    changed["dynamicSpotLights"][0]["enabled"] = "yes";
    ExpectRejected(changed, "non-boolean dynamic spot light enabled is rejected");
    changed["dynamicSpotLights"][0]["enabled"] = true;
    changed["dynamicSpotLights"][0]["flicker"] = "yes";
    ExpectRejected(changed, "non-boolean dynamic spot light flicker is rejected");
    changed["dynamicSpotLights"][0]["flicker"] = true;
    changed["dynamicSpotLights"][0]["flickerSpeed"] = "fast";
    ExpectRejected(changed, "non-number dynamic spot light flicker speed is rejected");
    changed["dynamicSpotLights"][0]["flickerSpeed"] = 1.0f;
    changed["dynamicSpotLights"][0]["flickerAmount"] = "deep";
    ExpectRejected(changed, "non-number dynamic spot light flicker amount is rejected");
    changed["dynamicSpotLights"][0]["flickerAmount"] = 0.5f;
    changed["dynamicSpotLights"][0]["castsShadow"] = "yes";
    ExpectRejected(changed, "non-boolean dynamic spot light castsShadow is rejected");
    changed["dynamicSpotLights"][0]["castsShadow"] = true;
    changed["dynamicSpotLights"][0]["shadowPriority"] = 1.5f;
    ExpectRejected(changed, "non-integer dynamic spot light shadow priority is rejected");
    changed["dynamicSpotLights"][0]["shadowPriority"] = 12;
    changed["dynamicSpotLights"][0]["shadowBias"] = "near";
    ExpectRejected(changed, "non-number dynamic spot light shadow bias is rejected");
    changed["dynamicSpotLights"][0]["shadowBias"] = 0.01f;
    changed["dynamicSpotLights"][0]["shadowStrength"] = "strong";
    ExpectRejected(changed, "non-number dynamic spot light shadow strength is rejected");
    changed["dynamicSpotLights"][0]["shadowStrength"] = 0.5f;
    changed["dynamicSpotLights"][0]["shadowSoftness"] = "soft";
    ExpectRejected(changed, "non-number dynamic spot light shadow softness is rejected");
    changed["dynamicSpotLights"][0]["shadowSoftness"] = 1.5f;
    changed["dynamicSpotLights"][0]["innerConeDegrees"] = "wide";
    ExpectRejected(changed, "non-number dynamic spot light inner cone is rejected");
    changed["dynamicSpotLights"][0]["innerConeDegrees"] = 20.0f;
    changed["dynamicSpotLights"][0]["outerConeDegrees"] = "wider";
    ExpectRejected(changed, "non-number dynamic spot light outer cone is rejected");
    changed["dynamicSpotLights"][0]["outerConeDegrees"] = 35.0f;
    changed["dynamicSpotLights"][0]["color"]["r"] = 300;
    ExpectRejected(changed, "invalid dynamic spot light color channel is rejected");
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
    first.staticSpotLights.push_back(SectorTopologyStaticSpotLight{
            6,
            Vector3{1.0f, 2.0f, 3.0f},
            Vector3{4.0f, 5.0f, 6.0f},
            WHITE,
            1.0f,
            16.0f,
            20.0f,
            35.0f,
            0.0f
    });
    first.staticSpotLights.push_back(SectorTopologyStaticSpotLight{
            3,
            Vector3{7.0f, 8.0f, 9.0f},
            Vector3{10.0f, 11.0f, 12.0f},
            WHITE,
            2.0f,
            24.0f,
            15.0f,
            45.0f,
            1.0f
    });
    second.staticSpotLights = first.staticSpotLights;
    std::reverse(second.staticSpotLights.begin(), second.staticSpotLights.end());
    first.dynamicPointLights.push_back(SectorTopologyDynamicPointLight{
            3, Vector3{3.0f, 4.0f, 5.0f}, WHITE, 1.0f, 8.0f, true});
    first.dynamicPointLights.push_back(SectorTopologyDynamicPointLight{
            1, Vector3{1.0f, 2.0f, 3.0f}, Color{40, 50, 60, 255}, 2.0f, 16.0f, false});
    second.dynamicPointLights = first.dynamicPointLights;
    std::reverse(second.dynamicPointLights.begin(), second.dynamicPointLights.end());
    first.dynamicSpotLights.push_back(SectorTopologyDynamicSpotLight{
            4,
            Vector3{4.0f, 5.0f, 6.0f},
            Vector3{5.0f, 6.0f, 7.0f},
            WHITE,
            1.0f,
            8.0f,
            20.0f,
            35.0f,
            true
    });
    first.dynamicSpotLights.push_back(SectorTopologyDynamicSpotLight{
            2,
            Vector3{2.0f, 3.0f, 4.0f},
            Vector3{3.0f, 4.0f, 5.0f},
            Color{80, 90, 100, 255},
            2.0f,
            16.0f,
            12.0f,
            45.0f,
            false
    });
    second.dynamicSpotLights = first.dynamicSpotLights;
    std::reverse(second.dynamicSpotLights.begin(), second.dynamicSpotLights.end());
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

void TestDynamicPointLightHelpers()
{
    SectorTopologyMap map = MakeSquare();
    Check(game::AllocateSectorTopologyDynamicLightId(map) == 1,
          "first topology dynamic light ID is 1");
    map.dynamicPointLights.push_back(SectorTopologyDynamicPointLight{
            4, Vector3{0.0f, 1.0f, 2.0f}, WHITE, 1.0f, 8.0f, true});
    map.dynamicPointLights.push_back(SectorTopologyDynamicPointLight{
            2, Vector3{3.0f, 4.0f, 5.0f}, WHITE, 2.0f, 16.0f, false});
    Check(game::AllocateSectorTopologyDynamicLightId(map) == 5,
          "topology dynamic light allocation returns max plus one");
    Check(game::FindSectorTopologyDynamicLight(map, 2) != nullptr,
          "topology dynamic light lookup finds existing ID");
    Check(game::RemoveSectorTopologyDynamicLight(map, 2),
          "topology dynamic light delete succeeds for existing ID");
    Check(game::FindSectorTopologyDynamicLight(map, 2) == nullptr
                  && game::FindSectorTopologyDynamicLight(map, 4) != nullptr,
          "topology dynamic light delete preserves remaining lights");
    Check(!game::RemoveSectorTopologyDynamicLight(map, 99),
          "topology dynamic light delete fails for unknown ID");
}

void TestStaticSpotLightHelpers()
{
    SectorTopologyMap map = MakeSquare();
    Check(game::AllocateSectorTopologyStaticSpotLightId(map) == 1,
          "first topology static spot light ID is 1");
    map.staticSpotLights.push_back(SectorTopologyStaticSpotLight{
            4,
            Vector3{0.0f, 1.0f, 2.0f},
            Vector3{1.0f, 2.0f, 3.0f},
            WHITE,
            1.0f,
            8.0f,
            20.0f,
            35.0f,
            0.0f
    });
    map.staticSpotLights.push_back(SectorTopologyStaticSpotLight{
            2,
            Vector3{3.0f, 4.0f, 5.0f},
            Vector3{6.0f, 7.0f, 8.0f},
            WHITE,
            2.0f,
            16.0f,
            15.0f,
            45.0f,
            1.0f
    });
    Check(game::AllocateSectorTopologyStaticSpotLightId(map) == 5,
          "topology static spot light allocation returns max plus one");
    Check(game::FindSectorTopologyStaticSpotLight(map, 2) != nullptr,
          "topology static spot light lookup finds existing ID");
    Check(game::RemoveSectorTopologyStaticSpotLight(map, 2),
          "topology static spot light delete succeeds for existing ID");
    Check(game::FindSectorTopologyStaticSpotLight(map, 2) == nullptr
                  && game::FindSectorTopologyStaticSpotLight(map, 4) != nullptr,
          "topology static spot light delete preserves remaining lights");
    Check(!game::RemoveSectorTopologyStaticSpotLight(map, 99),
          "topology static spot light delete fails for unknown ID");
}

void TestDynamicSpotLightHelpers()
{
    SectorTopologyMap map = MakeSquare();
    Check(game::AllocateSectorTopologyDynamicSpotLightId(map) == 1,
          "first topology dynamic spot light ID is 1");
    map.dynamicSpotLights.push_back(SectorTopologyDynamicSpotLight{
            4,
            Vector3{0.0f, 1.0f, 2.0f},
            Vector3{1.0f, 2.0f, 3.0f},
            WHITE,
            1.0f,
            8.0f,
            20.0f,
            35.0f,
            true
    });
    map.dynamicSpotLights.push_back(SectorTopologyDynamicSpotLight{
            2,
            Vector3{3.0f, 4.0f, 5.0f},
            Vector3{6.0f, 7.0f, 8.0f},
            WHITE,
            2.0f,
            16.0f,
            15.0f,
            45.0f,
            false
    });
    Check(game::AllocateSectorTopologyDynamicSpotLightId(map) == 5,
          "topology dynamic spot light allocation returns max plus one");
    Check(game::FindSectorTopologyDynamicSpotLight(map, 2) != nullptr,
          "topology dynamic spot light lookup finds existing ID");
    Check(game::RemoveSectorTopologyDynamicSpotLight(map, 2),
          "topology dynamic spot light delete succeeds for existing ID");
    Check(game::FindSectorTopologyDynamicSpotLight(map, 2) == nullptr
                  && game::FindSectorTopologyDynamicSpotLight(map, 4) != nullptr,
          "topology dynamic spot light delete preserves remaining lights");
    Check(!game::RemoveSectorTopologyDynamicSpotLight(map, 99),
          "topology dynamic spot light delete fails for unknown ID");
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

void TestGraphNativeEmptyAndLooseGraphRoundTrip()
{
    game::SectorAuthoringDocument empty;
    const Json emptySaved = Json::parse(SaveAuthoringText(empty));
    Check(emptySaved["formatVersion"] == 3, "graph-native format version is written");
    Check(emptySaved["topology"] == "authoringGraph", "graph-native topology marker is written");
    Check(emptySaved.contains("authoringGraph"), "graph-native source graph is written");
    Check(!emptySaved.contains("vertices")
                  && !emptySaved.contains("linedefs")
                  && !emptySaved.contains("sidedefs")
                  && !emptySaved.contains("sectors"),
          "graph-native save does not require strict topology arrays");

    game::SectorAuthoringDocument loaded;
    std::string error = "stale";
    Check(LoadAuthoringText(emptySaved.dump(), loaded, error), "empty graph-native document loads");
    Check(error.empty(), "successful graph-native load clears error");
    Check(loaded.graph.vertices.empty()
                  && loaded.graph.lines.empty()
                  && loaded.graph.lineSides.empty()
                  && loaded.graph.faceAnchors.empty(),
          "empty graph round-trips");
    Check(!loaded.derivation.success && !loaded.derivation.diagnostics.empty(),
          "empty graph loads with derivation diagnostics instead of failing parse");

    game::SectorAuthoringDocument loose;
    loose.graph.vertices.push_back(game::SectorAuthoringVertex{1, 0, 0});
    loose.graph.vertices.push_back(game::SectorAuthoringVertex{2, 32, 0});
    loose.graph.lines.push_back(game::SectorAuthoringLine{7, 1, 2});
    const std::string looseText = SaveAuthoringText(loose);
    Check(LoadAuthoringText(looseText, loaded, error), "loose line graph-native document loads");
    Check(loaded.graph.lines.size() == 1 && loaded.graph.lines[0].id == 7,
          "loose line source graph round-trips");
    Check(!loaded.derivation.success && !loaded.derivation.diagnostics.empty(),
          "loose line graph reports diagnostics after load");
}

void TestGraphNativeInvalidGraphAndTransactionalFailure()
{
    game::SectorAuthoringDocument invalid;
    invalid.graph.vertices.push_back(game::SectorAuthoringVertex{1, 0, 0});
    invalid.graph.lines.push_back(game::SectorAuthoringLine{3, 1, 99});
    const std::string invalidText = SaveAuthoringText(invalid);

    game::SectorAuthoringDocument loaded;
    std::string error;
    Check(LoadAuthoringText(invalidText, loaded, error),
          "invalid authoring references load as graph data");
    Check(!loaded.derivation.success && !loaded.derivation.diagnostics.empty(),
          "invalid authoring references produce load-time derivation diagnostics");
    Check(loaded.graph.lines.size() == 1 && loaded.graph.lines[0].endVertexId == 99,
          "invalid source graph is still preserved after load");

    game::SectorAuthoringDocument output;
    output.graph.vertices.push_back(game::SectorAuthoringVertex{42, 1, 2});
    Json malformed = Json::parse(invalidText);
    malformed["authoringGraph"]["vertices"] = Json::object();
    Check(!LoadAuthoringText(malformed.dump(), output, error), "malformed graph-native load fails");
    Check(!error.empty(), "malformed graph-native load reports an error");
    Check(output.graph.vertices.size() == 1 && output.graph.vertices[0].id == 42,
          "failed graph-native load leaves output document unchanged");
}

void TestGraphNativeValidGraphDerivesTopologyAndProperties()
{
    SectorTopologyMap source = MakeSquare();
    source.lineDefs[0].flags.blocksPlayer = true;
    source.sideDefs[0].wall = MakePart("wall", 4.0f, 5.0f, 6.0f, 7.0f);
    source.sideDefs[0].middle = MakePart("wall", 2.0f, 3.0f, 4.0f, 5.0f);
    source.sectors[0].name = "atrium";
    source.sectors[0].ceilingSky = true;
    source.sectors[0].floorZ = -4.0f;
    source.sectors[0].ceilingZ = 30.0f;
    source.sectors[0].ambientIntensity = 0.5f;

    const game::SectorAuthoringDocument original = MakeAuthoringDocumentFromMap(source);
    Check(original.derivation.success, "imported square graph derives before save");
    const Json saved = Json::parse(SaveAuthoringText(original));
    Check(saved["authoringGraph"]["lineSides"][0].contains("middle"),
          "authoring side middle settings are persisted");
    Check(saved["authoringGraph"]["faceAnchors"][0]["ceilingSky"] == true,
          "face anchor ceilingSky is persisted");
    Check(!saved.contains("bakedLightmap"),
          "graph-native save omits absent baked lightmap metadata");

    game::SectorAuthoringDocument loaded;
    std::string error;
    Check(LoadAuthoringText(saved.dump(), loaded, error), "valid graph-native document loads");
    Check(loaded.derivation.success, "valid graph regenerates derived topology on load");
    Check(loaded.derivation.topology.sectors.size() == 1
                  && loaded.derivation.topology.lineDefs.size() == 4,
          "derived topology is regenerated from graph-native source");
    const game::SectorTopologyLineDef* line = game::FindSectorTopologyLineDef(loaded.derivation.topology, 1);
    Check(line != nullptr && line->flags.blocksPlayer,
          "authoring linedef flags project after graph-native load");
    const game::SectorTopologySideDef* side = game::FindSectorTopologySideDef(
            loaded.derivation.topology,
            loaded.derivation.topology.lineDefs[0].frontSideDefId);
    Check(side != nullptr && side->wall.uv.scale.x == 4.0f && side->middle.textureId == "wall",
          "authoring side materials project after graph-native load");
    const game::SectorTopologySector* sector =
            game::FindSectorTopologySector(loaded.derivation.topology, 1);
    Check(sector != nullptr && sector->name == "atrium" && sector->ceilingSky
                  && Near(sector->floorZ, -4.0f)
                  && Near(sector->ambientIntensity, 0.5f),
          "face anchor properties project after graph-native load");
}

void TestGraphNativeMapLevelRoundTrip()
{
    SectorTopologyMap source = MakeSquare();
    source.texturesById.emplace("sky", SectorTextureDefinition{
            "sky", "textures/sky.png", SectorTextureFilter::Trilinear});
    source.staticLights.push_back(SectorTopologyStaticPointLight{
            9,
            Vector3{1.0f, 2.0f, 3.0f},
            Color{10, 20, 30, 255},
            2.0f,
            32.0f,
            1.0f
    });
    source.staticLights.back().castsShadow = false;
    source.staticSpotLights.push_back(SectorTopologyStaticSpotLight{
            12,
            Vector3{2.0f, 3.0f, 4.0f},
            Vector3{5.0f, 6.0f, 7.0f},
            Color{40, 50, 60, 255},
            1.75f,
            40.0f,
            16.0f,
            38.0f,
            0.75f
    });
    source.staticSpotLights.back().castsShadow = false;
    source.dynamicPointLights.push_back(SectorTopologyDynamicPointLight{
            10,
            Vector3{4.0f, 5.0f, 6.0f},
            Color{70, 80, 90, 255},
            1.25f,
            48.0f,
            false,
            true,
            3.0f,
            0.75f
    });
    source.dynamicSpotLights.push_back(SectorTopologyDynamicSpotLight{
            11,
            Vector3{7.0f, 8.0f, 9.0f},
            Vector3{10.0f, 11.0f, 12.0f},
            Color{100, 110, 120, 255},
            1.5f,
            56.0f,
            18.0f,
            44.0f,
            false,
            true,
            2.0f,
            0.5f,
            true,
            23,
            0.02f,
            0.8f,
            3.0f
    });
    source.previewSettings.walkSpeed = 9.0f;
    source.audioSettings.musicPath = "music/graph_theme.ogg";
    source.audioSettings.musicVolume = 0.4f;
    source.audioSettings.soundsById.emplace(
            "ambient_hum", SectorSoundDefinition{
                    "ambient_hum", "ambience/hum.wav", SectorSoundType::Sound});
    source.skySettings.textureId = "sky";
    source.skySettings.yawOffsetDegrees = 17.0f;
    source.directionalLight.enabled = true;
    source.directionalLight.directionToLight = Vector3{0.0f, 1.0f, 0.0f};
    source.directionalLight.intensity = 1.5f;
    source.fogSettings.enabled = true;
    source.fogSettings.density = 0.125f;
    source.lightmapSettings.ambientOcclusionStrength = 0.25f;
    source.bakedLightmap.path = "assets/levels/test/test.lightmap.png";
    source.bakedLightmap.width = 128;
    source.bakedLightmap.height = 128;
    source.bakedLightmap.version = game::kSectorLightmapArtifactVersion;
    source.bakedLightmap.format = game::kSectorLightmapArtifactFormat;
    source.bakedLightmap.sourceHash = "abc123";
    source.bakedLightmap.additionalAtlases.push_back(
            game::SectorLightmapAtlasMetadata{
                    "assets/levels/test/test.lightmap.1.png",
                    128,
                    128});
    source.bakedLightmap.objectProbes.path = "assets/levels/test/test.lightmap.object_probes.bin";
    source.bakedLightmap.objectProbes.version = 1;
    source.bakedLightmap.objectProbes.sourceHash = "abc123";
    source.bakedLightmap.objectProbes.count = 7;
    source.bakedLightmap.objectProbes.probeSpacingWorld = 5.5f;
    source.bakedLightmap.objectProbes.probeLowerHeightWorld = 0.7f;
    source.bakedLightmap.objectProbes.probeUpperHeightWorld = 1.4f;
    source.bakedLightmap.objectProbes.format = "ambientCubeF32LE";
    source.bakedLightmap.staticModels.path =
            "assets/levels/test/test.lightmap.static_models.bin";
    source.bakedLightmap.staticModels.version = 1;
    source.bakedLightmap.staticModels.sourceHash = "abc123";
    source.bakedLightmap.staticModels.modelCount = 2;
    source.bakedLightmap.staticModels.objectCount = 5;
    source.bakedLightmap.staticModels.format = "staticModelUvRemapF32LE";

    const game::SectorAuthoringDocument original = MakeAuthoringDocumentFromMap(source);
    const Json saved = Json::parse(SaveAuthoringText(original));
    Check(saved["textures"].contains("sky"), "graph-native texture registry is persisted");
    Check(saved["staticLights"][0]["id"] == 9
                  && saved["staticLights"][0]["castsShadow"] == false,
          "graph-native static lights are persisted");
    Check(saved["staticSpotLights"][0]["id"] == 12
                  && Near(saved["staticSpotLights"][0]["innerConeDegrees"].get<float>(), 16.0f)
                  && Near(saved["staticSpotLights"][0]["outerConeDegrees"].get<float>(), 38.0f)
                  && Near(saved["staticSpotLights"][0]["sourceRadius"].get<float>(), 0.75f)
                  && saved["staticSpotLights"][0]["castsShadow"] == false,
          "graph-native static spot lights are persisted");
    Check(saved["dynamicPointLights"][0]["id"] == 10,
          "graph-native dynamic point lights are persisted");
    Check(saved["dynamicPointLights"][0]["flicker"] == true
                  && Near(saved["dynamicPointLights"][0]["flickerSpeed"].get<float>(), 3.0f)
                  && Near(saved["dynamicPointLights"][0]["flickerAmount"].get<float>(), 0.75f),
          "graph-native dynamic point light flicker fields are persisted");
    Check(saved["dynamicSpotLights"][0]["id"] == 11
                  && saved["dynamicSpotLights"][0]["enabled"] == false
                  && Near(saved["dynamicSpotLights"][0]["innerConeDegrees"].get<float>(), 18.0f)
                  && Near(saved["dynamicSpotLights"][0]["outerConeDegrees"].get<float>(), 44.0f)
                  && saved["dynamicSpotLights"][0]["flicker"] == true
                  && saved["dynamicSpotLights"][0]["castsShadow"] == true
                  && saved["dynamicSpotLights"][0]["shadowPriority"] == 23
                  && Near(saved["dynamicSpotLights"][0]["shadowBias"].get<float>(), 0.02f)
                  && Near(saved["dynamicSpotLights"][0]["shadowStrength"].get<float>(), 0.8f)
                  && Near(saved["dynamicSpotLights"][0]["shadowSoftness"].get<float>(), 3.0f),
          "graph-native dynamic spot lights are persisted");
    Check(saved["previewSettings"]["walkSpeed"] == 9.0f, "graph-native preview settings are persisted");
    Check(saved["audio"]["music"] == "music/graph_theme.ogg"
                  && Near(saved["audio"]["musicVolume"].get<float>(), 0.4f)
                  && saved["audio"]["sounds"]["ambient_hum"]["path"]
                  == "ambience/hum.wav",
          "graph-native audio settings are persisted");
    Check(saved["skySettings"]["textureId"] == "sky", "graph-native sky settings are persisted");
    Check(saved["directionalLight"]["enabled"] == true,
          "graph-native directional light settings are persisted");
    Check(saved["fogSettings"]["enabled"] == true
                  && Near(saved["fogSettings"]["density"].get<float>(), 0.125f),
          "graph-native fog settings are persisted");
    Check(saved["lightmapSettings"]["ambientOcclusionStrength"] == 0.25f,
          "graph-native bake settings are persisted");
    Check(saved["bakedLightmap"].is_object(), "graph-native baked lightmap metadata is persisted");
    Check(saved["bakedLightmap"]["path"] == "assets/levels/test/test.lightmap.png",
          "graph-native baked lightmap path is persisted");
    Check(saved["bakedLightmap"]["width"] == 128
                  && saved["bakedLightmap"]["height"] == 128,
          "graph-native baked lightmap dimensions are persisted");
    Check(saved["bakedLightmap"]["sourceHash"] == "abc123",
          "graph-native baked lightmap source hash is persisted");
    Check(saved["bakedLightmap"]["additionalAtlases"].size() == 1
                  && saved["bakedLightmap"]["additionalAtlases"][0]["path"]
                          == "assets/levels/test/test.lightmap.1.png",
          "graph-native additional lightmap atlas metadata is persisted");
    Check(saved["bakedLightmap"]["objectProbes"].is_object(),
          "graph-native baked object probe metadata is persisted");
    Check(saved["bakedLightmap"]["objectProbes"]["path"]
                  == "assets/levels/test/test.lightmap.object_probes.bin",
          "graph-native baked object probe sidecar path is persisted");
    Check(saved["bakedLightmap"]["objectProbes"]["version"] == 1
                  && saved["bakedLightmap"]["objectProbes"]["sourceHash"] == "abc123"
                  && saved["bakedLightmap"]["objectProbes"]["count"] == 7
                  && Near(saved["bakedLightmap"]["objectProbes"]["probeSpacingWorld"].get<float>(), 5.5f)
                  && Near(saved["bakedLightmap"]["objectProbes"]["probeLowerHeightWorld"].get<float>(), 0.7f)
                  && Near(saved["bakedLightmap"]["objectProbes"]["probeUpperHeightWorld"].get<float>(), 1.4f)
                  && saved["bakedLightmap"]["objectProbes"]["format"] == "ambientCubeF32LE",
          "graph-native baked object probe sidecar metadata is persisted");
    Check(saved["bakedLightmap"]["staticModels"]["path"]
                      == "assets/levels/test/test.lightmap.static_models.bin"
                  && saved["bakedLightmap"]["staticModels"]["version"] == 1
                  && saved["bakedLightmap"]["staticModels"]["sourceHash"] == "abc123"
                  && saved["bakedLightmap"]["staticModels"]["modelCount"] == 2
                  && saved["bakedLightmap"]["staticModels"]["objectCount"] == 5
                  && saved["bakedLightmap"]["staticModels"]["format"]
                          == "staticModelUvRemapF32LE",
          "graph-native baked static model lightmap metadata is persisted");

    game::SectorAuthoringDocument loaded;
    std::string error;
    Check(LoadAuthoringText(saved.dump(), loaded, error), "graph-native map-level data loads");
    Check(loaded.mapData.texturesById.count("sky") == 1
                  && loaded.mapData.staticLights.size() == 1
                  && !loaded.mapData.staticLights[0].castsShadow
                  && loaded.mapData.staticSpotLights.size() == 1
                  && loaded.mapData.staticSpotLights[0].id == 12
                  && Near(loaded.mapData.staticSpotLights[0].innerConeDegrees, 16.0f)
                  && Near(loaded.mapData.staticSpotLights[0].outerConeDegrees, 38.0f)
                  && Near(loaded.mapData.staticSpotLights[0].sourceRadius, 0.75f)
                  && !loaded.mapData.staticSpotLights[0].castsShadow
                  && loaded.mapData.dynamicPointLights.size() == 1
                  && loaded.mapData.dynamicPointLights[0].flicker
                  && Near(loaded.mapData.dynamicPointLights[0].flickerSpeed, 3.0f)
                  && Near(loaded.mapData.dynamicPointLights[0].flickerAmount, 0.75f)
                  && loaded.mapData.dynamicSpotLights.size() == 1
                  && loaded.mapData.dynamicSpotLights[0].id == 11
                  && !loaded.mapData.dynamicSpotLights[0].enabled
                  && Near(loaded.mapData.dynamicSpotLights[0].innerConeDegrees, 18.0f)
                  && Near(loaded.mapData.dynamicSpotLights[0].outerConeDegrees, 44.0f)
                  && loaded.mapData.dynamicSpotLights[0].flicker
                  && loaded.mapData.dynamicSpotLights[0].castsShadow
                  && loaded.mapData.dynamicSpotLights[0].shadowPriority == 23
                  && Near(loaded.mapData.dynamicSpotLights[0].shadowBias, 0.02f)
                  && Near(loaded.mapData.dynamicSpotLights[0].shadowStrength, 0.8f)
                  && Near(loaded.mapData.dynamicSpotLights[0].shadowSoftness, 3.0f)
                  && Near(loaded.mapData.previewSettings.walkSpeed, 9.0f)
                  && loaded.mapData.audioSettings.musicPath
                          == "music/graph_theme.ogg"
                  && Near(loaded.mapData.audioSettings.musicVolume, 0.4f)
                  && loaded.mapData.audioSettings.soundsById.at("ambient_hum").path
                          == "ambience/hum.wav"
                  && loaded.mapData.skySettings.textureId == "sky"
                  && loaded.mapData.directionalLight.enabled
                  && loaded.mapData.fogSettings.enabled
                  && Near(loaded.mapData.fogSettings.density, 0.125f)
                  && Near(loaded.mapData.lightmapSettings.ambientOcclusionStrength, 0.25f)
                  && loaded.mapData.bakedLightmap.path == "assets/levels/test/test.lightmap.png"
                  && loaded.mapData.bakedLightmap.width == 128
                  && loaded.mapData.bakedLightmap.height == 128
                  && loaded.mapData.bakedLightmap.sourceHash == "abc123"
                  && loaded.mapData.bakedLightmap.additionalAtlases.size() == 1
                  && loaded.mapData.bakedLightmap.objectProbes.path
                          == "assets/levels/test/test.lightmap.object_probes.bin"
                  && loaded.mapData.bakedLightmap.objectProbes.version == 1
                  && loaded.mapData.bakedLightmap.objectProbes.sourceHash == "abc123"
                  && loaded.mapData.bakedLightmap.objectProbes.count == 7
                  && Near(loaded.mapData.bakedLightmap.objectProbes.probeSpacingWorld, 5.5f)
                  && Near(loaded.mapData.bakedLightmap.objectProbes.probeLowerHeightWorld, 0.7f)
                  && Near(loaded.mapData.bakedLightmap.objectProbes.probeUpperHeightWorld, 1.4f)
                  && loaded.mapData.bakedLightmap.objectProbes.format == "ambientCubeF32LE"
                  && loaded.mapData.bakedLightmap.staticModels.path
                          == "assets/levels/test/test.lightmap.static_models.bin"
                  && loaded.mapData.bakedLightmap.staticModels.modelCount == 2
                  && loaded.mapData.bakedLightmap.staticModels.objectCount == 5,
          "graph-native map-level fields round-trip");
    Check(loaded.derivation.success
                  && loaded.derivation.topology.audioSettings.musicPath
                          == "music/graph_theme.ogg"
                  && Near(loaded.derivation.topology.audioSettings.musicVolume, 0.4f)
                  && loaded.derivation.topology.audioSettings.soundsById.at(
                          "ambient_hum").path == "ambience/hum.wav"
                  && loaded.derivation.topology.texturesById.count("sky") == 1
                  && loaded.derivation.topology.staticLights.size() == 1
                  && !loaded.derivation.topology.staticLights[0].castsShadow
                  && loaded.derivation.topology.staticSpotLights.size() == 1
                  && loaded.derivation.topology.staticSpotLights[0].id == 12
                  && !loaded.derivation.topology.staticSpotLights[0].castsShadow
                  && loaded.derivation.topology.dynamicPointLights.size() == 1
                  && loaded.derivation.topology.dynamicPointLights[0].flicker
                  && Near(loaded.derivation.topology.dynamicPointLights[0].flickerSpeed, 3.0f)
                  && Near(loaded.derivation.topology.dynamicPointLights[0].flickerAmount, 0.75f)
                  && loaded.derivation.topology.dynamicSpotLights.size() == 1
                  && loaded.derivation.topology.dynamicSpotLights[0].id == 11
                  && !loaded.derivation.topology.dynamicSpotLights[0].enabled
                  && loaded.derivation.topology.dynamicSpotLights[0].castsShadow
                  && loaded.derivation.topology.dynamicSpotLights[0].shadowPriority == 23
                  && Near(loaded.derivation.topology.dynamicSpotLights[0].shadowBias, 0.02f)
                  && Near(loaded.derivation.topology.dynamicSpotLights[0].shadowStrength, 0.8f)
                  && Near(loaded.derivation.topology.dynamicSpotLights[0].shadowSoftness, 3.0f)
                  && loaded.derivation.topology.skySettings.textureId == "sky"
                  && loaded.derivation.topology.fogSettings.enabled
                  && Near(loaded.derivation.topology.fogSettings.density, 0.125f)
                  && loaded.derivation.topology.bakedLightmap.path == "assets/levels/test/test.lightmap.png"
                  && loaded.derivation.topology.bakedLightmap.sourceHash == "abc123"
                  && loaded.derivation.topology.bakedLightmap.additionalAtlases.size() == 1
                  && loaded.derivation.topology.bakedLightmap.objectProbes.path
                          == "assets/levels/test/test.lightmap.object_probes.bin"
                  && loaded.derivation.topology.bakedLightmap.objectProbes.count == 7
                  && loaded.derivation.topology.bakedLightmap.staticModels.path
                          == "assets/levels/test/test.lightmap.static_models.bin",
          "derived topology receives map-level fields after load");

    const Json resaved = Json::parse(SaveAuthoringText(loaded));
    Check(resaved["bakedLightmap"]["path"] == saved["bakedLightmap"]["path"]
                  && resaved["bakedLightmap"]["width"] == saved["bakedLightmap"]["width"]
                  && resaved["bakedLightmap"]["height"] == saved["bakedLightmap"]["height"]
                  && resaved["bakedLightmap"]["sourceHash"] == saved["bakedLightmap"]["sourceHash"]
                  && resaved["bakedLightmap"]["additionalAtlases"] == saved["bakedLightmap"]["additionalAtlases"]
                  && resaved["bakedLightmap"]["objectProbes"] == saved["bakedLightmap"]["objectProbes"]
                  && resaved["bakedLightmap"]["staticModels"] == saved["bakedLightmap"]["staticModels"],
          "graph-native save/load/save preserves baked lightmap metadata");

    Json legacyProbeMetadata = saved;
    legacyProbeMetadata["bakedLightmap"]["objectProbes"].erase("probeLowerHeightWorld");
    legacyProbeMetadata["bakedLightmap"]["objectProbes"].erase("probeUpperHeightWorld");
    legacyProbeMetadata["bakedLightmap"]["objectProbes"]["probeHeightWorld"] = 1.4f;
    game::SectorAuthoringDocument loadedLegacyProbeMetadata;
    Check(LoadAuthoringText(legacyProbeMetadata.dump(), loadedLegacyProbeMetadata, error),
          "legacy single-height object probe metadata loads");
    Check(Near(loadedLegacyProbeMetadata.mapData.bakedLightmap.objectProbes.probeLowerHeightWorld, 1.4f)
                  && Near(loadedLegacyProbeMetadata.mapData.bakedLightmap.objectProbes.probeUpperHeightWorld, 1.4f),
          "legacy object probe metadata migrates as a single layer");
}

void TestGraphNativeLegacyImportPathStillWorks()
{
    const SectorTopologyMap source = MakeAdjacentSquares();
    const game::SectorAuthoringGraph graph = game::ImportSectorTopologyMapToAuthoringGraph(source);
    const game::SectorAuthoringDerivationResult derived =
            game::DeriveSectorTopologyMapFromAuthoringGraph(graph);
    Check(derived.success, "topology-v2 import path still derives");
    Check(derived.topology.sectors.size() == source.sectors.size(),
          "topology-v2 import path preserves basic sector count");
}

void TestLevelMarkerRoundTripAndEntryResolution()
{
    SectorTopologyMap topology = MakeSquare();
    topology.levelMarkers.push_back(game::SectorCompiledLevelMarker{
            17, "default", Vector3{3.5f, 4.25f, 5.5f}, 0.75f});
    topology.levelMarkers.push_back(game::SectorCompiledLevelMarker{
            18, "return_from_hub", Vector3{8.0f, 2.0f, 9.0f}, -1.25f});
    std::string error;
    SectorTopologyMap loadedTopology;
    Check(LoadText(SaveText(topology), loadedTopology, error),
          "topology-v2 Level Markers round-trip");
    Check(loadedTopology.levelMarkers.size() == 2
                  && loadedTopology.levelMarkers[0].id == "default"
                  && Near(loadedTopology.levelMarkers[0].position.y, 4.25f)
                  && Near(loadedTopology.levelMarkers[1].yawRadians, -1.25f),
          "topology-v2 Level Marker values survive round-trip");
    Json invalidTopologyMarkers = Json::parse(SaveText(topology));
    invalidTopologyMarkers["levelMarkers"][1]["id"] = "default";
    ExpectRejected(
            invalidTopologyMarkers,
            "topology-v2 duplicate Level Marker reference IDs are rejected");

    const game::SectorCompiledLevelMarker* resolved = nullptr;
    Check(game::ResolveSectorLevelEntryMarker(loadedTopology, std::nullopt, resolved, error)
                  && resolved != nullptr && resolved->id == "default",
          "implicit level entry resolves the default marker");
    Check(game::ResolveSectorLevelEntryMarker(
                  loadedTopology,
                  std::optional<std::string>{"return_from_hub"},
                  resolved,
                  error)
                  && resolved != nullptr && resolved->id == "return_from_hub",
          "explicit level entry resolves its requested marker");
    Check(!game::ResolveSectorLevelEntryMarker(
                  loadedTopology,
                  std::optional<std::string>{"missing"},
                  resolved,
                  error)
                  && resolved == nullptr,
          "missing explicit level entry fails closed");
    SectorTopologyMap noMarkers = MakeSquare();
    Check(game::ResolveSectorLevelEntryMarker(noMarkers, std::nullopt, resolved, error)
                  && resolved == nullptr,
          "missing implicit default preserves geometry-center fallback");

    game::SectorAuthoringDocument document = MakeAuthoringDocumentFromMap(MakeSquare());
    document.graph.levelMarkers.push_back(game::SectorAuthoringLevelMarker{
            31, "default", 24, 40, 7.5f, 135.0f});
    document.derivation = game::DeriveSectorTopologyMapFromAuthoringGraph(document.graph);
    const Json saved = Json::parse(SaveAuthoringText(document));
    Check(saved["authoringGraph"].contains("levelMarkers")
                  && !saved.contains("levelMarkers"),
          "graph-native Level Markers serialize only in the authoring graph");
    game::SectorAuthoringDocument loadedDocument;
    Check(LoadAuthoringText(saved.dump(), loadedDocument, error),
          "graph-native Level Marker document loads");
    Check(loadedDocument.graph.levelMarkers.size() == 1
                  && loadedDocument.graph.levelMarkers[0].referenceId == "default"
                  && loadedDocument.graph.levelMarkers[0].x == 24
                  && Near(loadedDocument.graph.levelMarkers[0].orientationDegrees, 135.0f)
                  && loadedDocument.derivation.topology.levelMarkers.size() == 1
                  && Near(loadedDocument.derivation.topology.levelMarkers[0].position.x, 1.5f),
          "graph-native Level Marker authoring and compiled values survive load");

    Json duplicate = saved;
    duplicate["authoringGraph"]["levelMarkers"].push_back(
            duplicate["authoringGraph"]["levelMarkers"][0]);
    duplicate["authoringGraph"]["levelMarkers"][1]["editorId"] = 32;
    Check(!LoadAuthoringText(duplicate.dump(), loadedDocument, error),
          "duplicate Level Marker reference IDs are rejected");
}

void TestTriggerRoundTripAndValidation()
{
    const std::vector<game::SectorTriggerPoint> rectangle{
            {0, 0}, {32, 0}, {32, 48}, {0, 48}};
    SectorTopologyMap topology = MakeSquare();
    topology.triggers.push_back(game::SectorCompiledTrigger{
            21, "exit_hall", game::SectorTriggerShapeKind::Rectangle,
            rectangle, false, true, 250, "onExitHall"});

    std::string error;
    const Json topologyJson = Json::parse(SaveText(topology));
    Check(topologyJson["triggers"].size() == 1
                  && topologyJson["triggers"][0]["id"] == "exit_hall"
                  && topologyJson["triggers"][0]["delayMilliseconds"] == 250,
          "topology-v2 trigger fields serialize");
    SectorTopologyMap loadedTopology;
    Check(LoadText(topologyJson.dump(), loadedTopology, error)
                  && loadedTopology.triggers.size() == 1
                  && !loadedTopology.triggers[0].enabled
                  && loadedTopology.triggers[0].repeat
                  && loadedTopology.triggers[0].points[2].z == 48,
          "topology-v2 triggers round-trip");

    Json invalidScript = topologyJson;
    invalidScript["triggers"][0]["script"] = "not a function()";
    ExpectRejected(invalidScript, "invalid trigger Lua function names are rejected");
    Json invalidGeometry = topologyJson;
    invalidGeometry["triggers"][0]["points"][2]["x"] = 0;
    invalidGeometry["triggers"][0]["points"][2]["z"] = 0;
    ExpectRejected(invalidGeometry, "degenerate trigger geometry is rejected");

    game::SectorAuthoringDocument document = MakeAuthoringDocumentFromMap(MakeSquare());
    document.graph.triggers.push_back(game::SectorAuthoringTrigger{
            31, "entry_zone", game::SectorTriggerShapeKind::Polygon,
            {{16, 16}, {64, 16}, {48, 64}}, true, false, 0, "onEntryZone"});
    document.derivation = game::DeriveSectorTopologyMapFromAuthoringGraph(document.graph);
    Check(document.derivation.success, "authoring graph with trigger derives");
    const Json authoringJson = Json::parse(SaveAuthoringText(document));
    Check(authoringJson["authoringGraph"]["triggers"].size() == 1
                  && !authoringJson.contains("triggers")
                  && !authoringJson["authoringGraph"]["triggers"][0].contains("enabled")
                  && !authoringJson["authoringGraph"]["triggers"][0].contains("repeat")
                  && !authoringJson["authoringGraph"]["triggers"][0].contains("delayMilliseconds"),
          "authoring triggers are source-owned and omit default fields");
    game::SectorAuthoringDocument loadedDocument;
    Check(LoadAuthoringText(authoringJson.dump(), loadedDocument, error)
                  && loadedDocument.graph.triggers.size() == 1
                  && loadedDocument.graph.triggers[0].id == "entry_zone"
                  && loadedDocument.derivation.topology.triggers.size() == 1
                  && loadedDocument.derivation.topology.triggers[0].script == "onEntryZone",
          "authoring trigger and compiled runtime trigger round-trip");

    Json duplicate = authoringJson;
    duplicate["authoringGraph"]["triggers"].push_back(
            duplicate["authoringGraph"]["triggers"][0]);
    duplicate["authoringGraph"]["triggers"][1]["editorId"] = 32;
    Check(!LoadAuthoringText(duplicate.dump(), loadedDocument, error),
          "duplicate trigger string IDs are rejected");
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

void TestFootstepSetRoundTripAndDefaults()
{
    SectorTopologyMap map = MakeSquare();
    map.sectors[0].footstepSet = "DirtRoad_Mono";
    const Json saved = Json::parse(SaveText(map));
    Check(saved["sectors"][0]["footstepSet"] == "DirtRoad_Mono",
          "topology sector footstep override serializes");
    SectorTopologyMap loaded;
    std::string error;
    Check(LoadText(saved.dump(), loaded, error)
                  && loaded.sectors[0].footstepSet == "DirtRoad_Mono",
          "topology sector footstep override round-trips");

    SectorTopologyMap defaults = MakeSquare();
    const Json defaultSaved = Json::parse(SaveText(defaults));
    Check(!defaultSaved["sectors"][0].contains("footstepSet"),
          "default live footstep fallback is omitted from topology JSON");
    Json invalid = defaultSaved;
    invalid["sectors"][0]["footstepSet"] = "../Tile_Mono";
    Check(!LoadText(invalid.dump(), loaded, error),
          "unsafe topology footstep set IDs are rejected");

    game::SectorAuthoringDocument document = MakeAuthoringDocumentFromMap(defaults);
    document.graph.faceAnchors[0].footstepSet = "MetalSteps";
    document.derivation = game::DeriveSectorTopologyMapFromAuthoringGraph(document.graph);
    const Json authoringSaved = Json::parse(SaveAuthoringText(document));
    Check(authoringSaved["authoringGraph"]["faceAnchors"][0]["footstepSet"] == "MetalSteps",
          "authoring face footstep override serializes in the source graph");
    game::SectorAuthoringDocument loadedDocument;
    Check(LoadAuthoringText(authoringSaved.dump(), loadedDocument, error)
                  && loadedDocument.graph.faceAnchors[0].footstepSet == "MetalSteps"
                  && loadedDocument.derivation.topology.sectors[0].footstepSet == "MetalSteps",
          "authoring face footstep override derives into runtime topology");
}

void TestAuthoringEditorSettingsRoundTripAndValidation()
{
    game::SectorAuthoringDocument document = MakeAuthoringDocumentFromMap(MakeSquare());
    const Json defaultSaved = Json::parse(SaveAuthoringText(document));
    Check(!defaultSaved.contains("editorSettings"),
          "default authoring editor settings are omitted");

    game::SectorAuthoringDocument loaded;
    std::string error;
    Check(LoadAuthoringText(defaultSaved.dump(), loaded, error)
                  && loaded.editorSettings.gridSize
                             == game::SectorAuthoringEditorGridSizeDefault,
          "missing authoring editor settings use the default grid size");

    document.editorSettings.gridSize = 24;
    const Json saved = Json::parse(SaveAuthoringText(document));
    Check(saved["editorSettings"]["gridSize"] == 24,
          "non-default authoring grid size serializes");
    Check(LoadAuthoringText(saved.dump(), loaded, error)
                  && loaded.editorSettings.gridSize == 24,
          "authoring grid size round-trips");

    Json belowRange = saved;
    belowRange["editorSettings"]["gridSize"] = 0;
    Check(LoadAuthoringText(belowRange.dump(), loaded, error)
                  && loaded.editorSettings.gridSize
                             == game::SectorAuthoringEditorGridSizeMin,
          "authoring grid size clamps to the supported minimum");

    Json aboveRange = saved;
    aboveRange["editorSettings"]["gridSize"] = 1000;
    Check(LoadAuthoringText(aboveRange.dump(), loaded, error)
                  && loaded.editorSettings.gridSize
                             == game::SectorAuthoringEditorGridSizeMax,
          "authoring grid size clamps to the supported maximum");

    Json invalidGridSize = saved;
    invalidGridSize["editorSettings"]["gridSize"] = "large";
    Check(!LoadAuthoringText(invalidGridSize.dump(), loaded, error)
                  && !error.empty(),
          "non-integer authoring grid size is rejected");

    Json invalidSettings = saved;
    invalidSettings["editorSettings"] = Json::array();
    Check(!LoadAuthoringText(invalidSettings.dump(), loaded, error)
                  && !error.empty(),
          "non-object authoring editor settings are rejected");
}

} // namespace

int main()
{
    TestRoundTrip();
    TestTextureFilterSerialization();
    TestCeilingSkySerialization();
    TestStaticLightRoundTrip();
    TestStaticSpotLightRoundTrip();
    TestDynamicPointLightRoundTrip();
    TestDynamicSpotLightRoundTrip();
    TestLightAtmosphereRoundTripAndDefaultOmission();
    TestRuntimeObjectsRoundTripAndValidation();
    TestDynamicModelRoundTripAndDefaultOmission();
    TestNpcRoundTripDefaultsAndValidation();
    TestRuntimeObjectEditAndDeleteHelpers();
    TestLightmapMetadataRoundTrip();
    TestPreviewSettingsRoundTripAndValidation();
    TestAudioSettingsRoundTripAndValidation();
    TestSkySettingsRoundTripAndValidation();
    TestDirectionalLightRoundTripAndValidation();
    TestFogSettingsRoundTripAndValidation();
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
    TestStaticSpotLightHelpers();
    TestDynamicPointLightHelpers();
    TestDynamicSpotLightHelpers();
    TestIndependentFrontBackSidedefEdits();
    TestOppositeSidedefLookup();
    TestGraphNativeEmptyAndLooseGraphRoundTrip();
    TestGraphNativeInvalidGraphAndTransactionalFailure();
    TestGraphNativeValidGraphDerivesTopologyAndProperties();
    TestGraphNativeMapLevelRoundTrip();
    TestGraphNativeLegacyImportPathStillWorks();
    TestLevelMarkerRoundTripAndEntryResolution();
    TestTriggerRoundTripAndValidation();
    TestFootstepSetRoundTripAndDefaults();
    TestAuthoringEditorSettingsRoundTripAndValidation();
    TestFileApi();

    if (failures != 0) {
        std::cerr << failures << " topology serialization test(s) failed\n";
        return 1;
    }
    std::cout << "Sector topology serialization tests passed\n";
    return 0;
}
