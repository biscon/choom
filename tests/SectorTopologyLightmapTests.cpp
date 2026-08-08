#include "sector_editor/services/lightmap_bake/SectorEditorLightmapBakeController.h"
#include "engine/assets/AssetManager.h"
#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorTextureTypes.h"
#include "sector_demo/SectorTopologyGeometry.h"
#include "sector_demo/renderer/SectorLocalFogLighting.h"

#include <raymath.h>

#include <algorithm>
#include <array>
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

std::filesystem::path Ref077Phase05aSandboxDir()
{
    return std::filesystem::temp_directory_path() / "ref077_lightmap_bake_controller_phase_05a";
}

void WriteTextFile(const std::filesystem::path& path, const char* text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << text;
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

void TestLightmapBakeReportFormatting()
{
    game::SectorLightmapBakeResult result;
    result.width = 16;
    result.height = 8;
    result.validChartTexels = 32;
    result.allocatedChartRectanglePixels = 64;
    result.staticGeometryTriangles = 12;
    result.bvhNodes = 7;
    result.bvhLeaves = 4;
    result.bvhLeafTriangleLimit = 8;
    result.bvhAverageTrianglesPerLeaf = 3.25;
    result.bvhMaxTrianglesInLeaf = 6;
    result.staticLightCount = 5;
    result.staticSpotLightCount = 2;
    result.objectProbes.count = 9;
    result.objectProbes.path = "assets/levels/test.lightmap.object_probes.bin";
    result.objectProbePlacementDiagnostics = 1;
    result.directHardShadowStats = game::SectorLightmapRaycastStats{10, 20, 5, 30, 4, 1};
    result.softShadowSourceStats = game::SectorLightmapRaycastStats{20, 40, 8, 50, 6, 2};
    result.ambientOcclusionStats = game::SectorLightmapRaycastStats{0, 0, 0, 0, 0, 0};
    result.indirectBounceStats = game::SectorLightmapRaycastStats{5, 12, 3, 15, 2, 3};
    result.layoutSeconds = 0.11;
    result.bvhBuildSeconds = 0.22;
    result.directLightingSeconds = 1.23;
    result.ambientOcclusionSeconds = 2.34;
    result.indirectBounceSeconds = 3.45;
    result.objectProbeBakeSeconds = 0.44;
    result.objectProbeSidecarWriteSeconds = 0.55;
    result.gutterExportSeconds = 0.66;
    result.totalBakeSeconds = 9.99;

    const std::string expected =
            "Lightmap bake report\n"
            "  Atlases: 1\n"
            "  Atlas size: 16 x 8\n"
            "  Atlas pixels: 128\n"
            "  Valid chart texels: 32\n"
            "  Valid atlas occupancy: 25.00%\n"
            "  Allocated chart rectangle pixels: 64\n"
            "  Chart rectangle occupancy: 50.00%\n"
            "  Chart payload efficiency: 50.00%\n"
            "  Static geometry triangles: 12\n"
            "  BVH nodes: 7\n"
            "  BVH leaves: 4\n"
            "  BVH leaf triangle limit: 8\n"
            "  Average triangles per leaf: 3.25\n"
            "  Max triangles in leaf: 6\n"
            "  Static lights: 5 (3 point, 2 spot)\n"
            "\n"
            "  Object light probes: 9\n"
            "  Object probe placement diagnostics: 1\n"
            "  Object probe sidecar: assets/levels/test.lightmap.object_probes.bin\n"
            "\n"
            "  Direct hard-shadow rays: 10\n"
            "    AABB tests: 20\n"
            "    AABB hits: 5\n"
            "    Triangle tests: 30\n"
            "    Triangle hits: 4\n"
            "    Logical source-surface self hits ignored: 1\n"
            "    Average triangle tests/ray: 3.00\n"
            "\n"
            "  Soft-shadow source rays: 20\n"
            "    AABB tests: 40\n"
            "    AABB hits: 8\n"
            "    Triangle tests: 50\n"
            "    Triangle hits: 6\n"
            "    Logical source-surface self hits ignored: 2\n"
            "    Average triangle tests/ray: 2.50\n"
            "\n"
            "  AO rays: 0\n"
            "    AABB tests: 0\n"
            "    AABB hits: 0\n"
            "    Triangle tests: 0\n"
            "    Triangle hits: 0\n"
            "    Logical source-surface self hits ignored: 0\n"
            "    Average triangle tests/ray: 0.00\n"
            "\n"
            "  Indirect bounce rays: 5\n"
            "    AABB tests: 12\n"
            "    AABB hits: 3\n"
            "    Triangle tests: 15\n"
            "    Triangle hits: 2\n"
            "    Logical source-surface self hits ignored: 3\n"
            "    Average triangle tests/ray: 3.00\n"
            "\n"
            "  Total rays: 35\n"
            "  Total triangle tests: 95\n"
            "  Total logical source-surface self hits ignored: 6\n"
            "  Average triangle tests/ray: 2.71\n"
            "\n"
            "  Layout: 0.11s\n"
            "  BVH build: 0.22s\n"
            "  Direct lighting: 1.23s\n"
            "  AO: 2.34s\n"
            "  Indirect bounce: 3.45s\n"
            "  Object probe bake: 0.44s\n"
            "  Object probe sidecar write: 0.55s\n"
            "  Gutter dilation/export: 0.66s\n"
            "  Total bake: 9.99s";

    Check(game::FormatSectorLightmapBakeReport(result) == expected,
          "lightmap bake report formatting remains exact");

    result.atlases = {
            game::SectorLightmapAtlasMetadata{"atlas0.png", 16, 8},
            game::SectorLightmapAtlasMetadata{"atlas1.png", 16, 8}};
    const std::string multiAtlasReport =
            game::FormatSectorLightmapBakeReport(result);
    Check(multiAtlasReport.find("  Atlases: 2\n") != std::string::npos
                  && multiAtlasReport.find("  Atlas pixels: 256\n")
                          != std::string::npos
                  && multiAtlasReport.find(
                             "  Valid atlas occupancy: 12.50%\n")
                          != std::string::npos,
          "multi-atlas bake report aggregates capacity across every atlas");
}

void TestSectorAssetPathHelpers()
{
    Check(game::IsSectorAssetsPath("assets/textures/wall.png"), "asset path helper detects assets prefix");
    Check(!game::IsSectorAssetsPath("textures/wall.png"), "asset path helper ignores non-assets relative path");
    Check(!game::IsSectorAssetsPath("/tmp/assets/textures/wall.png"), "asset path helper ignores absolute path");

    Check(game::ResolveSectorAssetPath("").empty(), "empty asset path resolves to empty path");
    Check(game::ResolveSectorAssetPath("textures/wall.png") == "textures/wall.png",
          "non-assets relative path resolves unchanged");
    Check(game::ResolveSectorAssetPath("/tmp/wall.png") == "/tmp/wall.png",
          "absolute asset path resolves unchanged");
    Check(game::ResolveSectorAssetPath("assets/wall.png") == std::string(ASSETS_PATH) + "wall.png",
          "assets path resolves under ASSETS_PATH");
    Check(game::ResolveSectorAssetPath("assets/textures/wall.png") == std::string(ASSETS_PATH) + "textures/wall.png",
          "nested assets path resolves without double prefix");

    const std::filesystem::path assetRoot = std::filesystem::path(ASSETS_PATH);
    const std::filesystem::path nestedAsset = assetRoot / "levels" / "test" / "test.lightmap.png";
    Check(game::MakeSectorAssetRelativePath(nestedAsset.generic_string()) == "assets/levels/test/test.lightmap.png",
          "asset-root filesystem path converts to project-relative asset path");
    Check(game::MakeSectorAssetRelativePath("/tmp/outside.lightmap.png") == "/tmp/outside.lightmap.png",
          "filesystem path outside asset root stays unchanged");
    Check(game::MakeSectorLightmapAtlasPath(
                  "assets/levels/test/test.lightmap.png", 0)
                  == "assets/levels/test/test.lightmap.png"
                  && game::MakeSectorLightmapAtlasPath(
                             "assets/levels/test/test.lightmap.png", 2)
                          == "assets/levels/test/test.lightmap.2.png"
                  && game::MakeSectorLightmapAtlasPath(
                             "/tmp/test.lightmap.tmp.png", 1)
                          == "/tmp/test.lightmap.tmp.1.png",
          "lightmap atlas path helper preserves the primary and indexes additional outputs");
}

game::SectorLightmapBakeAsyncResult MakeInstallTestResult(const std::filesystem::path& sandbox)
{
    game::SectorLightmapBakeAsyncResult result;
    result.succeeded = true;
    result.expectedSourceHash = "hash-a";
    result.temporaryOutputPath = (sandbox / "temp.lightmap.tmp.png").generic_string();
    result.finalOutputPath = (sandbox / "installed" / "map.lightmap.png").generic_string();
    result.bakeResult.width = 64;
    result.bakeResult.height = 32;
    result.bakeResult.sourceHash = "hash-a";
    result.bakeResult.objectProbes.path =
            game::MakeSectorObjectProbeSidecarPathForLightmapPath(result.temporaryOutputPath);
    result.bakeResult.objectProbes.version =
            game::kSectorBakedObjectLightProbeSidecarVersion;
    result.bakeResult.objectProbes.sourceHash = "stale-probe-hash";
    result.bakeResult.objectProbes.count = 7;
    result.bakeResult.objectProbes.probeSpacingWorld = 4.0f;
    result.bakeResult.objectProbes.probeLowerHeightWorld = 0.6f;
    result.bakeResult.objectProbes.probeUpperHeightWorld = 1.5f;
    result.bakeResult.objectProbes.format =
            game::kSectorBakedObjectLightProbeSidecarFormat;
    result.bakeResult.totalBakeSeconds = 1.25;
    return result;
}

void WriteInstallTestTemps(const game::SectorLightmapBakeAsyncResult& result)
{
    WriteTextFile(result.temporaryOutputPath, "lightmap");
    std::vector<game::SectorBakedObjectLightProbe> probes(
            static_cast<size_t>(result.bakeResult.objectProbes.count));
    std::string error;
    Check(game::WriteSectorBakedObjectLightProbeSidecar(
                  result.bakeResult.objectProbes.path,
                  probes,
                  result.bakeResult.objectProbes.probeSpacingWorld,
                  result.bakeResult.objectProbes.probeLowerHeightWorld,
                  result.bakeResult.objectProbes.probeUpperHeightWorld,
                  error),
          "install fixture writes a valid object probe sidecar");
}

void AddInstallTestStaticModelSidecar(
        game::SectorLightmapBakeAsyncResult& result)
{
    game::SectorStaticModelLightmapData data;
    data.sourceHash = result.bakeResult.sourceHash;
    game::SectorStaticModelLightmapModel model;
    model.modelPath = "assets/models/install_fixture.gltf";
    model.geometryFingerprint = "install-geometry";
    game::SectorStaticModelLightmapMesh mesh;
    mesh.originalVertexCount = 3;
    mesh.sourceVertexIndices = {0, 1, 2};
    mesh.localLightmapUvs = {
            Vector2{0.0f, 0.0f},
            Vector2{1.0f, 0.0f},
            Vector2{0.0f, 1.0f}};
    mesh.indices = {0, 1, 2};
    model.meshes.push_back(mesh);
    data.models.push_back(model);
    game::SectorStaticModelLightmapObject object;
    object.objectId = 12;
    object.modelIndex = 0;
    object.containingSectorId = 10;
    object.meshPlacements.resize(1);
    object.meshPlacements[0].atlasIndex = 0;
    object.meshPlacements[0].atlasScale = Vector2{0.1f, 0.1f};
    object.meshPlacements[0].atlasBias = Vector2{0.2f, 0.3f};
    data.objects.push_back(object);

    result.bakeResult.staticModels.path =
            game::MakeSectorStaticModelSidecarPathForLightmapPath(
                    result.temporaryOutputPath);
    result.bakeResult.staticModels.version =
            game::kSectorStaticModelLightmapSidecarVersion;
    result.bakeResult.staticModels.sourceHash =
            result.bakeResult.sourceHash;
    result.bakeResult.staticModels.modelCount = 1;
    result.bakeResult.staticModels.objectCount = 1;
    result.bakeResult.staticModels.format =
            game::kSectorStaticModelLightmapSidecarFormat;
    std::string error;
    Check(game::WriteSectorStaticModelLightmapSidecar(
                  result.bakeResult.staticModels.path,
                  data,
                  error),
          "install fixture writes a valid static model sidecar");
}

void TestLightmapBakeInstallBoundaryRejectsStaleAndCleansTemps()
{
    const std::filesystem::path sandbox = Ref077Phase05aSandboxDir() / "stale";
    std::filesystem::remove_all(sandbox);
    game::SectorLightmapBakeAsyncResult result = MakeInstallTestResult(sandbox);
    WriteInstallTestTemps(result);

    game::SectorEditorLightmapBakeController controller;
    game::SectorEditorLightmapBakeInstallPayload payload;
    const bool installed = controller.InstallCompletedResultFiles(result, "hash-b", payload);

    Check(!installed, "stale lightmap install result is rejected");
    Check(payload.status == "Bake discarded: document changed during bake",
          "stale lightmap install reports unchanged status text");
    Check(!std::filesystem::exists(result.temporaryOutputPath), "stale install deletes temp lightmap");
    Check(!std::filesystem::exists(result.bakeResult.objectProbes.path), "stale install deletes temp probe sidecar");
    Check(!std::filesystem::exists(result.finalOutputPath), "stale install does not copy final lightmap");
    std::filesystem::remove_all(sandbox);
}

void TestLightmapBakeInstallBoundaryMissingTempsCleanUp()
{
    const std::filesystem::path sandbox = Ref077Phase05aSandboxDir() / "missing";
    std::filesystem::remove_all(sandbox);

    {
        game::SectorLightmapBakeAsyncResult result = MakeInstallTestResult(sandbox / "lightmap");
        std::vector<game::SectorBakedObjectLightProbe> probes(
                static_cast<size_t>(result.bakeResult.objectProbes.count));
        std::string error;
        Check(game::WriteSectorBakedObjectLightProbeSidecar(
                      result.bakeResult.objectProbes.path,
                      probes,
                      result.bakeResult.objectProbes.probeSpacingWorld,
                      result.bakeResult.objectProbes.probeLowerHeightWorld,
                      result.bakeResult.objectProbes.probeUpperHeightWorld,
                      error),
              "missing-lightmap fixture writes a valid probe sidecar");

        game::SectorEditorLightmapBakeController controller;
        game::SectorEditorLightmapBakeInstallPayload payload;
        const bool installed = controller.InstallCompletedResultFiles(result, "hash-a", payload);

        Check(!installed, "missing temp lightmap install result is rejected");
        Check(payload.status == "Bake failed: temporary lightmap output missing",
              "missing temp lightmap reports unchanged status text");
        Check(!std::filesystem::exists(result.bakeResult.objectProbes.path),
              "missing temp lightmap deletes temp probe sidecar");
    }

    {
        game::SectorLightmapBakeAsyncResult result = MakeInstallTestResult(sandbox / "sidecar");
        WriteTextFile(result.temporaryOutputPath, "lightmap");

        game::SectorEditorLightmapBakeController controller;
        game::SectorEditorLightmapBakeInstallPayload payload;
        const bool installed = controller.InstallCompletedResultFiles(result, "hash-a", payload);

        Check(!installed, "missing temp object probe sidecar install result is rejected");
        Check(payload.status == "Bake failed: temporary object probe output missing",
              "missing temp object probe sidecar reports unchanged status text");
        Check(!std::filesystem::exists(result.temporaryOutputPath),
              "missing temp object probe sidecar deletes temp lightmap");
    }

    std::filesystem::remove_all(sandbox);
}

void TestLightmapBakeInstallBoundaryCopyFailureCleanup()
{
    const std::filesystem::path sandbox = Ref077Phase05aSandboxDir() / "copy_failure";
    std::filesystem::remove_all(sandbox);

    {
        game::SectorLightmapBakeAsyncResult result = MakeInstallTestResult(sandbox / "sidecar");
        WriteInstallTestTemps(result);
        const std::string finalObjectProbePath =
                game::MakeSectorObjectProbeSidecarPathForLightmapPath(result.finalOutputPath);
        std::filesystem::create_directories(finalObjectProbePath);

        game::SectorEditorLightmapBakeController controller;
        game::SectorEditorLightmapBakeInstallPayload payload;
        const bool installed = controller.InstallCompletedResultFiles(result, "hash-a", payload);

        Check(!installed, "object probe sidecar copy failure rejects install");
        Check(payload.status.find("Bake failed: could not install object probe sidecar:") == 0,
              "object probe sidecar copy failure reports unchanged status prefix");
        Check(!std::filesystem::exists(result.temporaryOutputPath),
              "object probe sidecar copy failure deletes temp lightmap");
        Check(!std::filesystem::exists(result.bakeResult.objectProbes.path),
              "object probe sidecar copy failure deletes temp probe sidecar");
    }

    {
        game::SectorLightmapBakeAsyncResult result = MakeInstallTestResult(sandbox / "lightmap");
        WriteInstallTestTemps(result);
        const std::string finalObjectProbePath =
                game::MakeSectorObjectProbeSidecarPathForLightmapPath(result.finalOutputPath);
        std::filesystem::create_directories(result.finalOutputPath);

        game::SectorEditorLightmapBakeController controller;
        game::SectorEditorLightmapBakeInstallPayload payload;
        const bool installed = controller.InstallCompletedResultFiles(result, "hash-a", payload);

        Check(!installed, "lightmap copy failure rejects install");
        Check(payload.status.find("Bake failed: could not install lightmap:") == 0,
              "lightmap copy failure reports unchanged status prefix");
        Check(!std::filesystem::exists(result.temporaryOutputPath), "lightmap copy failure deletes temp lightmap");
        Check(!std::filesystem::exists(result.bakeResult.objectProbes.path),
              "lightmap copy failure deletes temp probe sidecar");
        Check(!std::filesystem::exists(finalObjectProbePath),
              "lightmap copy failure deletes copied final object probe sidecar");
    }

    std::filesystem::remove_all(sandbox);
}

void TestLightmapBakeInstallBoundarySuccessfulPayload()
{
    const std::filesystem::path sandbox = Ref077Phase05aSandboxDir() / "success";
    std::filesystem::remove_all(sandbox);
    game::SectorLightmapBakeAsyncResult result = MakeInstallTestResult(sandbox);
    WriteInstallTestTemps(result);

    game::SectorEditorLightmapBakeController controller;
    game::SectorEditorLightmapBakeInstallPayload payload;
    const bool installed = controller.InstallCompletedResultFiles(result, "hash-a", payload);

    const std::string finalObjectProbePath =
            game::MakeSectorObjectProbeSidecarPathForLightmapPath(result.finalOutputPath);
    Check(installed, "successful lightmap install payload is produced");
    Check(std::filesystem::exists(result.finalOutputPath), "successful install copies final lightmap");
    Check(std::filesystem::exists(finalObjectProbePath), "successful install copies final object probe sidecar");
    Check(!std::filesystem::exists(result.temporaryOutputPath), "successful install deletes temp lightmap");
    Check(!std::filesystem::exists(result.bakeResult.objectProbes.path),
          "successful install deletes temp object probe sidecar");
    Check(payload.finalLightmapPath == result.finalOutputPath, "install payload keeps final lightmap path");
    Check(payload.finalObjectProbePath == finalObjectProbePath, "install payload keeps final object probe path");
    Check(payload.bakeResult.width == result.bakeResult.width, "install payload preserves lightmap width");
    Check(payload.bakeResult.height == result.bakeResult.height, "install payload preserves lightmap height");
    Check(payload.bakeResult.sourceHash == result.bakeResult.sourceHash, "install payload preserves source hash");
    Check(payload.bakeResult.objectProbes.count == result.bakeResult.objectProbes.count,
          "install payload preserves object probe metadata");
    Check(payload.bakeResult.objectProbes.path == payload.finalObjectProbeAssetPath,
          "install payload rewrites object probe metadata to final asset path");
    Check(payload.bakeResult.objectProbes.sourceHash == result.bakeResult.sourceHash,
          "install payload rewrites object probe source hash to lightmap source hash");
    std::filesystem::remove_all(sandbox);
}

void TestLightmapBakeInstallBoundaryHandlesMultipleAtlases()
{
    const std::filesystem::path sandbox =
            Ref077Phase05aSandboxDir() / "multi_atlas";
    std::filesystem::remove_all(sandbox);
    game::SectorLightmapBakeAsyncResult result = MakeInstallTestResult(sandbox);
    result.bakeResult.atlases = {
            game::SectorLightmapAtlasMetadata{
                    result.temporaryOutputPath,
                    result.bakeResult.width,
                    result.bakeResult.height},
            game::SectorLightmapAtlasMetadata{
                    game::MakeSectorLightmapAtlasPath(
                            result.temporaryOutputPath, 1),
                    result.bakeResult.width,
                    result.bakeResult.height}};
    WriteInstallTestTemps(result);
    WriteTextFile(result.bakeResult.atlases[1].path, "lightmap-1");

    game::SectorEditorLightmapBakeController controller;
    game::SectorEditorLightmapBakeInstallPayload payload;
    Check(controller.InstallCompletedResultFiles(
                  result,
                  result.expectedSourceHash,
                  payload),
          "multi-atlas bake outputs install together");
    const std::string secondFinal = game::MakeSectorLightmapAtlasPath(
            result.finalOutputPath, 1);
    Check(payload.finalLightmapPaths.size() == 2
                  && std::filesystem::exists(result.finalOutputPath)
                  && std::filesystem::exists(secondFinal)
                  && !std::filesystem::exists(result.bakeResult.atlases[0].path)
                  && !std::filesystem::exists(result.bakeResult.atlases[1].path),
          "multi-atlas install publishes every final and cleans every temporary image");
    Check(payload.bakeResult.atlases.size() == 2
                  && payload.bakeResult.atlases[1].path
                          == game::MakeSectorAssetRelativePath(secondFinal),
          "multi-atlas install rewrites every atlas to its final asset path");

    std::filesystem::remove_all(sandbox);
    result = MakeInstallTestResult(sandbox / "missing");
    result.bakeResult.atlases = {
            game::SectorLightmapAtlasMetadata{
                    result.temporaryOutputPath,
                    result.bakeResult.width,
                    result.bakeResult.height},
            game::SectorLightmapAtlasMetadata{
                    game::MakeSectorLightmapAtlasPath(
                            result.temporaryOutputPath, 1),
                    result.bakeResult.width,
                    result.bakeResult.height}};
    WriteInstallTestTemps(result);
    payload = {};
    Check(!controller.InstallCompletedResultFiles(
                   result,
                   result.expectedSourceHash,
                   payload)
                  && payload.status
                          == "Bake failed: temporary lightmap output missing"
                  && !std::filesystem::exists(result.temporaryOutputPath),
          "missing additional atlas rejects installation and cleans remaining temporary outputs");
    std::filesystem::remove_all(sandbox);
}

void TestLightmapBakeInstallBoundaryStaticModelSidecarIsAtomic()
{
    const std::filesystem::path sandbox =
            Ref077Phase05aSandboxDir() / "static_models";
    std::filesystem::remove_all(sandbox);
    {
        game::SectorLightmapBakeAsyncResult result =
                MakeInstallTestResult(sandbox / "corrupt");
        WriteInstallTestTemps(result);
        AddInstallTestStaticModelSidecar(result);
        WriteTextFile(result.bakeResult.staticModels.path, "bad");
        game::SectorEditorLightmapBakeController controller;
        game::SectorEditorLightmapBakeInstallPayload payload;
        Check(!controller.InstallCompletedResultFiles(
                       result,
                       result.expectedSourceHash,
                       payload)
                      && payload.status.find(
                                 "Bake failed: invalid static model lightmap sidecar:")
                              == 0,
              "corrupt required static model sidecar rejects all-or-nothing installation");
        Check(!std::filesystem::exists(result.temporaryOutputPath)
                      && !std::filesystem::exists(
                              result.bakeResult.objectProbes.path)
                      && !std::filesystem::exists(
                              result.bakeResult.staticModels.path),
              "rejected static model sidecar cleans all temporary bake outputs");
    }
    {
        game::SectorLightmapBakeAsyncResult result =
                MakeInstallTestResult(sandbox / "missing_metadata_path");
        WriteInstallTestTemps(result);
        AddInstallTestStaticModelSidecar(result);
        const std::string temporaryStaticPath =
                result.bakeResult.staticModels.path;
        result.bakeResult.staticModels.path.clear();
        game::SectorEditorLightmapBakeController controller;
        game::SectorEditorLightmapBakeInstallPayload payload;
        Check(!controller.InstallCompletedResultFiles(
                       result,
                       result.expectedSourceHash,
                       payload)
                      && payload.status
                              == "Bake failed: incomplete static model lightmap metadata",
              "incomplete required static model metadata rejects installation");
        Check(!std::filesystem::exists(result.temporaryOutputPath)
                      && !std::filesystem::exists(
                              result.bakeResult.objectProbes.path)
                      && !std::filesystem::exists(temporaryStaticPath),
              "incomplete static model metadata cleans every temporary artifact");
    }
    {
        game::SectorLightmapBakeAsyncResult result =
                MakeInstallTestResult(sandbox / "success");
        WriteInstallTestTemps(result);
        AddInstallTestStaticModelSidecar(result);
        game::SectorEditorLightmapBakeController controller;
        game::SectorEditorLightmapBakeInstallPayload payload;
        Check(controller.InstallCompletedResultFiles(
                      result,
                      result.expectedSourceHash,
                      payload),
              "valid static model sidecar installs with the atlas and probes");
        Check(std::filesystem::exists(payload.finalLightmapPath)
                      && std::filesystem::exists(
                              payload.finalObjectProbePath)
                      && std::filesystem::exists(
                              payload.finalStaticModelPath)
                      && payload.bakeResult.staticModels.path
                              == payload.finalStaticModelAssetPath,
              "successful install publishes all three bake artifacts and final metadata paths");
    }
    std::filesystem::remove_all(sandbox);
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

void WriteSolidRgbTexture(
        const std::filesystem::path& path,
        Color color,
        int width = 2,
        int height = 2)
{
    std::filesystem::create_directories(path.parent_path());
    Image image = GenImageColor(width, height, color);
    Check(ExportImage(image, path.string().c_str()), "solid RGB test texture exports");
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

game::SectorTopologyMap MakeDisconnectedLargeSquares()
{
    game::SectorTopologyMap map;
    AddTextureDefaults(map);
    constexpr game::SectorCoord size = 30000;
    constexpr game::SectorCoord gap = 32;
    for (int squareIndex = 0; squareIndex < 3; ++squareIndex) {
        const int vertexBase = squareIndex * 4 + 1;
        const int lineBase = squareIndex * 4 + 1;
        const int sectorId = 10 + squareIndex;
        const game::SectorCoord x = squareIndex * (size + gap);
        map.vertices.insert(map.vertices.end(), {
                {vertexBase + 0, x, 0},
                {vertexBase + 1, x + size, 0},
                {vertexBase + 2, x + size, size},
                {vertexBase + 3, x, size}});
        for (int sideIndex = 0; sideIndex < 4; ++sideIndex) {
            const int lineId = lineBase + sideIndex;
            const int startVertexId = vertexBase + sideIndex;
            const int endVertexId = vertexBase + ((sideIndex + 1) % 4);
            map.lineDefs.push_back({
                    lineId,
                    startVertexId,
                    endVertexId,
                    lineId,
                    -1});
            AddSide(
                    map,
                    lineId,
                    lineId,
                    game::SectorTopologySideKind::Front,
                    sectorId);
        }
        map.sectors.push_back(Sector(sectorId));
    }
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

game::SectorTopologyMap MakeAdjacentWithSplitPortal()
{
    game::SectorTopologyMap map = MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f);
    map.vertices.push_back({7, 64, 32});
    game::SectorTopologyLineDef* shared = game::FindSectorTopologyLineDef(map, 2);
    Check(shared != nullptr, "split portal fixture shared linedef exists");
    if (shared != nullptr) {
        shared->endVertexId = 7;
    }
    map.lineDefs.push_back({8, 7, 3, 9, 10});
    AddSide(map, 9, 8, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 10, 8, game::SectorTopologySideKind::Back, 20);
    return map;
}

game::SectorTopologyMap MakeObjectProbeAdjacentCapMap()
{
    game::SectorTopologyMap map;
    AddTextureDefaults(map);
    map.vertices = {
            {1, 0, 0},
            {2, 1, 0},
            {3, 2, 0},
            {4, 3, 0},
            {5, 4, 0},
            {6, 5, 0},
            {7, 6, 0},
            {8, 7, 0},
            {9, 8, 0},
            {10, 9, 0},
            {11, 10, 0},
            {12, 11, 0},
            {13, 12, 0},
            {14, 13, 0},
            {15, 14, 0},
            {16, 15, 0},
            {17, 16, 0},
            {18, 17, 0}
    };

    for (int index = 0; index < 9; ++index) {
        const int lineId = index + 1;
        const int frontSideId = 100 + index;
        const int backSideId = 200 + index;
        const int adjacentSectorId = 20 + index;
        map.lineDefs.push_back({lineId, index * 2 + 1, index * 2 + 2, frontSideId, backSideId});
        AddSide(map, frontSideId, lineId, game::SectorTopologySideKind::Front, 10);
        AddSide(map, backSideId, lineId, game::SectorTopologySideKind::Back, adjacentSectorId);
        map.sectors.push_back(Sector(adjacentSectorId));
    }
    map.sectors.push_back(Sector(10));
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

    game::SectorTopologyMap dynamicPropMap = base;
    game::SectorPlacedRuntimeObject dynamicProp;
    dynamicProp.id = 77;
    dynamicProp.kind = "dynamic_model";
    dynamicProp.position = Vector3{24.0f, 0.0f, 24.0f};
    dynamicProp.dynamicModel.modelPath = "assets/models/characters/synthetic.glb";
    dynamicProp.dynamicModel.animation = "Walk";
    dynamicPropMap.runtimeObjects.push_back(dynamicProp);
    Check(game::ComputeSectorLightmapSourceHash(dynamicPropMap) == hash,
          "hash excludes dynamic props because they are neither baked receivers nor occluders");
    dynamicPropMap.runtimeObjects[0].position.x += 16.0f;
    dynamicPropMap.runtimeObjects[0].dynamicModel.animation = "Idle";
    dynamicPropMap.runtimeObjects[0].dynamicModel.animationSpeed = 2.0f;
    dynamicPropMap.runtimeObjects[0].dynamicModel.shadowMode =
            game::SectorDynamicModelShadowMode::ProjectedSilhouette;
    Check(game::ComputeSectorLightmapSourceHash(dynamicPropMap) == hash,
          "hash excludes dynamic prop transform, playback, and runtime shadow changes");

    game::SectorTopologyMap markerMap = base;
    markerMap.levelMarkers.push_back(game::SectorCompiledLevelMarker{
            1, "default", Vector3{24.0f, 0.0f, 24.0f}, 1.5f});
    Check(game::ComputeSectorLightmapSourceHash(markerMap) == hash,
          "hash excludes Level Markers because they do not affect baked geometry or lighting");

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
    changedLight = base;
    changedLight.staticLights[0].atmosphere.haze.enabled = true;
    changedLight.staticLights[0].atmosphere.haze.density = 0.25f;
    changedLight.staticLights[0].atmosphere.dust.enabled = true;
    changedLight.staticLights[0].atmosphere.dust.amount = 64;
    Check(game::ComputeSectorLightmapSourceHash(changedLight) == hash,
          "hash ignores visual-only static-light atmosphere settings");

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

    game::SectorTopologyMap changedStaticModel = base;
    game::SectorPlacedRuntimeObject staticModel;
    staticModel.id = 30;
    staticModel.kind = "static_model";
    staticModel.position = Vector3{8.0f, base.sectors[0].floorZ, 8.0f};
    staticModel.yawRadians = 0.25f;
    staticModel.staticModel.modelPath = "assets/models/props/crate.glb";
    staticModel.staticModel.heightOffsetWorld = 0.5f;
    changedStaticModel.runtimeObjects.push_back(staticModel);
    const std::string staticModelHash =
            game::ComputeSectorLightmapSourceHash(changedStaticModel);
    Check(staticModelHash != hash,
          "hash includes added assigned static props");
    game::SectorTopologyMap changedStaticModelCollision = changedStaticModel;
    changedStaticModelCollision.runtimeObjects[0].staticModel.collision = true;
    Check(game::ComputeSectorLightmapSourceHash(changedStaticModelCollision)
                  == staticModelHash,
          "hash excludes static prop gameplay collision");
    game::SectorTopologyMap changedStaticModelScale = changedStaticModel;
    changedStaticModelScale.runtimeObjects[0].staticModel.scale = 2.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedStaticModelScale)
                  != staticModelHash,
          "hash includes static prop scale edits");
    game::SectorTopologyMap changedStaticModelRotationX = changedStaticModel;
    changedStaticModelRotationX.runtimeObjects[0].staticModel.rotationXRadians =
            0.5f;
    Check(game::ComputeSectorLightmapSourceHash(changedStaticModelRotationX)
                  != staticModelHash,
          "hash includes static prop X rotation edits");
    game::SectorTopologyMap changedStaticModelRotationZ = changedStaticModel;
    changedStaticModelRotationZ.runtimeObjects[0].staticModel.rotationZRadians =
            -0.75f;
    Check(game::ComputeSectorLightmapSourceHash(changedStaticModelRotationZ)
                  != staticModelHash,
          "hash includes static prop Z rotation edits");
    changedStaticModel.runtimeObjects[0].position = Vector3{24.0f, 12.0f, 20.0f};
    changedStaticModel.runtimeObjects[0].yawRadians = 1.5f;
    changedStaticModel.runtimeObjects[0].staticModel.modelPath =
            "assets/models/props/replacement.gltf";
    changedStaticModel.runtimeObjects[0].staticModel.heightOffsetWorld = 3.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedStaticModel)
                  != staticModelHash,
          "hash includes static prop transform, asset, and height-offset edits");

    game::SectorTopologyMap doorUvMap = MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f);
    game::SectorPlacedRuntimeObject doorObject;
    doorObject.id = 1;
    doorObject.kind = "door";
    doorObject.door.anchor.lineDefId = 2;
    doorObject.door.anchor.frontSectorId = 10;
    doorObject.door.anchor.backSectorId = 20;
    doorObject.door.anchor.frontSideDefId = 2;
    doorObject.door.anchor.backSideDefId = 8;
    doorObject.door.anchor.endpointAX = 64;
    doorObject.door.anchor.endpointAY = 0;
    doorObject.door.anchor.endpointBX = 64;
    doorObject.door.anchor.endpointBY = 64;
    doorUvMap.runtimeObjects.push_back(doorObject);
    const std::string doorUvHash = game::ComputeSectorLightmapSourceHash(doorUvMap);
    doorUvMap.runtimeObjects[0].door.faceUvs.faces[static_cast<int>(game::SectorDoorFace::Front)].scale = {2.0f, 3.0f};
    doorUvMap.runtimeObjects[0].door.faceUvs.faces[static_cast<int>(game::SectorDoorFace::Front)].offset = {0.25f, 0.5f};
    Check(game::ComputeSectorLightmapSourceHash(doorUvMap) == doorUvHash,
          "hash ignores procedural door face UV edits");

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
    changedPreview = base;
    changedPreview.previewSettings.objectProbeDebugDrawMaxDistanceWorld = 96.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedPreview) == hash,
          "hash ignores object probe debug draw distance");

    game::SectorTopologyMap changedAudio = base;
    changedAudio.audioSettings.musicPath = "music/level_theme.ogg";
    changedAudio.audioSettings.soundsById.emplace(
            "door_open", "shared/door_open.wav");
    Check(game::ComputeSectorLightmapSourceHash(changedAudio) == hash,
          "hash ignores runtime-only audio settings");

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

    game::SectorTopologyMap changedFog = base;
    changedFog.fogSettings.enabled = true;
    changedFog.fogSettings.color = Color{12, 34, 56, 255};
    changedFog.fogSettings.startDistanceWorld = 7.0f;
    changedFog.fogSettings.density = 0.15f;
    changedFog.fogSettings.maxOpacity = 0.9f;
    changedFog.fogSettings.referenceHeightWorld = -4.0f;
    changedFog.fogSettings.heightFalloff = 2.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedFog) == hash,
          "hash ignores visual-only fog settings");
    game::SectorTopologyMap changedLocalFog = base;
    game::SectorCompiledLocalFogVolume localFog;
    localFog.sourceAuthoringFogVolumeId = 1;
    localFog.centerWorld = Vector3{1.0f, 0.5f, 2.0f};
    changedLocalFog.compiledLocalFogVolumes.push_back(localFog);
    Check(game::ComputeSectorLightmapSourceHash(changedLocalFog) == hash,
          "hash ignores visual-only compiled local fog volumes");

    game::SectorTopologyMap changedProbeSettings = base;
    changedProbeSettings.lightmapSettings.objectProbeSpacingWorld = 3.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedProbeSettings) != hash,
          "hash changes when object probe spacing changes");
    changedProbeSettings = base;
    changedProbeSettings.lightmapSettings.objectProbeLowerHeightWorld = 0.7f;
    Check(game::ComputeSectorLightmapSourceHash(changedProbeSettings) != hash,
          "hash changes when lower object probe height changes");
    changedProbeSettings = base;
    changedProbeSettings.lightmapSettings.objectProbeUpperHeightWorld = 1.6f;
    Check(game::ComputeSectorLightmapSourceHash(changedProbeSettings) != hash,
          "hash changes when upper object probe height changes");
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

    game::SectorPlacedRuntimeObject firstProp;
    firstProp.id = 20;
    firstProp.kind = "static_model";
    firstProp.position = Vector3{8.0f, 0.0f, 8.0f};
    firstProp.staticModel.modelPath = "assets/models/first.glb";
    firstProp.staticModel.geometryFingerprint = "first-geometry";
    game::SectorPlacedRuntimeObject secondProp = firstProp;
    secondProp.id = 10;
    secondProp.position.x = 24.0f;
    secondProp.staticModel.modelPath = "assets/models/second.glb";
    secondProp.staticModel.geometryFingerprint = "second-geometry";
    reordered.runtimeObjects.push_back(firstProp);
    reordered.runtimeObjects.push_back(secondProp);
    const std::string propHash =
            game::ComputeSectorLightmapSourceHash(reordered);
    std::reverse(
            reordered.runtimeObjects.begin(),
            reordered.runtimeObjects.end());
    Check(game::ComputeSectorLightmapSourceHash(reordered) == propHash,
          "hash is stable when assigned static prop records are reordered");
}

void TestBakeVersionInvalidatesOldLightmaps()
{
    Check(game::kSectorLightmapBakeVersion == 14,
          "lightmap bake version is bumped for layered object probes");

    const std::filesystem::path lightmapPath = Phase01bSandboxDir() / "phase06a_status_lightmap.png";
    WriteSolidAlphaTestTexture(lightmapPath, 255);

    game::SectorTopologyMap map = MakeSquare();
    map.bakedLightmap.path = lightmapPath.string();
    map.bakedLightmap.width = 2;
    map.bakedLightmap.height = 2;
    map.bakedLightmap.sourceHash = game::ComputeSectorLightmapSourceHash(map);
    Check(game::GetSectorLightmapStatus(map) == game::SectorLightmapStatus::Valid,
          "current bake version source hash keeps existing lightmap valid");

    const std::filesystem::path additionalAtlasPath =
            Phase01bSandboxDir() / "phase06a_status_lightmap.1.png";
    WriteSolidAlphaTestTexture(additionalAtlasPath, 255);
    map.bakedLightmap.additionalAtlases.push_back(
            game::SectorLightmapAtlasMetadata{
                    additionalAtlasPath.string(),
                    2,
                    2});
    Check(game::GetSectorLightmapStatus(map) == game::SectorLightmapStatus::Valid,
          "current multi-atlas metadata is valid when every atlas file exists");
    std::filesystem::remove(additionalAtlasPath);
    Check(game::GetSectorLightmapStatus(map) == game::SectorLightmapStatus::Stale,
          "missing additional atlas makes the installed lightmap stale");
    WriteSolidAlphaTestTexture(additionalAtlasPath, 255);

    const std::filesystem::path objectProbePath =
            game::MakeSectorObjectProbeSidecarPathForLightmapPath(lightmapPath.string());
    std::string probeError;
    Check(game::WriteSectorBakedObjectLightProbeSidecar(objectProbePath.string(), {}, 4.0f, 0.6f, 1.5f, probeError),
          "object probe version invalidation sidecar fixture writes");
    map.bakedLightmap.objectProbes.path = objectProbePath.string();
    map.bakedLightmap.objectProbes.version = game::kSectorBakedObjectLightProbeSidecarVersion;
    map.bakedLightmap.objectProbes.sourceHash = map.bakedLightmap.sourceHash;
    map.bakedLightmap.objectProbes.count = 0;
    map.bakedLightmap.objectProbes.probeSpacingWorld = 4.0f;
    map.bakedLightmap.objectProbes.probeLowerHeightWorld = 0.6f;
    map.bakedLightmap.objectProbes.probeUpperHeightWorld = 1.5f;
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
    std::filesystem::remove(additionalAtlasPath, removeError);
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

void TestTopologyLayoutRollsIntoAdditionalAtlases()
{
    game::SectorLightmapLayout layout;
    std::string error;
    Check(game::BuildSectorLightmapLayout(
                  MakeDisconnectedLargeSquares(),
                  layout,
                  error),
          "large disconnected topology builds a multi-atlas layout");
    bool sawPrimary = false;
    bool sawAdditional = false;
    for (const game::SectorLightmapChart& chart : layout.charts) {
        if (chart.surfaceIndex < 0) {
            continue;
        }
        sawPrimary = sawPrimary || chart.atlasIndex == 0;
        sawAdditional = sawAdditional || chart.atlasIndex > 0;
    }
    Check(layout.atlasCount > 1 && sawPrimary && sawAdditional,
          "topology shelf packing continues in another 2048 atlas instead of failing");
}

void TestSmallSyntheticMultiAtlasBake()
{
    game::SectorTopologyMap map = MakeSquare();
    map.staticLights.clear();
    map.lightmapSettings.ambientOcclusionStrength = 0.0f;
    map.lightmapSettings.indirectBounceStrength = 0.0f;
    game::SectorGeneratedGeometry geometry;
    game::SectorLightmapLayout layout;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error)
                  && game::BuildSectorLightmapLayout(map, layout, error),
          "synthetic multi-atlas bake fixture builds source geometry");

    std::vector<int> receivingSurfaceIndices;
    for (const game::SectorLightmapChart& chart : layout.charts) {
        if (chart.surfaceIndex >= 0) {
            receivingSurfaceIndices.push_back(chart.surfaceIndex);
        }
    }
    for (game::SectorLightmapChart& chart : layout.charts) {
        chart = game::SectorLightmapChart{};
    }
    layout.atlasWidth = 8;
    layout.atlasHeight = 8;
    layout.atlasCount = 2;
    for (int atlasIndex = 0; atlasIndex < 2; ++atlasIndex) {
        const int surfaceIndex = receivingSurfaceIndices[
                static_cast<size_t>(atlasIndex)];
        game::SectorLightmapChart& chart =
                layout.charts[static_cast<size_t>(surfaceIndex)];
        chart.surfaceIndex = surfaceIndex;
        chart.atlasIndex = atlasIndex;
        chart.x = 0;
        chart.y = 0;
        chart.width = 8;
        chart.height = 8;
        chart.usableX = 2;
        chart.usableY = 2;
        chart.usableWidth = 4;
        chart.usableHeight = 4;
        chart.vertexUvs.assign(
                geometry.surfaces[static_cast<size_t>(surfaceIndex)]
                        .vertices.size(),
                Vector2{0.5f, 0.5f});
    }

    const std::filesystem::path outputPath =
            Phase01bSandboxDir() / "small_multi.lightmap.png";
    game::SectorLightmapBakeResult result;
    Check(game::BakeSectorLightmap(
                  map,
                  layout,
                  outputPath.string().c_str(),
                  result,
                  error),
          "small synthetic bake exports multiple atlas buffers");
    const std::string secondPath = game::MakeSectorLightmapAtlasPath(
            outputPath.string(), 1);
    Image first = LoadImage(outputPath.string().c_str());
    Image second = LoadImage(secondPath.c_str());
    Check(result.atlases.size() == 2
                  && result.atlases[1].path == secondPath
                  && first.data != nullptr
                  && second.data != nullptr
                  && first.width == 8
                  && second.width == 8,
          "multi-atlas bake result reports and writes every indexed image");
    if (first.data != nullptr) {
        UnloadImage(first);
    }
    if (second.data != nullptr) {
        UnloadImage(second);
    }
    std::filesystem::remove(outputPath);
    std::filesystem::remove(secondPath);
    std::filesystem::remove(
            game::MakeSectorObjectProbeSidecarPathForLightmapPath(
                    outputPath.string()));
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

void TestGeneratedSurfaceNormalMapConventionAndBakedDirectLighting()
{
    const std::filesystem::path root =
            Phase01bSandboxDir() / "generated_surface_normal_maps";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    Check(game::SectorTextureNormalMapPath("assets/images/stone.png")
                  == "assets/images/stone_normal.png",
          "normal-map convention inserts suffix before extension");
    Check(game::SectorTextureNormalMapPath("/tmp/stone.wall.png")
                  == "/tmp/stone.wall_normal.png",
          "normal-map convention preserves dotted stems and directories");
    Check(game::SectorTextureNormalMapPath("stone") == "stone_normal",
          "normal-map convention supports extensionless texture paths");

    const std::filesystem::path flatBasePath = root / "flat_floor.png";
    const std::filesystem::path mappedBasePath = root / "mapped_floor.png";
    const std::filesystem::path mappedNormalPath = root / "mapped_floor_normal.png";
    WriteSolidRgbTexture(flatBasePath, WHITE);
    WriteSolidRgbTexture(mappedBasePath, WHITE);
    WriteSolidRgbTexture(mappedNormalPath, Color{255, 128, 128, 255});

    auto makeDirectionalMap = [](const std::filesystem::path& floorTexturePath) {
        game::SectorTopologyMap map = MakeSquare();
        map.texturesById["floor"].path = floorTexturePath.string();
        map.staticLights.clear();
        map.staticSpotLights.clear();
        map.sectors[0].ceilingSky = true;
        map.directionalLight.enabled = true;
        map.directionalLight.directionToLight = Vector3{0.0f, 1.0f, 0.0f};
        map.directionalLight.color = WHITE;
        map.directionalLight.intensity = 1.0f;
        map.lightmapSettings.ambientOcclusionStrength = 0.0f;
        map.lightmapSettings.indirectBounceStrength = 0.0f;
        return map;
    };

    const LightmapImageMetrics flat = BakeAndMeasure(
            makeDirectionalMap(flatBasePath),
            "generated_surface_flat_normal.lightmap.png");
    const LightmapImageMetrics mapped = BakeAndMeasure(
            makeDirectionalMap(mappedBasePath),
            "generated_surface_mapped_normal.lightmap.png");
    Check(flat.floorCenterRgb > 600,
          "missing companion normal map preserves geometric direct lighting");
    Check(mapped.floorCenterRgb + 300 < flat.floorCenterRgb,
          "companion normal map changes baked direct-light response");

    const std::filesystem::path hashBasePath = root / "hash_surface.png";
    const std::filesystem::path hashNormalPath = root / "hash_surface_normal.png";
    WriteSolidRgbTexture(hashBasePath, WHITE);
    std::filesystem::remove(hashNormalPath);
    game::SectorTopologyMap hashMap = MakeSquare();
    hashMap.texturesById["floor"].path = hashBasePath.string();
    const std::string missingNormalHash =
            game::ComputeSectorLightmapSourceHash(hashMap);
    WriteSolidRgbTexture(hashNormalPath, Color{128, 128, 255, 255});
    const std::string presentNormalHash =
            game::ComputeSectorLightmapSourceHash(hashMap);
    Check(presentNormalHash != missingNormalHash,
          "adding a referenced companion normal map changes the source hash");
    WriteSolidRgbTexture(hashNormalPath, Color{255, 128, 128, 255}, 3, 2);
    const std::string changedNormalHash =
            game::ComputeSectorLightmapSourceHash(hashMap);
    Check(changedNormalHash != presentNormalHash,
          "changing referenced normal-map content changes the source hash");
    std::filesystem::remove(hashNormalPath);
    Check(game::ComputeSectorLightmapSourceHash(hashMap) == missingNormalHash,
          "removing a companion normal map restores the missing-map source hash");

    const std::filesystem::path unusedBasePath = root / "unused.png";
    const std::filesystem::path unusedNormalPath = root / "unused_normal.png";
    WriteSolidRgbTexture(unusedBasePath, WHITE);
    game::SectorTextureDefinition unusedTexture;
    unusedTexture.id = "unused";
    unusedTexture.path = unusedBasePath.string();
    hashMap.texturesById[unusedTexture.id] = unusedTexture;
    const std::string withoutUnusedNormal =
            game::ComputeSectorLightmapSourceHash(hashMap);
    WriteSolidRgbTexture(unusedNormalPath, Color{255, 128, 128, 255});
    Check(game::ComputeSectorLightmapSourceHash(hashMap) == withoutUnusedNormal,
          "unreferenced companion normal maps do not affect the source hash");

    std::filesystem::remove_all(root);
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
    probes[0].layer = game::SectorBakedObjectLightProbeLayer::Lower;
    probes[0].position = Vector3{1.0f, 2.0f, 3.0f};
    probes[1].sectorId = -5;
    probes[1].layer = game::SectorBakedObjectLightProbeLayer::Upper;
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
    Check(game::WriteSectorBakedObjectLightProbeSidecar(path.string(), probes, 4.0f, 0.6f, 1.5f, error),
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
                  && Near(metadata.probeLowerHeightWorld, 0.6f)
                  && Near(metadata.probeUpperHeightWorld, 1.5f)
                  && metadata.format == game::kSectorBakedObjectLightProbeSidecarFormat,
          "object light probe sidecar metadata is populated");
    Check(loaded.size() == probes.size(), "object light probe sidecar preserves probe count");
    for (size_t probeIndex = 0; probeIndex < probes.size() && probeIndex < loaded.size(); ++probeIndex) {
        Check(loaded[probeIndex].sectorId == probes[probeIndex].sectorId,
              "object light probe sidecar preserves sector id");
        Check(loaded[probeIndex].layer == probes[probeIndex].layer,
              "object light probe sidecar preserves layer id");
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
        Check(game::WriteSectorBakedObjectLightProbeSidecar(path.string(), probes, 4.0f, 0.6f, 1.5f, error),
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
    PatchByte(badVersion, 4, 1);
    Check(readRejected(badVersion, nullptr), "object light probe sidecar rejects bad version");

    const std::filesystem::path truncated = writeFixture("truncated.object_probes.bin");
    TruncateFileByOneByte(truncated);
    Check(readRejected(truncated, nullptr), "object light probe sidecar rejects truncated file");

    const std::filesystem::path nonFinite = writeFixture("non_finite.object_probes.bin");
    PatchByte(nonFinite, 36, 0x00);
    PatchByte(nonFinite, 37, 0x00);
    PatchByte(nonFinite, 38, 0x80);
    PatchByte(nonFinite, 39, 0x7f);
    Check(readRejected(nonFinite, nullptr), "object light probe sidecar rejects non-finite floats");

    const std::filesystem::path badLayer = writeFixture("bad_layer.object_probes.bin");
    PatchByte(badLayer, 32, 2);
    Check(readRejected(badLayer, nullptr), "object light probe sidecar rejects unknown layer ids");

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
                  0.6f,
                  1.5f,
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
    Check(game::WriteSectorBakedObjectLightProbeSidecar(path.string(), probes, 4.0f, 0.6f, 1.5f, error),
          "runtime object probe sidecar fixture writes");

    game::SectorTopologyMap map = MakeProbeRectangle(1024, 1024);
    map.bakedLightmap.objectProbes.path = path.string();
    map.bakedLightmap.objectProbes.version = game::kSectorBakedObjectLightProbeSidecarVersion;
    map.bakedLightmap.objectProbes.sourceHash = game::ComputeSectorLightmapSourceHash(map);
    map.bakedLightmap.objectProbes.count = static_cast<int>(probes.size());
    map.bakedLightmap.objectProbes.probeSpacingWorld = 4.0f;
    map.bakedLightmap.objectProbes.probeLowerHeightWorld = 0.6f;
    map.bakedLightmap.objectProbes.probeUpperHeightWorld = 1.5f;
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
    Check(game::WriteSectorBakedObjectLightProbeSidecar(path.string(), probes, 4.0f, 0.6f, 1.5f, error),
          "runtime unavailable object probe fixture writes");

    game::SectorTopologyMap map = MakeProbeRectangle(1024, 1024);
    map.bakedLightmap.objectProbes.path = path.string();
    map.bakedLightmap.objectProbes.version = game::kSectorBakedObjectLightProbeSidecarVersion;
    map.bakedLightmap.objectProbes.sourceHash = game::ComputeSectorLightmapSourceHash(map);
    map.bakedLightmap.objectProbes.count = static_cast<int>(probes.size());
    map.bakedLightmap.objectProbes.probeSpacingWorld = 4.0f;
    map.bakedLightmap.objectProbes.probeLowerHeightWorld = 0.6f;
    map.bakedLightmap.objectProbes.probeUpperHeightWorld = 1.5f;
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
    Check(game::WriteSectorBakedObjectLightProbeSidecar(malformedPath.string(), probes, 4.0f, 0.6f, 1.5f, error),
          "runtime malformed object probe fixture writes");
    PatchByte(malformedPath, 0, static_cast<unsigned char>('X'));
    game::SectorTopologyMap malformed = map;
    malformed.bakedLightmap.objectProbes.path = malformedPath.string();
    Check(loadRejected(malformed), "runtime object probe load rejects malformed binary safely");
}

game::SectorBakedObjectLightProbe SamplingProbe(
        int sectorId,
        Vector3 position,
        Vector3 ambient,
        game::SectorBakedObjectLightProbeLayer layer =
                game::SectorBakedObjectLightProbeLayer::Lower)
{
    game::SectorBakedObjectLightProbe probe;
    probe.sectorId = sectorId;
    probe.layer = layer;
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
        return a.sectorId < b.sectorId
                || (a.sectorId == b.sectorId
                    && static_cast<unsigned int>(a.layer)
                            < static_cast<unsigned int>(b.layer));
    });

    game::SectorBakedObjectLightProbeRuntimeData data;
    data.probes = std::move(probes);
    for (size_t begin = 0; begin < data.probes.size();) {
        const int sectorId = data.probes[begin].sectorId;
        const game::SectorBakedObjectLightProbeLayer layer =
                data.probes[begin].layer;
        size_t end = begin + 1;
        while (end < data.probes.size()
                && data.probes[end].sectorId == sectorId
                && data.probes[end].layer == layer) {
            ++end;
        }

        game::SectorBakedObjectLightProbeSectorRange range;
        range.sectorId = sectorId;
        range.layer = layer;
        range.begin = static_cast<int>(begin);
        range.count = static_cast<int>(end - begin);
        data.sectorRanges.push_back(range);
        begin = end;
    }
    return data;
}

void TestObjectLightProbeSamplingBlendsHorizontalLayersByWorldHeight()
{
    using Layer = game::SectorBakedObjectLightProbeLayer;
    game::SectorBakedObjectLightProbeRuntimeData data = MakeSamplingRuntimeData({
            SamplingProbe(10, Vector3{0.0f, 0.6f, 0.0f}, Vector3{1.0f, 0.0f, 0.0f}, Layer::Lower),
            SamplingProbe(10, Vector3{10.0f, 0.6f, 0.0f}, Vector3{0.0f, 1.0f, 0.0f}, Layer::Lower),
            SamplingProbe(10, Vector3{0.0f, 1.5f, 0.0f}, Vector3{0.0f, 0.0f, 1.0f}, Layer::Upper),
            SamplingProbe(10, Vector3{10.0f, 1.5f, 0.0f}, Vector3{1.0f, 1.0f, 1.0f}, Layer::Upper),
    });

    const game::BakedObjectLightingVerticalSample vertical =
            game::SampleBakedObjectLightingVertical(
                    data,
                    Vector3{5.0f, 1.05f, 0.0f},
                    10,
                    nullptr);
    Check(vertical.lower.valid && vertical.upper.valid,
          "layered object probe sampling returns both valid layers");
    Check(SameVector(vertical.lower.ambientCube[0], Vector3{0.5f, 0.5f, 0.0f})
                  && SameVector(vertical.upper.ambientCube[0], Vector3{0.5f, 0.5f, 1.0f}),
          "layered object probe sampling interpolates each horizontal grid independently");
    Check(Near(vertical.lowerHeightWorld, 0.6f)
                  && Near(vertical.upperHeightWorld, 1.5f),
          "layered object probe sampling preserves interpolated layer heights");

    const game::BakedObjectLightingSample midpoint =
            game::ResolveBakedObjectLightingVerticalSample(vertical, 1.05f);
    Check(SameVector(midpoint.ambientCube[0], Vector3{0.5f, 0.5f, 0.5f}),
          "layered object probe lighting blends by requested world height");
    Check(SameVector(
                  game::ResolveBakedObjectLightingVerticalSample(vertical, -10.0f).ambientCube[0],
                  vertical.lower.ambientCube[0])
                  && SameVector(
                             game::ResolveBakedObjectLightingVerticalSample(vertical, 10.0f).ambientCube[0],
                             vertical.upper.ambientCube[0]),
          "layered object probe height blending clamps outside the layer interval");

    game::SectorBakedObjectLightProbeRuntimeData singleLayer = MakeSamplingRuntimeData({
            SamplingProbe(10, Vector3{2.0f, 0.4f, 2.0f}, Vector3{0.25f, 0.5f, 0.75f}),
    });
    const game::BakedObjectLightingVerticalSample duplicated =
            game::SampleBakedObjectLightingVertical(
                    singleLayer,
                    Vector3{2.0f, 1.0f, 2.0f},
                    10,
                    nullptr);
    Check(SameVector(duplicated.lower.ambientCube[0], duplicated.upper.ambientCube[0])
                  && Near(duplicated.lowerHeightWorld, duplicated.upperHeightWorld),
          "single-layer sectors duplicate their sample for stable vertical resolution");
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

void TestObjectLightProbeSamplingAdjacentPortalBlending()
{
    const game::SectorTopologyMap map = MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f);
    game::SectorBakedObjectLightProbeRuntimeData data = MakeSamplingRuntimeData({
            SamplingProbe(10, Vector3{-0.25f, 0.0f, 0.0f}, Vector3{1.0f, 0.0f, 0.0f}),
            SamplingProbe(20, Vector3{0.75f, 0.0f, 0.0f}, Vector3{0.0f, 0.0f, 1.0f}),
    });

    const game::BakedObjectLightingSample far =
            game::SampleBakedObjectLighting(data, Vector3{-2.0f, 0.0f, 0.0f}, 10, &map);
    Check(far.valid, "object light probe far portal sample remains valid");
    Check(SameVector(far.ambientCube[0], Vector3{1.0f, 0.0f, 0.0f}),
          "object light probe sampling far from portal uses preferred sector only");

    const game::BakedObjectLightingSample near =
            game::SampleBakedObjectLighting(data, Vector3{0.25f, 0.0f, 0.0f}, 10, &map);
    Check(near.valid, "object light probe near portal sample remains valid");
    Check(Near(near.ambientCube[0].x, 0.5f) && Near(near.ambientCube[0].z, 0.5f),
          "object light probe sampling near open portal blends preferred and adjacent sectors");
}

void TestObjectLightProbeSamplingAdjacentSectorDeduplicatesSplitPortal()
{
    const game::SectorTopologyMap map = MakeAdjacentWithSplitPortal();
    game::SectorBakedObjectLightProbeRuntimeData data = MakeSamplingRuntimeData({
            SamplingProbe(10, Vector3{-0.25f, 0.0f, 0.0f}, Vector3{1.0f, 0.0f, 0.0f}),
            SamplingProbe(20, Vector3{0.75f, 0.0f, 0.0f}, Vector3{0.0f, 0.0f, 1.0f}),
    });

    const game::BakedObjectLightingSample sample =
            game::SampleBakedObjectLighting(data, Vector3{0.25f, 0.0f, 0.125f}, 10, &map);
    Check(sample.valid, "object light probe split portal sample remains valid");
    Check(Near(sample.ambientCube[0].x, 0.5f) && Near(sample.ambientCube[0].z, 0.5f),
          "object light probe split portal streams adjacent sector only once");
}

void TestObjectLightProbeSamplingAdjacentSectorCapAndPreferredDeduplication()
{
    const game::SectorTopologyMap capMap = MakeObjectProbeAdjacentCapMap();
    std::vector<game::SectorBakedObjectLightProbe> capProbes;
    capProbes.push_back(SamplingProbe(10, Vector3{4.0f, 0.0f, 0.0f}, Vector3{0.0f, 0.0f, 0.0f}));
    for (int index = 0; index < 9; ++index) {
        const float red = index == game::kObjectProbeMaxAdjacentBlendSectors ? 1.0f : 0.0f;
        const float x = index == game::kObjectProbeMaxAdjacentBlendSectors ? 1.0f : 0.0f;
        capProbes.push_back(SamplingProbe(20 + index, Vector3{x, 0.0f, 0.0f}, Vector3{red, 0.0f, 0.0f}));
    }
    game::SectorBakedObjectLightProbeRuntimeData capData = MakeSamplingRuntimeData(std::move(capProbes));

    const game::BakedObjectLightingSample capped =
            game::SampleBakedObjectLighting(capData, Vector3{2.0f, 0.0f, 0.0f}, 10, &capMap);
    Check(capped.valid, "object light probe adjacent cap sample remains valid");
    Check(SameVector(capped.ambientCube[0], Vector3{0.0f, 0.0f, 0.0f}),
          "object light probe adjacent sector cap deterministically excludes the ninth adjacent sector");

    game::SectorTopologyMap preferredLoopMap = MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f);
    game::SectorTopologySideDef* loopBackSide = game::FindSectorTopologySideDef(preferredLoopMap, 8);
    Check(loopBackSide != nullptr, "object light probe preferred self-portal fixture sidedef exists");
    if (loopBackSide != nullptr) {
        loopBackSide->sectorId = 10;
    }
    game::SectorBakedObjectLightProbeRuntimeData preferredLoopData = MakeSamplingRuntimeData({
            SamplingProbe(10, Vector3{-0.25f, 0.0f, 0.0f}, Vector3{1.0f, 0.0f, 0.0f}),
            SamplingProbe(10, Vector3{0.85f, 0.0f, 0.0f}, Vector3{0.0f, 1.0f, 0.0f}),
            SamplingProbe(10, Vector3{0.95f, 0.0f, 0.0f}, Vector3{0.0f, 0.0f, 1.0f}),
    });

    const game::BakedObjectLightingSample preferredBaseline =
            game::SampleBakedObjectLighting(preferredLoopData, Vector3{0.25f, 0.0f, 0.0f}, 10, nullptr);
    const game::BakedObjectLightingSample preferredOnly =
            game::SampleBakedObjectLighting(preferredLoopData, Vector3{0.25f, 0.0f, 0.0f}, 10, &preferredLoopMap);
    Check(preferredBaseline.valid && preferredOnly.valid,
          "object light probe preferred self-portal sample remains valid");
    Check(SameVector(preferredOnly.ambientCube[0], preferredBaseline.ambientCube[0]),
          "object light probe sampling does not duplicate preferred sector through a self-resolving portal");
}

void TestObjectLightProbeSamplingDoesNotBlendThroughClosedOrUnavailableAdjacency()
{
    const game::SectorTopologyMap closedPortalMap = MakeAdjacent(0.0f, 8.0f, 8.0f, 16.0f);
    game::SectorBakedObjectLightProbeRuntimeData data = MakeSamplingRuntimeData({
            SamplingProbe(10, Vector3{-0.25f, 0.0f, 0.0f}, Vector3{1.0f, 0.0f, 0.0f}),
            SamplingProbe(20, Vector3{0.75f, 0.0f, 0.0f}, Vector3{0.0f, 0.0f, 1.0f}),
    });

    const game::BakedObjectLightingSample closed =
            game::SampleBakedObjectLighting(data, Vector3{0.25f, 0.0f, 0.0f}, 10, &closedPortalMap);
    Check(closed.valid, "object light probe closed portal sample remains valid");
    Check(SameVector(closed.ambientCube[0], Vector3{1.0f, 0.0f, 0.0f}),
          "object light probe sampling does not blend through vertically closed portals");

    game::SectorTopologyMap oneSidedMap = MakeProbeRectangle(64, 64);
    oneSidedMap.sectors.push_back(Sector(20));
    const game::BakedObjectLightingSample oneSided =
            game::SampleBakedObjectLighting(data, Vector3{0.25f, 0.0f, 0.0f}, 10, &oneSidedMap);
    Check(oneSided.valid, "object light probe one-sided wall sample remains valid");
    Check(SameVector(oneSided.ambientCube[0], Vector3{1.0f, 0.0f, 0.0f}),
          "object light probe sampling does not blend through one-sided or non-neighbor boundaries");

    const game::BakedObjectLightingSample nullMap =
            game::SampleBakedObjectLighting(data, Vector3{0.25f, 0.0f, 0.0f}, 10, nullptr);
    Check(nullMap.valid, "object light probe null map sample remains valid");
    Check(SameVector(nullMap.ambientCube[0], Vector3{1.0f, 0.0f, 0.0f}),
          "object light probe sampling with null map preserves preferred-sector-only behavior");

    const game::BakedObjectLightingSample missingSectorMap =
            game::SampleBakedObjectLighting(data, Vector3{0.25f, 0.0f, 0.0f}, 10, &oneSidedMap);
    Check(missingSectorMap.valid, "object light probe unavailable adjacency sample remains valid");
    Check(SameVector(missingSectorMap.ambientCube[0], Vector3{1.0f, 0.0f, 0.0f}),
          "object light probe preferred sector with probes does not fall back to all probes when adjacency is unavailable");
}

void TestObjectLightProbeSamplingKeepsAllProbeFallbackWithoutPreferredProbes()
{
    const game::SectorTopologyMap map = MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f);
    game::SectorBakedObjectLightProbeRuntimeData data = MakeSamplingRuntimeData({
            SamplingProbe(20, Vector3{0.75f, 0.0f, 0.0f}, Vector3{0.0f, 0.0f, 1.0f}),
    });

    const game::BakedObjectLightingSample sample =
            game::SampleBakedObjectLighting(data, Vector3{0.75f, 0.0f, 0.0f}, 10, &map);
    Check(sample.valid, "object light probe missing preferred sector probes fallback remains valid");
    Check(SameVector(sample.ambientCube[0], Vector3{0.0f, 0.0f, 1.0f}),
          "object light probe missing preferred sector probes still falls back to all loaded probes");
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

void TestLocalFogEffectivePathLengthSaturatesGrazingTraversal()
{
    const game::SectorLocalFogPathLimitSettings settings;
    const float shortPath = game::ComputeSectorLocalFogEffectivePathLength(0.25f, 0.5f, settings);
    Check(shortPath > 0.249f && shortPath <= 0.25f,
          "local fog path limit preserves short traversals");

    const float mediumPath = game::ComputeSectorLocalFogEffectivePathLength(2.0f, 0.5f, settings);
    const float longPath = game::ComputeSectorLocalFogEffectivePathLength(20.0f, 0.5f, settings);
    Check(mediumPath > shortPath && longPath > mediumPath,
          "local fog effective path remains monotonic");
    Check(longPath <= 1.5f && longPath > 1.49f,
          "local fog grazing path saturates at three times volume height");

    const float tinyVolumeLongPath =
            game::ComputeSectorLocalFogEffectivePathLength(20.0f, 0.05f, settings);
    Check(tinyVolumeLongPath <= 0.5f && tinyVolumeLongPath > 0.49f,
          "local fog path limit enforces the half-metre minimum cap");

    const float stepLength =
            game::ComputeSectorLocalFogEffectiveStepLength(20.0f, 0.5f, 8, settings);
    Check(Near(stepLength * 8.0f, longPath, 0.0001f),
          "local fog distributes capped optical distance across march samples exactly once");
}

void TestLocalFogProbeLightingReductionInterpolationAndFallback()
{
    game::BakedObjectLightingSample cube;
    cube.ambientCube[0] = Vector3{1.0f, 0.0f, 0.0f};
    cube.ambientCube[1] = Vector3{0.0f, 1.0f, 0.0f};
    cube.ambientCube[2] = Vector3{0.0f, 0.0f, 1.0f};
    cube.ambientCube[3] = Vector3{100.0f, 100.0f, 100.0f};
    cube.ambientCube[4] = Vector3{1.0f, 1.0f, 0.0f};
    cube.ambientCube[5] = Vector3{0.0f, 1.0f, 1.0f};
    Check(SameVector(
                  game::EvaluateSectorLocalFogProbeLighting(cube),
                  Vector3{0.4f, 0.6f, 0.4f}),
          "local fog uses the stable upper-hemisphere probe reduction and ignores the lower face");

    game::SectorLocalFogStaticLightingSamples interpolation;
    interpolation.corners[0] = Vector3{1.0f, 0.0f, 0.0f};
    interpolation.corners[1] = Vector3{0.0f, 1.0f, 0.0f};
    interpolation.corners[2] = Vector3{0.0f, 0.0f, 1.0f};
    interpolation.corners[3] = Vector3{1.0f, 1.0f, 1.0f};
    Check(SameVector(
                  game::InterpolateSectorLocalFogStaticLighting(interpolation, Vector2{-0.5f, -0.5f}),
                  interpolation.corners[0]),
          "local fog probe interpolation preserves the negative corner");
    Check(SameVector(
                  game::InterpolateSectorLocalFogStaticLighting(interpolation, Vector2{0.5f, 0.5f}),
                  interpolation.corners[3]),
          "local fog probe interpolation preserves the positive corner");
    Check(SameVector(
                  game::InterpolateSectorLocalFogStaticLighting(interpolation, Vector2{}),
                  Vector3{0.5f, 0.5f, 0.5f}),
          "local fog probe interpolation blends the four representative samples at volume centre");

    game::SectorTopologyMap map = MakeProbeRectangle(1024, 1024);
    game::SectorTopologySector* sector = game::FindSectorTopologySector(map, 10);
    Check(sector != nullptr, "local fog sector-ambient fallback fixture exists");
    if (sector == nullptr) return;
    sector->ambientColor = Color{64, 128, 255, 255};
    sector->ambientIntensity = 0.5f;

    game::SectorCompiledLocalFogVolume volume;
    volume.sourceAuthoringFogVolumeId = 1;
    volume.topologySectorId = 10;
    volume.centerWorld = Vector3{2.0f, 0.25f, 2.0f};
    volume.radiiWorld = Vector3{1.0f, 0.25f, 1.0f};
    const game::SectorBakedObjectLightProbeRuntimeData noProbes;
    const game::SectorLocalFogStaticLightingSamples fallback =
            game::SampleSectorLocalFogStaticLighting(map, noProbes, volume);
    for (const Vector3& corner : fallback.corners) {
        Check(SameVector(corner, Vector3{0.125490f, 0.250980f, 0.5f}),
              "local fog without probes uses containing-sector ambient instead of emissive tint");
    }
}

void TestObjectAmbientCubeNormalBlending()
{
    game::BakedObjectLightingSample cube;
    cube.ambientCube[0] = Vector3{1.0f, 0.0f, 0.0f};
    cube.ambientCube[1] = Vector3{0.0f, 1.0f, 0.0f};
    cube.ambientCube[2] = Vector3{0.0f, 0.0f, 1.0f};
    cube.ambientCube[3] = Vector3{1.0f, 1.0f, 0.0f};
    cube.ambientCube[4] = Vector3{1.0f, 0.0f, 1.0f};
    cube.ambientCube[5] = Vector3{0.0f, 1.0f, 1.0f};

    Check(SameVector(
                  game::EvaluateBakedObjectAmbientCubeLighting(cube, Vector3{2.0f, 0.0f, 0.0f}),
                  cube.ambientCube[0]),
          "ambient cube preserves an axis-aligned positive face");
    Check(SameVector(
                  game::EvaluateBakedObjectAmbientCubeLighting(cube, Vector3{0.0f, -3.0f, 0.0f}),
                  cube.ambientCube[3]),
          "ambient cube preserves an axis-aligned negative face");
    Check(SameVector(
                  game::EvaluateBakedObjectAmbientCubeLighting(cube, Vector3{1.0f, 1.0f, 0.0f}),
                  Vector3{0.5f, 0.0f, 0.5f}),
          "ambient cube uses squared normalized weights for diagonal normals");

    const Vector3 xDominant = game::EvaluateBakedObjectAmbientCubeLighting(
            cube,
            Vector3{0.71f, 0.70f, 0.0f});
    const Vector3 yDominant = game::EvaluateBakedObjectAmbientCubeLighting(
            cube,
            Vector3{0.70f, 0.71f, 0.0f});
    Check(Near(xDominant.x, yDominant.x, 0.03f)
                  && Near(xDominant.z, yDominant.z, 0.03f),
          "ambient cube remains continuous when the dominant normal axis changes");

    Check(SameVector(
                  game::EvaluateBakedObjectAmbientCubeLighting(cube, Vector3{}),
                  cube.ambientCube[2]),
          "ambient cube degenerate normal falls back to positive Y");
    Check(SameVector(
                  game::EvaluateBakedObjectAmbientCubeLighting(
                          cube,
                          Vector3{std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f}),
                  cube.ambientCube[2]),
          "ambient cube non-finite normal falls back safely");
}

void TestLightAtmosphereVolumeShapesAndProbeFallback()
{
    game::SectorTopologyMap sourceMap;
    game::SectorTopologyStaticPointLight disabledAtmosphere;
    disabledAtmosphere.id = 1;
    disabledAtmosphere.atmosphere.haze.density = 0.2f;
    sourceMap.staticLights.push_back(disabledAtmosphere);
    std::vector<game::SectorLightAtmosphereSource> sources;
    game::BuildSectorLightAtmosphereSources(sourceMap, nullptr, sources);
    Check(sources.empty(),
          "disabled light atmosphere does not create a per-frame renderer source");
    sourceMap.staticLights[0].atmosphere.haze.enabled = true;
    game::BuildSectorLightAtmosphereSources(sourceMap, nullptr, sources);
    Check(sources.size() == 1,
          "enabled static-light haze creates one bounded renderer source");
    game::SectorTopologyDynamicPointLight disabledDynamic;
    disabledDynamic.id = 2;
    disabledDynamic.enabled = false;
    disabledDynamic.atmosphere.haze.enabled = true;
    sourceMap.dynamicPointLights.push_back(disabledDynamic);
    game::BuildSectorLightAtmosphereSources(sourceMap, nullptr, sources);
    Check(sources.size() == 1,
          "disabled dynamic light does not create an atmosphere renderer source");

    game::SectorLightAtmosphereSource point;
    point.shape = game::SectorLightAtmosphereShape::Sphere;
    point.positionWorld = Vector3{1.0f, 2.0f, 3.0f};
    point.rangeWorld = 10.0f;
    game::SectorLightAtmosphereVolume pointVolume;
    Check(game::MakeSectorLightAtmosphereVolume(point, 0.5f, pointVolume)
                  && Near(pointVolume.extentWorld, 5.0f),
          "point-light atmosphere extent scales the authored light range");
    Check(game::IsPointInsideSectorLightAtmosphereVolume(
                  pointVolume, Vector3{1.0f, 2.0f, 7.9f})
                  && !game::IsPointInsideSectorLightAtmosphereVolume(
                          pointVolume, Vector3{1.0f, 2.0f, 8.1f}),
          "point-light atmosphere uses bounded spherical particle and haze containment");

    game::SectorLightAtmosphereSource spot;
    spot.shape = game::SectorLightAtmosphereShape::Cone;
    spot.positionWorld = Vector3{};
    spot.directionWorld = Vector3{0.0f, 0.0f, 1.0f};
    spot.rangeWorld = 8.0f;
    spot.outerConeCos = std::cos(30.0f * DEG2RAD);
    game::SectorLightAtmosphereVolume spotVolume;
    Check(game::MakeSectorLightAtmosphereVolume(spot, 1.0f, spotVolume),
          "spot-light atmosphere builds a bounded cone proxy");
    Check(game::IsPointInsideSectorLightAtmosphereVolume(
                  spotVolume, Vector3{1.0f, 0.0f, 4.0f})
                  && !game::IsPointInsideSectorLightAtmosphereVolume(
                          spotVolume, Vector3{3.0f, 0.0f, 4.0f})
                  && !game::IsPointInsideSectorLightAtmosphereVolume(
                          spotVolume, Vector3{0.0f, 0.0f, 8.1f}),
          "spot-light atmosphere bounds particles and haze to its finite cone");

    game::SectorLightHazeStaticLightingSamples grid;
    for (std::size_t index = 0; index < grid.corners.size(); ++index) {
        const float value = static_cast<float>(index);
        grid.corners[index] = Vector3{value, value, value};
    }
    Check(SameVector(
                  game::InterpolateSectorLightHazeStaticLighting(grid, Vector3{0.5f, 0.5f, 0.5f}),
                  Vector3{3.5f, 3.5f, 3.5f}),
          "light haze trilinearly interpolates the representative 2x2x2 probe grid");

    game::SectorTopologyMap map = MakeProbeRectangle(1024, 1024);
    game::SectorTopologySector* sector = game::FindSectorTopologySector(map, 10);
    Check(sector != nullptr, "light atmosphere fallback sector fixture exists");
    if (sector == nullptr) return;
    sector->ambientColor = Color{64, 128, 255, 255};
    sector->ambientIntensity = 0.5f;
    point.ownerSectorId = 10;
    point.positionWorld = Vector3{2.0f, 1.0f, 2.0f};
    point.rangeWorld = 2.0f;
    Check(game::MakeSectorLightAtmosphereVolume(point, 1.0f, pointVolume),
          "light atmosphere fallback volume builds");
    const game::SectorBakedObjectLightProbeRuntimeData noProbes;
    const game::SectorLightHazeStaticLightingSamples fallback =
            game::SampleSectorLightHazeStaticLighting(map, noProbes, pointVolume);
    for (const Vector3& corner : fallback.corners) {
        Check(SameVector(corner, Vector3{0.125490f, 0.250980f, 0.5f}),
              "light atmosphere without probes uses sector ambient rather than emissive tint");
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
    map.lightmapSettings.objectProbeLowerHeightWorld = 0.6f;
    map.lightmapSettings.objectProbeUpperHeightWorld = 1.5f;

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
                  && Near(result.objectProbes.probeLowerHeightWorld, 0.6f)
                  && Near(result.objectProbes.probeUpperHeightWorld, 1.5f)
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
    Check(CountProbesForSector(corridor, 10) == 8,
          "long corridor receives two layers of object light probes");

    const std::vector<game::SectorBakedObjectLightProbe> room =
            BuildObjectProbePlacementsForTest(MakeProbeRectangle(1024, 1024));
    Check(CountProbesForSector(room, 10) == 8,
          "large room receives two layers of object light probes");
    Check(HasProbeNear(room, 10, 2.0f, 2.0f)
                  && HasProbeNear(room, 10, 6.0f, 2.0f)
                  && HasProbeNear(room, 10, 2.0f, 6.0f)
                  && HasProbeNear(room, 10, 6.0f, 6.0f),
          "object light probe placement converts topology coordinates to world positions");
    int lowerCount = 0;
    int upperCount = 0;
    for (const game::SectorBakedObjectLightProbe& probe : room) {
        if (probe.layer == game::SectorBakedObjectLightProbeLayer::Lower) {
            ++lowerCount;
            Check(Near(probe.position.y, 0.6f),
                  "lower object light probe layer uses its floor-relative height");
        } else {
            ++upperCount;
            Check(Near(probe.position.y, 1.5f),
                  "upper object light probe layer uses its floor-relative height");
        }
    }
    Check(lowerCount == 4 && upperCount == 4,
          "large room receives matching lower and upper probe grids");
}

void TestObjectLightProbePlacementRejectsConcaveVoid()
{
    const std::vector<game::SectorBakedObjectLightProbe> probes =
            BuildObjectProbePlacementsForTest(MakeProbeConcaveSector());
    Check(CountProbesForSector(probes, 10) == 6,
          "concave sector layered object light probe placement keeps only interior grid points");
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
    bool sawSingleLayer = false;
    for (const game::SectorBakedObjectLightProbePlacementDiagnostic& diagnostic : diagnostics) {
        sawFallback = sawFallback || diagnostic.message.find("fallback") != std::string::npos;
        sawSingleLayer = sawSingleLayer || diagnostic.message.find("midpoint layer") != std::string::npos;
    }
    Check(sawFallback, "object light probe placement records small-sector fallback diagnostic");
    Check(sawSingleLayer, "object light probe placement records low-ceiling single-layer diagnostic");
}

void TestObjectLightProbePlacementConfiguredSingleLayerAndValidation()
{
    const game::SectorTopologyMap map = MakeProbeRectangle(1024, 1024);
    game::SectorBakedObjectLightProbePlacementSettings settings;
    settings.lowerHeightWorld = 0.7f;
    settings.upperHeightWorld = 0.7f;
    std::vector<game::SectorBakedObjectLightProbe> probes;
    std::vector<game::SectorBakedObjectLightProbePlacementDiagnostic> diagnostics;
    std::string error;
    Check(game::BuildSectorBakedObjectLightProbePlacements(
                  map, settings, probes, &diagnostics, error),
          "configured single object probe layer builds");
    Check(CountProbesForSector(probes, 10) == 4,
          "configured equal probe heights produce one horizontal layer");
    for (const game::SectorBakedObjectLightProbe& probe : probes) {
        Check(probe.layer == game::SectorBakedObjectLightProbeLayer::Lower
                      && Near(probe.position.y, 0.7f),
              "configured single layer preserves its requested floor-relative height");
    }

    settings.lowerHeightWorld = 1.5f;
    settings.upperHeightWorld = 0.6f;
    Check(!game::BuildSectorBakedObjectLightProbePlacements(
                  map, settings, probes, nullptr, error)
                  && !error.empty() && probes.empty(),
          "object probe placement rejects reversed layer heights safely");
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

struct CpuMeshFixture {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> uv2;
    std::vector<unsigned short> indices;
    Mesh mesh = {};

    void Bind()
    {
        mesh.vertexCount = static_cast<int>(positions.size() / 3);
        mesh.triangleCount = static_cast<int>(indices.size() / 3);
        mesh.vertices = positions.data();
        mesh.normals = normals.empty() ? nullptr : normals.data();
        mesh.texcoords2 = uv2.empty() ? nullptr : uv2.data();
        mesh.indices = indices.empty() ? nullptr : indices.data();
    }
};

CpuMeshFixture MakeAuthoredUv2Quad()
{
    CpuMeshFixture fixture;
    fixture.positions = {
            0.0f, 0.0f, 0.0f,
            2.0f, 0.0f, 0.0f,
            2.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f};
    fixture.normals = {
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f};
    fixture.uv2 = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 1.0f};
    fixture.indices = {0, 1, 2, 0, 2, 3};
    fixture.Bind();
    return fixture;
}

CpuMeshFixture MakeSharedVertexCubeWithInvalidUv2()
{
    CpuMeshFixture fixture;
    fixture.positions = {
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f};
    fixture.normals.resize(fixture.positions.size(), 0.0f);
    fixture.uv2.resize(8 * 2, 0.0f);
    fixture.indices = {
            0, 2, 1, 0, 3, 2,
            4, 5, 6, 4, 6, 7,
            0, 1, 5, 0, 5, 4,
            3, 7, 6, 3, 6, 2,
            0, 4, 7, 0, 7, 3,
            1, 2, 6, 1, 6, 5};
    fixture.Bind();
    return fixture;
}

void TestStaticModelUvPreparationAndImportedTransforms()
{
    CpuMeshFixture authored = MakeAuthoredUv2Quad();
    CpuMeshFixture invalid = MakeSharedVertexCubeWithInvalidUv2();
    std::array<Mesh, 2> meshes{authored.mesh, invalid.mesh};
    Model model = {};
    model.transform = MatrixMultiply(
            MatrixScale(2.0f, 1.0f, 0.5f),
            MatrixTranslate(3.0f, 4.0f, 5.0f));
    model.meshCount = static_cast<int>(meshes.size());
    model.meshes = meshes.data();

    game::SectorStaticModelLightmapModel prepared;
    std::string error;
    Check(game::CopySectorStaticModelForLightmap(
                  "fixture.gltf",
                  "geometry-a",
                  model,
                  prepared,
                  error),
          "mixed static model meshes prepare for lightmapping");
    Check(prepared.meshes.size() == 2,
          "mixed static model preserves mesh boundaries");
    if (prepared.meshes.size() != 2) {
        return;
    }
    Check(prepared.meshes[0].preservesAuthoredUv2,
          "finite non-overlapping authored UV2 is preserved");
    Check(!prepared.meshes[1].preservesAuthoredUv2,
          "degenerate authored UV2 is automatically unwrapped");
    Check(prepared.meshes[1].sourceVertexIndices.size() > 8,
          "xatlas duplicates shared cube vertices at lightmap seams");
    const Vector3 expected = Vector3Transform(
            Vector3{
                    authored.positions[0],
                    authored.positions[1],
                    authored.positions[2]},
            model.transform);
    Check(SameVector(prepared.meshes[0].importedPositions[0], expected),
          "imported model transform is applied before lightmap density and bake geometry");

    game::SectorStaticModelLightmapModel repeated;
    Check(game::CopySectorStaticModelForLightmap(
                  "fixture.gltf",
                  "geometry-a",
                  model,
                  repeated,
                  error),
          "static model unwrap can be repeated");
    Check(repeated.meshes.size() == prepared.meshes.size()
                  && repeated.meshes[1].sourceVertexIndices
                          == prepared.meshes[1].sourceVertexIndices
                  && repeated.meshes[1].indices
                          == prepared.meshes[1].indices,
          "xatlas vertex remaps and indices are deterministic");
    bool sameUvs = repeated.meshes.size() == prepared.meshes.size()
            && repeated.meshes[1].localLightmapUvs.size()
                    == prepared.meshes[1].localLightmapUvs.size();
    for (size_t i = 0;
            sameUvs && i < prepared.meshes[1].localLightmapUvs.size();
            ++i) {
        const Vector2 a = repeated.meshes[1].localLightmapUvs[i];
        const Vector2 b = prepared.meshes[1].localLightmapUvs[i];
        sameUvs = Near(a.x, b.x) && Near(a.y, b.y);
    }
    Check(sameUvs, "xatlas local UV output is deterministic");
}

void TestStaticModelPreparationReusesReadyEditorModels()
{
    const std::filesystem::path root =
            Phase01bSandboxDir() / "static_model_ready_reuse";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const std::filesystem::path bufferPath = root / "mesh.bin";
    const std::filesystem::path modelPath = root / "mesh.gltf";
    WriteTextFile(bufferPath, "abc");
    WriteTextFile(
            modelPath,
            R"({"asset":{"version":"2.0"},"buffers":[{"uri":"mesh.bin","byteLength":3}],"bufferViews":[],"accessors":[],"meshes":[]})");

    CpuMeshFixture fixture = MakeAuthoredUv2Quad();
    std::array<Mesh, 1> meshes{fixture.mesh};
    Model readyModel = {};
    readyModel.transform = MatrixIdentity();
    readyModel.meshCount = static_cast<int>(meshes.size());
    readyModel.meshes = meshes.data();

    game::SectorTopologyMap map = MakeSquare();
    game::SectorPlacedRuntimeObject first;
    first.id = 91;
    first.kind = "static_model";
    first.position = Vector3{2.0f, 0.0f, 2.0f};
    first.staticModel.modelPath = modelPath.string();
    first.staticModel.scale = 1.5f;
    map.runtimeObjects.push_back(first);
    game::SectorPlacedRuntimeObject repeated = first;
    repeated.id = 92;
    repeated.position.x = 3.0f;
    map.runtimeObjects.push_back(repeated);

    engine::AssetManager assets;
    int readyLookupCount = 0;
    game::SectorStaticModelLightmapData prepared;
    std::string error;
    const bool preparationSucceeded =
            game::PrepareSectorStaticModelsForLightmapBake(
                    map,
                    assets,
                    {},
                    prepared,
                    error,
                    [&readyModel, &readyLookupCount](
                            const std::string&) {
                        ++readyLookupCount;
                        return &readyModel;
                    });
    if (!preparationSucceeded) {
        std::fprintf(
                stderr,
                "Static model reuse preparation error: %s\n",
                error.c_str());
    }
    Check(preparationSucceeded,
          "static model bake preparation reuses ready editor model data");
    Check(readyLookupCount == 1
                  && prepared.models.size() == 1
                  && prepared.objects.size() == 2
                  && prepared.objects[0].modelIndex
                          == prepared.objects[1].modelIndex
                  && Near(prepared.objects[0].scale, 1.5f)
                  && Near(prepared.objects[1].scale, 1.5f),
          "reused ready models are looked up and copied once for repeated prop paths");

    Check(assets.Initialize()
                  && assets.GlobalScope().index == 0,
          "reuse-only bake preparation creates no temporary asset scope or reload request");
    assets.Shutdown();
    std::filesystem::remove_all(root);
}

game::SectorStaticModelLightmapData MakeStaticModelSidecarFixture()
{
    game::SectorStaticModelLightmapData data;
    data.sourceHash = "static-model-source-hash";
    game::SectorStaticModelLightmapModel model;
    model.modelPath = "assets/models/fixture.gltf";
    model.geometryFingerprint = "geometry-a";
    game::SectorStaticModelLightmapMesh mesh;
    mesh.originalVertexCount = 3;
    mesh.sourceVertexIndices = {0, 1, 2};
    mesh.localLightmapUvs = {
            Vector2{0.0f, 0.0f},
            Vector2{1.0f, 0.0f},
            Vector2{0.0f, 1.0f}};
    mesh.indices = {0, 1, 2};
    mesh.usableWidth = 16;
    mesh.usableHeight = 16;
    model.meshes.push_back(mesh);
    data.models.push_back(model);
    game::SectorStaticModelLightmapObject object;
    object.objectId = 17;
    object.modelIndex = 0;
    object.containingSectorId = 10;
    object.meshPlacements.resize(1);
    data.objects.push_back(object);
    return data;
}

void TestStaticModelChartPackingAndSidecarLifecycle()
{
    game::SectorStaticModelLightmapData data =
            MakeStaticModelSidecarFixture();
    data.objects.push_back(data.objects.front());
    data.objects.back().objectId = 18;
    std::string error;
    Check(game::PackSectorStaticModelLightmapCharts(
                  data,
                  2048,
                  2048,
                  2,
                  game::SectorStaticModelLightmapPackCursor{0, 100, 20, 24},
                  error),
          "static model charts append after a supplied topology shelf cursor");
    Check(data.objects[0].meshPlacements[0].x == 100
                  && data.objects[0].meshPlacements[0].y == 20
                  && data.objects[0].meshPlacements[0].atlasIndex == 0
                  && data.objects[1].meshPlacements[0].x
                          > data.objects[0].meshPlacements[0].x,
          "static model chart packing preserves the preceding topology placement");

    game::SectorStaticModelLightmapData rollover =
            MakeStaticModelSidecarFixture();
    rollover.objects.push_back(rollover.objects.front());
    rollover.objects.back().objectId = 19;
    Check(game::PackSectorStaticModelLightmapCharts(
                  rollover,
                  32,
                  32,
                  2,
                  {},
                  error)
                  && rollover.objects[0].meshPlacements[0].atlasIndex == 0
                  && rollover.objects[1].meshPlacements[0].atlasIndex == 1
                  && rollover.objects[1].meshPlacements[0].x == 0
                  && rollover.objects[1].meshPlacements[0].y == 0,
          "static model chart packing rolls into a new atlas without a fixed atlas-count cap");

    game::SectorStaticModelLightmapData overflow =
            MakeStaticModelSidecarFixture();
    overflow.models[0].meshes[0].usableWidth = 2048;
    Check(!game::PackSectorStaticModelLightmapCharts(
                   overflow,
                   2048,
                   2048,
                   2,
                   {},
                   error)
                  && error.find("larger than the 2048 atlas")
                          != std::string::npos,
          "oversized static model charts fail with an atlas diagnostic");

    const std::filesystem::path path =
            Phase01bSandboxDir() / "static_models_round_trip.bin";
    std::filesystem::create_directories(path.parent_path());
    Check(game::WriteSectorStaticModelLightmapSidecar(
                  path.string(),
                  data,
                  error),
          "static model lightmap sidecar writes");
    game::SectorBakedStaticModelLightmapMetadata metadata;
    metadata.path = path.string();
    metadata.version = game::kSectorStaticModelLightmapSidecarVersion;
    metadata.sourceHash = data.sourceHash;
    metadata.modelCount = static_cast<int>(data.models.size());
    metadata.objectCount = static_cast<int>(data.objects.size());
    metadata.format = game::kSectorStaticModelLightmapSidecarFormat;
    game::SectorStaticModelLightmapData loaded;
    Check(game::ReadSectorStaticModelLightmapSidecar(
                  path.string(),
                  &metadata,
                  loaded,
                  error),
          "static model lightmap sidecar round-trips");
    Check(loaded.models.size() == data.models.size()
                  && loaded.objects.size() == data.objects.size()
                  && loaded.models[0].meshes[0].sourceVertexIndices
                          == data.models[0].meshes[0].sourceVertexIndices
                  && Near(
                          loaded.objects[0].meshPlacements[0].atlasScale.x,
                          data.objects[0].meshPlacements[0].atlasScale.x)
                  && Near(
                          loaded.objects[0].meshPlacements[0].atlasBias.y,
                          data.objects[0].meshPlacements[0].atlasBias.y)
                  && loaded.objects[0].meshPlacements[0].atlasIndex
                          == data.objects[0].meshPlacements[0].atlasIndex,
          "static model sidecar preserves remaps and object atlas transforms");
    Check(game::AreSectorStaticModelLightmapAtlasIndicesValid(loaded, 1),
          "static model sidecar atlas indices validate against installed metadata");
    loaded.objects[0].meshPlacements[0].atlasIndex = 1;
    Check(!game::AreSectorStaticModelLightmapAtlasIndicesValid(loaded, 1),
          "static model placement outside installed atlas metadata is rejected");

    PatchByte(path, 4, 1);
    Check(!game::ReadSectorStaticModelLightmapSidecar(
                   path.string(),
                   &metadata,
                   loaded,
                   error),
          "version-1 static model sidecars are rejected after multi-atlas format adoption");
    Check(game::WriteSectorStaticModelLightmapSidecar(
                  path.string(),
                  data,
                  error),
          "static model sidecar fixture rewrites after old-version rejection");

    game::SectorBakedStaticModelLightmapMetadata stale = metadata;
    stale.sourceHash = "different";
    Check(!game::ReadSectorStaticModelLightmapSidecar(
                   path.string(),
                   &stale,
                   loaded,
                   error),
          "static model sidecar rejects stale metadata");
    WriteTextFile(path, "truncated");
    Check(!game::ReadSectorStaticModelLightmapSidecar(
                   path.string(),
                   &metadata,
                   loaded,
                   error),
          "static model sidecar rejects truncated data");
}

void TestStaticModelFingerprintRefreshAndHashInputs()
{
    const std::filesystem::path root =
            Phase01bSandboxDir() / "static_model_fingerprint";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const std::filesystem::path bufferPath = root / "mesh.bin";
    const std::filesystem::path modelPath = root / "mesh.gltf";
    WriteTextFile(bufferPath, "abc");
    WriteTextFile(
            modelPath,
            R"({"asset":{"version":"2.0"},"buffers":[{"uri":"mesh.bin","byteLength":3}],"bufferViews":[],"accessors":[],"meshes":[]})");

    game::SectorTopologyMap map = MakeSquare();
    game::SectorPlacedRuntimeObject object;
    object.id = 91;
    object.kind = "static_model";
    object.position = Vector3{16.0f, 0.0f, 16.0f};
    object.staticModel.modelPath = modelPath.string();
    map.runtimeObjects.push_back(object);
    std::string error;
    Check(game::RefreshSectorStaticModelGeometryFingerprints(map, error),
          "glTF geometry fingerprint refresh reads referenced buffers");
    const std::string fingerprint =
            map.runtimeObjects[0].staticModel.geometryFingerprint;
    const std::string hash = game::ComputeSectorLightmapSourceHash(map);
    WriteTextFile(bufferPath, "abcd");
    Check(game::RefreshSectorStaticModelGeometryFingerprints(map, error)
                  && map.runtimeObjects[0].staticModel.geometryFingerprint
                          != fingerprint
                  && game::ComputeSectorLightmapSourceHash(map) != hash,
          "referenced glTF buffer changes refresh the geometry fingerprint and source hash");

    const std::string geometryHash =
            game::ComputeSectorLightmapSourceHash(map);
    map.runtimeObjects[0].staticModel.geometryFingerprint = "manual-change";
    Check(game::ComputeSectorLightmapSourceHash(map) != geometryHash,
          "static model geometry fingerprint participates in the source hash");

    game::SectorTopologyMap empty = MakeSquare();
    game::SectorPlacedRuntimeObject unassigned = object;
    unassigned.staticModel.modelPath.clear();
    unassigned.staticModel.geometryFingerprint.clear();
    empty.runtimeObjects.push_back(unassigned);
    const std::string emptyHash =
            game::ComputeSectorLightmapSourceHash(MakeSquare());
    Check(game::ComputeSectorLightmapSourceHash(empty) == emptyHash,
          "empty static model assignments remain excluded from the source hash");
    std::filesystem::remove_all(root);
}

void TestStaticModelReceivesAndCastsBakedLighting()
{
    game::SectorTopologyMap map = MakeSquare();
    for (game::SectorTopologyVertex& vertex : map.vertices) {
        vertex.x *= 16;
        vertex.y *= 16;
    }
    map.staticLights.clear();
    map.staticLights.push_back(game::SectorTopologyStaticPointLight{
            501,
            Vector3{32.0f, game::SectorWorldToAuthoringDistance(3.0f), 32.0f},
            WHITE,
            3.0f,
            game::SectorWorldToAuthoringDistance(8.0f),
            0.0f});
    map.lightmapSettings.indirectBounceStrength = 0.0f;
    game::SectorPlacedRuntimeObject object;
    object.id = 77;
    object.kind = "static_model";
    object.position = Vector3{32.0f, 0.0f, 32.0f};
    object.staticModel.modelPath = "assets/models/test_fixture.gltf";
    object.staticModel.geometryFingerprint = "fixture-geometry";
    map.runtimeObjects.push_back(object);

    game::SectorStaticModelLightmapData staticModels;
    game::SectorStaticModelLightmapModel model;
    model.modelPath = object.staticModel.modelPath;
    model.geometryFingerprint = object.staticModel.geometryFingerprint;
    game::SectorStaticModelLightmapMesh mesh;
    mesh.originalVertexCount = 4;
    mesh.preservesAuthoredUv2 = true;
    mesh.usableWidth = 16;
    mesh.usableHeight = 16;
    mesh.sourceVertexIndices = {0, 1, 2, 3};
    mesh.importedPositions = {
            Vector3{-1.0f, 1.0f, -1.0f},
            Vector3{ 1.0f, 1.0f, -1.0f},
            Vector3{ 1.0f, 1.0f,  1.0f},
            Vector3{-1.0f, 1.0f,  1.0f}};
    mesh.importedNormals = {
            Vector3{0.0f, 1.0f, 0.0f},
            Vector3{0.0f, 1.0f, 0.0f},
            Vector3{0.0f, 1.0f, 0.0f},
            Vector3{0.0f, 1.0f, 0.0f}};
    mesh.localLightmapUvs = {
            Vector2{0.0f, 0.0f},
            Vector2{1.0f, 0.0f},
            Vector2{1.0f, 1.0f},
            Vector2{0.0f, 1.0f}};
    mesh.indices = {0, 2, 1, 0, 3, 2};
    model.meshes.push_back(mesh);
    staticModels.models.push_back(model);
    game::SectorStaticModelLightmapObject preparedObject;
    preparedObject.objectId = object.id;
    preparedObject.modelIndex = 0;
    preparedObject.containingSectorId = 10;
    preparedObject.worldPosition = Vector3{4.0f, 0.0f, 4.0f};
    preparedObject.meshPlacements.resize(1);
    staticModels.objects.push_back(preparedObject);

    const std::filesystem::path propPath =
            Phase01bSandboxDir() / "static_model_integrated.lightmap.png";
    game::SectorTopologyLightmapBakeInput input;
    input.mapSnapshot = map;
    input.staticModels = staticModels;
    input.expectedSourceHash = game::ComputeSectorLightmapSourceHash(map);
    input.temporaryOutputPath = propPath.string();
    game::SectorLightmapBakeResult propResult;
    std::string error;
    Check(game::BakeSectorLightmap(
                  input,
                  {},
                  propResult,
                  error),
          "prepared static model bakes through the integrated atlas and BVH path");
    Check(propResult.staticModels.objectCount == 1
                  && propResult.staticModels.modelCount == 1
                  && propResult.staticGeometryTriangles > 12,
          "integrated static model bake reports sidecar metadata and prop triangles");

    game::SectorStaticModelLightmapData installedData;
    Check(game::ReadSectorStaticModelLightmapSidecar(
                  propResult.staticModels.path,
                  &propResult.staticModels,
                  installedData,
                  error),
          "integrated static model bake writes readable runtime remap metadata");
    Image propImage = LoadImage(propPath.string().c_str());
    Check(propImage.data != nullptr,
          "integrated static model lightmap image loads");
    if (propImage.data != nullptr
            && !installedData.objects.empty()
            && !installedData.objects[0].meshPlacements.empty()) {
        const auto& placement =
                installedData.objects[0].meshPlacements[0];
        const int x = static_cast<int>(std::floor(
                (placement.atlasBias.x + placement.atlasScale.x * 0.5f)
                * static_cast<float>(propImage.width)));
        const int y = static_cast<int>(std::floor(
                (placement.atlasBias.y + placement.atlasScale.y * 0.5f)
                * static_cast<float>(propImage.height)));
        const Color sample = GetImageColor(propImage, x, y);
        Check(sample.r + sample.g + sample.b > 30,
              "static model texels receive baked static direct lighting");
    }
    if (propImage.data != nullptr) {
        UnloadImage(propImage);
    }

    game::SectorGeneratedGeometry geometry;
    game::SectorLightmapLayout layout;
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error)
                  && game::BuildSectorLightmapLayout(map, layout, error),
          "integrated prop shadow fixture builds stable topology layout");
    int floorSurfaceIndex = -1;
    for (size_t i = 0; i < geometry.surfaces.size(); ++i) {
        if (geometry.surfaces[i].ref.kind
                == game::SectorGeneratedSurfaceKind::Floor) {
            floorSurfaceIndex = static_cast<int>(i);
            break;
        }
    }
    Color propFloorSample = {};
    if (floorSurfaceIndex >= 0
            && floorSurfaceIndex < static_cast<int>(layout.charts.size())) {
        const game::SectorLightmapChart& chart =
                layout.charts[static_cast<size_t>(floorSurfaceIndex)];
        Image baked = LoadImage(propPath.string().c_str());
        propFloorSample = GetImageColor(
                baked,
                chart.usableX + chart.usableWidth / 2,
                chart.usableY + chart.usableHeight / 2);
        UnloadImage(baked);
    }

    game::SectorTopologyMap baselineMap = map;
    baselineMap.runtimeObjects.clear();
    const std::filesystem::path baselinePath =
            Phase01bSandboxDir() / "static_model_baseline.lightmap.png";
    game::SectorLightmapBakeResult baselineResult;
    Check(game::BakeSectorLightmap(
                  baselineMap,
                  layout,
                  baselinePath.string().c_str(),
                  baselineResult,
                  error),
          "static model shadow comparison baseline bakes");
    if (floorSurfaceIndex >= 0
            && floorSurfaceIndex < static_cast<int>(layout.charts.size())) {
        const game::SectorLightmapChart& chart =
                layout.charts[static_cast<size_t>(floorSurfaceIndex)];
        Image baked = LoadImage(baselinePath.string().c_str());
        const Color baselineFloorSample = GetImageColor(
                baked,
                chart.usableX + chart.usableWidth / 2,
                chart.usableY + chart.usableHeight / 2);
        UnloadImage(baked);
        Check(baselineFloorSample.r + baselineFloorSample.g
                          + baselineFloorSample.b
                      > propFloorSample.r + propFloorSample.g
                              + propFloorSample.b + 20,
              "opaque double-sided static model triangles cast baked shadows onto sector floors");
    }
}

} // namespace

int main()
{
    TestLightmapBakeReportFormatting();
    TestSectorAssetPathHelpers();
    TestLightmapBakeInstallBoundaryRejectsStaleAndCleansTemps();
    TestLightmapBakeInstallBoundaryMissingTempsCleanUp();
    TestLightmapBakeInstallBoundaryCopyFailureCleanup();
    TestLightmapBakeInstallBoundarySuccessfulPayload();
    TestLightmapBakeInstallBoundaryHandlesMultipleAtlases();
    TestLightmapBakeInstallBoundaryStaticModelSidecarIsAtomic();
    TestObjectLightProbeSidecarRoundTrip();
    TestObjectLightProbeSidecarRejectsInvalidFiles();
    TestObjectLightProbeRuntimeDataLoadsAndBuildsSectorRanges();
    TestObjectLightProbeRuntimeDataRejectsUnavailableInputs();
    TestObjectLightProbeSamplingBlendsHorizontalLayersByWorldHeight();
    TestObjectLightProbeSamplingInterpolatesAndPrefersSector();
    TestObjectLightProbeSamplingAdjacentPortalBlending();
    TestObjectLightProbeSamplingAdjacentSectorDeduplicatesSplitPortal();
    TestObjectLightProbeSamplingAdjacentSectorCapAndPreferredDeduplication();
    TestObjectLightProbeSamplingDoesNotBlendThroughClosedOrUnavailableAdjacency();
    TestObjectLightProbeSamplingKeepsAllProbeFallbackWithoutPreferredProbes();
    TestObjectLightProbeSamplingFallbacksAndFiniteOutput();
    TestObjectAmbientCubeNormalBlending();
    TestLocalFogEffectivePathLengthSaturatesGrazingTraversal();
    TestLocalFogProbeLightingReductionInterpolationAndFallback();
    TestLightAtmosphereVolumeShapesAndProbeFallback();
    TestObjectLightProbeBakeWritesSidecarAndStats();
    TestObjectLightProbeBakeCancellationDoesNotMarkValid();
    TestObjectLightProbePlacementGridCounts();
    TestObjectLightProbePlacementRejectsConcaveVoid();
    TestObjectLightProbePlacementRejectsHoles();
    TestObjectLightProbePlacementFallbackAndLowCeiling();
    TestObjectLightProbePlacementConfiguredSingleLayerAndValidation();
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
    TestTopologyLayoutRollsIntoAdditionalAtlases();
    TestSmallSyntheticMultiAtlasBake();
    TestMiddleSurfacesReceiveLightmapsWithoutOccluding();
    TestAlphaTestMiddleOccluderCollection();
    TestAlphaMaskCacheSampling();
    TestAlphaAwareStaticRayOcclusion();
    TestAlphaAwareStaticLightBakePaths();
    TestDirectionalLightBakeBehavior();
    TestGeneratedSurfaceNormalMapConventionAndBakedDirectLighting();
    TestStaticSpotlightBakeBehavior();
    TestStaticModelUvPreparationAndImportedTransforms();
    TestStaticModelPreparationReusesReadyEditorModels();
    TestStaticModelChartPackingAndSidecarLifecycle();
    TestStaticModelFingerprintRefreshAndHashInputs();
    TestStaticModelReceivesAndCastsBakedLighting();

    if (failures != 0) {
        std::fprintf(stderr, "%d sector topology lightmap test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
