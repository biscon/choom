#include "sector_demo/SectorTopologySerialization.h"
#include "util/json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <cmath>
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
    Check(!game::HasSectorTopologyValidationErrors(game::ValidateSectorTopologyMap(loaded)),
          "round-tripped map validates");
    SectorTopologyLoopSet loops;
    Check(game::ExtractSectorTopologyLoops(loaded, 1, loops)
                  && loops.outer.signedAreaTwice > 0,
          "round-tripped map extracts a CCW loop");
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
    TestStaticLightRoundTrip();
    TestLightmapMetadataRoundTrip();
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
