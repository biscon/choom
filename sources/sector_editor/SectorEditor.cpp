#include "sector_editor/SectorEditor.h"

#include "engine/assets/TextureLoadFlags.h"
#include "engine/input/InputEvents.h"
#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorTextureTypes.h"
#include "sector_demo/SectorTopologyCreation.h"
#include "sector_demo/SectorTopologyGeometry.h"
#include "sector_demo/SectorTopologySerialization.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"
#include "util/earcut.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

namespace game {

namespace {

constexpr float EditorWidth = 1920.0f;
constexpr float EditorHeight = 1080.0f;
constexpr float LeftPanelWidth = 360.0f;
constexpr float RightPanelWidth = 384.0f;
constexpr float BottomPanelHeight = 78.0f;
constexpr float PanelGap = 10.0f;
constexpr float TopologyUvScaleMin = 0.001f;
constexpr float TopologyUvScaleMax = 64.0f;
constexpr float MinZoom = 8.0f;
constexpr float MaxZoom = 256.0f;
constexpr float PanPixelsPerSecond = 720.0f;
constexpr float GeometryEpsilon = 0.001f;
constexpr float ScreenVertexSnapPixels = 10.0f;
constexpr float ScreenEdgePickPixels = 10.0f;
constexpr float ScreenLightPickPixels = 12.0f;
constexpr float PreviewHighlightLift = 0.006f;

struct LevelPaths {
    std::string jsonAssetPath;
    std::string lightmapAssetPath;
    std::filesystem::path jsonFilePath;
    std::filesystem::path lightmapFilePath;
    std::filesystem::path directoryPath;
};

bool IsValidLevelName(const std::string& name, std::string& error)
{
    if (name.empty()) {
        error = "Level name cannot be empty";
        return false;
    }
    for (char ch : name) {
        const bool asciiLetter = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
        const bool asciiDigit = ch >= '0' && ch <= '9';
        if (!(asciiLetter || asciiDigit || ch == '_' || ch == '-')) {
            error = "Use only letters, digits, underscore, or dash";
            return false;
        }
    }
    error.clear();
    return true;
}

bool BuildLevelPaths(const std::string& name, LevelPaths& paths, std::string& error)
{
    if (!IsValidLevelName(name, error)) {
        return false;
    }

    const std::filesystem::path relativeDirectory = std::filesystem::path("levels") / name;
    paths.directoryPath = std::filesystem::path(ASSETS_PATH) / relativeDirectory;
    paths.jsonFilePath = paths.directoryPath / (name + ".json");
    paths.lightmapFilePath = paths.directoryPath / (name + ".lightmap.png");
    paths.jsonAssetPath = (std::filesystem::path("assets") / relativeDirectory / (name + ".json")).generic_string();
    paths.lightmapAssetPath = (std::filesystem::path("assets") / relativeDirectory / (name + ".lightmap.png")).generic_string();
    error.clear();
    return true;
}

template<typename TextureMap>
void PopulateDefaultSectorTextures(TextureMap& texturesById)
{
    const auto addTexture = [&texturesById](const char* id, const char* path) {
        SectorTextureDefinition definition;
        definition.id = id;
        definition.path = path;
        definition.filter = SectorTextureFilter::Anisotropic8x;
        texturesById.emplace(id, std::move(definition));
    };
    addTexture("wall", "assets/images/wall.png");
    addTexture("floor", "assets/images/floor.png");
    addTexture("ceiling", "assets/images/ceiling.png");
    addTexture("step_wall", "assets/images/wall.png");
    addTexture("upper_wall", "assets/images/wall.png");
}

SectorTopologyMap CreateEmptySectorTopologyDocument()
{
    SectorTopologyMap map;
    PopulateDefaultSectorTextures(map.texturesById);
    return map;
}

bool SameBoundaryCutPoint(
        const SectorTopologyBoundaryCutPoint& a,
        const SectorTopologyBoundaryCutPoint& b)
{
    return a.vertexId == b.vertexId
            && a.lineDefId == b.lineDefId
            && a.point.x == b.point.x
            && a.point.y == b.point.y;
}

const SectorTextureDefinition* FindSectorTopologyTexture(
        const SectorTopologyMap& map,
        const std::string& id)
{
    if (id.empty()) {
        return nullptr;
    }

    const auto it = map.texturesById.find(id);
    return it == map.texturesById.end() ? nullptr : &it->second;
}

std::vector<std::string> SortedSectorTopologyTextureIds(const SectorTopologyMap& map)
{
    std::vector<std::string> ids;
    ids.reserve(map.texturesById.size());
    for (const auto& texture : map.texturesById) {
        ids.push_back(texture.first);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

Vector2 SectorTopologyVertexToMap(const SectorTopologyVertex& vertex)
{
    return Vector2{
            SectorCoordToVisibleAuthoring(vertex.x),
            SectorCoordToVisibleAuthoring(vertex.y)
    };
}

SectorPoint SectorTopologyCoordPointToSectorPoint(SectorTopologyCoordPoint point)
{
    return SectorPoint{
            SectorCoordToVisibleAuthoring(point.x),
            SectorCoordToVisibleAuthoring(point.y)
    };
}

bool SameTopologyPoint(SectorTopologyCoordPoint a, SectorTopologyCoordPoint b)
{
    return a.x == b.x && a.y == b.y;
}

void ResetEditorTopologyDocumentState(SectorEditorState& state)
{
    state.topologyMap = CreateEmptySectorTopologyDocument();
    state.topologyDocumentInitialized = true;
    state.topologyDocumentDirty = false;
    state.topologyDocumentStatus = "Topology document: empty";
}

std::vector<LevelListEntry> ScanLevels(std::string& error)
{
    std::vector<LevelListEntry> levels;
    error.clear();
    const std::filesystem::path levelsRoot = std::filesystem::path(ASSETS_PATH) / "levels";
    std::error_code ec;
    std::filesystem::create_directories(levelsRoot, ec);
    if (ec) {
        error = TextFormat("Could not create assets/levels: %s", ec.message().c_str());
        return levels;
    }

    std::filesystem::directory_iterator iterator(
            levelsRoot,
            std::filesystem::directory_options::skip_permission_denied,
            ec
    );
    const std::filesystem::directory_iterator end;
    if (ec) {
        error = TextFormat("Could not scan assets/levels: %s", ec.message().c_str());
        return levels;
    }

    for (; iterator != end; iterator.increment(ec)) {
        if (ec) {
            error = TextFormat("Stopped level scan early: %s", ec.message().c_str());
            break;
        }
        const std::filesystem::directory_entry& entry = *iterator;
        const std::filesystem::file_status status = entry.symlink_status(ec);
        if (ec || !std::filesystem::is_directory(status) || std::filesystem::is_symlink(status)) {
            ec.clear();
            continue;
        }

        const std::string name = entry.path().filename().string();
        std::string validationError;
        LevelPaths paths;
        if (!BuildLevelPaths(name, paths, validationError)) {
            continue;
        }
        const std::filesystem::file_status jsonStatus = std::filesystem::symlink_status(paths.jsonFilePath, ec);
        if (ec || !std::filesystem::is_regular_file(jsonStatus) || std::filesystem::is_symlink(jsonStatus)) {
            ec.clear();
            continue;
        }
        levels.push_back(LevelListEntry{name, paths.jsonAssetPath});
    }

    std::sort(levels.begin(), levels.end(), [](const LevelListEntry& a, const LevelListEntry& b) {
        return a.name < b.name;
    });
    return levels;
}

Vector2 SectorPointToVector2(SectorPoint point)
{
    return Vector2{point.x, point.y};
}

SectorPoint Vector2ToSectorPoint(Vector2 point)
{
    return SectorPoint{point.x, point.y};
}

bool Contains(Rectangle bounds, Vector2 point)
{
    return CheckCollisionPointRec(point, bounds);
}

bool StartsWith(const std::string& value, const char* prefix)
{
    const std::string prefixString(prefix);
    return value.compare(0, prefixString.size(), prefixString) == 0;
}

std::string ResolveEditorAssetPath(const std::string& path)
{
    if (StartsWith(path, "assets/")) {
        return std::string(ASSETS_PATH) + path.substr(7);
    }
    return path;
}

bool HasPngExtension(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    for (char& ch : extension) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return extension == ".png";
}

std::vector<std::string> ScanAssetImagePngs(std::string& message)
{
    std::vector<std::string> paths;
    message.clear();

    const std::filesystem::path assetsRoot = std::filesystem::path(ASSETS_PATH);
    const std::filesystem::path imagesRoot = assetsRoot / "images";
    std::error_code ec;
    if (!std::filesystem::exists(imagesRoot, ec) || !std::filesystem::is_directory(imagesRoot, ec)) {
        message = "assets/images was not found";
        return paths;
    }

    std::filesystem::recursive_directory_iterator it(imagesRoot, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end;
    if (ec) {
        message = TextFormat("Could not scan assets/images: %s", ec.message().c_str());
        return paths;
    }

    for (; it != end; it.increment(ec)) {
        if (ec) {
            message = TextFormat("Stopped scan early: %s", ec.message().c_str());
            break;
        }

        const std::filesystem::directory_entry& entry = *it;
        if (!entry.is_regular_file(ec) || ec || !HasPngExtension(entry.path())) {
            ec.clear();
            continue;
        }

        std::filesystem::path relativePath = std::filesystem::relative(entry.path(), assetsRoot, ec);
        if (ec) {
            ec.clear();
            continue;
        }
        paths.push_back(std::string{"assets/"} + relativePath.generic_string());
    }

    std::sort(paths.begin(), paths.end());
    if (paths.empty() && message.empty()) {
        message = "No PNG files found under assets/images";
    }
    return paths;
}

bool IsValidTextureIdCharacter(char ch)
{
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-';
}

bool IsValidTextureId(const std::string& id)
{
    if (id.empty()) {
        return false;
    }

    for (char ch : id) {
        if (!IsValidTextureIdCharacter(ch)) {
            return false;
        }
    }
    return true;
}

float ClampAmbientIntensity(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float ClampLightIntensity(float value)
{
    return std::clamp(value, 0.0f, 8.0f);
}

float ClampLightRadius(float value)
{
    return std::clamp(value, SectorWorldToAuthoringDistance(0.1f), SectorWorldToAuthoringDistance(64.0f));
}

float ClampLightSourceRadius(float value, float lightRadius)
{
    const float clamped = std::clamp(value, 0.0f, SectorWorldToAuthoringDistance(8.0f));
    return std::min(clamped, std::max(0.0f, lightRadius * 0.5f));
}

float ClampAmbientOcclusionRadius(float value)
{
    return std::clamp(value, SectorWorldToAuthoringDistance(0.05f), SectorWorldToAuthoringDistance(16.0f));
}

float ClampAmbientOcclusionStrength(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float ClampIndirectBounceRadius(float value)
{
    return std::clamp(value, SectorWorldToAuthoringDistance(0.05f), SectorWorldToAuthoringDistance(16.0f));
}

float ClampIndirectBounceStrength(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

int ClampAmbientChannel(int value)
{
    return std::clamp(value, 0, 255);
}

Color TopologySectorAmbientPreviewColor(const SectorTopologySector& sector, unsigned char alpha)
{
    const float intensity = ClampAmbientIntensity(sector.ambientIntensity);
    return Color{
            static_cast<unsigned char>(ClampAmbientChannel(static_cast<int>(std::lround(static_cast<float>(sector.ambientColor.r) * intensity)))),
            static_cast<unsigned char>(ClampAmbientChannel(static_cast<int>(std::lround(static_cast<float>(sector.ambientColor.g) * intensity)))),
            static_cast<unsigned char>(ClampAmbientChannel(static_cast<int>(std::lround(static_cast<float>(sector.ambientColor.b) * intensity)))),
            alpha
    };
}

const char* LightmapBakePhaseText(SectorLightmapBakePhase phase)
{
    switch (phase) {
        case SectorLightmapBakePhase::Idle: return "Waiting...";
        case SectorLightmapBakePhase::Preparing: return "Preparing bake...";
        case SectorLightmapBakePhase::BuildingLayout: return "Building lightmap layout...";
        case SectorLightmapBakePhase::BuildingBvh: return "Building BVH...";
        case SectorLightmapBakePhase::DirectLighting: return "Baking direct lighting...";
        case SectorLightmapBakePhase::AmbientOcclusion: return "Baking ambient occlusion...";
        case SectorLightmapBakePhase::IndirectBounce: return "Baking indirect bounce...";
        case SectorLightmapBakePhase::DilatingAndEncoding: return "Dilating charts and writing lightmap...";
        case SectorLightmapBakePhase::InstallingResult: return "Installing lightmap...";
        case SectorLightmapBakePhase::Completed: return "Lightmap bake complete";
        case SectorLightmapBakePhase::Cancelled: return "Lightmap bake cancelled";
        case SectorLightmapBakePhase::Failed: return "Lightmap bake failed";
    }
    return "Baking lightmap...";
}

float LightmapBakePhaseProgressBase(SectorLightmapBakePhase phase)
{
    switch (phase) {
        case SectorLightmapBakePhase::Idle: return 0.0f;
        case SectorLightmapBakePhase::Preparing: return 0.00f;
        case SectorLightmapBakePhase::BuildingLayout: return 0.01f;
        case SectorLightmapBakePhase::BuildingBvh: return 0.03f;
        case SectorLightmapBakePhase::DirectLighting: return 0.05f;
        case SectorLightmapBakePhase::AmbientOcclusion: return 0.20f;
        case SectorLightmapBakePhase::IndirectBounce: return 0.55f;
        case SectorLightmapBakePhase::DilatingAndEncoding: return 0.90f;
        case SectorLightmapBakePhase::InstallingResult: return 0.98f;
        case SectorLightmapBakePhase::Completed: return 1.0f;
        case SectorLightmapBakePhase::Cancelled: return 1.0f;
        case SectorLightmapBakePhase::Failed: return 1.0f;
    }
    return 0.0f;
}

float LightmapBakePhaseProgressWeight(SectorLightmapBakePhase phase)
{
    switch (phase) {
        case SectorLightmapBakePhase::Preparing: return 0.01f;
        case SectorLightmapBakePhase::BuildingLayout: return 0.02f;
        case SectorLightmapBakePhase::BuildingBvh: return 0.02f;
        case SectorLightmapBakePhase::DirectLighting: return 0.15f;
        case SectorLightmapBakePhase::AmbientOcclusion: return 0.35f;
        case SectorLightmapBakePhase::IndirectBounce: return 0.35f;
        case SectorLightmapBakePhase::DilatingAndEncoding: return 0.10f;
        case SectorLightmapBakePhase::InstallingResult: return 0.02f;
        default: return 0.0f;
    }
}

float LightmapBakeOverallProgress(SectorLightmapBakePhase phase, uint32_t completedWork, uint32_t totalWork)
{
    if (phase == SectorLightmapBakePhase::Completed
            || phase == SectorLightmapBakePhase::Cancelled
            || phase == SectorLightmapBakePhase::Failed) {
        return 1.0f;
    }
    const float local = totalWork > 0
            ? std::clamp(static_cast<float>(completedWork) / static_cast<float>(totalWork), 0.0f, 1.0f)
            : 0.0f;
    return std::clamp(
            LightmapBakePhaseProgressBase(phase) + LightmapBakePhaseProgressWeight(phase) * local,
            0.0f,
            1.0f
    );
}

std::string MakeTemporaryLightmapPath(const std::string& finalOutputPath)
{
    std::filesystem::path path(finalOutputPath);
    path.replace_extension(".tmp.png");
    return path.generic_string();
}

void DeleteFileIfExists(const std::string& path)
{
    if (path.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

std::string GeneratedTextureIdBase(const std::string& assetPath)
{
    std::string stem = std::filesystem::path(assetPath).stem().string();
    std::string id;
    id.reserve(stem.size());
    bool lastWasUnderscore = false;
    for (char ch : stem) {
        const unsigned char value = static_cast<unsigned char>(ch);
        if (std::isalnum(value)) {
            id.push_back(static_cast<char>(std::tolower(value)));
            lastWasUnderscore = false;
        } else if (ch == '-' || ch == '_' || std::isspace(value)) {
            if (!lastWasUnderscore && !id.empty()) {
                id.push_back('_');
                lastWasUnderscore = true;
            }
        }
    }

    while (!id.empty() && id.back() == '_') {
        id.pop_back();
    }
    return id.empty() ? "texture" : id;
}

const char* ToolName(SectorEditorTool tool)
{
    switch (tool) {
        case SectorEditorTool::Select: return "Select";
        case SectorEditorTool::Sector: return "Sector";
        case SectorEditorTool::InsertSectorInside: return "Insert Inside";
        case SectorEditorTool::Light: return "Light";
        case SectorEditorTool::Move: return "Move";
        case SectorEditorTool::Erase: return "Erase";
    }
    return "Unknown";
}

const char* TopologyWallPartName(TopologyWallPart part)
{
    switch (part) {
        case TopologyWallPart::Wall: return "Wall";
        case TopologyWallPart::Lower: return "Lower";
        case TopologyWallPart::Upper: return "Upper";
    }
    return "Wall";
}

const char* TopologyWallPartStatusName(TopologyWallPart part)
{
    switch (part) {
        case TopologyWallPart::Wall: return "wall";
        case TopologyWallPart::Lower: return "lower";
        case TopologyWallPart::Upper: return "upper";
    }
    return "wall";
}

const char* TopologyMaterialLayerName(TopologyMaterialLayer layer)
{
    switch (layer) {
        case TopologyMaterialLayer::Base: return "Base";
        case TopologyMaterialLayer::Decal: return "Decal";
    }
    return "Base";
}

const char* TopologyMaterialLayerStatusName(TopologyMaterialLayer layer)
{
    switch (layer) {
        case TopologyMaterialLayer::Base: return "material";
        case TopologyMaterialLayer::Decal: return "decal";
    }
    return "material";
}

const char* TopologyWallPartSurfaceStatusName(TopologyWallPart part)
{
    switch (part) {
        case TopologyWallPart::Wall: return "wall";
        case TopologyWallPart::Lower: return "lower wall";
        case TopologyWallPart::Upper: return "upper wall";
    }
    return "wall";
}

const char* SurfaceKindName(SectorSurfaceKind kind)
{
    switch (kind) {
        case SectorSurfaceKind::Floor: return "Floor";
        case SectorSurfaceKind::Ceiling: return "Ceiling";
        case SectorSurfaceKind::Wall: return "Wall";
        case SectorSurfaceKind::LowerWall: return "Lower";
        case SectorSurfaceKind::UpperWall: return "Upper";
        case SectorSurfaceKind::None: break;
    }
    return "None";
}

bool IsWallSurface(SectorSurfaceKind kind)
{
    return kind == SectorSurfaceKind::Wall
            || kind == SectorSurfaceKind::LowerWall
            || kind == SectorSurfaceKind::UpperWall;
}

SectorSurfaceKind ToEditorSurfaceKind(SectorGeneratedSurfaceKind kind)
{
    switch (kind) {
        case SectorGeneratedSurfaceKind::Floor: return SectorSurfaceKind::Floor;
        case SectorGeneratedSurfaceKind::Ceiling: return SectorSurfaceKind::Ceiling;
        case SectorGeneratedSurfaceKind::Wall: return SectorSurfaceKind::Wall;
        case SectorGeneratedSurfaceKind::LowerWall: return SectorSurfaceKind::LowerWall;
        case SectorGeneratedSurfaceKind::UpperWall: return SectorSurfaceKind::UpperWall;
    }
    return SectorSurfaceKind::None;
}

TopologyWallPart SurfaceKindToTopologyWallPart(SectorSurfaceKind kind)
{
    switch (kind) {
        case SectorSurfaceKind::LowerWall: return TopologyWallPart::Lower;
        case SectorSurfaceKind::UpperWall: return TopologyWallPart::Upper;
        case SectorSurfaceKind::Wall:
        case SectorSurfaceKind::Floor:
        case SectorSurfaceKind::Ceiling:
        case SectorSurfaceKind::None:
            break;
    }
    return TopologyWallPart::Wall;
}

TopologySurfaceEditTargetKind SurfaceKindToTopologyEditTargetKind(SectorSurfaceKind kind)
{
    switch (kind) {
        case SectorSurfaceKind::Floor: return TopologySurfaceEditTargetKind::SectorFloor;
        case SectorSurfaceKind::Ceiling: return TopologySurfaceEditTargetKind::SectorCeiling;
        case SectorSurfaceKind::Wall: return TopologySurfaceEditTargetKind::SideDefWall;
        case SectorSurfaceKind::LowerWall: return TopologySurfaceEditTargetKind::SideDefLower;
        case SectorSurfaceKind::UpperWall: return TopologySurfaceEditTargetKind::SideDefUpper;
        case SectorSurfaceKind::None: break;
    }
    return TopologySurfaceEditTargetKind::None;
}

TopologyWallPart TopologyEditTargetWallPart(TopologySurfaceEditTargetKind kind)
{
    switch (kind) {
        case TopologySurfaceEditTargetKind::SideDefLower: return TopologyWallPart::Lower;
        case TopologySurfaceEditTargetKind::SideDefUpper: return TopologyWallPart::Upper;
        case TopologySurfaceEditTargetKind::SideDefWall:
        case TopologySurfaceEditTargetKind::SectorFloor:
        case TopologySurfaceEditTargetKind::SectorCeiling:
        case TopologySurfaceEditTargetKind::None:
            break;
    }
    return TopologyWallPart::Wall;
}

TopologySurfaceEditTargetKind TopologyWallPartEditTargetKind(TopologyWallPart part)
{
    switch (part) {
        case TopologyWallPart::Wall: return TopologySurfaceEditTargetKind::SideDefWall;
        case TopologyWallPart::Lower: return TopologySurfaceEditTargetKind::SideDefLower;
        case TopologyWallPart::Upper: return TopologySurfaceEditTargetKind::SideDefUpper;
    }
    return TopologySurfaceEditTargetKind::SideDefWall;
}

const char* TopologyMaterialKindName(TopologySurfaceEditTargetKind kind)
{
    switch (kind) {
        case TopologySurfaceEditTargetKind::SectorFloor: return "floor";
        case TopologySurfaceEditTargetKind::SectorCeiling: return "ceiling";
        case TopologySurfaceEditTargetKind::SideDefWall: return "wall";
        case TopologySurfaceEditTargetKind::SideDefLower: return "lower";
        case TopologySurfaceEditTargetKind::SideDefUpper: return "upper";
        case TopologySurfaceEditTargetKind::None:
            break;
    }
    return "none";
}

bool IsWallTopologyEditTarget(TopologySurfaceEditTargetKind kind)
{
    return kind == TopologySurfaceEditTargetKind::SideDefWall
            || kind == TopologySurfaceEditTargetKind::SideDefLower
            || kind == TopologySurfaceEditTargetKind::SideDefUpper;
}

const char* TopologyUvFitModeStatusName(TopologyUvFitMode mode)
{
    switch (mode) {
        case TopologyUvFitMode::Width: return "width";
        case TopologyUvFitMode::Height: return "height";
        case TopologyUvFitMode::Both: return "width and height";
    }
    return "UV";
}

const char* TopologyUAlignDirectionStatusName(TopologyUAlignDirection direction)
{
    switch (direction) {
        case TopologyUAlignDirection::Previous: return "previous";
        case TopologyUAlignDirection::Next: return "next";
    }
    return "neighbor";
}

void ResetTopologyUv(SectorTopologyUvSettings& uv)
{
    uv.scale = Vector2{1.0f, 1.0f};
    uv.offset = Vector2{0.0f, 0.0f};
}

bool IsDefaultTopologyUv(SectorTopologyUvSettings uv)
{
    return uv.scale.x == 1.0f
            && uv.scale.y == 1.0f
            && uv.offset.x == 0.0f
            && uv.offset.y == 0.0f;
}

bool IsWhiteTint(Vector3 tint)
{
    return tint.x == 1.0f && tint.y == 1.0f && tint.z == 1.0f;
}

bool IsValidDecalTint(Vector3 tint)
{
    return std::isfinite(tint.x) && std::isfinite(tint.y) && std::isfinite(tint.z)
            && tint.x >= 0.0f && tint.x <= 1.0f
            && tint.y >= 0.0f && tint.y <= 1.0f
            && tint.z >= 0.0f && tint.z <= 1.0f;
}

Vector3 ClampDecalTint(Vector3 tint)
{
    return Vector3{
            std::clamp(std::isfinite(tint.x) ? tint.x : 1.0f, 0.0f, 1.0f),
            std::clamp(std::isfinite(tint.y) ? tint.y : 1.0f, 0.0f, 1.0f),
            std::clamp(std::isfinite(tint.z) ? tint.z : 1.0f, 0.0f, 1.0f)
    };
}

bool SameTint(Vector3 a, Vector3 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

Color DecalTintPreviewColor(Vector3 tint)
{
    const Vector3 clamped = ClampDecalTint(tint);
    return Color{
            static_cast<unsigned char>(std::lround(clamped.x * 255.0f)),
            static_cast<unsigned char>(std::lround(clamped.y * 255.0f)),
            static_cast<unsigned char>(std::lround(clamped.z * 255.0f)),
            255
    };
}

bool IsDefaultDecalLayer(const SectorTopologyDecalLayer& decal)
{
    return decal.textureId.empty()
            && IsDefaultTopologyUv(decal.uv)
            && decal.opacity == 1.0f
            && !decal.emissive
            && IsWhiteTint(decal.tint);
}

void ResetDecalLayer(SectorTopologyDecalLayer& decal)
{
    decal.textureId.clear();
    ResetTopologyUv(decal.uv);
    decal.opacity = 1.0f;
    decal.emissive = false;
    decal.tint = Vector3{1.0f, 1.0f, 1.0f};
}

TopologySectorTextureField TopologyEditTargetSectorTextureField(TopologySurfaceEditTargetKind kind)
{
    switch (kind) {
        case TopologySurfaceEditTargetKind::SectorFloor: return TopologySectorTextureField::Floor;
        case TopologySurfaceEditTargetKind::SectorCeiling: return TopologySectorTextureField::Ceiling;
        case TopologySurfaceEditTargetKind::SideDefWall:
        case TopologySurfaceEditTargetKind::SideDefLower:
        case TopologySurfaceEditTargetKind::SideDefUpper:
        case TopologySurfaceEditTargetKind::None:
            break;
    }
    return TopologySectorTextureField::None;
}

SectorSurfaceRef ToEditorSurfaceRef(const SectorGeneratedSurfaceRef& ref)
{
    SectorSurfaceRef surface;
    surface.kind = ToEditorSurfaceKind(ref.kind);
    surface.topologySectorId = ref.topologySectorId;
    surface.topologyLineDefId = ref.topologyLineDefId;
    surface.topologySideDefId = ref.topologySideDefId;
    surface.topologySide = ref.topologySide;
    return surface;
}

Vector3 SectorPointToWorld(SectorPoint point, float height)
{
    return SectorAuthoringToWorldPosition(point.x, height, point.y);
}

const char* ToolHelpText(SectorEditorTool tool)
{
    switch (tool) {
        case SectorEditorTool::Select: return "Select: click lights, linedefs, or sectors in canvas";
        case SectorEditorTool::Sector: return "Sector: left click add, click first closes, right click/Esc cancels, Backspace removes";
        case SectorEditorTool::InsertSectorInside: return "Insert sector: draw inside selected parent; Enter closes, right click/Esc cancels";
        case SectorEditorTool::Light: return "Light: click inside a sector to place a baked static point light";
        case SectorEditorTool::Move: return "Move: drag topology light or vertex";
        case SectorEditorTool::Erase: return "Erase: click sector to delete";
    }
    return "";
}

const char* TopologySectorTextureFieldLabel(TopologySectorTextureField field)
{
    switch (field) {
        case TopologySectorTextureField::Floor: return "floor texture";
        case TopologySectorTextureField::Ceiling: return "ceiling texture";
        case TopologySectorTextureField::DefaultWall: return "default wall texture";
        case TopologySectorTextureField::DefaultLower: return "default lower texture";
        case TopologySectorTextureField::DefaultUpper: return "default upper texture";
        case TopologySectorTextureField::None: break;
    }
    return "texture";
}

const char* TopologyPickerTargetLabel(const TexturePickerState& picker)
{
    if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::SideDef) {
        return picker.topologyLayer == TopologyMaterialLayer::Decal
                ? TextFormat("%s sidedef decal texture", TopologyWallPartStatusName(picker.topologyWallPart))
                : TextFormat("%s sidedef texture", TopologyWallPartStatusName(picker.topologyWallPart));
    }
    if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
        switch (picker.topologyField) {
            case TopologySectorTextureField::Floor: return "floor decal texture";
            case TopologySectorTextureField::Ceiling: return "ceiling decal texture";
            case TopologySectorTextureField::DefaultWall:
            case TopologySectorTextureField::DefaultLower:
            case TopologySectorTextureField::DefaultUpper:
            case TopologySectorTextureField::None:
                break;
        }
    }
    return TopologySectorTextureFieldLabel(picker.topologyField);
}

Color WithAlpha(Color color, unsigned char alpha)
{
    color.a = alpha;
    return color;
}

float Cross(Vector2 a, Vector2 b, Vector2 c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

float Cross(SectorPoint a, SectorPoint b, SectorPoint c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

float DistancePointToSegment(Vector2 point, Vector2 a, Vector2 b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float length2 = dx * dx + dy * dy;
    if (length2 <= 0.000001f) {
        const float px = point.x - a.x;
        const float py = point.y - a.y;
        return std::sqrt(px * px + py * py);
    }

    const float t = std::clamp(((point.x - a.x) * dx + (point.y - a.y) * dy) / length2, 0.0f, 1.0f);
    const Vector2 closest{a.x + dx * t, a.y + dy * t};
    const float px = point.x - closest.x;
    const float py = point.y - closest.y;
    return std::sqrt(px * px + py * py);
}

SectorTopologyWallPartSettings& TopologyWallPartSettingsFor(
        SectorTopologySideDef& sideDef,
        TopologyWallPart part)
{
    switch (part) {
        case TopologyWallPart::Wall: return sideDef.wall;
        case TopologyWallPart::Lower: return sideDef.lower;
        case TopologyWallPart::Upper: return sideDef.upper;
    }
    return sideDef.wall;
}

const SectorTopologyWallPartSettings& TopologyWallPartSettingsFor(
        const SectorTopologySideDef& sideDef,
        TopologyWallPart part)
{
    switch (part) {
        case TopologyWallPart::Wall: return sideDef.wall;
        case TopologyWallPart::Lower: return sideDef.lower;
        case TopologyWallPart::Upper: return sideDef.upper;
    }
    return sideDef.wall;
}

bool SectorTopologyWallLengthWorld(
        const SectorTopologyMap& map,
        const SectorTopologyLineDef& lineDef,
        float& outLengthWorld)
{
    outLengthWorld = 0.0f;
    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (!GetSectorTopologyLineVertices(map, lineDef, start, end)
            || start == nullptr || end == nullptr) {
        return false;
    }

    const double dx = static_cast<double>(end->x) - static_cast<double>(start->x);
    const double dy = static_cast<double>(end->y) - static_cast<double>(start->y);
    const double coordLength = std::sqrt(dx * dx + dy * dy);
    outLengthWorld = SectorCoordDistanceToWorldDistance(coordLength);
    return outLengthWorld > 0.0f && std::isfinite(outLengthWorld);
}

bool FindSectorLoopEdgeForSideDef(
        const SectorTopologyLoop& loop,
        int sideDefId,
        const SectorTopologyLoop*& outLoop,
        size_t& outEdgeIndex)
{
    bool found = false;
    for (size_t i = 0; i < loop.edges.size(); ++i) {
        if (loop.edges[i].sideDefId != sideDefId) {
            continue;
        }
        if (found) {
            outLoop = nullptr;
            outEdgeIndex = 0;
            return false;
        }
        found = true;
        outLoop = &loop;
        outEdgeIndex = i;
    }
    return found;
}

bool FindUniqueSectorLoopEdgeForSideDef(
        const SectorTopologyLoopSet& loops,
        int sideDefId,
        const SectorTopologyLoop*& outLoop,
        size_t& outEdgeIndex)
{
    outLoop = nullptr;
    outEdgeIndex = 0;
    int foundCount = 0;

    const auto visitLoop = [&](const SectorTopologyLoop& loop) {
        const SectorTopologyLoop* foundLoop = nullptr;
        size_t foundIndex = 0;
        if (FindSectorLoopEdgeForSideDef(loop, sideDefId, foundLoop, foundIndex)) {
            ++foundCount;
            outLoop = foundLoop;
            outEdgeIndex = foundIndex;
        }
    };

    visitLoop(loops.outer);
    for (const SectorTopologyLoop& hole : loops.holes) {
        visitLoop(hole);
    }
    return foundCount == 1 && outLoop != nullptr;
}

bool TopologyWallPartHasVisibleSpan(
        const SectorTopologyMap& map,
        const SectorTopologySideDef& sideDef,
        TopologyWallPart wallPart)
{
    const SectorTopologySector* sector = FindSectorTopologySector(map, sideDef.sectorId);
    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(map, sideDef.lineDefId);
    if (sector == nullptr || lineDef == nullptr) {
        return false;
    }

    const int oppositeSideDefId = sideDef.side == SectorTopologySideKind::Front
            ? lineDef->backSideDefId
            : lineDef->frontSideDefId;
    const SectorTopologySideDef* opposite = FindOppositeSectorTopologySideDef(map, sideDef.id);
    if (oppositeSideDefId != -1 && opposite == nullptr) {
        return false;
    }

    const SectorTopologySector* oppositeSector = nullptr;
    if (opposite != nullptr) {
        oppositeSector = FindSectorTopologySector(map, opposite->sectorId);
        if (oppositeSector == nullptr) {
            return false;
        }
    }

    switch (wallPart) {
        case TopologyWallPart::Wall:
            return opposite == nullptr
                    && sector->ceilingZ > sector->floorZ
                    && std::isfinite(sector->floorZ)
                    && std::isfinite(sector->ceilingZ);
        case TopologyWallPart::Lower:
            return oppositeSector != nullptr
                    && oppositeSector->floorZ > sector->floorZ
                    && std::isfinite(sector->floorZ)
                    && std::isfinite(oppositeSector->floorZ);
        case TopologyWallPart::Upper:
            return oppositeSector != nullptr
                    && sector->ceilingZ > oppositeSector->ceilingZ
                    && std::isfinite(sector->ceilingZ)
                    && std::isfinite(oppositeSector->ceilingZ);
    }
    return false;
}

const SectorTopologyLoopEdge* FindVisibleTopologyWallPartNeighborEdge(
        const SectorTopologyMap& map,
        const SectorTopologyLoop& loop,
        size_t selectedEdgeIndex,
        int sectorId,
        TopologyUAlignDirection direction,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer,
        std::string& outError)
{
    outError.clear();
    const size_t edgeCount = loop.edges.size();
    if (edgeCount < 2 || selectedEdgeIndex >= edgeCount) {
        return nullptr;
    }

    for (size_t step = 1; step < edgeCount; ++step) {
        const size_t edgeIndex = direction == TopologyUAlignDirection::Previous
                ? (selectedEdgeIndex + edgeCount - step) % edgeCount
                : (selectedEdgeIndex + step) % edgeCount;
        const SectorTopologyLoopEdge& edge = loop.edges[edgeIndex];
        const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, edge.sideDefId);
        if (sideDef == nullptr
                || sideDef->lineDefId != edge.lineDefId
                || sideDef->sectorId != sectorId
                || sideDef->side != edge.side) {
            outError = TextFormat(
                    "%s sidedef is no longer valid.",
                    direction == TopologyUAlignDirection::Previous ? "Previous" : "Next");
            return nullptr;
        }
        const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(map, edge.lineDefId);
        if (lineDef == nullptr) {
            outError = TextFormat(
                    "%s sidedef references a missing linedef.",
                    direction == TopologyUAlignDirection::Previous ? "Previous" : "Next");
            return nullptr;
        }
        const int expectedSideDefId = edge.side == SectorTopologySideKind::Front
                ? lineDef->frontSideDefId
                : lineDef->backSideDefId;
        if (expectedSideDefId != sideDef->id) {
            outError = TextFormat(
                    "%s sidedef is no longer valid.",
                    direction == TopologyUAlignDirection::Previous ? "Previous" : "Next");
            return nullptr;
        }
        if (!TopologyWallPartHasVisibleSpan(map, *sideDef, wallPart)) {
            continue;
        }
        if (layer == TopologyMaterialLayer::Decal
                && TopologyWallPartSettingsFor(*sideDef, wallPart).decal.textureId.empty()) {
            continue;
        }
        return &edge;
    }

    return nullptr;
}

float PolygonArea2(const std::vector<SectorPoint>& points)
{
    float area2 = 0.0f;
    for (size_t i = 0; i < points.size(); ++i) {
        const SectorPoint a = points[i];
        const SectorPoint b = points[(i + 1) % points.size()];
        area2 += a.x * b.y - a.y * b.x;
    }
    return area2;
}

bool SamePoint(SectorPoint a, SectorPoint b)
{
    return std::fabs(a.x - b.x) <= GeometryEpsilon && std::fabs(a.y - b.y) <= GeometryEpsilon;
}

bool PointOnSegment(SectorPoint point, SectorPoint a, SectorPoint b)
{
    if (std::fabs(Cross(a, b, point)) > GeometryEpsilon) {
        return false;
    }

    return point.x >= std::fmin(a.x, b.x) - GeometryEpsilon
            && point.x <= std::fmax(a.x, b.x) + GeometryEpsilon
            && point.y >= std::fmin(a.y, b.y) - GeometryEpsilon
            && point.y <= std::fmax(a.y, b.y) + GeometryEpsilon;
}

int Orientation(SectorPoint a, SectorPoint b, SectorPoint c)
{
    const float cross = Cross(a, b, c);
    if (cross > GeometryEpsilon) {
        return 1;
    }
    if (cross < -GeometryEpsilon) {
        return -1;
    }
    return 0;
}

bool SegmentsIntersect(SectorPoint a0, SectorPoint a1, SectorPoint b0, SectorPoint b1)
{
    const int o1 = Orientation(a0, a1, b0);
    const int o2 = Orientation(a0, a1, b1);
    const int o3 = Orientation(b0, b1, a0);
    const int o4 = Orientation(b0, b1, a1);

    if (o1 != o2 && o3 != o4) {
        return true;
    }

    return (o1 == 0 && PointOnSegment(b0, a0, a1))
            || (o2 == 0 && PointOnSegment(b1, a0, a1))
            || (o3 == 0 && PointOnSegment(a0, b0, b1))
            || (o4 == 0 && PointOnSegment(a1, b0, b1));
}

bool ProperSegmentsCross(SectorPoint a0, SectorPoint a1, SectorPoint b0, SectorPoint b1)
{
    const int o1 = Orientation(a0, a1, b0);
    const int o2 = Orientation(a0, a1, b1);
    const int o3 = Orientation(b0, b1, a0);
    const int o4 = Orientation(b0, b1, a1);
    return o1 != 0 && o2 != 0 && o3 != 0 && o4 != 0 && o1 != o2 && o3 != o4;
}

bool CollinearSegmentsOverlap(SectorPoint a0, SectorPoint a1, SectorPoint b0, SectorPoint b1)
{
    if (Orientation(a0, a1, b0) != 0 || Orientation(a0, a1, b1) != 0) {
        return false;
    }

    const bool useX = std::fabs(a0.x - a1.x) >= std::fabs(a0.y - a1.y);
    const float aMin = std::min(useX ? a0.x : a0.y, useX ? a1.x : a1.y);
    const float aMax = std::max(useX ? a0.x : a0.y, useX ? a1.x : a1.y);
    const float bMin = std::min(useX ? b0.x : b0.y, useX ? b1.x : b1.y);
    const float bMax = std::max(useX ? b0.x : b0.y, useX ? b1.x : b1.y);
    return std::max(aMin, bMin) < std::min(aMax, bMax) - GeometryEpsilon;
}

bool SameUndirectedSegment(SectorPoint a0, SectorPoint a1, SectorPoint b0, SectorPoint b1)
{
    return (SamePoint(a0, b0) && SamePoint(a1, b1))
            || (SamePoint(a0, b1) && SamePoint(a1, b0));
}

bool EdgesAreAdjacent(size_t edgeA, size_t edgeB, size_t edgeCount)
{
    if (edgeA == edgeB) {
        return true;
    }

    return (edgeA + 1) % edgeCount == edgeB || (edgeB + 1) % edgeCount == edgeA;
}

bool PointOnPolygonBoundary(SectorPoint point, const std::vector<SectorPoint>& polygon)
{
    for (size_t i = 0; i < polygon.size(); ++i) {
        if (PointOnSegment(point, polygon[i], polygon[(i + 1) % polygon.size()])) {
            return true;
        }
    }
    return false;
}

bool StrictPointInPolygon(SectorPoint point, const std::vector<SectorPoint>& polygon)
{
    if (polygon.size() < 3 || PointOnPolygonBoundary(point, polygon)) {
        return false;
    }

    bool inside = false;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const SectorPoint a = polygon[i];
        const SectorPoint b = polygon[j];
        const bool intersects = ((a.y > point.y) != (b.y > point.y))
                && (point.x < (b.x - a.x) * (point.y - a.y) / ((b.y - a.y) == 0.0f ? 0.00001f : (b.y - a.y)) + a.x);
        if (intersects) {
            inside = !inside;
        }
    }
    return inside;
}

bool FileExists(const std::string& path)
{
    std::ifstream file(path);
    return static_cast<bool>(file);
}

std::string ShortPath(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

} // namespace

bool SectorEditor::Init(engine::AssetManager& assets)
{
    Shutdown(assets);
    ResetToBlankMap(assets);
    return true;
}

void SectorEditor::Shutdown(engine::AssetManager& assets)
{
    ShutdownLightmapBake();
    preview.Shutdown(assets);
    if (!engine::IsNull(state.editorTextureScope)) {
        assets.UnloadScope(state.editorTextureScope);
    }
    if (!engine::IsNull(state.addMapTexture.previewScope)) {
        assets.UnloadScope(state.addMapTexture.previewScope);
    }
    state = SectorEditorState{};
    uiState = SectorEditorUiState{};
    canvasRect = {};
    statusText.clear();
    initialized = false;
}

void SectorEditor::Update(engine::Input& input, float dt)
{
    if (IsLightmapBakeBlocking()) {
        CancelVertexDrag(nullptr);
        CancelLightDrag(nullptr);
        CancelPendingSector(nullptr);
        return;
    }

    if (state.mode == SectorEditorMode::Preview3D) {
        if (state.texturePicker.open || HasDocumentModalOpen()) {
            return;
        }
        UpdatePreview3D(input, dt);
        return;
    }

    if (state.pendingSector.active
            && state.pendingSector.kind == PendingSectorDrawKind::InsertInside
            && !IsPendingInsertParentValid()) {
        CancelPendingSector("Insert sector cancelled: parent sector changed");
    }

    canvasRect = BuildCanvasRect();
    if (state.texturePicker.open || state.addMapTexture.open || HasDocumentModalOpen()) {
        return;
    }
    UpdateHoverAndMouse(input);
    HandleCanvasInput(input, dt);
}

void SectorEditor::Render(engine::AssetManager& assets)
{
    if (state.mode == SectorEditorMode::Preview3D) {
        RenderPreview3D(assets);
        return;
    }

    canvasRect = BuildCanvasRect();
    DrawRectangleRec(canvasRect, Color{12, 15, 20, 255});

    BeginScissorMode(
            static_cast<int>(std::round(canvasRect.x)),
            static_cast<int>(std::round(canvasRect.y)),
            static_cast<int>(std::round(canvasRect.width)),
            static_cast<int>(std::round(canvasRect.height))
    );

    if (state.showGrid) {
        DrawGrid();
    }
    DrawTopologyDocument();
    EndScissorMode();

    DrawRectangleLinesEx(canvasRect, 2.0f, Color{67, 76, 93, 255});
}

void SectorEditor::RenderUI(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    PollLightmapBakeResult(assets);

    if (state.mode == SectorEditorMode::Preview3D) {
        engine::BeginUI(ui, input);
        if (IsLightmapBakeBlocking()) {
            DrawLightmapBakeModal(ui, config, input, assets, font);
            uiState.keyboardCaptured = true;
            engine::EndUI(ui, config, input, assets);
            return;
        }
        if (state.previewUiHidden) {
            ui.hotId = 0;
            ui.activeId = 0;
            ui.focusedId = 0;
            ui.openOptionId = 0;
        } else {
            DrawPreviewOverlay(config, assets, font);
        }
        if (!state.previewUiHidden && !state.texturePicker.open && !state.decalTintModal.open) {
            DrawPreviewUvPanel(ui, config, input, assets, font);
        }
        if (state.decalTintModal.open) {
            DrawDecalTintModal(ui, config, input, assets, font);
        }
        DrawTexturePickerModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = ui.focusedId != 0;
        if (state.texturePicker.open || state.decalTintModal.open) {
            uiState.keyboardCaptured = true;
        }
        engine::EndUI(ui, config, input, assets);
        return;
    }

    engine::BeginUI(ui, input);
    if (IsLightmapBakeBlocking()) {
        DrawLightmapBakeModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.confirmationModal.open) {
        DrawConfirmationModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.saveLevelModal.open) {
        DrawSaveLevelModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.loadLevelModal.open) {
        DrawLoadLevelModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.decalTintModal.open) {
        DrawDecalTintModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.addMapTexture.open) {
        DrawAddMapTextureModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.texturePicker.open) {
        DrawTexturePickerModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }

    DrawToolsPanel(ui, config, input, assets, font);
    DrawSectorsPanel(ui, config, input, assets, font);
    DrawStatusPanel(ui, config, assets, font);
    DrawAddMapTextureModal(ui, config, input, assets, font);
    DrawTexturePickerModal(ui, config, input, assets, font);
    uiState.keyboardCaptured = ui.focusedId != 0;
    if (state.texturePicker.open || state.addMapTexture.open || HasDocumentModalOpen()) {
        uiState.keyboardCaptured = true;
    }
    engine::EndUI(ui, config, input, assets);
}

bool SectorEditor::IsPreview3DActive() const
{
    return state.mode == SectorEditorMode::Preview3D;
}

Vector2 SectorEditor::MapToScreen(Vector2 map) const
{
    return CanvasWorldToScreen(SectorAuthoringToWorldPosition(map));
}

Vector2 SectorEditor::ScreenToMap(Vector2 screen) const
{
    return SectorWorldToAuthoringPosition(ScreenToCanvasWorld(screen));
}

Vector2 SectorEditor::CanvasWorldToScreen(Vector2 canvasWorld) const
{
    return Vector2{
            canvasRect.x + canvasRect.width * 0.5f + (canvasWorld.x - state.viewCenter.x) * state.viewZoom,
            canvasRect.y + canvasRect.height * 0.5f + (canvasWorld.y - state.viewCenter.y) * state.viewZoom
    };
}

Vector2 SectorEditor::ScreenToCanvasWorld(Vector2 screen) const
{
    return Vector2{
            state.viewCenter.x + (screen.x - (canvasRect.x + canvasRect.width * 0.5f)) / state.viewZoom,
            state.viewCenter.y + (screen.y - (canvasRect.y + canvasRect.height * 0.5f)) / state.viewZoom
    };
}

Vector2 SectorEditor::SnapMapPoint(Vector2 map) const
{
    const float grid = static_cast<float>(std::max(1, state.gridSize));
    Vector2 snapped{
            std::round(map.x / grid) * grid,
            std::round(map.y / grid) * grid
    };

    if (state.currentTool != SectorEditorTool::Sector
            && state.currentTool != SectorEditorTool::InsertSectorInside) {
        return snapped;
    }

    const float threshold = std::max(
            SectorWorldToAuthoringDistance(ScreenVertexSnapPixels / std::max(1.0f, state.viewZoom)),
            grid * 0.20f
    );
    float bestDistance2 = threshold * threshold;
    bool found = false;
    Vector2 best = snapped;
    for (const SectorTopologyVertex& topologyVertex : state.topologyMap.vertices) {
        const Vector2 vertex = SectorTopologyVertexToMap(topologyVertex);
        const float dx = vertex.x - map.x;
        const float dy = vertex.y - map.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 <= bestDistance2) {
            bestDistance2 = distance2;
            best = vertex;
            found = true;
        }
    }

    return found ? best : snapped;
}

Rectangle SectorEditor::BuildLeftPanelRect() const
{
    return Rectangle{0.0f, 0.0f, LeftPanelWidth, EditorHeight - BottomPanelHeight};
}

Rectangle SectorEditor::BuildRightPanelRect() const
{
    return Rectangle{EditorWidth - RightPanelWidth, 0.0f, RightPanelWidth, EditorHeight - BottomPanelHeight};
}

Rectangle SectorEditor::BuildBottomPanelRect() const
{
    return Rectangle{0.0f, EditorHeight - BottomPanelHeight, EditorWidth, BottomPanelHeight};
}

Rectangle SectorEditor::BuildCanvasRect() const
{
    return Rectangle{
            LeftPanelWidth + PanelGap,
            PanelGap,
            EditorWidth - LeftPanelWidth - RightPanelWidth - PanelGap * 2.0f,
            EditorHeight - BottomPanelHeight - PanelGap * 2.0f
    };
}

bool SectorEditor::IsMouseOverCanvas(const engine::Input& input) const
{
    return Contains(canvasRect, input.MousePosition());
}

void SectorEditor::UpdateHoverAndMouse(engine::Input& input)
{
    state.rawMouseMap = ScreenToMap(input.MousePosition());
    state.snappedMouseMap = SnapMapPoint(state.rawMouseMap);
    state.hasHoveredVertex = false;
    state.hoveredTopologyLightId = -1;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};

    if (!initialized || !IsMouseOverCanvas(input)) {
        return;
    }

    if (state.currentTool == SectorEditorTool::Move || state.currentTool == SectorEditorTool::Select) {
        const int lightId = FindTopologyLightNearScreenPoint(input.MousePosition());
        if (lightId >= 0) {
            state.hoveredTopologyLightId = lightId;
            if (!state.pendingTopologyVertexMerge.active) {
                state.inspectedTopologyVertexId = -1;
            }
        } else {
            int vertexId = -1;
            SectorTopologyCoordPoint point;
            if (FindTopologyVertexNearScreenPoint(input.MousePosition(), vertexId, point)) {
                state.hasHoveredVertex = true;
                state.hoveredTopologyVertexId = vertexId;
                state.hoveredTopologyVertexPoint = point;
                state.inspectedTopologyVertexId = vertexId;
            } else if (!state.pendingTopologyVertexMerge.active) {
                state.inspectedTopologyVertexId = -1;
            }
        }
    }
}

void SectorEditor::HandleCanvasInput(engine::Input& input, float dt)
{
    if (!initialized) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    if (state.vertexDrag.active) {
                        CancelVertexDrag("Cancelled vertex move");
                    } else if (state.lightDrag.active) {
                        CancelLightDrag("Cancelled light move");
                    } else if (state.pendingTopologyVertexMerge.active) {
                        CancelPendingTopologyVertexMerge("Cancelled vertex merge");
                    } else if (state.pendingTopologyLineSplitAtPoint.active) {
                        CancelPendingTopologyLineSplitAtPoint("Cancelled split at point");
                    } else if (state.pendingTopologySectorCut.active) {
                        CancelPendingTopologySectorCut("Cancelled sector cut");
                    } else if (state.pendingSector.active) {
                        CancelPendingSector("Cancelled sector");
                    } else if (state.selectedTopologyLightId >= 0
                            || state.topologySelectionKind != TopologySelectionKind::None) {
                        ClearSelection();
                    } else {
                        state.currentTool = SectorEditorTool::Select;
                    }
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.pendingSector.active && event.key.key == KEY_BACKSPACE) {
                    RemoveLastPendingSectorPoint();
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.pendingSector.active && event.key.key == KEY_ENTER) {
                    FinalizePendingSector();
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_DELETE) {
                    if (state.topologySelectionKind == TopologySelectionKind::Light
                            && state.selectedTopologyLightId >= 0) {
                        DeleteSelectedLight();
                    } else if (state.topologySelectionKind == TopologySelectionKind::Sector
                            && state.selectedTopologySectorId >= 0) {
                        OpenDeleteSelectedTopologySectorConfirmation();
                    } else if (state.topologySelectionKind == TopologySelectionKind::Vertex
                            && state.selectedTopologyVertexId >= 0) {
                        statusText = "Standalone vertex deletion is not available; use Dissolve Vertex for simple degree-2 vertices.";
                    } else if (state.topologySelectionKind == TopologySelectionKind::SideDef
                            || state.topologySelectionKind == TopologySelectionKind::LineDef) {
                        statusText = "Direct linedef/sidedef deletion is not available yet.";
                    } else {
                        statusText = "Select a topology sector to delete.";
                    }
                    engine::ConsumeEvent(event);
                    return;
                }
            }
    );

    if (state.pendingTopologyVertexMerge.active) {
        UpdatePendingTopologyVertexMerge(input);

        input.ForEachEvent(
                engine::InputEventType::MouseButtonPressed,
                true,
                [this](engine::InputEvent& event) {
                    if (event.mouseButton.button == MOUSE_RIGHT_BUTTON) {
                        CancelPendingTopologyVertexMerge("Cancelled vertex merge");
                        engine::ConsumeEvent(event);
                    }
                }
        );

        input.ForEachEvent(
                engine::InputEventType::MouseClick,
                true,
                [this](engine::InputEvent& event) {
                    if (!Contains(canvasRect, event.mouseClick.releasePosition)) {
                        return;
                    }
                    if (event.mouseClick.button == MOUSE_RIGHT_BUTTON) {
                        CancelPendingTopologyVertexMerge("Cancelled vertex merge");
                        engine::ConsumeEvent(event);
                        return;
                    }
                    if (event.mouseClick.button == MOUSE_LEFT_BUTTON) {
                        CommitPendingTopologyVertexMerge();
                        engine::ConsumeEvent(event);
                    }
                }
        );
        return;
    }

    if (state.pendingTopologySectorCut.active) {
        UpdatePendingTopologySectorCut(input);

        input.ForEachEvent(
                engine::InputEventType::MouseButtonPressed,
                true,
                [this](engine::InputEvent& event) {
                    if (event.mouseButton.button == MOUSE_RIGHT_BUTTON) {
                        CancelPendingTopologySectorCut("Cancelled sector cut");
                        engine::ConsumeEvent(event);
                    }
                }
        );

        input.ForEachEvent(
                engine::InputEventType::MouseClick,
                true,
                [this](engine::InputEvent& event) {
                    if (!Contains(canvasRect, event.mouseClick.releasePosition)) {
                        return;
                    }
                    if (event.mouseClick.button == MOUSE_RIGHT_BUTTON) {
                        CancelPendingTopologySectorCut("Cancelled sector cut");
                        engine::ConsumeEvent(event);
                        return;
                    }
                    if (event.mouseClick.button == MOUSE_LEFT_BUTTON) {
                        CommitPendingTopologySectorCut();
                        engine::ConsumeEvent(event);
                    }
                }
        );
        return;
    }

    if (state.vertexDrag.active || state.lightDrag.active) {
        if (state.vertexDrag.active) {
            UpdateVertexDrag(input);
        }
        if (state.lightDrag.active) {
            UpdateLightDrag(input);
        }

        input.ForEachEvent(
                engine::InputEventType::MouseButtonPressed,
                true,
                [this](engine::InputEvent& event) {
                    if (event.mouseButton.button == MOUSE_RIGHT_BUTTON) {
                        if (state.vertexDrag.active) {
                            CancelVertexDrag("Cancelled vertex move");
                        }
                        if (state.lightDrag.active) {
                            CancelLightDrag("Cancelled light move");
                        }
                        engine::ConsumeEvent(event);
                    }
                }
        );

        input.ForEachEvent(
                engine::InputEventType::MouseButtonReleased,
                true,
                [this](engine::InputEvent& event) {
                    if (event.mouseButton.button == MOUSE_LEFT_BUTTON) {
                        if (state.vertexDrag.active) {
                            FinishVertexDrag();
                        }
                        if (state.lightDrag.active) {
                            FinishLightDrag();
                        }
                        engine::ConsumeEvent(event);
                    }
                }
        );

        input.ForEachEvent(
                engine::InputEventType::MouseWheel,
                true,
                [](engine::InputEvent& event) {
                    engine::ConsumeEvent(event);
                }
        );
        return;
    }

    if (state.pendingTopologyLineSplitAtPoint.active) {
        UpdatePendingTopologyLineSplitAtPoint(input);

        input.ForEachEvent(
                engine::InputEventType::MouseButtonPressed,
                true,
                [this](engine::InputEvent& event) {
                    if (event.mouseButton.button == MOUSE_RIGHT_BUTTON) {
                        CancelPendingTopologyLineSplitAtPoint("Cancelled split at point");
                        engine::ConsumeEvent(event);
                    }
                }
        );

        input.ForEachEvent(
                engine::InputEventType::MouseClick,
                true,
                [this](engine::InputEvent& event) {
                    if (!Contains(canvasRect, event.mouseClick.releasePosition)) {
                        return;
                    }
                    if (event.mouseClick.button == MOUSE_RIGHT_BUTTON) {
                        CancelPendingTopologyLineSplitAtPoint("Cancelled split at point");
                        engine::ConsumeEvent(event);
                        return;
                    }
                    if (event.mouseClick.button == MOUSE_LEFT_BUTTON) {
                        CommitPendingTopologyLineSplitAtPoint();
                        engine::ConsumeEvent(event);
                    }
                }
        );
    }

    if (!uiState.keyboardCaptured) {
        Vector2 pan{};
        if (input.IsKeyDown(KEY_A)) {
            pan.x -= 1.0f;
        }
        if (input.IsKeyDown(KEY_D)) {
            pan.x += 1.0f;
        }
        if (input.IsKeyDown(KEY_W)) {
            pan.y -= 1.0f;
        }
        if (input.IsKeyDown(KEY_S)) {
            pan.y += 1.0f;
        }

        if (pan.x != 0.0f || pan.y != 0.0f) {
            const float length = std::sqrt(pan.x * pan.x + pan.y * pan.y);
            pan.x /= length;
            pan.y /= length;
            const float mapUnits = (PanPixelsPerSecond * dt) / std::max(1.0f, state.viewZoom);
            state.viewCenter.x += pan.x * mapUnits;
            state.viewCenter.y += pan.y * mapUnits;
        }
    }

    if (!IsMouseOverCanvas(input)) {
        return;
    }

    if (state.pendingTopologyLineSplitAtPoint.active || state.pendingTopologySectorCut.active) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::MouseButtonPressed,
            true,
            [this](engine::InputEvent& event) {
                if (state.currentTool != SectorEditorTool::Move
                        || event.mouseButton.button != MOUSE_LEFT_BUTTON
                        || !Contains(canvasRect, event.mouseButton.position)) {
                    return;
                }

                const int lightId = FindTopologyLightNearScreenPoint(event.mouseButton.position);
                if (lightId >= 0) {
                    StartLightDrag(lightId);
                } else {
                    int vertexId = -1;
                    SectorTopologyCoordPoint point;
                    if (FindTopologyVertexNearScreenPoint(event.mouseButton.position, vertexId, point)) {
                        StartVertexDrag(vertexId, point);
                    } else {
                        statusText = "Move: click a topology light or vertex";
                    }
                }
                engine::ConsumeEvent(event);
            }
    );

    input.ForEachEvent(
            engine::InputEventType::MouseWheel,
            true,
            [this, &input](engine::InputEvent& event) {
                const Vector2 mouseBefore = ScreenToCanvasWorld(input.MousePosition());
                const float zoomFactor = event.wheel.value > 0.0f ? 1.12f : 1.0f / 1.12f;
                state.viewZoom = std::clamp(state.viewZoom * zoomFactor, MinZoom, MaxZoom);
                const Vector2 mouseAfter = ScreenToCanvasWorld(input.MousePosition());
                state.viewCenter.x += mouseBefore.x - mouseAfter.x;
                state.viewCenter.y += mouseBefore.y - mouseAfter.y;
                engine::ConsumeEvent(event);
            }
    );

    input.ForEachEvent(
            engine::InputEventType::MouseClick,
            true,
            [this](engine::InputEvent& event) {
                if (!Contains(canvasRect, event.mouseClick.releasePosition)) {
                    return;
                }

                if (event.mouseClick.button == MOUSE_RIGHT_BUTTON && state.pendingSector.active) {
                    CancelPendingSector("Cancelled sector");
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.mouseClick.button != MOUSE_LEFT_BUTTON) {
                    return;
                }

                if (state.currentTool == SectorEditorTool::Select) {
                    const int lightId = FindTopologyLightNearScreenPoint(event.mouseClick.releasePosition);
                    if (lightId >= 0) {
                        SelectTopologyLight(lightId);
                        statusText = TextFormat("Selected topology light %d", lightId);
                        engine::ConsumeEvent(event);
                        return;
                    }

                    int vertexId = -1;
                    SectorTopologyCoordPoint vertexPoint;
                    if (FindTopologyVertexNearScreenPoint(
                                event.mouseClick.releasePosition,
                                vertexId,
                                vertexPoint)) {
                        SelectTopologyVertex(vertexId);
                        statusText = TextFormat("Selected topology vertex %d", vertexId);
                        engine::ConsumeEvent(event);
                        return;
                    }

                    int lineDefId = -1;
                    int sideDefId = -1;
                    SectorTopologySideKind side = SectorTopologySideKind::Front;
                    bool preferredMissing = false;
                    if (FindTopologyLineNearScreenPoint(
                                event.mouseClick.releasePosition,
                                ScreenToMap(event.mouseClick.releasePosition),
                                lineDefId,
                                sideDefId,
                                side,
                                preferredMissing)) {
                        if (sideDefId >= 0) {
                            SelectTopologySideDef(sideDefId, state.selectedTopologyWallPart);
                            statusText = preferredMissing
                                    ? TextFormat(
                                            "Selected topology %s sidedef %d; clicked side has no sidedef",
                                            SectorTopologySideKindName(side),
                                            sideDefId)
                                    : TextFormat(
                                            "Selected topology %s sidedef %d",
                                            SectorTopologySideKindName(side),
                                            sideDefId);
                        } else {
                            SelectTopologyLineDef(lineDefId, side, state.selectedTopologyWallPart);
                            statusText = TextFormat("Selected topology linedef %d (no sidedef)", lineDefId);
                        }
                        engine::ConsumeEvent(event);
                        return;
                    }

                    bool multipleMatches = false;
                    const int sectorId = FindTopologySectorAt(
                            ScreenToMap(event.mouseClick.releasePosition),
                            &multipleMatches);
                    if (sectorId >= 0) {
                        SelectTopologySector(sectorId);
                        statusText = multipleMatches
                                ? TextFormat("Selected topology sector %d (lowest ID on boundary)", sectorId)
                                : TextFormat("Selected topology sector %d", sectorId);
                    } else {
                        ClearSelection();
                        statusText = "Selected: none";
                    }
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::Sector
                        || state.currentTool == SectorEditorTool::InsertSectorInside) {
                    if (state.currentTool == SectorEditorTool::InsertSectorInside
                            && (!state.pendingSector.active
                                    || state.pendingSector.kind != PendingSectorDrawKind::InsertInside)) {
                        statusText = "Select a topology sector before inserting inside it.";
                        engine::ConsumeEvent(event);
                        return;
                    }

                    const SectorPoint point = CurrentSnappedSectorPoint();
                    if (CanClosePendingSectorAt(point)) {
                        FinalizePendingSector();
                    } else {
                        AddPendingSectorPoint(point);
                    }
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::Light) {
                    AddStaticLightAt(SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition)));
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::Erase) {
                    const int sectorId = FindTopologySectorAt(
                            ScreenToMap(event.mouseClick.releasePosition));
                    if (sectorId >= 0) {
                        SelectTopologySector(sectorId);
                        OpenDeleteTopologySectorConfirmation(sectorId);
                    } else {
                        statusText = "Select a topology sector to delete.";
                    }
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::Move) {
                    statusText = "Move: click a topology light or vertex";
                    engine::ConsumeEvent(event);
                }
            }
    );
}

void SectorEditor::StartVertexDrag(int vertexId, SectorTopologyCoordPoint point)
{
    if (!IsValidSectorTopologyId(vertexId)
            || FindSectorTopologyVertex(state.topologyMap, vertexId) == nullptr) {
        return;
    }

    SelectTopologyVertex(vertexId);
    state.vertexDrag.active = true;
    state.vertexDrag.topologyVertexId = vertexId;
    state.vertexDrag.originalPoint = point;
    state.vertexDrag.previewPoint = point;
    state.vertexDrag.lastValidatedPoint = point;
    state.vertexDrag.hasPreviewPoint = true;
    state.vertexDrag.hasValidatedPreview = true;
    state.vertexDrag.lastPreviewValid = true;
    state.vertexDrag.errorMessage.clear();

    size_t connectedCount = 0;
    for (const SectorTopologyLineDef& lineDef : state.topologyMap.lineDefs) {
        if (lineDef.startVertexId == vertexId || lineDef.endVertexId == vertexId) {
            ++connectedCount;
        }
    }
    statusText = connectedCount > 1
            ? TextFormat("Moving topology vertex %d (%zu connected linedefs)", vertexId, connectedCount)
            : TextFormat("Moving topology vertex %d", vertexId);
}

void SectorEditor::UpdateVertexDrag(engine::Input& input)
{
    if (!state.vertexDrag.active) {
        return;
    }

    std::string error;
    SectorTopologyCoordPoint snappedPoint;
    if (!SnapTopologyVertexMoveTarget(ScreenToMap(input.MousePosition()), snappedPoint, error)) {
        state.vertexDrag.errorMessage = error;
        state.vertexDrag.hasPreviewPoint = false;
        state.vertexDrag.lastPreviewValid = false;
        state.vertexDrag.hasMergeTarget = false;
        state.vertexDrag.mergeTargetVertexId = -1;
        statusText = TextFormat("Move rejected: %s", error.c_str());
        return;
    }

    state.vertexDrag.previewPoint = snappedPoint;
    state.vertexDrag.hasPreviewPoint = true;
    const int mergeTargetVertexId = FindTopologyVertexAtCoordPoint(
            state.vertexDrag.previewPoint,
            state.vertexDrag.topologyVertexId);
    if (SameTopologyPoint(state.vertexDrag.previewPoint, state.vertexDrag.originalPoint)) {
        state.vertexDrag.errorMessage.clear();
        state.vertexDrag.hasValidatedPreview = true;
        state.vertexDrag.lastValidatedPoint = state.vertexDrag.previewPoint;
        state.vertexDrag.lastPreviewValid = true;
        state.vertexDrag.hasMergeTarget = false;
        state.vertexDrag.mergeTargetVertexId = -1;
        statusText = "Moving vertex: original point";
        return;
    }

    if (mergeTargetVertexId >= 0) {
        state.vertexDrag.errorMessage.clear();
        state.vertexDrag.lastPreviewValid = true;
        state.vertexDrag.hasMergeTarget = true;
        state.vertexDrag.mergeTargetVertexId = mergeTargetVertexId;
        state.vertexDrag.hasValidatedPreview = true;
        state.vertexDrag.lastValidatedPoint = state.vertexDrag.previewPoint;
        statusText = TextFormat("Release to merge into vertex %d", mergeTargetVertexId);
        return;
    }

    state.vertexDrag.hasMergeTarget = false;
    state.vertexDrag.mergeTargetVertexId = -1;
    if (!state.vertexDrag.hasValidatedPreview
            || !SameTopologyPoint(state.vertexDrag.previewPoint, state.vertexDrag.lastValidatedPoint)) {
        SectorTopologyMap previewMap = state.topologyMap;
        state.vertexDrag.lastPreviewValid = MoveSectorTopologyVertex(
                previewMap,
                state.vertexDrag.topologyVertexId,
                state.vertexDrag.previewPoint,
                &error);
        state.vertexDrag.lastValidatedPoint = state.vertexDrag.previewPoint;
        state.vertexDrag.hasValidatedPreview = true;
        state.vertexDrag.errorMessage = state.vertexDrag.lastPreviewValid ? std::string{} : error;
    }

    if (state.vertexDrag.lastPreviewValid) {
        state.vertexDrag.errorMessage.clear();
        statusText = TextFormat("Moving topology vertex %d", state.vertexDrag.topologyVertexId);
    } else {
        statusText = TextFormat("Move rejected: %s", state.vertexDrag.errorMessage.c_str());
    }
}

void SectorEditor::FinishVertexDrag()
{
    if (!state.vertexDrag.active) {
        return;
    }

    const int vertexId = state.vertexDrag.topologyVertexId;
    const SectorTopologyCoordPoint original = state.vertexDrag.originalPoint;
    const SectorTopologyCoordPoint target = state.vertexDrag.previewPoint;

    if (!state.vertexDrag.hasPreviewPoint) {
        const std::string error = state.vertexDrag.errorMessage.empty()
                ? "Move target is outside topology coordinate range"
                : state.vertexDrag.errorMessage;
        state.vertexDrag = VertexDragState{};
        statusText = TextFormat("Move rejected: %s", error.c_str());
        return;
    }

    if (SameTopologyPoint(target, original)) {
        state.vertexDrag = VertexDragState{};
        statusText = "Vertex unchanged";
        return;
    }

    if (state.vertexDrag.hasMergeTarget) {
        const int targetVertexId = state.vertexDrag.mergeTargetVertexId;
        SectorTopologyMergeVerticesResult merge;
        std::string error;
        if (!MergeSectorTopologyVertices(state.topologyMap, vertexId, targetVertexId, &merge, &error)) {
            state.vertexDrag = VertexDragState{};
            statusText = TextFormat("Merge rejected: %s", error.c_str());
            return;
        }

        ClearTransientTopologyEditStateAfterGeometryChange();
        SelectTopologyVertex(merge.mergedVertexId);
        state.inspectedTopologyVertexId = merge.mergedVertexId;
        state.hasHoveredVertex = true;
        state.hoveredTopologyVertexId = merge.mergedVertexId;
        const SectorTopologyVertex* merged = FindSectorTopologyVertex(
                state.topologyMap,
                merge.mergedVertexId);
        state.hoveredTopologyVertexPoint = merged != nullptr
                ? SectorTopologyCoordPoint{merged->x, merged->y}
                : SectorTopologyCoordPoint{};
        state.vertexDrag = VertexDragState{};
        MarkTopologyDocumentEdited(TextFormat(
                "Merged vertex %d into vertex %d.",
                merge.removedVertexId,
                merge.mergedVertexId));
        return;
    }

    std::string error;
    if (!MoveSectorTopologyVertex(state.topologyMap, vertexId, target, &error)) {
        state.vertexDrag = VertexDragState{};
        statusText = TextFormat("Move rejected: %s", error.c_str());
        return;
    }

    ClearTransientTopologyEditStateAfterGeometryChange();
    SelectTopologyVertex(vertexId);
    state.vertexDrag = VertexDragState{};
    MarkTopologyDocumentEdited(TextFormat(
            "Moved topology vertex %d %.2f,%.2f -> %.2f,%.2f",
            vertexId,
            SectorCoordToVisibleAuthoring(original.x),
            SectorCoordToVisibleAuthoring(original.y),
            SectorCoordToVisibleAuthoring(target.x),
            SectorCoordToVisibleAuthoring(target.y)
    ));
}

void SectorEditor::CancelVertexDrag(const char* message)
{
    state.vertexDrag = VertexDragState{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::StartLightDrag(int topologyLightId)
{
    const SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
            state.topologyMap,
            topologyLightId);
    if (light == nullptr) {
        return;
    }

    SelectTopologyLight(topologyLightId);
    state.lightDrag.active = true;
    state.lightDrag.topologyLightId = topologyLightId;
    state.lightDrag.originalPosition = light->position;
    state.lightDrag.snappedPosition = light->position;
    statusText = TextFormat("Moving topology light %d", light->id);
}

void SectorEditor::UpdateLightDrag(engine::Input& input)
{
    if (!state.lightDrag.active) {
        return;
    }

    SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
            state.topologyMap,
            state.lightDrag.topologyLightId);
    if (light == nullptr) {
        return;
    }

    const Vector2 snapped = SnapMapPoint(ScreenToMap(input.MousePosition()));
    state.lightDrag.snappedPosition = Vector3{snapped.x, state.lightDrag.originalPosition.y, snapped.y};
    light->position.x = state.lightDrag.snappedPosition.x;
    light->position.y = state.lightDrag.originalPosition.y;
    light->position.z = state.lightDrag.snappedPosition.z;
    statusText = TextFormat("Moving topology light %d", light->id);
}

void SectorEditor::FinishLightDrag()
{
    if (!state.lightDrag.active) {
        return;
    }

    const int lightId = state.lightDrag.topologyLightId;
    const Vector3 original = state.lightDrag.originalPosition;
    state.lightDrag = LightDragState{};
    SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(state.topologyMap, lightId);
    if (light == nullptr) {
        return;
    }

    SelectTopologyLight(lightId);
    if (std::fabs(light->position.x - original.x) <= GeometryEpsilon
            && std::fabs(light->position.z - original.z) <= GeometryEpsilon) {
        light->position = original;
        statusText = "Light unchanged";
        return;
    }

    state.topologyDocumentDirty = true;
    state.hasUnsavedChanges = true;
    statusText = TextFormat(
            "Moved topology light %d to X %.2f, Z %.2f",
            light->id,
            light->position.x,
            light->position.z
    );
}

void SectorEditor::CancelLightDrag(const char* message)
{
    if (state.lightDrag.active) {
        SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
                state.topologyMap,
                state.lightDrag.topologyLightId);
        if (light != nullptr) {
            light->position = state.lightDrag.originalPosition;
            SelectTopologyLight(light->id);
        }
    }

    state.lightDrag = LightDragState{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::StartPendingTopologyVertexMerge(int sourceVertexId)
{
    const SectorTopologyVertex* source = FindSectorTopologyVertex(state.topologyMap, sourceVertexId);
    if (source == nullptr) {
        state.inspectedTopologyVertexId = -1;
        statusText = "Select a topology vertex before merging.";
        return;
    }

    CancelPendingSector(nullptr);
    CancelVertexDrag(nullptr);
    CancelLightDrag(nullptr);
    CancelPendingTopologyLineSplitAtPoint(nullptr);
    CancelPendingTopologySectorCut(nullptr);

    PendingTopologyVertexMerge pending;
    pending.active = true;
    pending.sourceVertexId = sourceVertexId;
    pending.message = "Click target vertex to merge, Escape/right click to cancel.";
    state.pendingTopologyVertexMerge = pending;
    state.currentTool = SectorEditorTool::Move;
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.inspectedTopologyVertexId = sourceVertexId;
    statusText = pending.message;
}

void SectorEditor::CancelPendingTopologyVertexMerge(const char* message)
{
    state.pendingTopologyVertexMerge = PendingTopologyVertexMerge{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::UpdatePendingTopologyVertexMerge(engine::Input& input)
{
    PendingTopologyVertexMerge& pending = state.pendingTopologyVertexMerge;
    if (!pending.active) {
        return;
    }

    if (FindSectorTopologyVertex(state.topologyMap, pending.sourceVertexId) == nullptr) {
        CancelPendingTopologyVertexMerge("Vertex merge cancelled: source vertex no longer exists.");
        return;
    }

    pending.hoveredTargetVertexId = -1;
    pending.hasValidTarget = false;
    pending.message = "Click target vertex to merge, Escape/right click to cancel.";

    if (!IsMouseOverCanvas(input)) {
        statusText = pending.message;
        return;
    }

    int targetVertexId = -1;
    SectorTopologyCoordPoint point;
    if (!FindTopologyVertexNearScreenPoint(input.MousePosition(), targetVertexId, point)) {
        pending.message = "Click target vertex to merge, Escape/right click to cancel.";
        statusText = pending.message;
        return;
    }

    pending.hoveredTargetVertexId = targetVertexId;
    pending.hasValidTarget = targetVertexId != pending.sourceVertexId;
    pending.message = pending.hasValidTarget
            ? TextFormat("Click to merge into vertex %d.", targetVertexId)
            : "Choose a different target vertex.";
    statusText = pending.message;
}

void SectorEditor::CommitPendingTopologyVertexMerge()
{
    if (!state.pendingTopologyVertexMerge.active) {
        return;
    }

    const PendingTopologyVertexMerge pending = state.pendingTopologyVertexMerge;
    if (!pending.hasValidTarget) {
        statusText = pending.message.empty() ? "Choose a target vertex to merge into." : pending.message;
        return;
    }

    SectorTopologyMergeVerticesResult merge;
    std::string error;
    if (!MergeSectorTopologyVertices(
                state.topologyMap,
                pending.sourceVertexId,
                pending.hoveredTargetVertexId,
                &merge,
                &error)) {
        state.pendingTopologyVertexMerge.message =
                error.empty() ? "Merge rejected." : TextFormat("Merge rejected: %s", error.c_str());
        statusText = state.pendingTopologyVertexMerge.message;
        return;
    }

    ClearTransientTopologyEditStateAfterGeometryChange();
    state.pendingTopologyVertexMerge = PendingTopologyVertexMerge{};
    SelectTopologyVertex(merge.mergedVertexId);
    state.inspectedTopologyVertexId = merge.mergedVertexId;
    state.hasHoveredVertex = true;
    state.hoveredTopologyVertexId = merge.mergedVertexId;
    const SectorTopologyVertex* merged = FindSectorTopologyVertex(state.topologyMap, merge.mergedVertexId);
    state.hoveredTopologyVertexPoint = merged != nullptr
            ? SectorTopologyCoordPoint{merged->x, merged->y}
            : SectorTopologyCoordPoint{};
    MarkTopologyDocumentEdited(TextFormat(
            "Merged vertex %d into vertex %d.",
            merge.removedVertexId,
            merge.mergedVertexId));
}

void SectorEditor::StartPendingTopologyLineSplitAtPoint()
{
    const SectorTopologyLineDef* lineDef = SelectedTopologyLineDef();
    if (lineDef == nullptr) {
        ClearStaleTopologySelection();
        statusText = "Select a topology linedef before splitting at point.";
        return;
    }

    CancelPendingSector(nullptr);
    CancelVertexDrag(nullptr);
    CancelLightDrag(nullptr);
    CancelPendingTopologyVertexMerge(nullptr);
    CancelPendingTopologySectorCut(nullptr);

    PendingTopologyLineSplitAtPoint pending;
    pending.active = true;
    pending.lineDefId = lineDef->id;
    pending.sideDefId = state.topologySelectionKind == TopologySelectionKind::SideDef
            ? state.selectedTopologySideDefId
            : -1;
    pending.side = state.selectedTopologySideKind;
    pending.wallPart = state.selectedTopologyWallPart;
    pending.message = "Click a snapped point on the selected linedef; Escape or right click cancels.";
    state.pendingTopologyLineSplitAtPoint = pending;
    state.currentTool = SectorEditorTool::Select;
    state.hoveredSurface3D = SectorSurfaceHit{};
    statusText = pending.message;
}

void SectorEditor::CancelPendingTopologyLineSplitAtPoint(const char* message)
{
    state.pendingTopologyLineSplitAtPoint = PendingTopologyLineSplitAtPoint{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

bool SectorEditor::ValidatePendingTopologyLineSplitAtPointTarget(const char* staleMessage)
{
    if (!state.pendingTopologyLineSplitAtPoint.active) {
        return false;
    }

    const PendingTopologyLineSplitAtPoint& pending = state.pendingTopologyLineSplitAtPoint;
    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(
            state.topologyMap,
            pending.lineDefId);
    if (lineDef == nullptr) {
        CancelPendingTopologyLineSplitAtPoint(staleMessage);
        return false;
    }

    if (pending.sideDefId >= 0) {
        const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(
                state.topologyMap,
                pending.sideDefId);
        if (sideDef == nullptr || sideDef->lineDefId != lineDef->id) {
            CancelPendingTopologyLineSplitAtPoint(staleMessage);
            return false;
        }
    }

    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (!GetSectorTopologyLineVertices(state.topologyMap, *lineDef, start, end)) {
        CancelPendingTopologyLineSplitAtPoint(staleMessage);
        return false;
    }
    return true;
}

void SectorEditor::UpdatePendingTopologyLineSplitAtPoint(engine::Input& input)
{
    if (!ValidatePendingTopologyLineSplitAtPointTarget(
                "Split at point cancelled: target linedef no longer exists.")) {
        return;
    }

    PendingTopologyLineSplitAtPoint& pending = state.pendingTopologyLineSplitAtPoint;
    pending.hasCandidatePoint = false;
    pending.hasValidCandidate = false;

    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(
            state.topologyMap,
            pending.lineDefId);
    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (lineDef == nullptr || !GetSectorTopologyLineVertices(state.topologyMap, *lineDef, start, end)) {
        CancelPendingTopologyLineSplitAtPoint("Split at point cancelled: target linedef no longer exists.");
        return;
    }

    if (!Contains(canvasRect, input.MousePosition())) {
        pending.message = "Move over the selected linedef to choose a split point.";
        statusText = pending.message;
        return;
    }

    SectorTopologyCoordPoint candidate;
    std::string error;
    if (!ToTopologyCoordPoint(Vector2ToSectorPoint(state.snappedMouseMap), candidate, error)) {
        pending.message = error;
        statusText = error;
        return;
    }

    pending.candidatePoint = candidate;
    pending.hasCandidatePoint = true;

    const SectorTopologyCoordPoint startPoint{start->x, start->y};
    const SectorTopologyCoordPoint endPoint{end->x, end->y};
    if (!SectorTopologyPointStrictlyInsideSegment(candidate, startPoint, endPoint)) {
        pending.message = "Split point must be inside the selected linedef.";
        statusText = pending.message;
        return;
    }

    for (const SectorTopologyVertex& vertex : state.topologyMap.vertices) {
        if (vertex.x == candidate.x && vertex.y == candidate.y) {
            pending.message = "Split point is already occupied by a topology vertex.";
            statusText = pending.message;
            return;
        }
    }

    pending.hasValidCandidate = true;
    pending.message = "Click to split selected linedef; Escape or right click cancels.";
    statusText = pending.message;
}

void SectorEditor::CommitPendingTopologyLineSplitAtPoint()
{
    if (!ValidatePendingTopologyLineSplitAtPointTarget(
                "Split at point cancelled: target linedef no longer exists.")) {
        return;
    }

    const PendingTopologyLineSplitAtPoint pending = state.pendingTopologyLineSplitAtPoint;
    if (!pending.hasCandidatePoint || !pending.hasValidCandidate) {
        statusText = pending.message.empty()
                ? "Choose a valid split point on the selected linedef."
                : pending.message;
        return;
    }

    SectorTopologySplitLineResult split;
    std::string error;
    if (!SplitSectorTopologyLineDefAtPoint(
                state.topologyMap,
                pending.lineDefId,
                pending.candidatePoint,
                &split,
                &error)) {
        statusText = error.empty() ? "Cannot split topology linedef at point" : error;
        return;
    }

    state.pendingTopologyLineSplitAtPoint = PendingTopologyLineSplitAtPoint{};
    state.hasHoveredVertex = false;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }

    if (pending.sideDefId >= 0) {
        const int secondSideDefId = pending.side == SectorTopologySideKind::Front
                ? split.secondFrontSideDefId
                : split.secondBackSideDefId;
        if (FindSectorTopologySideDef(state.topologyMap, secondSideDefId) != nullptr) {
            SelectTopologySideDef(secondSideDefId, pending.wallPart);
        } else {
            SelectTopologyLineDef(split.secondLineDefId, pending.side, pending.wallPart);
        }
    } else {
        SelectTopologyLineDef(split.secondLineDefId, pending.side, pending.wallPart);
    }

    state.topologyDocumentDirty = true;
    state.topologyRenderWarning.clear();
    state.hasUnsavedChanges = true;
    statusText = TextFormat(
            "Split topology linedef %d at point; selected linedef %d",
            pending.lineDefId,
            split.secondLineDefId);
}

void SectorEditor::StartPendingTopologySectorCut()
{
    const SectorTopologySector* sector = SelectedTopologySector();
    if (sector == nullptr) {
        ClearStaleTopologySelection();
        statusText = "Select a topology sector before cutting it.";
        return;
    }

    CancelPendingSector(nullptr);
    CancelVertexDrag(nullptr);
    CancelLightDrag(nullptr);
    CancelPendingTopologyVertexMerge(nullptr);
    CancelPendingTopologyLineSplitAtPoint(nullptr);

    PendingTopologySectorCut pending;
    pending.active = true;
    pending.sectorId = sector->id;
    pending.message = "Click first boundary point for sector cut; Escape or right click cancels.";
    state.pendingTopologySectorCut = pending;
    state.currentTool = SectorEditorTool::Select;
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    statusText = pending.message;
}

void SectorEditor::CancelPendingTopologySectorCut(const char* message)
{
    state.pendingTopologySectorCut = PendingTopologySectorCut{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

bool SectorEditor::ValidatePendingTopologySectorCutTarget(const char* staleMessage)
{
    if (!state.pendingTopologySectorCut.active) {
        return false;
    }
    if (FindSectorTopologySector(state.topologyMap, state.pendingTopologySectorCut.sectorId) == nullptr) {
        CancelPendingTopologySectorCut(staleMessage);
        return false;
    }
    return true;
}

void SectorEditor::UpdatePendingTopologySectorCut(engine::Input& input)
{
    if (!ValidatePendingTopologySectorCutTarget(
                "Sector cut cancelled: selected sector no longer exists.")) {
        return;
    }

    PendingTopologySectorCut& pending = state.pendingTopologySectorCut;
    pending.hasCandidatePoint = false;
    pending.hasValidCandidate = false;

    if (!Contains(canvasRect, input.MousePosition())) {
        pending.message = pending.hasFirstPoint
                ? "Move over the selected sector boundary to choose the second cut point."
                : "Move over the selected sector boundary to choose the first cut point.";
        statusText = pending.message;
        return;
    }

    SectorTopologyBoundaryCutPoint candidate;
    if (!FindSelectedSectorBoundaryCutPointNearScreenPoint(input.MousePosition(), candidate)) {
        pending.message = pending.hasFirstPoint
                ? "Choose a second point on the selected sector outer boundary."
                : "Choose a first point on the selected sector outer boundary.";
        statusText = pending.message;
        return;
    }

    pending.candidatePoint = candidate;
    pending.hasCandidatePoint = true;

    std::string endpointError;
    if (!ValidateSectorTopologySectorBoundaryCutPoint(
                state.topologyMap,
                pending.sectorId,
                pending.candidatePoint,
                &endpointError)) {
        pending.hasValidCandidate = false;
        pending.cacheHasFirstPoint = false;
        pending.message = endpointError.empty() ? "Sector cut endpoint rejected." : endpointError;
        statusText = pending.message;
        return;
    }

    if (!pending.hasFirstPoint) {
        pending.hasValidCandidate = true;
        pending.message = "Click to place first cut point.";
        statusText = pending.message;
        return;
    }

    const bool cacheMatches =
            pending.cacheHasFirstPoint
            && pending.cachedSectorId == pending.sectorId
            && SameBoundaryCutPoint(pending.cachedFirstPoint, pending.firstPoint)
            && SameBoundaryCutPoint(pending.cachedCandidatePoint, pending.candidatePoint);
    if (!cacheMatches) {
        SectorTopologyMap candidateMap = state.topologyMap;
        SectorTopologyCutSectorResult previewResult;
        std::string error;
        pending.cachedValid = CutSectorTopologySectorBetweenBoundaryPoints(
                candidateMap,
                pending.sectorId,
                pending.firstPoint,
                pending.candidatePoint,
                &previewResult,
                &error);
        pending.cachedError = pending.cachedValid ? std::string{} : error;
        pending.cachedSectorId = pending.sectorId;
        pending.cachedFirstPoint = pending.firstPoint;
        pending.cachedCandidatePoint = pending.candidatePoint;
        pending.cacheHasFirstPoint = true;
    }

    pending.hasValidCandidate = pending.cachedValid;
    pending.message = pending.cachedValid
            ? "Click to cut selected sector; Escape or right click cancels."
            : (pending.cachedError.empty() ? "Sector cut rejected." : pending.cachedError);
    statusText = pending.message;
}

void SectorEditor::CommitPendingTopologySectorCut()
{
    if (!ValidatePendingTopologySectorCutTarget(
                "Sector cut cancelled: selected sector no longer exists.")) {
        return;
    }

    PendingTopologySectorCut& pending = state.pendingTopologySectorCut;
    if (!pending.hasCandidatePoint || !pending.hasValidCandidate) {
        statusText = pending.message.empty()
                ? "Choose a valid boundary point for sector cut."
                : pending.message;
        return;
    }

    if (!pending.hasFirstPoint) {
        pending.firstPoint = pending.candidatePoint;
        pending.hasFirstPoint = true;
        pending.cacheHasFirstPoint = false;
        pending.message = "Click second boundary point for sector cut; Escape or right click cancels.";
        statusText = pending.message;
        return;
    }

    const PendingTopologySectorCut committed = pending;
    SectorTopologyCutSectorResult cut;
    std::string error;
    if (!CutSectorTopologySectorBetweenBoundaryPoints(
                state.topologyMap,
                committed.sectorId,
                committed.firstPoint,
                committed.candidatePoint,
                &cut,
                &error)) {
        pending.cacheHasFirstPoint = false;
        pending.hasValidCandidate = false;
        pending.message = error.empty() ? "Sector cut rejected." : error;
        statusText = pending.message;
        return;
    }

    ClearTransientTopologyEditStateAfterGeometryChange();
    state.pendingTopologySectorCut = PendingTopologySectorCut{};
    SelectTopologySector(cut.originalSectorId);
    MarkTopologyDocumentEdited(TextFormat(
            "Cut topology sector %d; created sector %d.",
            cut.originalSectorId,
            cut.newSectorId));
}

void SectorEditor::UpdatePreview3D(engine::Input& input, float dt)
{
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this](engine::InputEvent& event) {
                if (event.key.key == KEY_F1) {
                    state.useBakedAmbientOcclusion = !state.useBakedAmbientOcclusion;
                    statusText = state.useBakedAmbientOcclusion
                            ? "Baked AO enabled"
                            : "Baked AO disabled";
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_F2) {
                    state.previewUiHidden = !state.previewUiHidden;
                    if (state.previewUiHidden) {
                        state.hoveredSurface3D = SectorSurfaceHit{};
                    }
                    statusText = state.previewUiHidden
                            ? "3D UI hidden"
                            : "3D UI shown";
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_TAB || event.key.key == KEY_ESCAPE) {
                    LeavePreview3D();
                    engine::ConsumeEvent(event);
                }
            }
    );

    if (state.mode == SectorEditorMode::Preview3D) {
        preview.Update(input, dt);
        UpdatePreview3DSelection(input);
    }
}

void SectorEditor::UpdatePreview3DSelection(engine::Input& input)
{
    if (!initialized
            || !preview.IsReady()
            || preview.IsMouseLookEnabled()
            || state.previewUiHidden
            || state.texturePicker.open) {
        state.hoveredSurface3D = SectorSurfaceHit{};
        return;
    }

    const Rectangle viewport{0.0f, 0.0f, EditorWidth, EditorHeight};
    const Vector2 mouse = input.MousePosition();
    const bool overPanel = IsValidTopologySurfaceEditTarget(state.selectedTopologySurface3D)
            && Contains(BuildPreviewUvPanelRect(), mouse);
    state.hoveredSurface3D = overPanel
            ? SectorSurfaceHit{}
            : PickSectorSurface3D(mouse, viewport);

    input.ForEachEvent(
            engine::InputEventType::MouseClick,
            true,
            [this, overPanel](engine::InputEvent& event) {
                if (event.mouseClick.button != MOUSE_LEFT_BUTTON) {
                    return;
                }
                if (overPanel) {
                    engine::ConsumeEvent(event);
                    return;
                }
                if (state.hoveredSurface3D.hit) {
                    SelectSurface3D(state.hoveredSurface3D.surface);
                    statusText = TextFormat("Selected 3D %s", SurfaceKindName(state.hoveredSurface3D.surface.kind));
                    engine::ConsumeEvent(event);
                }
            }
    );
}

void SectorEditor::CancelPendingSector(const char* message)
{
    const bool wasInsert = state.pendingSector.kind == PendingSectorDrawKind::InsertInside;
    state.pendingSector = PendingSectorDraw{};
    if (wasInsert) {
        state.currentTool = SectorEditorTool::Select;
    }
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::RemoveLastPendingSectorPoint()
{
    if (!state.pendingSector.active || state.pendingSector.points.empty()) {
        return;
    }

    state.pendingSector.points.pop_back();
    state.pendingSector.errorMessage.clear();
    if (state.pendingSector.points.empty()) {
        CancelPendingSector("Cancelled sector");
    } else {
        statusText = state.pendingSector.kind == PendingSectorDrawKind::InsertInside
                ? TextFormat(
                        "Insert sector: %zu points inside %s",
                        state.pendingSector.points.size(),
                        state.pendingSector.parentSectorLabel.c_str())
                : TextFormat("Pending sector: %zu points", state.pendingSector.points.size());
    }
}

void SectorEditor::AddPendingSectorPoint(SectorPoint point)
{
    const PendingSectorDrawKind pendingKind = state.pendingSector.active
            ? state.pendingSector.kind
            : PendingSectorDrawKind::NewSector;
    std::string error;
    SectorPoint canonicalPoint;
    if (!ToCanonicalSectorPoint(point, canonicalPoint, error)) {
        state.pendingSector.errorMessage = error;
        statusText = error;
        return;
    }
    if (!ValidatePendingTopologyPoint(canonicalPoint, error)) {
        state.pendingSector.errorMessage = error;
        statusText = error;
        return;
    }

    state.pendingSector.active = true;
    state.pendingSector.kind = pendingKind;
    state.pendingSector.points.push_back(canonicalPoint);
    state.pendingSector.errorMessage.clear();
    if (state.pendingSector.kind == PendingSectorDrawKind::InsertInside) {
        statusText = TextFormat(
                "Insert sector: %zu points inside %s",
                state.pendingSector.points.size(),
                state.pendingSector.parentSectorLabel.c_str());
    } else {
        statusText = TextFormat("Pending sector: %zu points", state.pendingSector.points.size());
    }
}

void SectorEditor::FinalizePendingSector()
{
    if (!state.pendingSector.active || state.pendingSector.points.size() < 3) {
        state.pendingSector.errorMessage = "Need at least 3 points to close sector";
        statusText = state.pendingSector.errorMessage;
        return;
    }

    std::string error;
    std::vector<SectorTopologyCoordPoint> topologyPoints;
    if (!BuildPendingTopologyPoints(topologyPoints, error)) {
        state.pendingSector.errorMessage = error;
        statusText = error;
        return;
    }

    int createdSectorId = -1;
    bool created = false;
    if (state.pendingSector.kind == PendingSectorDrawKind::InsertInside) {
        SectorTopologyInsertPolygonOptions options;
        created = InsertSectorTopologyPolygon(
                state.topologyMap,
                state.pendingSector.parentTopologySectorId,
                topologyPoints,
                options,
                &createdSectorId,
                &error);
    } else {
        SectorTopologyCreatePolygonOptions options = BuildTopologyCreateOptions();
        created = CreateSectorTopologyPolygon(
                state.topologyMap,
                topologyPoints,
                options,
                &createdSectorId,
                &error);
    }
    if (!created) {
        state.pendingSector.errorMessage = error.empty() ? "Could not create topology sector" : error;
        statusText = state.pendingSector.errorMessage;
        return;
    }

    const bool insertedInside = state.pendingSector.kind == PendingSectorDrawKind::InsertInside;
    state.pendingSector = PendingSectorDraw{};
    state.hasHoveredVertex = false;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    SelectTopologySector(createdSectorId);
    state.topologyDocumentDirty = true;
    state.topologyRenderWarning.clear();
    state.hasUnsavedChanges = true;
    statusText = insertedInside
            ? TextFormat("Inserted topology sector %d", createdSectorId)
            : TextFormat("Created topology sector %d", createdSectorId);
}

void SectorEditor::StartInsertSectorInside()
{
    const SectorTopologySector* parent = SelectedTopologySector();
    if (parent == nullptr) {
        ClearStaleTopologySelection();
        statusText = "Select a topology sector before inserting inside it.";
        return;
    }

    PendingSectorDraw pending;
    pending.active = true;
    pending.kind = PendingSectorDrawKind::InsertInside;
    pending.parentTopologySectorId = parent->id;
    pending.parentSectorLabel = parent->name.empty()
            ? TextFormat("sector %d", parent->id)
            : parent->name;
    state.pendingSector = std::move(pending);
    state.currentTool = SectorEditorTool::InsertSectorInside;
    state.hoveredSurface3D = SectorSurfaceHit{};
    statusText = TextFormat("Draw a sector inside %s", state.pendingSector.parentSectorLabel.c_str());
}

bool SectorEditor::IsPendingInsertParentValid() const
{
    if (!state.pendingSector.active
            || state.pendingSector.kind != PendingSectorDrawKind::InsertInside
            || state.pendingSector.parentTopologySectorId < 0) {
        return false;
    }
    return FindSectorTopologySector(state.topologyMap, state.pendingSector.parentTopologySectorId) != nullptr;
}

bool SectorEditor::CanClosePendingSectorAt(SectorPoint point) const
{
    if (!state.pendingSector.active
            || state.pendingSector.points.size() < 3) {
        return false;
    }

    SectorTopologyCoordPoint candidate;
    SectorTopologyCoordPoint first;
    std::string error;
    return ToTopologyCoordPoint(point, candidate, error)
            && ToTopologyCoordPoint(state.pendingSector.points.front(), first, error)
            && candidate.x == first.x
            && candidate.y == first.y;
}

SectorPoint SectorEditor::CurrentSnappedSectorPoint() const
{
    const SectorPoint point = Vector2ToSectorPoint(state.snappedMouseMap);
    SectorPoint canonical;
    std::string error;
    return ToCanonicalSectorPoint(point, canonical, error) ? canonical : point;
}

bool SectorEditor::ToTopologyCoordPoint(
        SectorPoint point,
        SectorTopologyCoordPoint& outPoint,
        std::string& error) const
{
    SectorCoord x = 0;
    SectorCoord y = 0;
    if (!VisibleAuthoringToSectorCoord(point.x, x)
            || !VisibleAuthoringToSectorCoord(point.y, y)) {
        error = "Point is outside topology coordinate range";
        return false;
    }
    outPoint = SectorTopologyCoordPoint{x, y};
    error.clear();
    return true;
}

bool SectorEditor::ToCanonicalSectorPoint(
        SectorPoint point,
        SectorPoint& outPoint,
        std::string& error) const
{
    SectorTopologyCoordPoint topologyPoint;
    if (!ToTopologyCoordPoint(point, topologyPoint, error)) {
        return false;
    }
    outPoint = SectorTopologyCoordPointToSectorPoint(topologyPoint);
    return true;
}

bool SectorEditor::BuildPendingTopologyPoints(
        std::vector<SectorTopologyCoordPoint>& outPoints,
        std::string& error) const
{
    if (!state.pendingSector.active
            || (state.pendingSector.kind != PendingSectorDrawKind::NewSector
                    && state.pendingSector.kind != PendingSectorDrawKind::InsertInside)) {
        error = "No topology sector draw is active";
        return false;
    }

    outPoints.clear();
    outPoints.reserve(state.pendingSector.points.size());
    for (SectorPoint point : state.pendingSector.points) {
        SectorTopologyCoordPoint topologyPoint;
        if (!ToTopologyCoordPoint(point, topologyPoint, error)) {
            return false;
        }
        outPoints.push_back(topologyPoint);
    }
    error.clear();
    return true;
}

bool SectorEditor::ValidatePendingTopologyPoint(SectorPoint point, std::string& error) const
{
    SectorTopologyCoordPoint candidate;
    if (!ToTopologyCoordPoint(point, candidate, error)) {
        return false;
    }

    if (!state.pendingSector.active || state.pendingSector.points.empty()) {
        error.clear();
        return true;
    }
    std::vector<SectorTopologyCoordPoint> existing;
    if (!BuildPendingTopologyPoints(existing, error)) {
        return false;
    }

    const SectorTopologyCoordPoint previous = existing.back();
    if (candidate.x == previous.x && candidate.y == previous.y) {
        error = "Duplicate point";
        return false;
    }

    if (existing.size() >= 2
            && candidate.x == existing.front().x
            && candidate.y == existing.front().y) {
        error = existing.size() >= 3
                ? "Click first point to close sector"
                : "Need at least 3 points to close sector";
        return false;
    }

    for (SectorTopologyCoordPoint point : existing) {
        if (candidate.x == point.x && candidate.y == point.y) {
            error = "Duplicate point";
            return false;
        }
    }

    error.clear();
    return true;
}

SectorTopologyCreatePolygonOptions SectorEditor::BuildTopologyCreateOptions() const
{
    SectorTopologyCreatePolygonOptions options;
    options.floorZ = state.defaultSectorFloorZ;
    options.ceilingZ = state.defaultSectorCeilingZ;
    options.floorTextureId = state.defaultFloorTextureId;
    options.ceilingTextureId = state.defaultCeilingTextureId;
    options.defaultWall.textureId = state.defaultWallTextureId;
    options.defaultLower.textureId = state.defaultLowerWallTextureId.empty()
            ? state.defaultWallTextureId
            : state.defaultLowerWallTextureId;
    options.defaultUpper.textureId = state.defaultUpperWallTextureId.empty()
            ? state.defaultWallTextureId
            : state.defaultUpperWallTextureId;
    return options;
}

SectorTopologySector* SectorEditor::SelectedTopologySector()
{
    if (state.topologySelectionKind != TopologySelectionKind::Sector) {
        return nullptr;
    }
    return FindSectorTopologySector(state.topologyMap, state.selectedTopologySectorId);
}

const SectorTopologySector* SectorEditor::SelectedTopologySector() const
{
    if (state.topologySelectionKind != TopologySelectionKind::Sector) {
        return nullptr;
    }
    return FindSectorTopologySector(state.topologyMap, state.selectedTopologySectorId);
}

SectorTopologyVertex* SectorEditor::SelectedTopologyVertex()
{
    if (state.topologySelectionKind != TopologySelectionKind::Vertex) {
        return nullptr;
    }
    return FindSectorTopologyVertex(state.topologyMap, state.selectedTopologyVertexId);
}

const SectorTopologyVertex* SectorEditor::SelectedTopologyVertex() const
{
    if (state.topologySelectionKind != TopologySelectionKind::Vertex) {
        return nullptr;
    }
    return FindSectorTopologyVertex(state.topologyMap, state.selectedTopologyVertexId);
}

SectorTopologySideDef* SectorEditor::SelectedTopologySideDef()
{
    if (state.topologySelectionKind != TopologySelectionKind::SideDef) {
        return nullptr;
    }
    return FindSectorTopologySideDef(state.topologyMap, state.selectedTopologySideDefId);
}

const SectorTopologySideDef* SectorEditor::SelectedTopologySideDef() const
{
    if (state.topologySelectionKind != TopologySelectionKind::SideDef) {
        return nullptr;
    }
    return FindSectorTopologySideDef(state.topologyMap, state.selectedTopologySideDefId);
}

SectorTopologyLineDef* SectorEditor::SelectedTopologyLineDef()
{
    if (state.topologySelectionKind != TopologySelectionKind::LineDef
            && state.topologySelectionKind != TopologySelectionKind::SideDef) {
        return nullptr;
    }
    return FindSectorTopologyLineDef(state.topologyMap, state.selectedTopologyLineDefId);
}

const SectorTopologyLineDef* SectorEditor::SelectedTopologyLineDef() const
{
    if (state.topologySelectionKind != TopologySelectionKind::LineDef
            && state.topologySelectionKind != TopologySelectionKind::SideDef) {
        return nullptr;
    }
    return FindSectorTopologyLineDef(state.topologyMap, state.selectedTopologyLineDefId);
}

SectorTopologyStaticPointLight* SectorEditor::SelectedTopologyLight()
{
    if (state.topologySelectionKind != TopologySelectionKind::Light) {
        return nullptr;
    }
    return FindSectorTopologyStaticLight(state.topologyMap, state.selectedTopologyLightId);
}

const SectorTopologyStaticPointLight* SectorEditor::SelectedTopologyLight() const
{
    if (state.topologySelectionKind != TopologySelectionKind::Light) {
        return nullptr;
    }
    return FindSectorTopologyStaticLight(state.topologyMap, state.selectedTopologyLightId);
}

void SectorEditor::ClearStaleTopologySelection()
{
    bool stale = false;
    if (state.topologySelectionKind == TopologySelectionKind::Sector) {
        stale = state.selectedTopologySectorId < 0
                || FindSectorTopologySector(state.topologyMap, state.selectedTopologySectorId) == nullptr;
    } else if (state.topologySelectionKind == TopologySelectionKind::Vertex) {
        stale = state.selectedTopologyVertexId < 0
                || FindSectorTopologyVertex(state.topologyMap, state.selectedTopologyVertexId) == nullptr;
    } else if (state.topologySelectionKind == TopologySelectionKind::SideDef) {
        const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(
                state.topologyMap,
                state.selectedTopologySideDefId);
        const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(
                state.topologyMap,
                state.selectedTopologyLineDefId);
        stale = sideDef == nullptr
                || lineDef == nullptr
                || sideDef->lineDefId != lineDef->id;
    } else if (state.topologySelectionKind == TopologySelectionKind::LineDef) {
        stale = state.selectedTopologyLineDefId < 0
                || FindSectorTopologyLineDef(state.topologyMap, state.selectedTopologyLineDefId) == nullptr;
    } else if (state.topologySelectionKind == TopologySelectionKind::Light) {
        stale = state.selectedTopologyLightId < 0
                || FindSectorTopologyStaticLight(state.topologyMap, state.selectedTopologyLightId) == nullptr;
    }

    if (stale) {
        if (state.pendingTopologyLineSplitAtPoint.active
                && (state.pendingTopologyLineSplitAtPoint.lineDefId == state.selectedTopologyLineDefId
                        || FindSectorTopologyLineDef(
                                state.topologyMap,
                                state.pendingTopologyLineSplitAtPoint.lineDefId) == nullptr)) {
            CancelPendingTopologyLineSplitAtPoint(
                    "Split at point cancelled: target linedef no longer exists.");
        }
        state.topologySelectionKind = TopologySelectionKind::None;
        state.selectedTopologySectorId = -1;
        state.selectedTopologyVertexId = -1;
        state.selectedTopologySideDefId = -1;
        state.selectedTopologyLineDefId = -1;
        state.selectedTopologyLightId = -1;
        state.selectedTopologySideKind = SectorTopologySideKind::Front;
        uiState.idBufferSectorIndex = -1;
        uiState.idBufferLightIndex = -1;
        SyncSelectedSectorIdBuffer();
        SyncSelectedLightIdBuffer();
    }
}

void SectorEditor::MarkTopologyDocumentEdited(const char* status)
{
    state.topologyDocumentDirty = true;
    state.hasUnsavedChanges = true;
    if (status != nullptr && status[0] != '\0') {
        statusText = status;
    }
}

void SectorEditor::ClearTransientTopologyEditStateAfterGeometryChange()
{
    ClearStaleTopologySelection();
    state.topologyRenderWarning.clear();
    state.pendingTopologySectorCut = PendingTopologySectorCut{};
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
}

void SectorEditor::SyncSelectedSectorIdBuffer()
{
    const SectorTopologySector* sector = SelectedTopologySector();
    if (sector == nullptr) {
        uiState.selectedSectorIdBuffer[0] = '\0';
        uiState.idBufferSectorIndex = -1;
        uiState.idEditError.clear();
        return;
    }

    if (uiState.idBufferSectorIndex == state.selectedTopologySectorId) {
        return;
    }

    std::snprintf(
            uiState.selectedSectorIdBuffer,
            sizeof(uiState.selectedSectorIdBuffer),
            "%s",
            sector->name.c_str());
    uiState.idBufferSectorIndex = state.selectedTopologySectorId;
    uiState.idEditError.clear();
}

void SectorEditor::SyncSelectedLightIdBuffer()
{
    const SectorTopologyStaticPointLight* light = SelectedTopologyLight();
    if (light == nullptr) {
        uiState.selectedLightIdBuffer[0] = '\0';
        uiState.idBufferLightIndex = -1;
        if (state.topologySelectionKind == TopologySelectionKind::None) {
            uiState.idEditError.clear();
        }
        return;
    }

    if (uiState.idBufferLightIndex == light->id) {
        return;
    }

    std::snprintf(uiState.selectedLightIdBuffer, sizeof(uiState.selectedLightIdBuffer), "%d", light->id);
    uiState.idBufferLightIndex = light->id;
    uiState.idEditError.clear();
}

bool SectorEditor::TryRenameSelectedTopologySector()
{
    SectorTopologySector* sector = SelectedTopologySector();
    if (sector == nullptr) {
        uiState.idEditError = "No topology sector selected";
        statusText = uiState.idEditError;
        return false;
    }

    const std::string newName = uiState.selectedSectorIdBuffer;
    if (newName == sector->name) {
        uiState.idEditError.clear();
        return true;
    }

    sector->name = newName;
    uiState.idEditError.clear();
    MarkTopologyDocumentEdited(TextFormat("Renamed topology sector %d", sector->id));
    return true;
}

bool SectorEditor::TryRenameSelectedLight()
{
    if (SelectedTopologyLight() == nullptr) {
        uiState.idEditError = "No light selected";
        statusText = uiState.idEditError;
        return false;
    }

    uiState.idEditError = "Topology light IDs are stable";
    statusText = uiState.idEditError;
    return false;
}

void SectorEditor::OpenDeleteSelectedTopologySectorConfirmation()
{
    const SectorTopologySector* sector = SelectedTopologySector();
    if (sector == nullptr) {
        ClearStaleTopologySelection();
        statusText = "Select a topology sector to delete.";
        return;
    }
    OpenDeleteTopologySectorConfirmation(sector->id);
}

void SectorEditor::OpenDeleteTopologySectorConfirmation(int sectorId)
{
    const SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, sectorId);
    if (sector == nullptr) {
        ClearStaleTopologySelection();
        statusText = "Select a topology sector to delete.";
        return;
    }

    const std::string label = sector->name.empty()
            ? TextFormat("topology sector %d", sector->id)
            : TextFormat("%s", sector->name.c_str());
    OpenConfirmation(
            "Delete Sector",
            TextFormat("Delete sector \"%s\"?", label.c_str()),
            [this, sectorId]() { DeleteSelectedTopologySectorConfirmed(sectorId); });
}

bool SectorEditor::DeleteSelectedTopologySectorConfirmed(int sectorId)
{
    const bool hadPendingSplit = state.pendingTopologyLineSplitAtPoint.active;
    const int pendingSplitLineDefId = state.pendingTopologyLineSplitAtPoint.lineDefId;
    SectorTopologyDeleteSectorResult result;
    std::string error;
    if (!DeleteSectorTopologySector(state.topologyMap, sectorId, &result, &error)) {
        statusText = error.empty() ? "Cannot delete topology sector" : error;
        return false;
    }

    state.hasHoveredVertex = false;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    for (engine::UIFloatInputState& inputState : uiState.topologySectorUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
    for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
    state.pendingTopologyLineSplitAtPoint = PendingTopologyLineSplitAtPoint{};
    ClearStaleTopologySelection();
    state.topologyRenderWarning.clear();
    state.topologyDocumentDirty = true;
    state.hasUnsavedChanges = true;
    const bool pendingSplitTargetRemoved = hadPendingSplit
            && FindSectorTopologyLineDef(state.topologyMap, pendingSplitLineDefId) == nullptr;
    statusText = pendingSplitTargetRemoved
            ? TextFormat(
                    "Deleted topology sector %d; split at point cancelled",
                    result.deletedSectorId)
            : TextFormat(
            "Deleted topology sector %d; removed %d sidedefs, %d linedefs, %d vertices",
            result.deletedSectorId,
            result.removedSideDefCount,
            result.removedLineDefCount,
            result.removedVertexCount);
    return true;
}

bool SectorEditor::DeleteSelectedLight()
{
    const SectorTopologyStaticPointLight* light = SelectedTopologyLight();
    if (light == nullptr) {
        return false;
    }

    const int lightId = light->id;
    OpenConfirmation(
            "Delete Light",
            TextFormat("Delete topology light %d?", lightId),
            [this, lightId]() { DeleteLightById(lightId); });
    return true;
}

bool SectorEditor::DeleteLightById(int topologyLightId)
{
    if (FindSectorTopologyStaticLight(state.topologyMap, topologyLightId) == nullptr) {
        ClearStaleTopologySelection();
        statusText = "Select a topology light to delete.";
        return false;
    }

    if (!RemoveSectorTopologyStaticLight(state.topologyMap, topologyLightId)) {
        statusText = "Failed to delete topology light.";
        return false;
    }

    if (state.selectedTopologyLightId == topologyLightId) {
        ClearSelection();
    }
    if (state.hoveredTopologyLightId == topologyLightId) {
        state.hoveredTopologyLightId = -1;
    }
    if (state.lightDrag.topologyLightId == topologyLightId) {
        state.lightDrag = LightDragState{};
    }
    state.topologyDocumentDirty = true;
    state.hasUnsavedChanges = true;
    statusText = TextFormat("Deleted topology light %d", topologyLightId);
    return true;
}

void SectorEditor::AddStaticLightAt(Vector2 mapPoint)
{
    const int sectorId = FindTopologySectorAt(mapPoint);
    const SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, sectorId);
    if (sector == nullptr) {
        statusText = "Light placement failed: click inside a sector";
        return;
    }

    const int lightId = AllocateSectorTopologyStaticLightId(state.topologyMap);
    if (!IsValidSectorTopologyId(lightId)) {
        statusText = "Light placement failed: no topology light IDs available";
        return;
    }

    SectorTopologyStaticPointLight light;
    light.id = lightId;
    light.position = Vector3{mapPoint.x, sector->floorZ + SectorWorldToAuthoringDistance(1.8f), mapPoint.y};
    light.color = WHITE;
    light.intensity = 1.0f;
    light.radius = SectorWorldToAuthoringDistance(8.0f);
    light.sourceRadius = SectorWorldToAuthoringDistance(0.25f);

    state.topologyMap.staticLights.push_back(light);
    SelectTopologyLight(lightId);
    state.topologyDocumentDirty = true;
    state.hasUnsavedChanges = true;
    statusText = TextFormat("Added topology light %d", lightId);
}

bool SectorEditor::BakeLightmaps()
{
    return StartLightmapBake();
}

bool SectorEditor::StartLightmapBake()
{
    if (lightmapBake.progress.running.load() || lightmapBake.worker.joinable() || lightmapBake.modalOpen) {
        statusText = "Lightmap bake already running";
        return false;
    }

    if (!state.hasCurrentLevelPath) {
        statusText = "Save the level before baking lightmaps";
        return false;
    }

    if (state.topologyMap.sectors.empty()) {
        statusText = "Bake failed: no sectors";
        return false;
    }

    LevelPaths levelPaths;
    std::string pathError;
    if (!BuildLevelPaths(state.currentLevelName, levelPaths, pathError)) {
        statusText = TextFormat("Bake failed: %s", pathError.c_str());
        return false;
    }
    const std::string finalOutputPath = levelPaths.lightmapFilePath.string();
    const std::string temporaryOutputPath = MakeTemporaryLightmapPath(finalOutputPath);

    SectorTopologyLightmapBakeInput input;
    input.mapSnapshot = state.topologyMap;
    input.expectedSourceHash = ComputeSectorLightmapSourceHash(state.topologyMap);
    input.finalOutputPath = finalOutputPath;
    input.temporaryOutputPath = temporaryOutputPath;

    DeleteFileIfExists(temporaryOutputPath);

    lightmapBake.progress.phase.store(SectorLightmapBakePhase::Preparing);
    lightmapBake.progress.completedWork.store(0);
    lightmapBake.progress.totalWork.store(1);
    lightmapBake.progress.cancelRequested.store(false);
    lightmapBake.progress.running.store(true);
    lightmapBake.modalOpen = true;
    lightmapBake.awaitingAcknowledgement = false;
    lightmapBake.cancelButtonPressed = false;
    lightmapBake.terminalMessage.clear();
    lightmapBake.terminalSuccess = false;
    lightmapBake.terminalCancelled = false;
    lightmapBake.temporaryOutputPath = temporaryOutputPath;
    lightmapBake.startTimeSeconds = GetTime();
    lightmapBake.completedTimeSeconds = 0.0;
    {
        std::lock_guard<std::mutex> lock(lightmapBake.resultMutex);
        lightmapBake.pendingResult.reset();
    }

    LightmapBakeProgress* progress = &lightmapBake.progress;
    std::mutex* resultMutex = &lightmapBake.resultMutex;
    std::optional<SectorLightmapBakeAsyncResult>* pendingResult = &lightmapBake.pendingResult;

    lightmapBake.worker = std::thread([input = std::move(input), progress, resultMutex, pendingResult]() mutable {
        SectorLightmapBakeAsyncResult asyncResult;
        asyncResult.expectedSourceHash = input.expectedSourceHash;
        asyncResult.sourceMapRevision = input.editorMapRevision;
        asyncResult.finalOutputPath = input.finalOutputPath;
        asyncResult.temporaryOutputPath = input.temporaryOutputPath;

        SectorLightmapBakeCallbacks callbacks;
        callbacks.onProgress = [progress](SectorLightmapBakePhase phase, uint32_t completedWork, uint32_t totalWork) {
            progress->phase.store(phase);
            progress->completedWork.store(completedWork);
            progress->totalWork.store(totalWork);
        };
        callbacks.isCancellationRequested = [progress]() {
            return progress->cancelRequested.load();
        };

        std::string error;
        const bool succeeded = BakeSectorLightmap(input, callbacks, asyncResult.bakeResult, error);
        asyncResult.cancelled = !succeeded && progress->cancelRequested.load();
        asyncResult.succeeded = succeeded && !asyncResult.cancelled;
        asyncResult.errorMessage = error.empty()
                ? (asyncResult.cancelled ? "Bake cancelled" : "Bake failed")
                : error;
        if (asyncResult.succeeded) {
            asyncResult.bakeReportText = FormatSectorLightmapBakeReport(asyncResult.bakeResult);
        }

        {
            std::lock_guard<std::mutex> lock(*resultMutex);
            *pendingResult = std::move(asyncResult);
        }

        if (progress->cancelRequested.load()) {
            progress->phase.store(SectorLightmapBakePhase::Cancelled);
        } else if (succeeded) {
            progress->phase.store(SectorLightmapBakePhase::Completed);
        } else {
            progress->phase.store(SectorLightmapBakePhase::Failed);
        }
        progress->completedWork.store(1);
        progress->totalWork.store(1);
        progress->running.store(false);
    });

    statusText = "Baking lightmap...";
    return true;
}

void SectorEditor::PollLightmapBakeResult(engine::AssetManager& assets)
{
    std::optional<SectorLightmapBakeAsyncResult> pending;
    {
        std::lock_guard<std::mutex> lock(lightmapBake.resultMutex);
        if (lightmapBake.pendingResult.has_value()) {
            pending = std::move(lightmapBake.pendingResult);
            lightmapBake.pendingResult.reset();
        }
    }

    if (!pending.has_value()) {
        return;
    }

    JoinLightmapBakeWorker();
    lightmapBake.completedTimeSeconds = GetTime();
    ConsumeLightmapBakeResult(*pending, assets);
}

void SectorEditor::RequestLightmapBakeCancel()
{
    if (!lightmapBake.progress.running.load()) {
        return;
    }
    lightmapBake.progress.cancelRequested.store(true);
    lightmapBake.cancelButtonPressed = true;
    statusText = "Cancelling bake...";
}

void SectorEditor::JoinLightmapBakeWorker()
{
    if (lightmapBake.worker.joinable()) {
        lightmapBake.worker.join();
    }
}

void SectorEditor::ShutdownLightmapBake()
{
    if (lightmapBake.progress.running.load()) {
        lightmapBake.progress.cancelRequested.store(true);
    }
    JoinLightmapBakeWorker();
    DeleteFileIfExists(lightmapBake.temporaryOutputPath);
    lightmapBake.temporaryOutputPath.clear();
    {
        std::lock_guard<std::mutex> lock(lightmapBake.resultMutex);
        if (lightmapBake.pendingResult.has_value()) {
            DeleteFileIfExists(lightmapBake.pendingResult->temporaryOutputPath);
            lightmapBake.pendingResult.reset();
        }
    }
    lightmapBake.modalOpen = false;
    lightmapBake.awaitingAcknowledgement = false;
    lightmapBake.progress.running.store(false);
    lightmapBake.progress.cancelRequested.store(false);
    lightmapBake.progress.phase.store(SectorLightmapBakePhase::Idle);
}

bool SectorEditor::IsLightmapBakeBlocking() const
{
    return lightmapBake.modalOpen || lightmapBake.progress.running.load();
}

bool SectorEditor::ConsumeLightmapBakeResult(const SectorLightmapBakeAsyncResult& result, engine::AssetManager& assets)
{
    lightmapBake.progress.phase.store(result.cancelled
            ? SectorLightmapBakePhase::Cancelled
            : (result.succeeded ? SectorLightmapBakePhase::InstallingResult : SectorLightmapBakePhase::Failed));

    if (result.cancelled) {
        DeleteFileIfExists(result.temporaryOutputPath);
        lightmapBake.terminalMessage = "Lightmap bake cancelled";
        lightmapBake.terminalCancelled = true;
        lightmapBake.awaitingAcknowledgement = true;
        statusText = lightmapBake.terminalMessage;
        return false;
    }

    if (!result.succeeded) {
        DeleteFileIfExists(result.temporaryOutputPath);
        lightmapBake.terminalMessage = result.errorMessage.empty() ? "Bake failed" : result.errorMessage;
        lightmapBake.terminalSuccess = false;
        lightmapBake.awaitingAcknowledgement = true;
        statusText = lightmapBake.terminalMessage;
        TraceLog(LOG_WARNING, "%s", lightmapBake.terminalMessage.c_str());
        return false;
    }

    const bool installed = InstallLightmapBakeResult(result, assets);
    lightmapBake.modalOpen = false;
    lightmapBake.awaitingAcknowledgement = false;
    lightmapBake.cancelButtonPressed = false;
    lightmapBake.terminalSuccess = installed;
    lightmapBake.terminalCancelled = false;
    lightmapBake.temporaryOutputPath.clear();
    lightmapBake.progress.phase.store(installed ? SectorLightmapBakePhase::Completed : SectorLightmapBakePhase::Failed);
    return installed;
}

bool SectorEditor::InstallLightmapBakeResult(const SectorLightmapBakeAsyncResult& result, engine::AssetManager& assets)
{
    if (ComputeSectorLightmapSourceHash(state.topologyMap) != result.expectedSourceHash) {
        DeleteFileIfExists(result.temporaryOutputPath);
        statusText = "Bake discarded: document changed during bake";
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(result.temporaryOutputPath, ec) || ec) {
        DeleteFileIfExists(result.temporaryOutputPath);
        statusText = "Bake failed: temporary lightmap output missing";
        return false;
    }

    const std::filesystem::path finalPath(result.finalOutputPath);
    if (!finalPath.parent_path().empty()) {
        std::filesystem::create_directories(finalPath.parent_path(), ec);
        if (ec) {
            DeleteFileIfExists(result.temporaryOutputPath);
            statusText = TextFormat("Bake failed: could not create output directory: %s", ec.message().c_str());
            return false;
        }
    }

    std::filesystem::copy_file(
            result.temporaryOutputPath,
            result.finalOutputPath,
            std::filesystem::copy_options::overwrite_existing,
            ec
    );
    if (ec) {
        DeleteFileIfExists(result.temporaryOutputPath);
        statusText = TextFormat("Bake failed: could not install lightmap: %s", ec.message().c_str());
        return false;
    }
    DeleteFileIfExists(result.temporaryOutputPath);

    LevelPaths levelPaths;
    std::string pathError;
    if (!BuildLevelPaths(state.currentLevelName, levelPaths, pathError)) {
        DeleteFileIfExists(result.temporaryOutputPath);
        statusText = TextFormat("Bake failed: %s", pathError.c_str());
        return false;
    }
    state.topologyMap.bakedLightmap.path = levelPaths.lightmapAssetPath;
    state.topologyMap.bakedLightmap.width = result.bakeResult.width;
    state.topologyMap.bakedLightmap.height = result.bakeResult.height;
    state.topologyMap.bakedLightmap.sourceHash = result.bakeResult.sourceHash;
    state.hasUnsavedChanges = true;
    state.topologyDocumentDirty = true;

    std::istringstream report(result.bakeReportText);
    std::string line;
    while (std::getline(report, line)) {
        TraceLog(LOG_INFO, "%s", line.c_str());
    }
    TraceLog(LOG_INFO, "INFO: Lightmap bake completed asynchronously in %.2fs", result.bakeResult.totalBakeSeconds);

    if (state.mode == SectorEditorMode::Preview3D && preview.IsReady()) {
        RebuildPreviewMeshesPreservingView(assets);
    }

    statusText = TextFormat("Baked lightmap in %.1fs", result.bakeResult.totalBakeSeconds);
    return true;
}

bool SectorEditor::FindTopologyVertexNearScreenPoint(
        Vector2 screenPoint,
        int& outVertexId,
        SectorTopologyCoordPoint& outPoint) const
{
    float bestDistance2 = ScreenVertexSnapPixels * ScreenVertexSnapPixels;
    int bestVertexId = -1;
    SectorTopologyCoordPoint bestPoint{};

    for (const SectorTopologyVertex& vertex : state.topologyMap.vertices) {
        const Vector2 screenVertex = MapToScreen(SectorTopologyVertexToMap(vertex));
        const float dx = screenVertex.x - screenPoint.x;
        const float dy = screenVertex.y - screenPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 > bestDistance2) {
            continue;
        }
        if (bestVertexId >= 0
                && std::fabs(distance2 - bestDistance2) <= 0.001f
                && vertex.id >= bestVertexId) {
            continue;
        }
        bestDistance2 = distance2;
        bestVertexId = vertex.id;
        bestPoint = SectorTopologyCoordPoint{vertex.x, vertex.y};
    }

    if (bestVertexId < 0) {
        outVertexId = -1;
        outPoint = SectorTopologyCoordPoint{};
        return false;
    }

    outVertexId = bestVertexId;
    outPoint = bestPoint;
    return true;
}

bool SectorEditor::FindSelectedSectorBoundaryCutPointNearScreenPoint(
        Vector2 screenPoint,
        SectorTopologyBoundaryCutPoint& outPoint) const
{
    outPoint = SectorTopologyBoundaryCutPoint{};
    const SectorTopologySector* sector = SelectedTopologySector();
    if (sector == nullptr) {
        return false;
    }

    SectorTopologyLoopSet loops;
    if (!ExtractSectorTopologyLoops(state.topologyMap, sector->id, loops)) {
        return false;
    }

    float bestVertexDistance2 = ScreenVertexSnapPixels * ScreenVertexSnapPixels;
    int bestVertexId = -1;
    SectorTopologyCoordPoint bestVertexPoint{};
    for (int vertexId : loops.outer.vertexIds) {
        const SectorTopologyVertex* vertex = FindSectorTopologyVertex(state.topologyMap, vertexId);
        if (vertex == nullptr) {
            continue;
        }
        const Vector2 screenVertex = MapToScreen(SectorTopologyVertexToMap(*vertex));
        const float dx = screenVertex.x - screenPoint.x;
        const float dy = screenVertex.y - screenPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 > bestVertexDistance2) {
            continue;
        }
        if (bestVertexId >= 0
                && std::fabs(distance2 - bestVertexDistance2) <= 0.001f
                && vertex->id >= bestVertexId) {
            continue;
        }
        bestVertexDistance2 = distance2;
        bestVertexId = vertex->id;
        bestVertexPoint = SectorTopologyCoordPoint{vertex->x, vertex->y};
    }
    if (bestVertexId >= 0) {
        outPoint.vertexId = bestVertexId;
        outPoint.point = bestVertexPoint;
        return true;
    }

    SectorTopologyCoordPoint snappedPoint;
    std::string error;
    if (!ToTopologyCoordPoint(Vector2ToSectorPoint(state.snappedMouseMap), snappedPoint, error)) {
        return false;
    }

    float bestLineDistance = ScreenEdgePickPixels;
    int bestLineDefId = -1;
    for (const SectorTopologyLoopEdge& edge : loops.outer.edges) {
        const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(state.topologyMap, edge.lineDefId);
        if (lineDef == nullptr) {
            continue;
        }
        const SectorTopologyVertex* start = nullptr;
        const SectorTopologyVertex* end = nullptr;
        if (!GetSectorTopologyLineVertices(state.topologyMap, *lineDef, start, end)) {
            continue;
        }
        const SectorTopologyCoordPoint startPoint{start->x, start->y};
        const SectorTopologyCoordPoint endPoint{end->x, end->y};
        if (!SectorTopologyPointStrictlyInsideSegment(snappedPoint, startPoint, endPoint)) {
            continue;
        }

        const Vector2 screenStart = MapToScreen(SectorTopologyVertexToMap(*start));
        const Vector2 screenEnd = MapToScreen(SectorTopologyVertexToMap(*end));
        const float distance = DistancePointToSegment(screenPoint, screenStart, screenEnd);
        if (distance > ScreenEdgePickPixels) {
            continue;
        }
        if (bestLineDefId >= 0
                && (distance > bestLineDistance + 0.001f
                        || (std::fabs(distance - bestLineDistance) <= 0.001f
                                && lineDef->id >= bestLineDefId))) {
            continue;
        }
        bestLineDistance = distance;
        bestLineDefId = lineDef->id;
    }

    if (bestLineDefId < 0) {
        return false;
    }

    outPoint.lineDefId = bestLineDefId;
    outPoint.point = snappedPoint;
    return true;
}

int SectorEditor::FindTopologyVertexAtCoordPoint(
        SectorTopologyCoordPoint point,
        int excludedVertexId) const
{
    int bestVertexId = -1;
    for (const SectorTopologyVertex& vertex : state.topologyMap.vertices) {
        if (vertex.id == excludedVertexId) {
            continue;
        }
        if (vertex.x != point.x || vertex.y != point.y) {
            continue;
        }
        if (bestVertexId < 0 || vertex.id < bestVertexId) {
            bestVertexId = vertex.id;
        }
    }
    return bestVertexId;
}

bool SectorEditor::SnapTopologyVertexMoveTarget(
        Vector2 mapPoint,
        SectorTopologyCoordPoint& outPoint,
        std::string& error) const
{
    const float grid = static_cast<float>(std::max(1, state.gridSize));
    Vector2 snapped{
            std::round(mapPoint.x / grid) * grid,
            std::round(mapPoint.y / grid) * grid
    };

    const float threshold = std::max(
            SectorWorldToAuthoringDistance(ScreenVertexSnapPixels / std::max(1.0f, state.viewZoom)),
            grid * 0.20f
    );
    float bestDistance2 = threshold * threshold;
    bool found = false;
    Vector2 best = snapped;
    int bestVertexId = -1;

    for (const SectorTopologyVertex& vertex : state.topologyMap.vertices) {
        if (state.vertexDrag.active && vertex.id == state.vertexDrag.topologyVertexId) {
            continue;
        }

        const Vector2 vertexMap = SectorTopologyVertexToMap(vertex);
        const float dx = vertexMap.x - mapPoint.x;
        const float dy = vertexMap.y - mapPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 > bestDistance2) {
            continue;
        }
        if (found
                && std::fabs(distance2 - bestDistance2) <= 0.001f
                && vertex.id >= bestVertexId) {
            continue;
        }

        bestDistance2 = distance2;
        best = vertexMap;
        bestVertexId = vertex.id;
        found = true;
    }

    const Vector2 canonical = found ? best : snapped;
    SectorCoord x = 0;
    SectorCoord y = 0;
    if (!VisibleAuthoringToSectorCoord(canonical.x, x)
            || !VisibleAuthoringToSectorCoord(canonical.y, y)) {
        error = "Move target is outside topology coordinate range";
        outPoint = SectorTopologyCoordPoint{};
        return false;
    }

    outPoint = SectorTopologyCoordPoint{x, y};
    error.clear();
    return true;
}

void SectorEditor::RenderPreview3D(engine::AssetManager& assets)
{
    preview.Render(assets, state.useBakedAmbientOcclusion);
    if (!state.previewUiHidden) {
        DrawPreviewSurfaceHighlights();
    }
}

SectorSurfaceHit SectorEditor::PickSectorSurface3D(Vector2 mousePosition, Rectangle viewportRect) const
{
    SectorSurfaceHit best;
    if (!preview.IsReady()) {
        return best;
    }

    const Vector2 localMouse{
            mousePosition.x - viewportRect.x,
            mousePosition.y - viewportRect.y
    };
    const Ray ray = GetScreenToWorldRayEx(
            localMouse,
            preview.Camera(),
            static_cast<int>(std::round(viewportRect.width)),
            static_cast<int>(std::round(viewportRect.height))
    );

    const SectorGeneratedSurfaceHit hit = PickSectorGeneratedGeometry(
            preview.GeneratedGeometry(),
            ray,
            GeometryEpsilon);
    if (!hit.hit) {
        return best;
    }

    best.hit = true;
    best.surface = ToEditorSurfaceRef(hit.ref);
    best.worldPosition = hit.worldPosition;
    best.distance = hit.distance;
    return best;
}

void SectorEditor::DrawPreviewSurfaceHighlights() const
{
    if (!preview.IsReady() || preview.IsMouseLookEnabled()) {
        return;
    }

    auto drawSurface = [this](SectorSurfaceRef surface, Color color, float thickness) {
        if (!IsValidSurfaceRef(surface)) {
            return;
        }
        const float lift = IsWallSurface(surface.kind) ? PreviewHighlightLift : PreviewHighlightLift * 2.0f;
        for (const SectorGeneratedSurface& generated : preview.GeneratedGeometry().surfaces) {
            const SectorSurfaceRef generatedRef = ToEditorSurfaceRef(generated.ref);
            if (!SameSurfaceRef(surface, generatedRef)) {
                continue;
            }
            const Vector3 offset = Vector3Scale(generated.normal, lift);
            for (size_t i = 0; i + 2 < generated.vertices.size(); i += 3) {
                const Vector3 a = Vector3Add(generated.vertices[i + 0].position, offset);
                const Vector3 b = Vector3Add(generated.vertices[i + 1].position, offset);
                const Vector3 c = Vector3Add(generated.vertices[i + 2].position, offset);
                DrawLine3D(a, b, color);
                DrawLine3D(b, c, color);
                DrawLine3D(c, a, color);
            }
        }
        (void)thickness;
    };

    BeginMode3D(preview.Camera());
    if (state.hoveredSurface3D.hit
            && !SameSurfaceRef(state.hoveredSurface3D.surface, state.selectedSurface3D)) {
        drawSurface(state.hoveredSurface3D.surface, Color{248, 238, 124, 235}, 2.0f);
    }
    if (state.selectedSurface3D.kind != SectorSurfaceKind::None) {
        drawSurface(state.selectedSurface3D, Color{84, 204, 255, 255}, 3.0f);
    }
    EndMode3D();
}

void SectorEditor::DrawPreviewOverlay(
        const engine::UIConfig& config,
        engine::AssetManager& assets,
        engine::FontHandle font) const
{
    const Rectangle panel{32.0f, 32.0f, 760.0f, 172.0f};
    DrawRectangleRec(panel, Color{12, 15, 20, 205});
    DrawRectangleLinesEx(panel, config.borderThickness, config.borderColor);

    const Vector3 position = preview.Position();
    engine::Text(
            config,
            assets,
            Rectangle{panel.x + 18.0f, panel.y + 14.0f, panel.width - 36.0f, 34.0f},
            font,
            "3D Mode",
            engine::UITextJustify::Left
    );
    const char* interactionText = preview.IsMouseLookEnabled()
            ? "WASD move | Mouse look | Space/Ctrl up/down | F1 AO | F2 hide UI | F11 cursor | Tab/Escape return"
            : "F1 AO | F2 hide UI | F11 cursor | click surface to select | Tab/Escape return";
    engine::Text(
            config,
            assets,
            Rectangle{panel.x + 18.0f, panel.y + 54.0f, panel.width - 36.0f, 30.0f},
            font,
            interactionText,
            engine::UITextJustify::Left,
            config.mutedTextColor
    );
    engine::Text(
            config,
            assets,
            Rectangle{panel.x + 18.0f, panel.y + 92.0f, panel.width - 36.0f, 30.0f},
            font,
            TextFormat(
                    "pos %.2f %.2f %.2f | sectors %zu | batches %zu | triangles %d",
                    position.x,
                    position.y,
                    position.z,
                    preview.SectorCount(),
                    preview.BatchCount(),
                    preview.TriangleCount()
            ),
            engine::UITextJustify::Left,
            config.mutedTextColor
    );
    engine::Text(
            config,
            assets,
            Rectangle{panel.x + 18.0f, panel.y + 130.0f, panel.width - 36.0f, 30.0f},
            font,
            TextFormat(
                    "assets %.0f%% | Lightmap: %s | AO: %s | %s%s",
                    preview.AssetProgress(assets) * 100.0f,
                    preview.LightmapStatusText(),
                    state.useBakedAmbientOcclusion ? "on" : "off",
                    statusText.empty() ? "Ready" : statusText.c_str(),
                    state.topologyDocumentDirty ? " | unsaved changes" : ""
            ),
            engine::UITextJustify::Left,
            state.topologyDocumentDirty ? Color{236, 196, 92, 255} : config.mutedTextColor
    );
}

Rectangle SectorEditor::BuildPreviewUvPanelRect() const
{
    return Rectangle{330.0f, EditorHeight - 252.0f, 1260.0f, 220.0f};
}

void SectorEditor::DrawPreviewUvPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    if (preview.IsMouseLookEnabled()) {
        return;
    }

    if (!IsValidSurfaceRef(state.selectedSurface3D)
            || !IsValidTopologySurfaceEditTarget(state.selectedTopologySurface3D)) {
        state.selectedSurface3D = SectorSurfaceRef{};
        state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
        return;
    }

    const TopologySurfaceEditTarget target = state.selectedTopologySurface3D;
    const TopologyMaterialLayer layer = state.activeTopologyMaterialLayer;
    const Rectangle panel = BuildPreviewUvPanelRect();
    DrawRectangleRec(panel, Color{12, 15, 20, 230});
    DrawRectangleLinesEx(panel, config.borderThickness, config.borderColor);

    std::string targetLabel;

    if (target.kind == TopologySurfaceEditTargetKind::SectorFloor
            || target.kind == TopologySurfaceEditTargetKind::SectorCeiling) {
        const SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, target.sectorId);
        if (sector == nullptr) {
            state.selectedSurface3D = SectorSurfaceRef{};
            state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
            return;
        }
        targetLabel = TextFormat(
                "%s | sector %d",
                target.kind == TopologySurfaceEditTargetKind::SectorFloor ? "Floor" : "Ceiling",
                sector->id);
    } else {
        const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, target.sideDefId);
        if (sideDef == nullptr) {
            state.selectedSurface3D = SectorSurfaceRef{};
            state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
            return;
        }
        const TopologyWallPart wallPart = TopologyEditTargetWallPart(target.kind);
        (void)wallPart;
        targetLabel = TextFormat(
                "%s | sideDef %d line %d",
                SurfaceKindName(state.selectedSurface3D.kind),
                sideDef->id,
                sideDef->lineDefId);
    }

    const float margin = 18.0f;
    const float top = panel.y + margin;
    const float inputTop = panel.y + 104.0f;
    const float colW = 132.0f;
    const float gap = 14.0f;
    const float startX = panel.x + 390.0f;

    engine::Text(
            ui,
            config,
            assets,
            Rectangle{panel.x + margin, top, 350.0f, 34.0f},
            font,
            targetLabel.c_str(),
            engine::UITextJustify::Left,
            config.textColor
    );
    const float layerLabelW = 68.0f;
    const float layerButtonW = 78.0f;
    engine::Text(
            ui,
            config,
            assets,
            Rectangle{panel.x + margin, top + 36.0f, layerLabelW, 30.0f},
            font,
            "Layer:",
            engine::UITextJustify::Left,
            config.mutedTextColor);
    if (engine::ToolButton(
                ui,
                config,
                input,
                assets,
                "sector_editor_3d_layer_base",
                Rectangle{panel.x + margin + layerLabelW, top + 34.0f, layerButtonW, 32.0f},
                font,
                "Base",
                layer == TopologyMaterialLayer::Base)) {
        state.activeTopologyMaterialLayer = TopologyMaterialLayer::Base;
        ResetSurface3DUiState();
    }
    if (engine::ToolButton(
                ui,
                config,
                input,
                assets,
                "sector_editor_3d_layer_decal",
                Rectangle{panel.x + margin + layerLabelW + layerButtonW + 8.0f, top + 34.0f, layerButtonW, 32.0f},
                font,
                "Decal",
                layer == TopologyMaterialLayer::Decal)) {
        state.activeTopologyMaterialLayer = TopologyMaterialLayer::Decal;
        ResetSurface3DUiState();
    }

    const std::string currentTexture = CurrentTextureForSurface(target, layer);
    const bool missingTexture = !currentTexture.empty()
            && FindSectorTopologyTexture(state.topologyMap, currentTexture) == nullptr;
    engine::Text(
            ui,
            config,
            assets,
            Rectangle{panel.x + margin, top + 72.0f, 350.0f, 30.0f},
            font,
            TextFormat("%s texture %s", TopologyMaterialLayerName(layer), currentTexture.empty() ? "<none>" : currentTexture.c_str()),
            engine::UITextJustify::Left,
            missingTexture ? config.invalidColor : config.mutedTextColor
    );

    const bool decalAssigned = layer != TopologyMaterialLayer::Decal || IsDecalAssigned(target);
    const SectorTopologyUvSettings* uv = UvForSurface(target, layer);
    Vector2 uvScale = uv == nullptr ? Vector2{1.0f, 1.0f} : uv->scale;
    Vector2 uvOffset = uv == nullptr ? Vector2{0.0f, 0.0f} : uv->offset;
    if (!decalAssigned) {
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{startX, inputTop, 390.0f, 34.0f},
                font,
                "No decal assigned",
                engine::UITextJustify::Left,
                config.mutedTextColor);
    }

    auto drawFloat = [&](const char* id, const char* label, float value, engine::UIFloatInputState& inputState, int component, float minValue, float maxValue, float x) {
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{x, inputTop - 28.0f, colW, 24.0f},
                font,
                label,
                engine::UITextJustify::Left,
                config.mutedTextColor
        );
        float edited = value;
        const engine::UINumericInputResult result = engine::FloatInput(
                ui,
                config,
                input,
                assets,
                id,
                Rectangle{x, inputTop, colW, 38.0f},
                font,
                edited,
                inputState,
                minValue,
                maxValue,
                3
        );
        if (result.changed && edited != value && std::isfinite(edited)) {
            ApplySurface3DUvValue(target, layer, component, edited, assets);
        }
    };

    if (decalAssigned) {
        drawFloat("sector_editor_3d_uv_scale_u", "Scale U", uvScale.x, uiState.surface3DUvScaleUInput, 0, TopologyUvScaleMin, TopologyUvScaleMax, startX);
        drawFloat("sector_editor_3d_uv_scale_v", "Scale V", uvScale.y, uiState.surface3DUvScaleVInput, 1, TopologyUvScaleMin, TopologyUvScaleMax, startX + (colW + gap));
        drawFloat("sector_editor_3d_uv_offset_u", "Offset U", uvOffset.x, uiState.surface3DUvOffsetUInput, 2, -1024.0f, 1024.0f, startX + (colW + gap) * 2.0f);
        drawFloat("sector_editor_3d_uv_offset_v", "Offset V", uvOffset.y, uiState.surface3DUvOffsetVInput, 3, -1024.0f, 1024.0f, startX + (colW + gap) * 3.0f);
    }

    const float actionTop = inputTop + 52.0f;
    const float actionH = 34.0f;
    const float smallActionW = 96.0f;
    float actionX = startX;
    auto openTexturePicker = [&]() {
        if (target.kind == TopologySurfaceEditTargetKind::SectorFloor
                || target.kind == TopologySurfaceEditTargetKind::SectorCeiling) {
            OpenTopologyTexturePicker(target.sectorId, TopologyEditTargetSectorTextureField(target.kind), layer);
        } else {
            OpenTopologySideDefTexturePicker(target.sideDefId, TopologyEditTargetWallPart(target.kind), layer);
        }
        if (state.texturePicker.open) {
            state.texturePicker.rebuildPreviewOnApply = true;
        }
    };
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_3d_texture",
                Rectangle{actionX, actionTop, smallActionW, actionH},
                font,
                "Texture")) {
        openTexturePicker();
    }
    actionX += smallActionW + gap;

    if (decalAssigned) {
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_reset_uv",
                    Rectangle{actionX, actionTop, smallActionW, actionH},
                    font,
                    "Reset UV")) {
            ResetSurface3DUv(target, layer, assets);
        }
        actionX += smallActionW + gap;
    }

    if (layer == TopologyMaterialLayer::Decal && decalAssigned) {
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_fit_decal",
                    Rectangle{actionX, actionTop, smallActionW, actionH},
                    font,
                    "Fit Decal")) {
            FitSelectedDecal(target, &assets);
        }
        actionX += smallActionW + gap;
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_clear_decal",
                    Rectangle{actionX, actionTop, 104.0f, actionH},
                    font,
                    "Clear Decal")) {
            ClearSurfaceDecal(target, &assets);
        }
        actionX += 104.0f + gap;
    }

    if (IsWallTopologyEditTarget(target.kind) && decalAssigned && layer == TopologyMaterialLayer::Base) {
        const float fitButtonW = 118.0f;
        const float fitTop = panel.y + 194.0f;
        const float fitButtonH = 26.0f;
        const float alignStartX = startX + (fitButtonW + gap) * 3.0f;
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_fit_width",
                    Rectangle{startX, fitTop, fitButtonW, fitButtonH},
                    font,
                    "Fit Width")) {
            FitSelectedWallMaterial(target, TopologyUvFitMode::Width, &assets, layer);
        }
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_fit_height",
                    Rectangle{startX + fitButtonW + gap, fitTop, fitButtonW, fitButtonH},
                    font,
                    "Fit Height")) {
            FitSelectedWallMaterial(target, TopologyUvFitMode::Height, &assets, layer);
        }
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_fit_both",
                    Rectangle{startX + (fitButtonW + gap) * 2.0f, fitTop, fitButtonW, fitButtonH},
                    font,
                    "Fit Both")) {
            FitSelectedWallMaterial(target, TopologyUvFitMode::Both, &assets, layer);
        }
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_align_vertical",
                    Rectangle{alignStartX, fitTop, fitButtonW, fitButtonH},
                    font,
                    "Align Vertical")) {
            AlignSelectedWallMaterialVertical(target, &assets, layer);
        }
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_align_u_prev",
                    Rectangle{alignStartX + fitButtonW + gap, fitTop, fitButtonW, fitButtonH},
                    font,
                    "Align U Prev")) {
            AlignSelectedWallMaterialU(target, TopologyUAlignDirection::Previous, &assets, layer);
        }
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_align_u_next",
                    Rectangle{alignStartX + (fitButtonW + gap) * 2.0f, fitTop, fitButtonW, fitButtonH},
                    font,
                    "Align U Next")) {
            AlignSelectedWallMaterialU(target, TopologyUAlignDirection::Next, &assets, layer);
        }
    }

    if (layer == TopologyMaterialLayer::Decal && decalAssigned) {
        const SectorTopologyDecalLayer* decal = DecalForSurface(target);
        if (decal != nullptr) {
            engine::Text(ui, config, assets, Rectangle{startX + (colW + gap) * 4.0f, inputTop - 28.0f, colW, 24.0f}, font, "Opacity", engine::UITextJustify::Left, config.mutedTextColor);
            float opacity = decal->opacity;
            const engine::UINumericInputResult result = engine::FloatInput(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_decal_opacity",
                    Rectangle{startX + (colW + gap) * 4.0f, inputTop, colW, 38.0f},
                    font,
                    opacity,
                    uiState.surface3DDecalOpacityInput,
                    0.0f,
                    1.0f,
                    3);
            if (result.changed && opacity != decal->opacity && std::isfinite(opacity)) {
                ApplySurfaceDecalOpacity(target, opacity, &assets);
            }
            bool emissive = decal->emissive;
            if (engine::Checkbox(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_3d_decal_emissive",
                        Rectangle{actionX, actionTop, 112.0f, actionH},
                        font,
                        "Emissive",
                        emissive)) {
                ApplySurfaceDecalEmissive(target, emissive, &assets);
            }
            actionX += 112.0f + gap;
            const Rectangle label{actionX, actionTop, 36.0f, actionH};
            engine::Text(ui, config, assets, label, font, "Tint:", engine::UITextJustify::Left, config.mutedTextColor);
            const Rectangle swatch{label.x + label.width + 8.0f, label.y + 1.0f, 48.0f, actionH - 2.0f};
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_3d_decal_tint",
                        swatch,
                        font,
                        "")) {
                OpenDecalTintModal(target);
            }
            DrawRectangleRec(swatch, DecalTintPreviewColor(decal->tint));
            DrawRectangleLinesEx(swatch, config.borderThickness, config.borderColor);
        }
    } else if (layer == TopologyMaterialLayer::Base) {
        if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_3d_copy_material",
                Rectangle{actionX, actionTop, 112.0f, actionH},
                font,
                "Copy Material")) {
            CopyTopologyMaterial(target);
        }
        actionX += 112.0f + gap;

        if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_3d_paste_material",
                Rectangle{actionX, actionTop, 112.0f, actionH},
                font,
                "Paste Material")) {
            PasteTopologyMaterial(target, assets);
        }
    }

    input.ForEachEvent(
            engine::InputEventType::MouseClick,
            true,
            [panel](engine::InputEvent& event) {
                if (Contains(panel, event.mouseClick.releasePosition)
                        || Contains(panel, event.mouseClick.pressPosition)) {
                    engine::ConsumeEvent(event);
                }
            }
    );
}

void SectorEditor::DrawGrid() const
{
    const int grid = std::max(1, state.gridSize);
    const Vector2 minMap = ScreenToMap(Vector2{canvasRect.x, canvasRect.y});
    const Vector2 maxMap = ScreenToMap(Vector2{canvasRect.x + canvasRect.width, canvasRect.y + canvasRect.height});
    const int startX = static_cast<int>(std::floor(minMap.x / static_cast<float>(grid))) * grid;
    const int endX = static_cast<int>(std::ceil(maxMap.x / static_cast<float>(grid))) * grid;
    const int startY = static_cast<int>(std::floor(minMap.y / static_cast<float>(grid))) * grid;
    const int endY = static_cast<int>(std::ceil(maxMap.y / static_cast<float>(grid))) * grid;

    const Color gridColor{44, 50, 62, 155};
    const Color majorColor{62, 70, 86, 185};
    const Color axisColor{112, 148, 148, 230};

    for (int x = startX; x <= endX; x += grid) {
        const Vector2 a = MapToScreen(Vector2{static_cast<float>(x), minMap.y});
        const Vector2 b = MapToScreen(Vector2{static_cast<float>(x), maxMap.y});
        const bool axis = x == 0;
        const bool major = grid < 8 && x % 8 == 0;
        DrawLineEx(a, b, axis && state.showAxes ? 2.0f : 1.0f, axis && state.showAxes ? axisColor : (major ? majorColor : gridColor));
    }

    for (int y = startY; y <= endY; y += grid) {
        const Vector2 a = MapToScreen(Vector2{minMap.x, static_cast<float>(y)});
        const Vector2 b = MapToScreen(Vector2{maxMap.x, static_cast<float>(y)});
        const bool axis = y == 0;
        const bool major = grid < 8 && y % 8 == 0;
        DrawLineEx(a, b, axis && state.showAxes ? 2.0f : 1.0f, axis && state.showAxes ? axisColor : (major ? majorColor : gridColor));
    }

    if (state.showAxes) {
        const Vector2 origin = MapToScreen(Vector2{0.0f, 0.0f});
        DrawCircleV(origin, 5.0f, Color{180, 210, 190, 255});
    }
}

void SectorEditor::DrawTopologyDocument()
{
    state.topologyRenderWarning.clear();
    if (!initialized) {
        DrawText("Topology map failed to load", static_cast<int>(canvasRect.x + 24.0f), static_cast<int>(canvasRect.y + 24.0f), 28, RED);
        return;
    }

    const auto issues = ValidateSectorTopologyMap(state.topologyMap);
    if (!issues.empty()) {
        state.topologyRenderWarning = "Topology render warning: "
                + FormatSectorTopologyValidationIssue(issues.front());
    }

    const Color fill = Color{82, 112, 154, 42};
    const Color outline = Color{116, 139, 174, 235};
    const Color selectedFill = Color{72, 220, 128, 38};
    const Color selectedOutline = Color{86, 232, 142, 135};
    ClearStaleTopologySelection();
    const SectorTopologySector* selectedSector = SelectedTopologySector();
    for (const SectorTopologySector& sector : state.topologyMap.sectors) {
        if (selectedSector != nullptr && sector.id == selectedSector->id) {
            continue;
        }
        DrawTopologySectorLoops(sector, fill, outline);
    }
    if (selectedSector != nullptr) {
        DrawTopologySectorLoops(*selectedSector, selectedFill, selectedOutline, 10.0f);
    }

    DrawTopologySelectedLineHighlight();
    DrawTopologyLineDefs();
    DrawTopologyVertices();
    DrawVertexMoveOverlay();
    DrawPendingTopologyVertexMerge();
    DrawPendingTopologyLineSplitAtPoint();
    DrawPendingTopologySectorCut();
    DrawStaticLights();
    DrawLightMoveOverlay();
    DrawPendingSector();
    DrawTopologySnapCrosshair();

    if (!state.topologyRenderWarning.empty()) {
        DrawText(
                state.topologyRenderWarning.c_str(),
                static_cast<int>(canvasRect.x + 16.0f),
                static_cast<int>(canvasRect.y + 14.0f),
                18,
                Color{236, 196, 92, 255}
        );
    }
}

void SectorEditor::DrawTopologySectorLoops(
        const SectorTopologySector& sector,
        Color fill,
        Color outline,
        float outlineThickness)
{
    SectorTopologyLoopSet loops;
    std::vector<SectorTopologyValidationIssue> loopIssues;
    if (!ExtractSectorTopologyLoops(state.topologyMap, sector.id, loops, &loopIssues)) {
        if (state.topologyRenderWarning.empty() && !loopIssues.empty()) {
            state.topologyRenderWarning = "Topology render warning: "
                    + FormatSectorTopologyValidationIssue(loopIssues.front());
        }
        return;
    }

    using DrawEarcutPoint = std::array<double, 2>;
    std::vector<std::vector<DrawEarcutPoint>> polygon;
    std::vector<Vector2> flattened;
    bool missingVertex = false;
    const auto appendLoop = [&](const SectorTopologyLoop& loop) {
        polygon.emplace_back();
        polygon.back().reserve(loop.vertexIds.size());
        for (int vertexId : loop.vertexIds) {
            const SectorTopologyVertex* vertex = FindSectorTopologyVertex(state.topologyMap, vertexId);
            if (vertex == nullptr) {
                missingVertex = true;
                continue;
            }
            const Vector2 map = SectorTopologyVertexToMap(*vertex);
            polygon.back().push_back(DrawEarcutPoint{map.x, map.y});
            flattened.push_back(map);
        }
    };
    appendLoop(loops.outer);
    for (const SectorTopologyLoop& hole : loops.holes) {
        appendLoop(hole);
    }

    if (missingVertex) {
        if (state.topologyRenderWarning.empty()) {
            state.topologyRenderWarning = TextFormat(
                    "Topology render warning: sector %d references a missing loop vertex",
                    sector.id
            );
        }
        return;
    }

    const std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(polygon);
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        if (indices[i] < flattened.size()
                && indices[i + 1] < flattened.size()
                && indices[i + 2] < flattened.size()) {
            DrawTriangle(
                    MapToScreen(flattened[indices[i]]),
                    MapToScreen(flattened[indices[i + 1]]),
                    MapToScreen(flattened[indices[i + 2]]),
                    fill
            );
        }
    }

    const auto drawLoopOutline = [&](const SectorTopologyLoop& loop, Color color) {
        if (loop.vertexIds.size() < 2) {
            return;
        }
        for (size_t i = 0; i < loop.vertexIds.size(); ++i) {
            const SectorTopologyVertex* a = FindSectorTopologyVertex(state.topologyMap, loop.vertexIds[i]);
            const SectorTopologyVertex* b = FindSectorTopologyVertex(
                    state.topologyMap,
                    loop.vertexIds[(i + 1) % loop.vertexIds.size()]
            );
            if (a == nullptr || b == nullptr) {
                continue;
            }
            DrawLineEx(
                    MapToScreen(SectorTopologyVertexToMap(*a)),
                    MapToScreen(SectorTopologyVertexToMap(*b)),
                    outlineThickness,
                    color
            );
        }
    };
    drawLoopOutline(loops.outer, outline);
    for (const SectorTopologyLoop& hole : loops.holes) {
        drawLoopOutline(hole, Color{164, 187, 220, 245});
    }

    if (state.showSectorIds && !loops.outer.vertexIds.empty()) {
        Vector2 center{};
        int count = 0;
        for (int vertexId : loops.outer.vertexIds) {
            const SectorTopologyVertex* vertex = FindSectorTopologyVertex(state.topologyMap, vertexId);
            if (vertex == nullptr) {
                continue;
            }
            const Vector2 map = SectorTopologyVertexToMap(*vertex);
            center.x += map.x;
            center.y += map.y;
            ++count;
        }
        if (count > 0) {
            center.x /= static_cast<float>(count);
            center.y /= static_cast<float>(count);
            const Vector2 screen = MapToScreen(center);
            const char* label = sector.name.empty()
                    ? TextFormat("%d", sector.id)
                    : sector.name.c_str();
            DrawText(label, static_cast<int>(screen.x - 18.0f), static_cast<int>(screen.y - 10.0f), 18, RAYWHITE);
        }
    }
}

void SectorEditor::DrawTopologySelectedLineHighlight() const
{
    if (state.topologySelectionKind != TopologySelectionKind::SideDef
            && state.topologySelectionKind != TopologySelectionKind::LineDef) {
        return;
    }

    const SectorTopologyLineDef* lineDef = SelectedTopologyLineDef();
    if (lineDef == nullptr) {
        return;
    }

    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (!GetSectorTopologyLineVertices(state.topologyMap, *lineDef, start, end)) {
        return;
    }

    Vector2 a = MapToScreen(SectorTopologyVertexToMap(*start));
    Vector2 b = MapToScreen(SectorTopologyVertexToMap(*end));
    Vector2 dir{b.x - a.x, b.y - a.y};
    const float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (length <= GeometryEpsilon) {
        return;
    }

    dir.x /= length;
    dir.y /= length;
    Vector2 normal{-dir.y, dir.x};
    Color color{72, 210, 246, 138};
    if (state.topologySelectionKind == TopologySelectionKind::LineDef) {
        normal = Vector2{0.0f, 0.0f};
        color = Color{210, 214, 224, 125};
    } else if (state.selectedTopologySideKind == SectorTopologySideKind::Back) {
        normal.x = -normal.x;
        normal.y = -normal.y;
        color = Color{94, 238, 186, 132};
    }

    const float offset = state.topologySelectionKind == TopologySelectionKind::LineDef ? 0.0f : 7.0f;
    a.x += normal.x * offset;
    a.y += normal.y * offset;
    b.x += normal.x * offset;
    b.y += normal.y * offset;
    DrawLineEx(a, b, 10.0f, color);
}

void SectorEditor::DrawTopologyLineDefs() const
{
    const Color oneSidedColor = Color{142, 184, 230, 255};
    const Color twoSidedColor = Color{122, 220, 244, 255};
    const Color warningColor = Color{230, 82, 82, 255};
    const Color frontColor = Color{196, 244, 255, 230};
    const Color backColor = Color{236, 154, 214, 220};
    const Color arrowColor = Color{236, 196, 92, 240};

    for (const SectorTopologyLineDef& lineDef : state.topologyMap.lineDefs) {
        const SectorTopologyVertex* start = nullptr;
        const SectorTopologyVertex* end = nullptr;
        const bool validEndpoints = GetSectorTopologyLineVertices(state.topologyMap, lineDef, start, end);
        if (!validEndpoints) {
            const SectorTopologyVertex* partial = FindSectorTopologyVertex(state.topologyMap, lineDef.startVertexId);
            if (partial == nullptr) {
                partial = FindSectorTopologyVertex(state.topologyMap, lineDef.endVertexId);
            }
            if (partial != nullptr) {
                const Vector2 point = MapToScreen(SectorTopologyVertexToMap(*partial));
                DrawCircleLines(static_cast<int>(std::round(point.x)), static_cast<int>(std::round(point.y)), 11.0f, warningColor);
            }
            continue;
        }

        const bool hasFront = lineDef.frontSideDefId >= 0
                && FindSectorTopologySideDef(state.topologyMap, lineDef.frontSideDefId) != nullptr;
        const bool hasBack = lineDef.backSideDefId >= 0
                && FindSectorTopologySideDef(state.topologyMap, lineDef.backSideDefId) != nullptr;
        const bool twoSided = hasFront && hasBack;
        const Color lineColor = validEndpoints ? (twoSided ? twoSidedColor : oneSidedColor) : warningColor;

        const Vector2 a = MapToScreen(SectorTopologyVertexToMap(*start));
        const Vector2 b = MapToScreen(SectorTopologyVertexToMap(*end));
        DrawLineEx(a, b, twoSided ? 3.5f : 3.0f, lineColor);

        Vector2 dir{b.x - a.x, b.y - a.y};
        const float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (length <= GeometryEpsilon) {
            continue;
        }
        dir.x /= length;
        dir.y /= length;
        const Vector2 normal{-dir.y, dir.x};
        const Vector2 mid{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};

        const Vector2 arrowStart{mid.x - dir.x * 10.0f, mid.y - dir.y * 10.0f};
        const Vector2 arrowEnd{mid.x + dir.x * 10.0f, mid.y + dir.y * 10.0f};
        DrawLineEx(arrowStart, arrowEnd, 2.0f, arrowColor);
        DrawLineEx(arrowEnd, Vector2{arrowEnd.x - dir.x * 6.0f + normal.x * 4.0f, arrowEnd.y - dir.y * 6.0f + normal.y * 4.0f}, 2.0f, arrowColor);
        DrawLineEx(arrowEnd, Vector2{arrowEnd.x - dir.x * 6.0f - normal.x * 4.0f, arrowEnd.y - dir.y * 6.0f - normal.y * 4.0f}, 2.0f, arrowColor);

        if (hasFront) {
            const Vector2 frontEnd{mid.x + normal.x * 15.0f, mid.y + normal.y * 15.0f};
            DrawLineEx(mid, frontEnd, 2.0f, frontColor);
            DrawCircleV(frontEnd, 3.0f, frontColor);
        }
        if (hasBack) {
            const Vector2 backEnd{mid.x - normal.x * 15.0f, mid.y - normal.y * 15.0f};
            DrawLineEx(mid, backEnd, 2.0f, backColor);
            DrawCircleV(backEnd, 3.0f, backColor);
        }
    }
}

void SectorEditor::DrawTopologyVertices() const
{
    const Color pointColor = Color{245, 226, 154, 255};
    const Color outlineColor = Color{20, 24, 32, 255};
    const Color selectedFill = Color{72, 220, 128, 92};
    const Color selectedOutline = Color{86, 232, 142, 245};
    const Color hoverOutline = Color{122, 220, 244, 245};
    for (const SectorTopologyVertex& vertex : state.topologyMap.vertices) {
        const Vector2 screen = MapToScreen(SectorTopologyVertexToMap(vertex));
        const bool selected = state.topologySelectionKind == TopologySelectionKind::Vertex
                && vertex.id == state.selectedTopologyVertexId;
        const bool hovered = state.hasHoveredVertex && vertex.id == state.hoveredTopologyVertexId;
        if (selected) {
            DrawCircleV(screen, 12.0f, selectedFill);
            DrawCircleLines(
                    static_cast<int>(std::round(screen.x)),
                    static_cast<int>(std::round(screen.y)),
                    13.0f,
                    selectedOutline);
        }
        if (hovered && !selected) {
            DrawCircleLines(
                    static_cast<int>(std::round(screen.x)),
                    static_cast<int>(std::round(screen.y)),
                    11.0f,
                    hoverOutline);
        }
        DrawCircleV(screen, 4.5f, pointColor);
        DrawCircleLines(static_cast<int>(std::round(screen.x)), static_cast<int>(std::round(screen.y)), 7.0f, outlineColor);
    }
}

void SectorEditor::DrawTopologySnapCrosshair() const
{
    if (!Contains(canvasRect, GetMousePosition())) {
        return;
    }

    const bool useCanonicalSectorPoint = state.currentTool == SectorEditorTool::Sector
            || state.pendingSector.active;
    const Vector2 snap = useCanonicalSectorPoint
            ? MapToScreen(SectorPointToVector2(CurrentSnappedSectorPoint()))
            : MapToScreen(state.snappedMouseMap);
    DrawLineEx(Vector2{snap.x - 9.0f, snap.y}, Vector2{snap.x + 9.0f, snap.y}, 2.0f, Color{235, 224, 130, 255});
    DrawLineEx(Vector2{snap.x, snap.y - 9.0f}, Vector2{snap.x, snap.y + 9.0f}, 2.0f, Color{235, 224, 130, 255});
}

void SectorEditor::DrawPendingSector() const
{
    if (!state.pendingSector.active || state.pendingSector.points.empty()) {
        return;
    }

    const SectorPoint cursorPoint = CurrentSnappedSectorPoint();
    const bool closeable = CanClosePendingSectorAt(cursorPoint);
    std::string previewError = state.pendingSector.errorMessage;
    if (!closeable && state.pendingSector.active) {
        std::string candidateError;
        if (!ValidatePendingTopologyPoint(cursorPoint, candidateError)
                && candidateError != "Click first point to close sector") {
            previewError = candidateError;
        }
    }

    if (state.pendingSector.points.size() >= 3 && previewError.empty()) {
        std::string finalError;
        std::vector<SectorTopologyCoordPoint> topologyPoints;
        SectorTopologyMap previewMap = state.topologyMap;
        int previewSectorId = -1;
        if (!BuildPendingTopologyPoints(topologyPoints, finalError)) {
            previewError = finalError;
        } else if (state.pendingSector.kind == PendingSectorDrawKind::InsertInside) {
            SectorTopologyInsertPolygonOptions options;
            if (!InsertSectorTopologyPolygon(
                        previewMap,
                        state.pendingSector.parentTopologySectorId,
                        topologyPoints,
                        options,
                        &previewSectorId,
                        &finalError)) {
                previewError = finalError;
            }
        } else if (!CreateSectorTopologyPolygon(
                    previewMap,
                    topologyPoints,
                    BuildTopologyCreateOptions(),
                    &previewSectorId,
                    &finalError)) {
            previewError = finalError;
        }
    }

    const bool invalid = !previewError.empty() && previewError != "Click first point to close sector";
    const Color lineColor = invalid ? Color{220, 88, 88, 245} : Color{236, 196, 92, 245};
    const Color previewColor = invalid ? Color{220, 88, 88, 170} : Color{236, 196, 92, 175};
    const Color pointColor = Color{245, 226, 154, 255};
    const Color firstColor = closeable ? Color{120, 230, 154, 255} : Color{245, 226, 154, 255};

    for (size_t i = 0; i + 1 < state.pendingSector.points.size(); ++i) {
        const Vector2 a = MapToScreen(SectorPointToVector2(state.pendingSector.points[i]));
        const Vector2 b = MapToScreen(SectorPointToVector2(state.pendingSector.points[i + 1]));
        DrawLineEx(a, b, 3.0f, lineColor);
    }

    const Vector2 last = MapToScreen(SectorPointToVector2(state.pendingSector.points.back()));
    const Vector2 cursor = MapToScreen(SectorPointToVector2(cursorPoint));
    if (!SamePoint(cursorPoint, state.pendingSector.points.back())) {
        DrawLineEx(last, cursor, 2.0f, previewColor);
    }

    if (state.pendingSector.points.size() >= 2) {
        const Vector2 first = MapToScreen(SectorPointToVector2(state.pendingSector.points.front()));
        DrawLineEx(cursor, first, 1.5f, WithAlpha(previewColor, closeable ? 230 : 120));
    }

    for (size_t i = 0; i < state.pendingSector.points.size(); ++i) {
        const Vector2 point = MapToScreen(SectorPointToVector2(state.pendingSector.points[i]));
        const bool first = i == 0;
        DrawCircleV(point, first ? 6.0f : 5.0f, first ? firstColor : pointColor);
        DrawCircleLines(static_cast<int>(std::round(point.x)), static_cast<int>(std::round(point.y)), first ? 8.0f : 7.0f, Color{20, 24, 32, 255});
    }
}

void SectorEditor::DrawVertexMoveOverlay() const
{
    if (state.currentTool != SectorEditorTool::Move) {
        return;
    }

    if (!state.vertexDrag.active && state.hasHoveredVertex) {
        const Vector2 point = MapToScreen(SectorPointToVector2(
                SectorTopologyCoordPointToSectorPoint(state.hoveredTopologyVertexPoint)));
        size_t connectedCount = 0;
        for (const SectorTopologyLineDef& lineDef : state.topologyMap.lineDefs) {
            if (lineDef.startVertexId == state.hoveredTopologyVertexId
                    || lineDef.endVertexId == state.hoveredTopologyVertexId) {
                ++connectedCount;
            }
        }
        const Color color = connectedCount > 1
                ? Color{122, 220, 244, 255}
                : Color{245, 226, 154, 255};
        DrawCircleLines(static_cast<int>(std::round(point.x)), static_cast<int>(std::round(point.y)), 11.0f, color);
        DrawCircleV(point, 4.5f, color);
        return;
    }

    if (!state.vertexDrag.active) {
        return;
    }

    const bool invalid = !state.vertexDrag.errorMessage.empty();
    const Color targetColor = invalid ? Color{230, 82, 82, 255} : Color{120, 230, 154, 255};
    const Color previewColor = invalid ? Color{230, 82, 82, 205} : Color{122, 220, 244, 220};
    const Color originalColor = Color{245, 226, 154, 230};
    const Vector2 original = MapToScreen(SectorPointToVector2(
            SectorTopologyCoordPointToSectorPoint(state.vertexDrag.originalPoint)));

    if (!state.vertexDrag.hasPreviewPoint) {
        DrawCircleLines(static_cast<int>(std::round(original.x)), static_cast<int>(std::round(original.y)), 10.0f, originalColor);
        return;
    }

    const int draggedVertexId = state.vertexDrag.topologyVertexId;
    const Vector2 previewMap = SectorPointToVector2(
            SectorTopologyCoordPointToSectorPoint(state.vertexDrag.previewPoint));
    for (const SectorTopologyLineDef& lineDef : state.topologyMap.lineDefs) {
        if (lineDef.startVertexId != draggedVertexId && lineDef.endVertexId != draggedVertexId) {
            continue;
        }

        const int otherVertexId = lineDef.startVertexId == draggedVertexId
                ? lineDef.endVertexId
                : lineDef.startVertexId;
        const SectorTopologyVertex* otherVertex = FindSectorTopologyVertex(state.topologyMap, otherVertexId);
        if (otherVertex == nullptr) {
            continue;
        }
        DrawLineEx(
                MapToScreen(previewMap),
                MapToScreen(SectorTopologyVertexToMap(*otherVertex)),
                4.0f,
                previewColor
        );
    }

    const Vector2 target = MapToScreen(previewMap);
    DrawLineEx(original, target, 2.0f, WithAlpha(targetColor, 180));
    DrawCircleLines(static_cast<int>(std::round(original.x)), static_cast<int>(std::round(original.y)), 10.0f, originalColor);
    DrawCircleLines(static_cast<int>(std::round(target.x)), static_cast<int>(std::round(target.y)), state.vertexDrag.hasMergeTarget ? 17.0f : 13.0f, targetColor);
    if (state.vertexDrag.hasMergeTarget) {
        DrawCircleLines(static_cast<int>(std::round(target.x)), static_cast<int>(std::round(target.y)), 22.0f, Color{236, 196, 92, 235});
    }
    DrawCircleV(target, 5.0f, targetColor);
}

void SectorEditor::DrawPendingTopologyVertexMerge() const
{
    if (!state.pendingTopologyVertexMerge.active) {
        return;
    }

    const PendingTopologyVertexMerge& pending = state.pendingTopologyVertexMerge;
    const SectorTopologyVertex* source = FindSectorTopologyVertex(
            state.topologyMap,
            pending.sourceVertexId);
    if (source == nullptr) {
        return;
    }

    const Vector2 sourceScreen = MapToScreen(SectorTopologyVertexToMap(*source));
    DrawCircleLines(
            static_cast<int>(std::round(sourceScreen.x)),
            static_cast<int>(std::round(sourceScreen.y)),
            17.0f,
            Color{236, 196, 92, 255});
    DrawCircleLines(
            static_cast<int>(std::round(sourceScreen.x)),
            static_cast<int>(std::round(sourceScreen.y)),
            23.0f,
            Color{236, 196, 92, 170});

    if (pending.hoveredTargetVertexId < 0) {
        return;
    }

    const SectorTopologyVertex* target = FindSectorTopologyVertex(
            state.topologyMap,
            pending.hoveredTargetVertexId);
    if (target == nullptr) {
        return;
    }

    const Color targetColor = pending.hasValidTarget
            ? Color{120, 230, 154, 255}
            : Color{230, 82, 82, 255};
    const Vector2 targetScreen = MapToScreen(SectorTopologyVertexToMap(*target));
    DrawLineEx(sourceScreen, targetScreen, 2.0f, WithAlpha(targetColor, 170));
    DrawCircleLines(
            static_cast<int>(std::round(targetScreen.x)),
            static_cast<int>(std::round(targetScreen.y)),
            18.0f,
            targetColor);
    DrawCircleV(targetScreen, 5.5f, targetColor);
}

void SectorEditor::DrawLightMoveOverlay() const
{
    if (state.currentTool != SectorEditorTool::Move) {
        return;
    }

    if (state.lightDrag.active) {
        const SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
                state.topologyMap,
                state.lightDrag.topologyLightId);
        if (light == nullptr) {
            return;
        }

        const Vector2 center = MapToScreen(Vector2{light->position.x, light->position.z});
        const float radiusPixels = SectorAuthoringToWorldDistance(light->radius) * state.viewZoom;
        Color color = light->color;
        color.a = 245;
        DrawCircleLines(
                static_cast<int>(std::round(center.x)),
                static_cast<int>(std::round(center.y)),
                radiusPixels,
                WithAlpha(color, 165)
        );
        DrawCircleLines(static_cast<int>(std::round(center.x)), static_cast<int>(std::round(center.y)), 15.0f, Color{120, 230, 154, 255});
        DrawCircleV(center, 6.5f, Color{120, 230, 154, 255});
        return;
    }

    const SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
            state.topologyMap,
            state.hoveredTopologyLightId);
    if (light == nullptr) {
        return;
    }

    const Vector2 center = MapToScreen(Vector2{light->position.x, light->position.z});
    DrawCircleLines(static_cast<int>(std::round(center.x)), static_cast<int>(std::round(center.y)), 13.0f, Color{245, 226, 154, 255});
}

void SectorEditor::DrawPendingTopologyLineSplitAtPoint() const
{
    if (!state.pendingTopologyLineSplitAtPoint.active) {
        return;
    }

    const PendingTopologyLineSplitAtPoint& pending = state.pendingTopologyLineSplitAtPoint;
    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(
            state.topologyMap,
            pending.lineDefId);
    if (lineDef == nullptr) {
        return;
    }

    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (!GetSectorTopologyLineVertices(state.topologyMap, *lineDef, start, end)) {
        return;
    }

    const Vector2 a = MapToScreen(SectorTopologyVertexToMap(*start));
    const Vector2 b = MapToScreen(SectorTopologyVertexToMap(*end));
    DrawLineEx(a, b, 13.0f, Color{236, 196, 92, 120});
    DrawLineEx(a, b, 4.0f, Color{236, 196, 92, 245});

    if (!pending.hasCandidatePoint) {
        return;
    }

    const Vector2 candidate = MapToScreen(SectorPointToVector2(
            SectorTopologyCoordPointToSectorPoint(pending.candidatePoint)));
    const Color color = pending.hasValidCandidate
            ? Color{120, 230, 154, 255}
            : Color{230, 82, 82, 245};
    DrawCircleLines(
            static_cast<int>(std::round(candidate.x)),
            static_cast<int>(std::round(candidate.y)),
            pending.hasValidCandidate ? 13.0f : 11.0f,
            color);
    DrawCircleV(candidate, pending.hasValidCandidate ? 5.5f : 4.5f, color);
}

void SectorEditor::DrawPendingTopologySectorCut() const
{
    if (!state.pendingTopologySectorCut.active) {
        return;
    }

    const PendingTopologySectorCut& pending = state.pendingTopologySectorCut;
    const SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, pending.sectorId);
    if (sector == nullptr) {
        return;
    }

    auto pointToScreen = [this](const SectorTopologyBoundaryCutPoint& point, Vector2& outScreen) {
        if (point.vertexId >= 0) {
            const SectorTopologyVertex* vertex = FindSectorTopologyVertex(state.topologyMap, point.vertexId);
            if (vertex == nullptr) {
                return false;
            }
            outScreen = MapToScreen(SectorTopologyVertexToMap(*vertex));
            return true;
        }
        outScreen = MapToScreen(SectorPointToVector2(
                SectorTopologyCoordPointToSectorPoint(point.point)));
        return true;
    };

    Vector2 firstScreen{};
    if (pending.hasFirstPoint && pointToScreen(pending.firstPoint, firstScreen)) {
        DrawCircleLines(
                static_cast<int>(std::round(firstScreen.x)),
                static_cast<int>(std::round(firstScreen.y)),
                15.0f,
                Color{236, 196, 92, 255});
        DrawCircleV(firstScreen, 5.5f, Color{236, 196, 92, 255});
    }

    if (!pending.hasCandidatePoint) {
        return;
    }

    Vector2 candidateScreen{};
    if (!pointToScreen(pending.candidatePoint, candidateScreen)) {
        return;
    }

    const Color color = pending.hasFirstPoint
            ? (pending.hasValidCandidate ? Color{120, 230, 154, 255} : Color{230, 82, 82, 245})
            : (pending.hasValidCandidate ? Color{236, 196, 92, 255} : Color{230, 82, 82, 245});
    if (pending.hasFirstPoint) {
        DrawLineEx(firstScreen, candidateScreen, 3.0f, WithAlpha(color, 210));
    }
    DrawCircleLines(
            static_cast<int>(std::round(candidateScreen.x)),
            static_cast<int>(std::round(candidateScreen.y)),
            pending.hasValidCandidate ? 13.0f : 11.0f,
            color);
    DrawCircleV(candidateScreen, pending.hasValidCandidate ? 5.0f : 4.0f, color);
}

void SectorEditor::DrawStaticLights() const
{
    for (const SectorTopologyStaticPointLight& light : state.topologyMap.staticLights) {
        const Vector2 center = MapToScreen(Vector2{light.position.x, light.position.z});
        const bool selected = state.topologySelectionKind == TopologySelectionKind::Light
                && light.id == state.selectedTopologyLightId;
        const bool hovered = light.id == state.hoveredTopologyLightId;
        Color color = light.color;
        color.a = selected ? 255 : hovered ? 235 : 205;
        const float radiusPixels = SectorAuthoringToWorldDistance(light.radius) * state.viewZoom;

        if (selected || hovered || state.currentTool == SectorEditorTool::Light) {
            DrawCircleLines(
                    static_cast<int>(std::round(center.x)),
                    static_cast<int>(std::round(center.y)),
                    radiusPixels,
                    WithAlpha(color, selected ? 150 : 90)
            );
        }
        if (selected && light.sourceRadius > 0.0f) {
            const float sourceRadiusPixels = SectorAuthoringToWorldDistance(light.sourceRadius) * state.viewZoom;
            if (sourceRadiusPixels >= 3.0f) {
                DrawCircleLines(
                        static_cast<int>(std::round(center.x)),
                        static_cast<int>(std::round(center.y)),
                        sourceRadiusPixels,
                        WithAlpha(color, 210)
                );
            }
        }

        DrawCircleV(center, selected ? 7.0f : 5.5f, color);
        DrawCircleLines(static_cast<int>(std::round(center.x)), static_cast<int>(std::round(center.y)), selected ? 11.0f : 9.0f, Color{20, 24, 32, 255});
        DrawLineEx(Vector2{center.x - 10.0f, center.y}, Vector2{center.x + 10.0f, center.y}, 2.0f, color);
        DrawLineEx(Vector2{center.x, center.y - 10.0f}, Vector2{center.x, center.y + 10.0f}, 2.0f, color);
    }
}

void SectorEditor::DrawToolsPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const engine::UIPanelResult panel = engine::BeginPanel(
            ui,
            config,
            assets,
            "sector_editor_tools",
            BuildLeftPanelRect(),
            font,
            "Tools"
    );

    const float rowH = 46.0f;
    const float gap = config.rowSpacing;
    const float toolsContentH = 1110.0f;
    const float scrollContentW = std::max(0.0f, panel.contentRect.width - config.scrollbarSize);
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui,
            config,
            input,
            "sector_editor_tools_scroll",
            panel.contentRect,
            Vector2{scrollContentW, toolsContentH},
            uiState.toolsScroll,
            false
    );

    const float contentW = scroll.viewport.width;
    float y = 0.0f;
    const auto separator = [&]() {
        engine::Separator(
                config,
                Rectangle{
                        scroll.viewport.x,
                        scroll.viewport.y - uiState.toolsScroll.offset.y + y,
                        contentW,
                        12.0f
                }
        );
        y += 22.0f;
    };

    const SectorEditorTool tools[] = {
            SectorEditorTool::Select,
            SectorEditorTool::Sector,
            SectorEditorTool::Light,
            SectorEditorTool::Move,
            SectorEditorTool::Erase
    };

    for (SectorEditorTool tool : tools) {
        if (engine::ToolButton(
                ui,
                config,
                input,
                assets,
                TextFormat("sector_editor_tool_%s", ToolName(tool)),
                Rectangle{0.0f, y, contentW, rowH},
                font,
                ToolName(tool),
                state.currentTool == tool)) {
            if ((state.currentTool == SectorEditorTool::Sector
                        || state.currentTool == SectorEditorTool::InsertSectorInside)
                    && tool != state.currentTool) {
                CancelPendingSector("Cancelled sector");
            }
            if (state.vertexDrag.active && tool != SectorEditorTool::Move) {
                CancelVertexDrag("Cancelled vertex move");
            }
            if (state.lightDrag.active && tool != SectorEditorTool::Move) {
                CancelLightDrag("Cancelled light move");
            }
            if (state.pendingTopologyLineSplitAtPoint.active) {
                CancelPendingTopologyLineSplitAtPoint("Cancelled split at point");
            }
            if (state.pendingTopologyVertexMerge.active) {
                CancelPendingTopologyVertexMerge("Cancelled vertex merge");
            }
            state.currentTool = tool;
        }
        y += rowH + gap;
    }

    separator();

    const float documentButtonW = (contentW - gap) * 0.5f;
    if (engine::Button(ui, config, input, assets, "sector_editor_new", Rectangle{0.0f, y, documentButtonW, rowH}, font, "New")) {
        OpenNewConfirmation(assets);
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_load", Rectangle{documentButtonW + gap, y, documentButtonW, rowH}, font, "Load")) {
        OpenLoadLevelModal();
    }
    y += rowH + gap;
    if (engine::Button(ui, config, input, assets, "sector_editor_save", Rectangle{0.0f, y, documentButtonW, rowH}, font, "Save")) {
        OpenSaveLevelModal();
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_reload", Rectangle{documentButtonW + gap, y, documentButtonW, rowH}, font, "Reload")) {
        OpenReloadConfirmation(assets);
    }
    y += rowH + gap;

    if (engine::Button(ui, config, input, assets, "sector_editor_add_map_texture", Rectangle{0.0f, y, contentW, rowH}, font, "Add Map Texture")) {
        OpenAddMapTextureModal(assets);
    }
    y += rowH + gap;

    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 28.0f}, font, "Lightmap Settings", engine::UITextJustify::Left, config.mutedTextColor);
    y += 32.0f;

    const float lightmapLabelW = 180.0f;
    const auto drawLightmapSetting = [&](const char* id, const char* label, float& value, engine::UIFloatInputState& inputState, float minValue, float maxValue, int decimals, const char* status) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, lightmapLabelW, rowH}, font, label, engine::UITextJustify::Left, config.mutedTextColor);
        float edited = value;
        const engine::UINumericInputResult result = engine::FloatInput(
                ui,
                config,
                input,
                assets,
                id,
                Rectangle{lightmapLabelW + gap, y, std::max(0.0f, contentW - lightmapLabelW - gap), rowH},
                font,
                edited,
                inputState,
                minValue,
                maxValue,
                decimals
        );
        if (result.changed && edited != value) {
            value = edited;
            state.hasUnsavedChanges = true;
            state.topologyDocumentDirty = true;
            statusText = status;
        }
        y += rowH + gap;
    };

    state.topologyMap.lightmapSettings.ambientOcclusionRadius = ClampAmbientOcclusionRadius(state.topologyMap.lightmapSettings.ambientOcclusionRadius);
    state.topologyMap.lightmapSettings.ambientOcclusionStrength = ClampAmbientOcclusionStrength(state.topologyMap.lightmapSettings.ambientOcclusionStrength);
    state.topologyMap.lightmapSettings.indirectBounceRadius = ClampIndirectBounceRadius(state.topologyMap.lightmapSettings.indirectBounceRadius);
    state.topologyMap.lightmapSettings.indirectBounceStrength = ClampIndirectBounceStrength(state.topologyMap.lightmapSettings.indirectBounceStrength);
    drawLightmapSetting(
            "sector_editor_ao_radius",
            "AO radius",
            state.topologyMap.lightmapSettings.ambientOcclusionRadius,
            uiState.ambientOcclusionRadiusInput,
            SectorWorldToAuthoringDistance(0.05f),
            SectorWorldToAuthoringDistance(16.0f),
            2,
            "Updated AO radius"
    );
    drawLightmapSetting(
            "sector_editor_ao_strength",
            "AO strength",
            state.topologyMap.lightmapSettings.ambientOcclusionStrength,
            uiState.ambientOcclusionStrengthInput,
            0.0f,
            1.0f,
            3,
            "Updated AO strength"
    );
    drawLightmapSetting(
            "sector_editor_bounce_radius",
            "Bounce radius",
            state.topologyMap.lightmapSettings.indirectBounceRadius,
            uiState.indirectBounceRadiusInput,
            SectorWorldToAuthoringDistance(0.05f),
            SectorWorldToAuthoringDistance(16.0f),
            2,
            "Updated bounce radius"
    );
    drawLightmapSetting(
            "sector_editor_bounce_strength",
            "Bounce strength",
            state.topologyMap.lightmapSettings.indirectBounceStrength,
            uiState.indirectBounceStrengthInput,
            0.0f,
            1.0f,
            3,
            "Updated bounce strength"
    );

    if (engine::Button(ui, config, input, assets, "sector_editor_bake_lightmaps", Rectangle{0.0f, y, contentW, rowH}, font, "Bake Lightmaps")) {
        BakeLightmaps();
    }
    y += rowH + gap;

    separator();

    const float gridLabelW = 64.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, gridLabelW, rowH}, font, "Grid", engine::UITextJustify::Left, config.mutedTextColor);
    engine::IntInput(
            ui,
            config,
            input,
            assets,
            "sector_editor_grid",
            Rectangle{gridLabelW + gap, y, std::max(0.0f, contentW - gridLabelW - gap), rowH},
            font,
            state.gridSize,
            uiState.gridSizeInput,
            1,
            64,
            1
    );
    y += rowH + gap;

    engine::Checkbox(ui, config, input, assets, "sector_editor_show_grid", Rectangle{0.0f, y, contentW, rowH}, font, "Show grid", state.showGrid);
    y += rowH + gap;
    engine::Checkbox(ui, config, input, assets, "sector_editor_show_axes", Rectangle{0.0f, y, contentW, rowH}, font, "Show axes", state.showAxes);
    y += rowH + gap;
    engine::Checkbox(ui, config, input, assets, "sector_editor_show_ids", Rectangle{0.0f, y, contentW, rowH}, font, "Show ids", state.showSectorIds);
    y += rowH + gap;

    separator();

    if (engine::Button(ui, config, input, assets, "sector_editor_preview_3d", Rectangle{0.0f, y, contentW, rowH}, font, "3D Mode")) {
        TryEnterPreview3D(assets, ui);
    }

    engine::EndScrollArea(ui, config, input, scroll, uiState.toolsScroll);
    engine::EndPanel(ui, config, panel);
}

void SectorEditor::DrawSectorsPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const engine::UIPanelResult panel = engine::BeginPanel(
            ui,
            config,
            assets,
            "sector_editor_sectors",
            BuildRightPanelRect(),
            font,
            "Inspector"
    );

    ClearStaleTopologySelection();
    SyncSelectedSectorIdBuffer();
    SyncSelectedLightIdBuffer();

    const bool hasSelectedTopologySector = SelectedTopologySector() != nullptr;
    const bool hasSelectedTopologyVertex = SelectedTopologyVertex() != nullptr;
    const bool hasSelectedTopologySideDef = SelectedTopologySideDef() != nullptr;
    const bool hasSelectedTopologyLineDef = state.topologySelectionKind == TopologySelectionKind::LineDef
            && SelectedTopologyLineDef() != nullptr;
    const bool hasSelectedLight = SelectedTopologyLight() != nullptr;
    const SectorTopologyVertex* inspectedVertex = FindSectorTopologyVertex(
            state.topologyMap,
            state.inspectedTopologyVertexId);
    const bool hasInspectedVertex = !hasSelectedTopologySector
            && !hasSelectedTopologyVertex
            && !hasSelectedTopologySideDef
            && !hasSelectedTopologyLineDef
            && !hasSelectedLight
            && state.currentTool == SectorEditorTool::Move
            && inspectedVertex != nullptr;

    const float rowH = 40.0f;
    const float gap = 8.0f;
    const float scrollContentW = std::max(0.0f, panel.contentRect.width - config.scrollbarSize);
    const auto inspectorContentHeight = [&]() {
        if (hasSelectedLight) {
            float height = 38.0f; // Light title.
            height += rowH + gap; // Id.
            if (!uiState.idEditError.empty()) {
                height += 36.0f;
            }
            height += rowH + gap; // Delete.
            height += 6.0f * (rowH + gap); // Position/intensity/radius/source radius.
            height += 3.0f * (rowH + gap); // RGB.
            height += 36.0f + gap; // Swatch.
            height += rowH + gap; // Bake.
            return height;
        }
        if (hasSelectedTopologySector) {
            const float textureRowH = 36.0f + gap;
            const float uvSettingsH = 2.0f * (62.0f + gap);
            const float materialSurfaceSectionH = 18.0f + 30.0f
                    + (36.0f + gap) // Layer toggle.
                    + textureRowH
                    + uvSettingsH
                    + rowH + gap // Opacity.
                    + 36.0f + gap // Emissive.
                    + rowH + gap // Tint.
                    + 36.0f + gap; // Fit/Clear decal action row.
            const float defaultWallSectionH = 18.0f + 30.0f + textureRowH + uvSettingsH;

            float height = 38.0f; // Sector title.
            height += rowH + gap; // Name.
            if (!uiState.idEditError.empty()) {
                height += 36.0f;
            }
            height += 3.0f * (rowH + gap); // Delete/insert/cut.
            height += 2.0f * (rowH + gap); // Floor/ceiling heights.
            height += 18.0f + 30.0f; // Lighting separator/title.
            height += rowH + gap; // Ambient intensity.
            height += 3.0f * (rowH + gap); // RGB.
            height += 36.0f + gap; // Ambient swatch.
            height += 2.0f * materialSurfaceSectionH; // Floor/ceiling material sections.
            height += 3.0f * defaultWallSectionH; // Default wall/lower/upper sections.
            height += 96.0f; // Bottom breathing room for the last controls.
            return height;
        }
        if (hasSelectedTopologyVertex) {
            return 280.0f;
        }
        if (hasSelectedTopologySideDef) {
            return 1240.0f;
        }
        if (hasSelectedTopologyLineDef) {
            return 218.0f;
        }
        if (hasInspectedVertex || state.pendingTopologyVertexMerge.active) {
            return 170.0f;
        }
        return 42.0f;
    };
    const float contentH = inspectorContentHeight();
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui,
            config,
            input,
            "sector_editor_inspector_scroll",
            panel.contentRect,
            Vector2{scrollContentW, contentH},
            uiState.inspectorScroll,
            false
    );

    const float contentW = scroll.viewport.width;
    float y = 0.0f;

    if (hasSelectedTopologySector) {
        if (DrawTopologySectorInspector(ui, config, input, assets, font, scroll, contentW, rowH, gap)) {
            engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
            engine::EndPanel(ui, config, panel);
            return;
        }
    }

    if (hasSelectedTopologySideDef || hasSelectedTopologyLineDef) {
        if (DrawTopologySideDefInspector(ui, config, input, assets, font, scroll, contentW, rowH, gap)) {
            engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
            engine::EndPanel(ui, config, panel);
            return;
        }
    }

    if (hasSelectedLight) {
        SectorTopologyStaticPointLight& light = *SelectedTopologyLight();
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, TextFormat("Static Light: %d", light.id), engine::UITextJustify::Left, config.textColor);
        y += 38.0f;

        const float labelW = 88.0f;
        engine::Text(ui, config, assets, Rectangle{0.0f, y, labelW, rowH}, font, "Id", engine::UITextJustify::Left, config.mutedTextColor);
        engine::Text(ui, config, assets, Rectangle{labelW, y, contentW - labelW, rowH}, font, TextFormat("%d", light.id), engine::UITextJustify::Left, config.textColor);
        y += rowH + gap;

        if (!uiState.idEditError.empty()) {
            engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, uiState.idEditError.c_str(), engine::UITextJustify::Left, config.invalidColor);
            y += 36.0f;
        }

        if (engine::Button(ui, config, input, assets, "sector_editor_delete_light", Rectangle{0.0f, y, contentW, rowH}, font, "Delete Light")) {
            DeleteSelectedLight();
            engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
            engine::EndPanel(ui, config, panel);
            return;
        }
        y += rowH + gap;

        const float numberLabelW = 92.0f;
        const float numberFieldW = 112.0f;
        auto drawLightFloat = [&](const char* id, const char* label, float& value, engine::UIFloatInputState& inputState, float minValue, float maxValue, int decimals) {
            engine::Text(ui, config, assets, Rectangle{0.0f, y, numberLabelW, rowH}, font, label, engine::UITextJustify::Right, config.mutedTextColor);
            float edited = value;
            const engine::UINumericInputResult result = engine::FloatInput(ui, config, input, assets, id, Rectangle{numberLabelW, y, numberFieldW, rowH}, font, edited, inputState, minValue, maxValue, decimals);
            if (result.changed && edited != value) {
                value = edited;
                state.topologyDocumentDirty = true;
                state.hasUnsavedChanges = true;
                statusText = TextFormat("Updated topology light %d", light.id);
            }
            y += rowH + gap;
        };

        drawLightFloat("sector_editor_light_x", "X:", light.position.x, uiState.lightXInput, -8192.0f, 8192.0f, 2);
        drawLightFloat("sector_editor_light_y", "Y:", light.position.y, uiState.lightYInput, -512.0f, 512.0f, 2);
        drawLightFloat("sector_editor_light_z", "Z:", light.position.z, uiState.lightZInput, -8192.0f, 8192.0f, 2);
        drawLightFloat("sector_editor_light_intensity", "Intensity:", light.intensity, uiState.lightIntensityInput, 0.0f, 8.0f, 3);
        light.intensity = ClampLightIntensity(light.intensity);
        drawLightFloat("sector_editor_light_radius", "Radius:", light.radius, uiState.lightRadiusInput, SectorWorldToAuthoringDistance(0.1f), SectorWorldToAuthoringDistance(64.0f), 2);
        light.radius = ClampLightRadius(light.radius);
        light.sourceRadius = ClampLightSourceRadius(light.sourceRadius, light.radius);
        {
            engine::Text(ui, config, assets, Rectangle{0.0f, y, numberLabelW, rowH}, font, "Source:", engine::UITextJustify::Right, config.mutedTextColor);
            float edited = light.sourceRadius;
            const engine::UINumericInputResult result = engine::FloatInput(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_light_source_radius",
                    Rectangle{numberLabelW, y, numberFieldW, rowH},
                    font,
                    edited,
                    uiState.lightSourceRadiusInput,
                    0.0f,
                    SectorWorldToAuthoringDistance(8.0f),
                    3
            );
            edited = ClampLightSourceRadius(edited, light.radius);
            if (result.changed && edited != light.sourceRadius) {
                light.sourceRadius = edited;
                state.topologyDocumentDirty = true;
                state.hasUnsavedChanges = true;
                statusText = "Updated light source radius";
            }
            y += rowH + gap;
        }

        auto drawLightChannel = [&](const char* id, const char* label, unsigned char& channel, engine::UIIntInputState& inputState) {
            engine::Text(ui, config, assets, Rectangle{0.0f, y, numberLabelW, rowH}, font, label, engine::UITextJustify::Right, config.mutedTextColor);
            int value = static_cast<int>(channel);
            const engine::UINumericInputResult result = engine::IntInput(
                    ui,
                    config,
                    input,
                    assets,
                    id,
                    Rectangle{numberLabelW, y, contentW - numberLabelW, rowH},
                    font,
                    value,
                    inputState,
                    0,
                    255,
                    1
            );
            if (result.changed && value != static_cast<int>(channel)) {
                channel = static_cast<unsigned char>(ClampAmbientChannel(value));
                light.color.a = 255;
                state.topologyDocumentDirty = true;
                state.hasUnsavedChanges = true;
                statusText = TextFormat("Updated topology light %d color", light.id);
            }
            y += rowH + gap;
        };
        drawLightChannel("sector_editor_light_r", "R:", light.color.r, uiState.lightRedInput);
        drawLightChannel("sector_editor_light_g", "G:", light.color.g, uiState.lightGreenInput);
        drawLightChannel("sector_editor_light_b", "B:", light.color.b, uiState.lightBlueInput);

        const Rectangle swatch{
                scroll.viewport.x + numberLabelW,
                scroll.viewport.y - uiState.inspectorScroll.offset.y + y + 2.0f,
                std::min(120.0f, contentW - numberLabelW),
                28.0f
        };
        DrawRectangleRec(swatch, light.color);
        DrawRectangleLinesEx(swatch, 1.0f, config.borderColor);
        y += 36.0f + gap;

        if (engine::Button(ui, config, input, assets, "sector_editor_light_bake", Rectangle{0.0f, y, contentW, rowH}, font, "Bake Lightmaps")) {
            BakeLightmaps();
        }

        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    if (hasSelectedTopologyVertex || hasInspectedVertex || state.pendingTopologyVertexMerge.active) {
        const int vertexId = state.pendingTopologyVertexMerge.active
                ? state.pendingTopologyVertexMerge.sourceVertexId
                : (hasSelectedTopologyVertex ? state.selectedTopologyVertexId : inspectedVertex->id);
        const SectorTopologyVertex* vertex = FindSectorTopologyVertex(state.topologyMap, vertexId);
        if (vertex != nullptr) {
            int incidentLineCount = 0;
            for (const SectorTopologyLineDef& lineDef : state.topologyMap.lineDefs) {
                if (lineDef.startVertexId == vertex->id || lineDef.endVertexId == vertex->id) {
                    ++incidentLineCount;
                }
            }
            engine::Text(
                    ui,
                    config,
                    assets,
                    Rectangle{0.0f, y, contentW, 34.0f},
                    font,
                    TextFormat("Topology Vertex: %d", vertex->id),
                    engine::UITextJustify::Left,
                    config.textColor);
            y += 38.0f;
            engine::Text(
                    ui,
                    config,
                    assets,
                    Rectangle{0.0f, y, contentW, 30.0f},
                    font,
                    TextFormat("ID: %d", vertex->id),
                    engine::UITextJustify::Left,
                    config.mutedTextColor);
            y += 30.0f;
            engine::Text(
                    ui,
                    config,
                    assets,
                    Rectangle{0.0f, y, contentW, 30.0f},
                    font,
                    TextFormat(
                            "Position: %.2f, %.2f",
                            SectorCoordToVisibleAuthoring(vertex->x),
                            SectorCoordToVisibleAuthoring(vertex->y)),
                    engine::UITextJustify::Left,
                    config.mutedTextColor);
            y += 34.0f;
            engine::Text(
                    ui,
                    config,
                    assets,
                    Rectangle{0.0f, y, contentW, 30.0f},
                    font,
                    TextFormat("Incident linedefs: %d", incidentLineCount),
                    engine::UITextJustify::Left,
                    config.mutedTextColor);
            y += 34.0f;

            if (state.pendingTopologyVertexMerge.active) {
                engine::Text(
                        ui,
                        config,
                        assets,
                        Rectangle{0.0f, y, contentW, 44.0f},
                        font,
                        state.pendingTopologyVertexMerge.message.c_str(),
                        engine::UITextJustify::Left,
                        state.pendingTopologyVertexMerge.hasValidTarget
                                ? config.textColor
                                : config.mutedTextColor);
                y += 48.0f;
                if (engine::Button(
                            ui,
                            config,
                            input,
                            assets,
                            "sector_editor_cancel_vertex_merge",
                            Rectangle{0.0f, y, contentW, rowH},
                            font,
                            "Cancel Merge")) {
                    CancelPendingTopologyVertexMerge("Cancelled vertex merge");
                }
            } else if (hasSelectedTopologyVertex) {
                if (engine::Button(
                            ui,
                            config,
                            input,
                            assets,
                            "sector_editor_dissolve_vertex",
                            Rectangle{0.0f, y, contentW, rowH},
                            font,
                            "Dissolve Vertex")) {
                    DissolveSelectedTopologyVertex();
                }
                y += rowH + gap;
                if (engine::Button(
                            ui,
                            config,
                            input,
                            assets,
                            "sector_editor_merge_vertex_into",
                            Rectangle{0.0f, y, contentW, rowH},
                            font,
                            "Merge Into...")) {
                    StartPendingTopologyVertexMerge(vertex->id);
                }
            } else if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_merge_vertex_into",
                        Rectangle{0.0f, y, contentW, rowH},
                        font,
                        "Merge Into...")) {
                StartPendingTopologyVertexMerge(vertex->id);
            }
        } else {
            state.inspectedTopologyVertexId = -1;
            if (hasSelectedTopologyVertex) {
                ClearStaleTopologySelection();
            }
        }

        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 42.0f}, font, "Selected: none", engine::UITextJustify::Left, config.mutedTextColor);

    // TODO: Add undo/redo.
    // TODO: Add validation issue highlighting.

    engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
    engine::EndPanel(ui, config, panel);
}

bool SectorEditor::DrawTopologySectorInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::UIScrollAreaResult scroll,
        float contentW,
        float rowH,
        float gap)
{
    SectorTopologySector* sector = SelectedTopologySector();
    if (sector == nullptr) {
        return false;
    }

    float y = 0.0f;
    engine::Text(
            ui,
            config,
            assets,
            Rectangle{0.0f, y, contentW, 34.0f},
            font,
            TextFormat("Topology Sector: %d", sector->id),
            engine::UITextJustify::Left,
            config.textColor);
    y += 38.0f;

    const float labelW = 92.0f;
    const float numberFieldW = 112.0f;

    engine::Text(ui, config, assets, Rectangle{0.0f, y, labelW, rowH}, font, "Name", engine::UITextJustify::Left, config.mutedTextColor);
    const engine::UITextInputResult nameResult = engine::TextInput(
            ui,
            config,
            input,
            assets,
            "sector_editor_selected_topology_sector_name",
            Rectangle{labelW, y, contentW - labelW, rowH},
            font,
            uiState.selectedSectorIdBuffer,
            sizeof(uiState.selectedSectorIdBuffer),
            0,
            sizeof(uiState.selectedSectorIdBuffer) - 1,
            engine::UITextJustify::Left);
    if (nameResult.submitted) {
        TryRenameSelectedTopologySector();
    }
    y += rowH + gap;

    if (!uiState.idEditError.empty()) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, uiState.idEditError.c_str(), engine::UITextJustify::Left, config.invalidColor);
        y += 36.0f;
    }

    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_delete_sector",
                Rectangle{0.0f, y, contentW, rowH},
                font,
                "Delete Sector")) {
        OpenDeleteSelectedTopologySectorConfirmation();
    }
    y += rowH + gap;

    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_insert_sector_inside",
                Rectangle{0.0f, y, contentW, rowH},
                font,
                "Insert Sector Inside")) {
        StartInsertSectorInside();
    }
    y += rowH + gap;

    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_cut_sector",
                Rectangle{0.0f, y, contentW, rowH},
                font,
                "Cut Sector")) {
        StartPendingTopologySectorCut();
        return true;
    }
    y += rowH + gap;

    auto drawHeight = [&](const char* id, const char* label, float current, engine::UIFloatInputState& inputState, bool floorField) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, labelW, rowH}, font, label, engine::UITextJustify::Right, config.mutedTextColor);
        float edited = current;
        const engine::UINumericInputResult result = engine::FloatInput(
                ui, config, input, assets, id,
                Rectangle{labelW, y, numberFieldW, rowH},
                font, edited, inputState, -512.0f, 512.0f, 2);
        if (result.changed && edited != current) {
            const float nextFloor = floorField ? edited : sector->floorZ;
            const float nextCeiling = floorField ? sector->ceilingZ : edited;
            if (!std::isfinite(nextFloor) || !std::isfinite(nextCeiling) || nextCeiling <= nextFloor) {
                statusText = "Invalid topology sector heights: ceiling must be greater than floor";
            } else {
                if (floorField) {
                    sector->floorZ = edited;
                } else {
                    sector->ceilingZ = edited;
                }
                MarkTopologyDocumentEdited(TextFormat("Updated topology sector %d height", sector->id));
            }
        }
        y += rowH + gap;
    };

    drawHeight("sector_editor_topology_floor", "Floor:", sector->floorZ, uiState.floorInput, true);
    drawHeight("sector_editor_topology_ceiling", "Ceiling:", sector->ceilingZ, uiState.ceilingInput, false);

    engine::Separator(config, Rectangle{scroll.viewport.x, scroll.viewport.y - uiState.inspectorScroll.offset.y + y, contentW, 12.0f});
    y += 18.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, "Lighting", engine::UITextJustify::Left, config.textColor);
    y += 30.0f;

    engine::Text(ui, config, assets, Rectangle{0.0f, y, labelW, rowH}, font, "Intensity:", engine::UITextJustify::Right, config.mutedTextColor);
    float ambientIntensity = ClampAmbientIntensity(sector->ambientIntensity);
    const engine::UINumericInputResult ambientResult = engine::FloatInput(
            ui, config, input, assets, "sector_editor_topology_ambient_intensity",
            Rectangle{labelW, y, numberFieldW, rowH},
            font, ambientIntensity, uiState.ambientIntensityInput, 0.0f, 1.0f, 3);
    if (ambientResult.changed && ambientIntensity != sector->ambientIntensity) {
        sector->ambientIntensity = ambientIntensity;
        MarkTopologyDocumentEdited("Updated topology sector ambient intensity");
    }
    y += rowH + gap;

    auto drawAmbientChannel = [&](const char* id, const char* label, unsigned char& channel, engine::UIIntInputState& inputState) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, labelW, rowH}, font, label, engine::UITextJustify::Right, config.mutedTextColor);
        int value = static_cast<int>(channel);
        const engine::UINumericInputResult result = engine::IntInput(
                ui, config, input, assets, id,
                Rectangle{labelW, y, contentW - labelW, rowH},
                font, value, inputState, 0, 255, 1);
        if (result.changed && value != static_cast<int>(channel)) {
            channel = static_cast<unsigned char>(ClampAmbientChannel(value));
            sector->ambientColor.a = 255;
            MarkTopologyDocumentEdited("Updated topology sector ambient color");
        }
        y += rowH + gap;
    };
    drawAmbientChannel("sector_editor_topology_ambient_r", "R:", sector->ambientColor.r, uiState.ambientRedInput);
    drawAmbientChannel("sector_editor_topology_ambient_g", "G:", sector->ambientColor.g, uiState.ambientGreenInput);
    drawAmbientChannel("sector_editor_topology_ambient_b", "B:", sector->ambientColor.b, uiState.ambientBlueInput);

    const Rectangle swatch{
            scroll.viewport.x + labelW,
            scroll.viewport.y - uiState.inspectorScroll.offset.y + y + 2.0f,
            std::min(120.0f, contentW - labelW),
            28.0f};
    DrawRectangleRec(swatch, TopologySectorAmbientPreviewColor(*sector, 255));
    DrawRectangleLinesEx(swatch, 1.0f, config.borderColor);
    y += 36.0f + gap;

    auto drawTextureRow = [&](const char* id, const char* label, const std::string& textureId, TopologySectorTextureField field, TopologyMaterialLayer layer) {
        const float buttonW = 38.0f;
        const float labelColumnW = 82.0f;
        const Rectangle row{0.0f, y, contentW, 36.0f};
        const bool missing = !textureId.empty() && FindSectorTopologyTexture(state.topologyMap, textureId) == nullptr;
        engine::Text(ui, config, assets, Rectangle{row.x, row.y, labelColumnW, row.height}, font, label, engine::UITextJustify::Left, config.mutedTextColor);
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{row.x + labelColumnW, row.y, row.width - labelColumnW - buttonW - gap, row.height},
                font,
                textureId.empty() ? "<none>" : textureId.c_str(),
                engine::UITextJustify::Left,
                missing ? config.invalidColor : config.mutedTextColor);
        if (engine::Button(ui, config, input, assets, id, Rectangle{row.x + row.width - buttonW, row.y, buttonW, row.height}, font, ">")) {
            OpenTopologyTexturePicker(sector->id, field, layer);
        }
        y += row.height + gap;
    };

    auto drawUvSettings = [&](const char* idPrefix, SectorTopologyUvSettings& uv, int stateOffset) {
        const float uvColumnW = (contentW - gap) * 0.5f;
        const float uvBlockH = 62.0f;
        auto drawFloat = [&](int stateIndex, const char* suffix, const char* label, float value, float minValue, float maxValue, Rectangle bounds, auto applyValue) {
            engine::Text(ui, config, assets, Rectangle{bounds.x, bounds.y, bounds.width, 26.0f}, font, label, engine::UITextJustify::Left, config.mutedTextColor);
            float edited = value;
            const std::string inputId = std::string(idPrefix) + suffix;
            const engine::UINumericInputResult result = engine::FloatInput(
                    ui, config, input, assets, inputId.c_str(),
                    Rectangle{bounds.x, bounds.y + 26.0f, bounds.width, 36.0f},
                    font, edited, uiState.topologySectorUvInputs[stateOffset + stateIndex], minValue, maxValue, 3);
            if (result.changed && edited != value && std::isfinite(edited)) {
                applyValue(edited);
                state.topologyRenderWarning.clear();
                MarkTopologyDocumentEdited(TextFormat("Updated topology sector %d UV", sector->id));
            }
        };

        drawFloat(0, "_scale_u", "Scale U", uv.scale.x, TopologyUvScaleMin, TopologyUvScaleMax, Rectangle{0.0f, y, uvColumnW, uvBlockH}, [&](float value) { uv.scale.x = value; });
        drawFloat(1, "_scale_v", "Scale V", uv.scale.y, TopologyUvScaleMin, TopologyUvScaleMax, Rectangle{uvColumnW + gap, y, uvColumnW, uvBlockH}, [&](float value) { uv.scale.y = value; });
        y += uvBlockH + gap;
        drawFloat(2, "_offset_u", "Offset U", uv.offset.x, -1024.0f, 1024.0f, Rectangle{0.0f, y, uvColumnW, uvBlockH}, [&](float value) { uv.offset.x = value; });
        drawFloat(3, "_offset_v", "Offset V", uv.offset.y, -1024.0f, 1024.0f, Rectangle{uvColumnW + gap, y, uvColumnW, uvBlockH}, [&](float value) { uv.offset.y = value; });
        y += uvBlockH + gap;
    };

    auto drawLayerToggle = [&](const char* idPrefix) {
        const float labelColumnW = 82.0f;
        const float buttonW = (contentW - labelColumnW - gap) * 0.5f;
        engine::Text(ui, config, assets, Rectangle{0.0f, y, labelColumnW, 36.0f}, font, "Layer:", engine::UITextJustify::Left, config.mutedTextColor);
        if (engine::ToolButton(
                    ui,
                    config,
                    input,
                    assets,
                    TextFormat("%s_layer_base", idPrefix),
                    Rectangle{labelColumnW, y, buttonW, 36.0f},
                    font,
                    "Base",
                    state.activeTopologyMaterialLayer == TopologyMaterialLayer::Base)) {
            state.activeTopologyMaterialLayer = TopologyMaterialLayer::Base;
            for (engine::UIFloatInputState& inputState : uiState.topologySectorUvInputs) {
                inputState = engine::UIFloatInputState{};
            }
        }
        if (engine::ToolButton(
                    ui,
                    config,
                    input,
                    assets,
                    TextFormat("%s_layer_decal", idPrefix),
                    Rectangle{labelColumnW + buttonW + gap, y, buttonW, 36.0f},
                    font,
                    "Decal",
                    state.activeTopologyMaterialLayer == TopologyMaterialLayer::Decal)) {
            state.activeTopologyMaterialLayer = TopologyMaterialLayer::Decal;
            for (engine::UIFloatInputState& inputState : uiState.topologySectorUvInputs) {
                inputState = engine::UIFloatInputState{};
            }
        }
        y += 36.0f + gap;
    };

    auto drawSurfaceSection = [&](const char* title, const char* textureButtonId, const std::string& textureId, TopologySectorTextureField field, const char* uvPrefix, SectorTopologyUvSettings& uv, SectorTopologyDecalLayer& decal, int stateOffset, int opacityStateIndex, TopologySurfaceEditTargetKind materialKind) {
        engine::Separator(config, Rectangle{scroll.viewport.x, scroll.viewport.y - uiState.inspectorScroll.offset.y + y, contentW, 12.0f});
        y += 18.0f;
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, title, engine::UITextJustify::Left, config.textColor);
        y += 30.0f;
        const TopologySurfaceEditTarget target{materialKind, sector->id};
        drawLayerToggle(uvPrefix);
        const TopologyMaterialLayer layer = state.activeTopologyMaterialLayer;
        if (layer == TopologyMaterialLayer::Base) {
            const float buttonW = (contentW - gap) * 0.5f;
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        TextFormat("%s_copy_material", uvPrefix),
                        Rectangle{0.0f, y, buttonW, 36.0f},
                        font,
                        "Copy")) {
                CopyTopologyMaterial(target);
            }
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        TextFormat("%s_paste_material", uvPrefix),
                        Rectangle{buttonW + gap, y, buttonW, 36.0f},
                        font,
                        "Paste")) {
                PasteTopologyMaterial(target, assets);
            }
            y += 36.0f + gap;
            drawTextureRow(textureButtonId, "Texture:", textureId, field, TopologyMaterialLayer::Base);
            drawUvSettings(uvPrefix, uv, stateOffset);
            return;
        }

        drawTextureRow(textureButtonId, "Texture:", decal.textureId, field, TopologyMaterialLayer::Decal);
        if (decal.textureId.empty()) {
            engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 32.0f}, font, "No decal assigned", engine::UITextJustify::Left, config.mutedTextColor);
            y += 32.0f + gap;
            return;
        }

        drawUvSettings(uvPrefix, decal.uv, stateOffset);
        engine::Text(ui, config, assets, Rectangle{0.0f, y, 82.0f, rowH}, font, "Opacity:", engine::UITextJustify::Left, config.mutedTextColor);
        float opacity = decal.opacity;
        const engine::UINumericInputResult opacityResult = engine::FloatInput(
                ui,
                config,
                input,
                assets,
                TextFormat("%s_decal_opacity", uvPrefix),
                Rectangle{82.0f, y, contentW - 82.0f, rowH},
                font,
                opacity,
                uiState.topologySectorDecalOpacityInputs[opacityStateIndex],
                0.0f,
                1.0f,
                3);
        if (opacityResult.changed && opacity != decal.opacity && std::isfinite(opacity)) {
            ApplySurfaceDecalOpacity(target, opacity, nullptr);
        }
        y += rowH + gap;
        bool emissive = decal.emissive;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    TextFormat("%s_decal_emissive", uvPrefix),
                    Rectangle{0.0f, y, contentW, 36.0f},
                    font,
                    "Emissive",
                    emissive)) {
            ApplySurfaceDecalEmissive(target, emissive, nullptr);
        }
        y += 36.0f + gap;

        engine::Text(ui, config, assets, Rectangle{0.0f, y, 82.0f, rowH}, font, "Tint:", engine::UITextJustify::Left, config.mutedTextColor);
        const Rectangle swatchLocal{82.0f, y + 3.0f, 56.0f, rowH - 6.0f};
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    TextFormat("%s_decal_tint", uvPrefix),
                    swatchLocal,
                    font,
                    "")) {
            OpenDecalTintModal(target);
        }
        const Rectangle swatchScreen{
                scroll.viewport.x + swatchLocal.x,
                scroll.viewport.y - uiState.inspectorScroll.offset.y + swatchLocal.y,
                swatchLocal.width,
                swatchLocal.height};
        DrawRectangleRec(swatchScreen, DecalTintPreviewColor(decal.tint));
        DrawRectangleLinesEx(swatchScreen, config.borderThickness, config.borderColor);
        y += rowH + gap;

        const float decalButtonW = (contentW - gap) * 0.5f;
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    TextFormat("%s_fit_decal", uvPrefix),
                    Rectangle{0.0f, y, decalButtonW, 36.0f},
                    font,
                    "Fit Decal")) {
            FitSelectedDecal(target, nullptr);
        }
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    TextFormat("%s_clear_decal", uvPrefix),
                    Rectangle{decalButtonW + gap, y, decalButtonW, 36.0f},
                    font,
                    "Clear Decal")) {
            ClearSurfaceDecal(target, nullptr);
        }
        y += 36.0f + gap;
    };

    drawSurfaceSection("Floor", "sector_editor_topology_pick_floor", sector->floorTextureId, TopologySectorTextureField::Floor, "sector_editor_topology_floor_uv", sector->floorUv, sector->floorDecal, 0, 0, TopologySurfaceEditTargetKind::SectorFloor);
    drawSurfaceSection("Ceiling", "sector_editor_topology_pick_ceiling", sector->ceilingTextureId, TopologySectorTextureField::Ceiling, "sector_editor_topology_ceiling_uv", sector->ceilingUv, sector->ceilingDecal, 4, 1, TopologySurfaceEditTargetKind::SectorCeiling);

    auto drawWallDefaultSection = [&](const char* title, const char* textureButtonId, SectorTopologyWallPartSettings& part, TopologySectorTextureField field, const char* uvPrefix, int stateOffset) {
        engine::Separator(config, Rectangle{scroll.viewport.x, scroll.viewport.y - uiState.inspectorScroll.offset.y + y, contentW, 12.0f});
        y += 18.0f;
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, title, engine::UITextJustify::Left, config.textColor);
        y += 30.0f;
        drawTextureRow(textureButtonId, "Texture:", part.textureId, field, TopologyMaterialLayer::Base);
        drawUvSettings(uvPrefix, part.uv, stateOffset);
    };

    drawWallDefaultSection("Default Wall", "sector_editor_topology_pick_default_wall", sector->defaultWall, TopologySectorTextureField::DefaultWall, "sector_editor_topology_wall_uv", 8);
    drawWallDefaultSection("Default Lower", "sector_editor_topology_pick_default_lower", sector->defaultLower, TopologySectorTextureField::DefaultLower, "sector_editor_topology_lower_uv", 12);
    drawWallDefaultSection("Default Upper", "sector_editor_topology_pick_default_upper", sector->defaultUpper, TopologySectorTextureField::DefaultUpper, "sector_editor_topology_upper_uv", 16);

    return true;
}

bool SectorEditor::DrawTopologySideDefInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::UIScrollAreaResult scroll,
        float contentW,
        float rowH,
        float gap)
{
    const SectorTopologyLineDef* lineDef = SelectedTopologyLineDef();
    if (lineDef == nullptr) {
        return false;
    }

    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    const bool hasEndpoints = GetSectorTopologyLineVertices(state.topologyMap, *lineDef, start, end);

    float y = 0.0f;
    SectorTopologySideDef* sideDef = SelectedTopologySideDef();
    engine::Text(
            ui,
            config,
            assets,
            Rectangle{0.0f, y, contentW, 34.0f},
            font,
            sideDef != nullptr
                    ? TextFormat(
                            "Topology %s SideDef: %d",
                            SectorTopologySideKindName(sideDef->side),
                            sideDef->id)
                    : TextFormat("Topology LineDef: %d", lineDef->id),
            engine::UITextJustify::Left,
            config.textColor);
    y += 38.0f;

    if (hasEndpoints) {
        const Vector2 from = SectorTopologyVertexToMap(*start);
        const Vector2 to = SectorTopologyVertexToMap(*end);
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 30.0f},
                font,
                TextFormat("Line %d  From %.2f, %.2f  To %.2f, %.2f", lineDef->id, from.x, from.y, to.x, to.y),
                engine::UITextJustify::Left,
                config.mutedTextColor);
        y += 32.0f;
    } else {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, "Line endpoints are invalid", engine::UITextJustify::Left, config.invalidColor);
        y += 32.0f;
    }

    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_split_linedef",
                Rectangle{0.0f, y, contentW, 38.0f},
                font,
                "Split Linedef")) {
        SplitSelectedTopologyLineDef();
        return true;
    }
    y += 38.0f + gap;

    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_split_linedef_at_point",
                Rectangle{0.0f, y, contentW, 38.0f},
                font,
                "Split At Point")) {
        StartPendingTopologyLineSplitAtPoint();
        return true;
    }
    y += 38.0f + gap;

    if (sideDef == nullptr) {
        const int preferredSideDefId = state.selectedTopologySideKind == SectorTopologySideKind::Front
                ? lineDef->frontSideDefId
                : lineDef->backSideDefId;
        const SectorTopologySideDef* preferredSideDef = FindSectorTopologySideDef(
                state.topologyMap,
                preferredSideDefId);
        const SectorTopologySideDef* opposite = preferredSideDef != nullptr
                ? FindOppositeSectorTopologySideDef(state.topologyMap, preferredSideDef->id)
                : nullptr;
        if (preferredSideDef != nullptr && opposite != nullptr
                && preferredSideDef->sectorId != opposite->sectorId) {
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_topology_join_sectors",
                        Rectangle{0.0f, y, contentW, 38.0f},
                        font,
                        "Join Sectors")) {
                JoinSelectedTopologySectors();
                return true;
            }
            y += 38.0f + gap;
        }

        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 64.0f},
                font,
                preferredSideDef == nullptr
                        ? "Line-only selection: this linedef has no sidedef to edit."
                        : "Select a sidedef to edit wall settings.",
                engine::UITextJustify::Left,
                config.mutedTextColor);
        return true;
    }

    engine::Text(
            ui,
            config,
            assets,
            Rectangle{0.0f, y, contentW, 30.0f},
            font,
            TextFormat("Sector %d  Line %d", sideDef->sectorId, sideDef->lineDefId),
            engine::UITextJustify::Left,
            config.mutedTextColor);
    y += 34.0f;

    const SectorTopologySideDef* opposite = FindOppositeSectorTopologySideDef(
            state.topologyMap,
            sideDef->id);
    if (opposite != nullptr) {
        if (opposite->sectorId != sideDef->sectorId) {
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_topology_join_sectors",
                        Rectangle{0.0f, y, contentW, 38.0f},
                        font,
                        "Join Sectors")) {
                JoinSelectedTopologySectors();
                return true;
            }
            y += 38.0f + gap;
        } else {
            engine::Text(
                    ui,
                    config,
                    assets,
                    Rectangle{0.0f, y, contentW, 34.0f},
                    font,
                    "Join Sectors unavailable: both sides already belong to the same sector.",
                    engine::UITextJustify::Left,
                    config.mutedTextColor);
            y += 38.0f;
        }

        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_switch_opposite_side",
                    Rectangle{0.0f, y, contentW, 38.0f},
                    font,
                    TextFormat("Switch to opposite side (%s)", SectorTopologySideKindName(opposite->side)))) {
            const int oppositeId = opposite->id;
            SelectTopologySideDef(oppositeId, state.selectedTopologyWallPart);
            statusText = TextFormat("Selected opposite topology sidedef %d", oppositeId);
            return true;
        }
        y += 38.0f + gap;
    } else {
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 30.0f},
                font,
                "Join Sectors unavailable: opposite side is missing.",
                engine::UITextJustify::Left,
                config.mutedTextColor);
        y += 34.0f;
    }

    auto drawTextureRow = [&](const char* id, const char* label, const std::string& textureId, TopologyWallPart wallPart, TopologyMaterialLayer layer) {
        const float buttonW = 38.0f;
        const float labelColumnW = 74.0f;
        const Rectangle row{0.0f, y, contentW, 36.0f};
        const bool missing = !textureId.empty() && FindSectorTopologyTexture(state.topologyMap, textureId) == nullptr;
        engine::Text(ui, config, assets, Rectangle{row.x, row.y, labelColumnW, row.height}, font, label, engine::UITextJustify::Left, config.mutedTextColor);
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{row.x + labelColumnW, row.y, row.width - labelColumnW - buttonW - gap, row.height},
                font,
                textureId.empty() ? "<none>" : textureId.c_str(),
                engine::UITextJustify::Left,
                missing ? config.invalidColor : config.mutedTextColor);
        if (engine::Button(ui, config, input, assets, id, Rectangle{row.x + row.width - buttonW, row.y, buttonW, row.height}, font, ">")) {
            OpenTopologySideDefTexturePicker(sideDef->id, wallPart, layer);
        }
        y += row.height + gap;
    };

    y += 4.0f;
    const float partButtonW = (contentW - gap * 2.0f) / 3.0f;
    const TopologyWallPart parts[] = {TopologyWallPart::Wall, TopologyWallPart::Lower, TopologyWallPart::Upper};
    for (int i = 0; i < 3; ++i) {
        const TopologyWallPart part = parts[i];
        if (engine::ToolButton(
                    ui,
                    config,
                    input,
                    assets,
                    TextFormat("sector_editor_topology_sidedef_part_%d", i),
                    Rectangle{static_cast<float>(i) * (partButtonW + gap), y, partButtonW, 38.0f},
                    font,
                    TopologyWallPartName(part),
                    state.selectedTopologyWallPart == part)) {
            state.selectedTopologyWallPart = part;
            for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
                inputState = engine::UIFloatInputState{};
            }
            statusText = TextFormat("Editing topology %s UV", TopologyWallPartStatusName(part));
        }
    }
    y += 38.0f + gap;

    const TopologySurfaceEditTarget selectedMaterialTarget{
            TopologyWallPartEditTargetKind(state.selectedTopologyWallPart),
            sideDef->sectorId,
            sideDef->lineDefId,
            sideDef->id,
            sideDef->side};
    const float layerLabelW = 74.0f;
    const float layerButtonW = (contentW - layerLabelW - gap) * 0.5f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, layerLabelW, 36.0f}, font, "Layer:", engine::UITextJustify::Left, config.mutedTextColor);
    if (engine::ToolButton(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_layer_base",
                Rectangle{layerLabelW, y, layerButtonW, 36.0f},
                font,
                "Base",
                state.activeTopologyMaterialLayer == TopologyMaterialLayer::Base)) {
        state.activeTopologyMaterialLayer = TopologyMaterialLayer::Base;
        for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
            inputState = engine::UIFloatInputState{};
        }
        uiState.topologySideDefDecalOpacityInput = engine::UIFloatInputState{};
    }
    if (engine::ToolButton(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_layer_decal",
                Rectangle{layerLabelW + layerButtonW + gap, y, layerButtonW, 36.0f},
                font,
                "Decal",
                state.activeTopologyMaterialLayer == TopologyMaterialLayer::Decal)) {
        state.activeTopologyMaterialLayer = TopologyMaterialLayer::Decal;
        for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
            inputState = engine::UIFloatInputState{};
        }
        uiState.topologySideDefDecalOpacityInput = engine::UIFloatInputState{};
    }
    y += 36.0f + gap;

    SectorTopologyWallPartSettings& selectedPart = TopologyWallPartSettingsFor(*sideDef, state.selectedTopologyWallPart);
    const TopologyMaterialLayer layer = state.activeTopologyMaterialLayer;
    drawTextureRow(
            "sector_editor_topology_sidedef_pick_selected_part",
            "Texture:",
            layer == TopologyMaterialLayer::Decal ? selectedPart.decal.textureId : selectedPart.textureId,
            state.selectedTopologyWallPart,
            layer);

    if (layer == TopologyMaterialLayer::Decal && selectedPart.decal.textureId.empty()) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 32.0f}, font, "No decal assigned", engine::UITextJustify::Left, config.mutedTextColor);
        y += 32.0f + gap;
        return true;
    }

    if (layer == TopologyMaterialLayer::Base) {
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_copy_material",
                    Rectangle{0.0f, y, contentW, 38.0f},
                    font,
                    TextFormat("Copy %s Material", TopologyWallPartName(state.selectedTopologyWallPart)))) {
            CopyTopologyMaterial(selectedMaterialTarget);
        }
        y += 38.0f + gap;

        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_paste_material",
                    Rectangle{0.0f, y, contentW, 38.0f},
                    font,
                    TextFormat("Paste %s Material", TopologyWallPartName(state.selectedTopologyWallPart)))) {
            PasteTopologyMaterial(selectedMaterialTarget, assets);
        }
        y += 38.0f + gap;
    }

    SectorTopologyUvSettings& selectedUv = layer == TopologyMaterialLayer::Decal ? selectedPart.decal.uv : selectedPart.uv;
    const float uvColumnW = (contentW - gap) * 0.5f;
    const float uvBlockH = 62.0f;
    auto drawUvInput = [&](int stateIndex, const char* id, const char* label, float value, float minValue, float maxValue, Rectangle bounds, auto applyValue) {
        engine::Text(ui, config, assets, Rectangle{bounds.x, bounds.y, bounds.width, 26.0f}, font, label, engine::UITextJustify::Left, config.mutedTextColor);
        float edited = value;
        const engine::UINumericInputResult result = engine::FloatInput(
                ui,
                config,
                input,
                assets,
                id,
                Rectangle{bounds.x, bounds.y + 26.0f, bounds.width, 36.0f},
                font,
                edited,
                uiState.topologySideDefUvInputs[stateIndex],
                minValue,
                maxValue,
                3);
        if (result.changed && edited != value) {
            if (!std::isfinite(edited)) {
                statusText = "Invalid topology sidedef UV value";
                return;
            }
            applyValue(edited);
            state.topologyRenderWarning.clear();
            MarkTopologyDocumentEdited(TextFormat(
                    "Updated topology sidedef %d %s %s UV",
                    sideDef->id,
                    TopologyWallPartStatusName(state.selectedTopologyWallPart),
                    TopologyMaterialLayerStatusName(layer)));
        }
    };

    drawUvInput(
            0,
            "sector_editor_topology_sidedef_uv_scale_u",
            "Scale U",
            selectedUv.scale.x,
            TopologyUvScaleMin,
            TopologyUvScaleMax,
            Rectangle{0.0f, y, uvColumnW, uvBlockH},
            [&](float value) { selectedUv.scale.x = value; });
    drawUvInput(
            1,
            "sector_editor_topology_sidedef_uv_scale_v",
            "Scale V",
            selectedUv.scale.y,
            TopologyUvScaleMin,
            TopologyUvScaleMax,
            Rectangle{uvColumnW + gap, y, uvColumnW, uvBlockH},
            [&](float value) { selectedUv.scale.y = value; });
    y += uvBlockH + gap;
    drawUvInput(
            2,
            "sector_editor_topology_sidedef_uv_offset_u",
            "Offset U",
            selectedUv.offset.x,
            -1024.0f,
            1024.0f,
            Rectangle{0.0f, y, uvColumnW, uvBlockH},
            [&](float value) { selectedUv.offset.x = value; });
    drawUvInput(
            3,
            "sector_editor_topology_sidedef_uv_offset_v",
            "Offset V",
            selectedUv.offset.y,
            -1024.0f,
            1024.0f,
            Rectangle{uvColumnW + gap, y, uvColumnW, uvBlockH},
            [&](float value) { selectedUv.offset.y = value; });
    y += uvBlockH + gap;

    if (layer == TopologyMaterialLayer::Decal) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, 82.0f, rowH}, font, "Opacity:", engine::UITextJustify::Left, config.mutedTextColor);
        float opacity = selectedPart.decal.opacity;
        const engine::UINumericInputResult opacityResult = engine::FloatInput(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_decal_opacity",
                Rectangle{82.0f, y, contentW - 82.0f, rowH},
                font,
                opacity,
                uiState.topologySideDefDecalOpacityInput,
                0.0f,
                1.0f,
                3);
        if (opacityResult.changed && opacity != selectedPart.decal.opacity && std::isfinite(opacity)) {
            ApplySurfaceDecalOpacity(selectedMaterialTarget, opacity, &assets);
        }
        y += rowH + gap;

        bool emissive = selectedPart.decal.emissive;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_decal_emissive",
                    Rectangle{0.0f, y, contentW, 36.0f},
                    font,
                    "Emissive",
                    emissive)) {
            ApplySurfaceDecalEmissive(selectedMaterialTarget, emissive, &assets);
        }
        y += 36.0f + gap;

        engine::Text(ui, config, assets, Rectangle{0.0f, y, 82.0f, rowH}, font, "Tint:", engine::UITextJustify::Left, config.mutedTextColor);
        const Rectangle swatchLocal{82.0f, y + 3.0f, 56.0f, rowH - 6.0f};
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_decal_tint",
                    swatchLocal,
                    font,
                    "")) {
            OpenDecalTintModal(selectedMaterialTarget);
        }
        const Rectangle swatchScreen{
                scroll.viewport.x + swatchLocal.x,
                scroll.viewport.y - uiState.inspectorScroll.offset.y + swatchLocal.y,
                swatchLocal.width,
                swatchLocal.height};
        DrawRectangleRec(swatchScreen, DecalTintPreviewColor(selectedPart.decal.tint));
        DrawRectangleLinesEx(swatchScreen, config.borderThickness, config.borderColor);
        y += rowH + gap;

        const float decalButtonW = (contentW - gap) * 0.5f;
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_fit_decal",
                    Rectangle{0.0f, y, decalButtonW, 38.0f},
                    font,
                    "Fit Decal")) {
            FitSelectedDecal(selectedMaterialTarget, &assets);
        }
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_clear_decal",
                    Rectangle{decalButtonW + gap, y, decalButtonW, 38.0f},
                    font,
                    "Clear Decal")) {
            ClearSurfaceDecal(selectedMaterialTarget, &assets);
        }
        y += 38.0f + gap;
    }

    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_reset_uv",
                Rectangle{0.0f, y, contentW, 38.0f},
                font,
                TextFormat("Reset %s UV", TopologyWallPartName(state.selectedTopologyWallPart)))) {
        ResetTopologyUv(selectedUv);
        for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
            inputState = engine::UIFloatInputState{};
        }
        state.topologyRenderWarning.clear();
        MarkTopologyDocumentEdited(TextFormat(
                "Reset topology sidedef %d %s %s UV",
                sideDef->id,
                TopologyWallPartStatusName(state.selectedTopologyWallPart),
                TopologyMaterialLayerStatusName(layer)));
    }
    y += 38.0f + gap;

    const float fitButtonW = (contentW - gap * 2.0f) / 3.0f;
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_fit_width",
                Rectangle{0.0f, y, fitButtonW, 34.0f},
                font,
                "Fit Width")) {
        FitSelectedWallMaterial(selectedMaterialTarget, TopologyUvFitMode::Width, &assets, layer);
    }
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_fit_height",
                Rectangle{fitButtonW + gap, y, fitButtonW, 34.0f},
                font,
                "Fit Height")) {
        FitSelectedWallMaterial(selectedMaterialTarget, TopologyUvFitMode::Height, &assets, layer);
    }
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_fit_both",
                Rectangle{(fitButtonW + gap) * 2.0f, y, fitButtonW, 34.0f},
                font,
                "Fit Both")) {
        FitSelectedWallMaterial(selectedMaterialTarget, TopologyUvFitMode::Both, &assets, layer);
    }
    y += 34.0f + gap;

    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_align_vertical",
                Rectangle{0.0f, y, contentW, 34.0f},
                font,
                "Align Vertical")) {
        AlignSelectedWallMaterialVertical(selectedMaterialTarget, &assets, layer);
    }
    y += 34.0f + gap;

    const float alignButtonW = (contentW - gap) * 0.5f;
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_align_u_prev",
                Rectangle{0.0f, y, alignButtonW, 34.0f},
                font,
                "Align U Prev")) {
        AlignSelectedWallMaterialU(selectedMaterialTarget, TopologyUAlignDirection::Previous, &assets, layer);
    }
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_align_u_next",
                Rectangle{alignButtonW + gap, y, alignButtonW, 34.0f},
                font,
                "Align U Next")) {
        AlignSelectedWallMaterialU(selectedMaterialTarget, TopologyUAlignDirection::Next, &assets, layer);
    }

    return true;
}

void SectorEditor::DrawAddMapTextureModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    AddMapTextureState& modalState = state.addMapTexture;
    if (!modalState.open) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this, &assets](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    CloseAddMapTextureModal(assets);
                    engine::ConsumeEvent(event);
                } else if (event.key.key == KEY_ENTER || event.key.key == KEY_KP_ENTER) {
                    AddSelectedMapTexture(assets);
                    engine::ConsumeEvent(event);
                }
            }
    );
    if (!modalState.open) {
        return;
    }

    RefreshAddMapTexturePreview(assets);

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 135});

    const Rectangle modal{
            (EditorWidth - 880.0f) * 0.5f,
            (EditorHeight - 660.0f) * 0.5f,
            880.0f,
            660.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 245});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    float y = modal.y + 18.0f;
    engine::Text(config, assets, Rectangle{modal.x + 22.0f, y, modal.width - 44.0f, 36.0f}, font, "Add Map Texture");
    y += 50.0f;

    const Rectangle listBounds{modal.x + 22.0f, y, 380.0f, 470.0f};
    const Vector2 contentSize{
            listBounds.width,
            std::max(listBounds.height, config.listItemHeight * static_cast<float>(modalState.optionLabels.size()))
    };
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui,
            config,
            input,
            "sector_editor_add_texture_scroll",
            listBounds,
            contentSize,
            modalState.scroll
    );
    if (!modalState.optionLabels.empty()) {
        const int oldSelection = modalState.selectedPathIndex;
        engine::List(
                ui,
                config,
                input,
                assets,
                "sector_editor_add_texture_list",
                Rectangle{0.0f, 0.0f, listBounds.width - (scroll.scrollY ? config.scrollbarSize : 0.0f), contentSize.y},
                font,
                modalState.optionLabels.data(),
                modalState.optionLabels.size(),
                modalState.selectedPathIndex
        );
        if (modalState.selectedPathIndex != oldSelection) {
            SelectAddMapTexturePath(modalState.selectedPathIndex);
        }
    }
    engine::EndScrollArea(ui, config, input, scroll, modalState.scroll);

    if (!modalState.scanMessage.empty()) {
        engine::Text(
                config,
                assets,
                Rectangle{listBounds.x, listBounds.y + listBounds.height + 8.0f, listBounds.width, 34.0f},
                font,
                modalState.scanMessage.c_str(),
                engine::UITextJustify::Left,
                modalState.paths.empty() ? config.invalidColor : config.mutedTextColor
        );
    }

    const float rightX = modal.x + 430.0f;
    y = modal.y + 68.0f;
    const Rectangle previewBounds{rightX, y, 410.0f, 260.0f};
    engine::Image(config, assets, previewBounds, modalState.previewTexture);
    y += 276.0f;

    const std::string selectedPath = modalState.selectedPathIndex >= 0
            && modalState.selectedPathIndex < static_cast<int>(modalState.paths.size())
            ? modalState.paths[static_cast<size_t>(modalState.selectedPathIndex)]
            : std::string{};
    engine::Text(config, assets, Rectangle{rightX, y, 410.0f, 68.0f}, font, TextFormat("Path: %s", selectedPath.empty() ? "<none>" : selectedPath.c_str()), engine::UITextJustify::Left, config.mutedTextColor);
    y += 76.0f;

    engine::Text(config, assets, Rectangle{rightX, y, 110.0f, 38.0f}, font, "Texture ID", engine::UITextJustify::Left, config.mutedTextColor);
    engine::TextInput(
            ui,
            config,
            input,
            assets,
            "sector_editor_add_texture_id",
            Rectangle{rightX + 136.0f, y, 274.0f, 38.0f},
            font,
            modalState.textureIdBuffer,
            sizeof(modalState.textureIdBuffer),
            0,
            sizeof(modalState.textureIdBuffer) - 1
    );
    y += 54.0f;

    engine::Text(config, assets, Rectangle{rightX, y, 110.0f, 38.0f}, font, "Filtering", engine::UITextJustify::Left, config.mutedTextColor);
    if (engine::ToolButton(ui, config, input, assets, "sector_editor_add_texture_filter_point", Rectangle{rightX + 136.0f, y, 132.0f, 38.0f}, font, "Point", modalState.filter == SectorTextureFilter::Point)) {
        modalState.filter = SectorTextureFilter::Point;
    }
    if (engine::ToolButton(ui, config, input, assets, "sector_editor_add_texture_filter_bilinear", Rectangle{rightX + 278.0f, y, 132.0f, 38.0f}, font, "Bilinear", modalState.filter == SectorTextureFilter::Bilinear)) {
        modalState.filter = SectorTextureFilter::Bilinear;
    }
    y += 44.0f;
    if (engine::ToolButton(ui, config, input, assets, "sector_editor_add_texture_filter_trilinear", Rectangle{rightX + 136.0f, y, 132.0f, 38.0f}, font, "Trilinear", modalState.filter == SectorTextureFilter::Trilinear)) {
        modalState.filter = SectorTextureFilter::Trilinear;
    }
    if (engine::ToolButton(ui, config, input, assets, "sector_editor_add_texture_filter_aniso8x", Rectangle{rightX + 278.0f, y, 132.0f, 38.0f}, font, "Aniso 8x", modalState.filter == SectorTextureFilter::Anisotropic8x)) {
        modalState.filter = SectorTextureFilter::Anisotropic8x;
    }
    y += 54.0f;

    std::string validation;
    ValidateAddMapTextureId(validation);
    modalState.validationMessage = validation;
    if (!modalState.validationMessage.empty()) {
        engine::Text(config, assets, Rectangle{rightX, y, 410.0f, 56.0f}, font, modalState.validationMessage.c_str(), engine::UITextJustify::Left, config.invalidColor);
    }

    const float buttonY = modal.y + modal.height - 64.0f;
    const float buttonW = 150.0f;
    if (engine::Button(ui, config, input, assets, "sector_editor_add_texture_add", Rectangle{modal.x + modal.width - buttonW * 2.0f - 34.0f, buttonY, buttonW, 44.0f}, font, "Add")) {
        AddSelectedMapTexture(assets);
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_add_texture_cancel", Rectangle{modal.x + modal.width - buttonW - 22.0f, buttonY, buttonW, 44.0f}, font, "Cancel")) {
        CloseAddMapTextureModal(assets);
    }

    input.ForEachEvent(
            engine::InputEventType::Any,
            true,
            [](engine::InputEvent& event) {
                engine::ConsumeEvent(event);
            }
    );
}

void SectorEditor::DrawTexturePickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    TexturePickerState& picker = state.texturePicker;
    if (!picker.open) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this, &assets](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    state.texturePicker = TexturePickerState{};
                    engine::ConsumeEvent(event);
                } else if (event.key.key == KEY_ENTER || event.key.key == KEY_KP_ENTER) {
                    ApplyTexturePickerSelection(assets);
                    engine::ConsumeEvent(event);
                }
            }
    );
    if (!picker.open) {
        return;
    }

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 135});

    const Rectangle modal{
            (EditorWidth - 820.0f) * 0.5f,
            (EditorHeight - 620.0f) * 0.5f,
            820.0f,
            620.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 245});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    float y = modal.y + 18.0f;
    engine::Text(
            config,
            assets,
            Rectangle{modal.x + 22.0f, y, modal.width - 44.0f, 36.0f},
            font,
            TextFormat("Pick %s", TopologyPickerTargetLabel(picker)));
    y += 50.0f;

    const Rectangle listBounds{modal.x + 22.0f, y, 350.0f, 420.0f};
    const Vector2 contentSize{
            listBounds.width,
            std::max(listBounds.height, config.listItemHeight * static_cast<float>(picker.optionLabels.size()))
    };
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui,
            config,
            input,
            "sector_editor_texture_picker_scroll",
            listBounds,
            contentSize,
            picker.scroll
    );
    if (!picker.optionLabels.empty()) {
        engine::List(
                ui,
                config,
                input,
                assets,
                "sector_editor_texture_picker_list",
                Rectangle{0.0f, 0.0f, listBounds.width - (scroll.scrollY ? config.scrollbarSize : 0.0f), contentSize.y},
                font,
                picker.optionLabels.data(),
                picker.optionLabels.size(),
                picker.selectedTextureIndex
        );
    }
    engine::EndScrollArea(ui, config, input, scroll, picker.scroll);

    std::string previewTextureId;
    if (picker.selectedTextureIndex >= 0 && picker.selectedTextureIndex < static_cast<int>(picker.textureIds.size())) {
        previewTextureId = picker.textureIds[static_cast<size_t>(picker.selectedTextureIndex)];
    }
    if (previewTextureId.empty()) {
        previewTextureId = CurrentTextureForPickerTarget();
    }

    const Rectangle previewBounds{modal.x + 402.0f, y, 376.0f, 300.0f};
    engine::Image(config, assets, previewBounds, EditorTextureHandleForId(previewTextureId));
    y += 316.0f;

    const SectorTextureDefinition* previewTexture = FindSectorTopologyTexture(state.topologyMap, previewTextureId);
    const std::string path = previewTexture == nullptr ? std::string{} : previewTexture->path;
    engine::Text(config, assets, Rectangle{modal.x + 402.0f, y, 376.0f, 34.0f}, font, TextFormat("Id: %s", previewTextureId.empty() ? "<none>" : previewTextureId.c_str()), engine::UITextJustify::Left, config.textColor);
    y += 38.0f;
    engine::Text(config, assets, Rectangle{modal.x + 402.0f, y, 376.0f, 72.0f}, font, TextFormat("Path: %s", path.empty() ? "<sector default>" : path.c_str()), engine::UITextJustify::Left, config.mutedTextColor);

    const float buttonY = modal.y + modal.height - 64.0f;
    const float buttonW = 150.0f;
    if (engine::Button(ui, config, input, assets, "sector_editor_texture_picker_select", Rectangle{modal.x + modal.width - buttonW * 2.0f - 34.0f, buttonY, buttonW, 44.0f}, font, "Select")) {
        ApplyTexturePickerSelection(assets);
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_texture_picker_cancel", Rectangle{modal.x + modal.width - buttonW - 22.0f, buttonY, buttonW, 44.0f}, font, "Cancel")) {
        state.texturePicker = TexturePickerState{};
    }

    input.ForEachEvent(
            engine::InputEventType::Any,
            true,
            [](engine::InputEvent& event) {
                engine::ConsumeEvent(event);
            }
    );
}

void SectorEditor::DrawSaveLevelModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    SaveLevelModalState& modalState = state.saveLevelModal;
    if (!modalState.open) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    state.saveLevelModal = SaveLevelModalState{};
                    engine::ConsumeEvent(event);
                } else if (event.key.key == KEY_ENTER || event.key.key == KEY_KP_ENTER) {
                    SaveLevelFromModal();
                    engine::ConsumeEvent(event);
                }
            }
    );
    if (!modalState.open) {
        return;
    }

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 135});
    const Rectangle modal{
            (EditorWidth - 660.0f) * 0.5f,
            (EditorHeight - 300.0f) * 0.5f,
            660.0f,
            300.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 245});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    engine::Text(config, assets, Rectangle{modal.x + 24.0f, modal.y + 20.0f, modal.width - 48.0f, 40.0f}, font, "Save Level");
    engine::Text(config, assets, Rectangle{modal.x + 24.0f, modal.y + 82.0f, 100.0f, 42.0f}, font, "Name:", engine::UITextJustify::Left, config.mutedTextColor);
    const engine::UITextInputResult inputResult = engine::TextInput(
            ui,
            config,
            input,
            assets,
            "sector_editor_save_level_name",
            Rectangle{modal.x + 126.0f, modal.y + 80.0f, modal.width - 150.0f, 42.0f},
            font,
            modalState.nameBuffer,
            sizeof(modalState.nameBuffer),
            0,
            sizeof(modalState.nameBuffer) - 1
    );
    if (inputResult.changed) {
        modalState.errorMessage.clear();
    }

    if (!modalState.errorMessage.empty()) {
        engine::Text(
                config,
                assets,
                Rectangle{modal.x + 24.0f, modal.y + 140.0f, modal.width - 48.0f, 48.0f},
                font,
                modalState.errorMessage.c_str(),
                engine::UITextJustify::Left,
                config.invalidColor
        );
    }

    const float buttonY = modal.y + modal.height - 66.0f;
    const float buttonW = 150.0f;
    if (engine::Button(ui, config, input, assets, "sector_editor_save_level_confirm", Rectangle{modal.x + modal.width - buttonW * 2.0f - 36.0f, buttonY, buttonW, 44.0f}, font, "Save")) {
        SaveLevelFromModal();
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_save_level_cancel", Rectangle{modal.x + modal.width - buttonW - 24.0f, buttonY, buttonW, 44.0f}, font, "Cancel")) {
        state.saveLevelModal = SaveLevelModalState{};
    }

    input.ForEachEvent(engine::InputEventType::Any, true, [](engine::InputEvent& event) {
        engine::ConsumeEvent(event);
    });
}

void SectorEditor::DrawLoadLevelModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    LoadLevelModalState& modalState = state.loadLevelModal;
    if (!modalState.open) {
        return;
    }

    const auto requestLoad = [this, &assets]() {
        LoadLevelModalState& loadState = state.loadLevelModal;
        if (loadState.selectedIndex < 0
                || loadState.selectedIndex >= static_cast<int>(loadState.levels.size())) {
            loadState.errorMessage = "Select a level to load";
            return;
        }
        const LevelListEntry selected = loadState.levels[static_cast<size_t>(loadState.selectedIndex)];
        if (state.topologyDocumentDirty) {
            OpenConfirmation(
                    "Load Level",
                    "Discard unsaved changes and load selected level?",
                    [this, &assets, selected]() { LoadLevel(assets, selected.name, selected.jsonAssetPath); }
            );
        } else {
            LoadLevel(assets, selected.name, selected.jsonAssetPath);
        }
    };

    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this, &requestLoad](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    state.loadLevelModal = LoadLevelModalState{};
                    engine::ConsumeEvent(event);
                } else if (event.key.key == KEY_ENTER || event.key.key == KEY_KP_ENTER) {
                    requestLoad();
                    engine::ConsumeEvent(event);
                }
            }
    );
    if (!modalState.open) {
        return;
    }

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 135});
    const Rectangle modal{
            (EditorWidth - 760.0f) * 0.5f,
            (EditorHeight - 660.0f) * 0.5f,
            760.0f,
            660.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 245});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);
    engine::Text(config, assets, Rectangle{modal.x + 24.0f, modal.y + 20.0f, modal.width - 48.0f, 40.0f}, font, "Load Level");

    const Rectangle listBounds{modal.x + 24.0f, modal.y + 74.0f, modal.width - 48.0f, 450.0f};
    const Vector2 contentSize{
            listBounds.width,
            std::max(listBounds.height, config.listItemHeight * static_cast<float>(modalState.optionLabels.size()))
    };
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui,
            config,
            input,
            "sector_editor_load_level_scroll",
            listBounds,
            contentSize,
            modalState.scroll
    );
    if (!modalState.optionLabels.empty()) {
        engine::List(
                ui,
                config,
                input,
                assets,
                "sector_editor_load_level_list",
                Rectangle{0.0f, 0.0f, listBounds.width - (scroll.scrollY ? config.scrollbarSize : 0.0f), contentSize.y},
                font,
                modalState.optionLabels.data(),
                modalState.optionLabels.size(),
                modalState.selectedIndex
        );
    }
    engine::EndScrollArea(ui, config, input, scroll, modalState.scroll);

    const char* message = modalState.errorMessage.empty()
            ? (modalState.levels.empty() ? "No levels found." : "")
            : modalState.errorMessage.c_str();
    if (message[0] != '\0') {
        engine::Text(
                config,
                assets,
                Rectangle{modal.x + 24.0f, modal.y + 536.0f, modal.width - 48.0f, 40.0f},
                font,
                message,
                engine::UITextJustify::Left,
                modalState.errorMessage.empty() ? config.mutedTextColor : config.invalidColor
        );
    }

    const float buttonY = modal.y + modal.height - 66.0f;
    const float buttonW = 150.0f;
    if (engine::Button(ui, config, input, assets, "sector_editor_load_level_confirm", Rectangle{modal.x + modal.width - buttonW * 2.0f - 36.0f, buttonY, buttonW, 44.0f}, font, "Load")) {
        requestLoad();
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_load_level_cancel", Rectangle{modal.x + modal.width - buttonW - 24.0f, buttonY, buttonW, 44.0f}, font, "Cancel")) {
        state.loadLevelModal = LoadLevelModalState{};
    }

    input.ForEachEvent(engine::InputEventType::Any, true, [](engine::InputEvent& event) {
        engine::ConsumeEvent(event);
    });
}

void SectorEditor::DrawConfirmationModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    ConfirmationModalState& modalState = state.confirmationModal;
    if (!modalState.open) {
        return;
    }

    bool okayRequested = false;
    bool cancelRequested = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&okayRequested, &cancelRequested](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    cancelRequested = true;
                    engine::ConsumeEvent(event);
                } else if (event.key.key == KEY_ENTER || event.key.key == KEY_KP_ENTER) {
                    okayRequested = true;
                    engine::ConsumeEvent(event);
                }
            }
    );

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 145});
    const Rectangle modal{
            (EditorWidth - 680.0f) * 0.5f,
            (EditorHeight - 300.0f) * 0.5f,
            680.0f,
            300.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 248});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);
    engine::Text(config, assets, Rectangle{modal.x + 26.0f, modal.y + 22.0f, modal.width - 52.0f, 42.0f}, font, modalState.title.c_str());
    engine::Text(config, assets, Rectangle{modal.x + 26.0f, modal.y + 86.0f, modal.width - 52.0f, 88.0f}, font, modalState.message.c_str(), engine::UITextJustify::Left, config.mutedTextColor);

    const float buttonY = modal.y + modal.height - 68.0f;
    const float buttonW = 150.0f;
    okayRequested = okayRequested || engine::Button(ui, config, input, assets, "sector_editor_confirmation_okay", Rectangle{modal.x + modal.width - buttonW * 2.0f - 38.0f, buttonY, buttonW, 44.0f}, font, "Okay");
    cancelRequested = cancelRequested || engine::Button(ui, config, input, assets, "sector_editor_confirmation_cancel", Rectangle{modal.x + modal.width - buttonW - 26.0f, buttonY, buttonW, 44.0f}, font, "Cancel");

    input.ForEachEvent(engine::InputEventType::Any, true, [](engine::InputEvent& event) {
        engine::ConsumeEvent(event);
    });

    if (cancelRequested) {
        state.confirmationModal = ConfirmationModalState{};
        return;
    }
    if (okayRequested) {
        std::function<void()> action = std::move(state.confirmationModal.onOkay);
        state.confirmationModal = ConfirmationModalState{};
        if (action) {
            action();
        }
    }
}

void SectorEditor::DrawDecalTintModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    DecalTintModalState& modalState = state.decalTintModal;
    if (!modalState.open) {
        return;
    }

    bool okayRequested = false;
    bool cancelRequested = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&okayRequested, &cancelRequested](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    cancelRequested = true;
                    engine::ConsumeEvent(event);
                } else if (event.key.key == KEY_ENTER || event.key.key == KEY_KP_ENTER) {
                    okayRequested = true;
                    engine::ConsumeEvent(event);
                }
            }
    );

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 145});
    const Rectangle modal{
            (EditorWidth - 560.0f) * 0.5f,
            (EditorHeight - 390.0f) * 0.5f,
            560.0f,
            390.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 248});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    float y = modal.y + 22.0f;
    engine::Text(config, assets, Rectangle{modal.x + 26.0f, y, modal.width - 52.0f, 42.0f}, font, "Decal Tint");
    y += 58.0f;

    const float labelW = 72.0f;
    const float inputW = 120.0f;
    const float inputH = 38.0f;
    const float gap = 12.0f;
    auto drawFloat = [&](const char* id, const char* label, float& value, engine::UIFloatInputState& inputState) {
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{modal.x + 28.0f, y, labelW, inputH},
                font,
                label,
                engine::UITextJustify::Left,
                config.mutedTextColor);
        const engine::UINumericInputResult result = engine::FloatInput(
                ui,
                config,
                input,
                assets,
                id,
                Rectangle{modal.x + 28.0f + labelW, y, inputW, inputH},
                font,
                value,
                inputState,
                0.0f,
                1.0f,
                3);
        if (result.changed) {
            if (!std::isfinite(value)) {
                modalState.errorMessage = "Tint values must be finite.";
            } else {
                value = std::clamp(value, 0.0f, 1.0f);
                modalState.errorMessage.clear();
            }
        }
        y += inputH + gap;
    };

    drawFloat("sector_editor_decal_tint_r", "R", modalState.tint.x, modalState.redInput);
    drawFloat("sector_editor_decal_tint_g", "G", modalState.tint.y, modalState.greenInput);
    drawFloat("sector_editor_decal_tint_b", "B", modalState.tint.z, modalState.blueInput);

    const Rectangle swatch{modal.x + 270.0f, modal.y + 88.0f, 210.0f, 124.0f};
    DrawRectangleRec(swatch, DecalTintPreviewColor(modalState.tint));
    DrawRectangleLinesEx(swatch, config.borderThickness, config.borderColor);
    engine::Text(
            config,
            assets,
            Rectangle{swatch.x, swatch.y + swatch.height + 10.0f, swatch.width, 26.0f},
            font,
            "Preview",
            engine::UITextJustify::Center,
            config.mutedTextColor);

    if (!modalState.errorMessage.empty()) {
        engine::Text(
                config,
                assets,
                Rectangle{modal.x + 28.0f, modal.y + 250.0f, modal.width - 56.0f, 34.0f},
                font,
                modalState.errorMessage.c_str(),
                engine::UITextJustify::Left,
                config.invalidColor);
    }

    const float buttonY = modal.y + modal.height - 66.0f;
    const float buttonW = 124.0f;
    if (engine::Button(ui, config, input, assets, "sector_editor_decal_tint_reset", Rectangle{modal.x + 28.0f, buttonY, buttonW, 44.0f}, font, "Reset White")) {
        modalState.tint = Vector3{1.0f, 1.0f, 1.0f};
        modalState.redInput = engine::UIFloatInputState{};
        modalState.greenInput = engine::UIFloatInputState{};
        modalState.blueInput = engine::UIFloatInputState{};
        modalState.errorMessage.clear();
    }
    okayRequested = okayRequested || engine::Button(ui, config, input, assets, "sector_editor_decal_tint_ok", Rectangle{modal.x + modal.width - buttonW * 2.0f - 38.0f, buttonY, buttonW, 44.0f}, font, "OK");
    cancelRequested = cancelRequested || engine::Button(ui, config, input, assets, "sector_editor_decal_tint_cancel", Rectangle{modal.x + modal.width - buttonW - 26.0f, buttonY, buttonW, 44.0f}, font, "Cancel");

    input.ForEachEvent(engine::InputEventType::Any, true, [](engine::InputEvent& event) {
        engine::ConsumeEvent(event);
    });

    if (cancelRequested) {
        state.decalTintModal = DecalTintModalState{};
        return;
    }
    if (!okayRequested) {
        return;
    }
    if (!IsValidDecalTint(modalState.tint)) {
        modalState.errorMessage = "Tint values must be between 0 and 1.";
        statusText = modalState.errorMessage;
        return;
    }

    const TopologySurfaceEditTarget target = modalState.target;
    const SectorTopologyDecalLayer* decal = DecalForSurface(target);
    if (!IsValidTopologySurfaceEditTarget(target) || decal == nullptr || decal->textureId.empty()) {
        modalState.errorMessage = "Decal target is no longer valid.";
        statusText = modalState.errorMessage;
        return;
    }

    const Vector3 tint = modalState.tint;
    const bool changed = !SameTint(decal->tint, tint);
    if (changed && !ApplySurfaceDecalTint(target, tint, &assets)) {
        modalState.errorMessage = statusText.empty() ? "Could not set decal tint." : statusText;
        return;
    }
    state.decalTintModal = DecalTintModalState{};
}

void SectorEditor::DrawLightmapBakeModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    if (!IsLightmapBakeBlocking()) {
        return;
    }

    const bool running = lightmapBake.progress.running.load();
    const SectorLightmapBakePhase phase = lightmapBake.progress.phase.load();
    const uint32_t completedWork = lightmapBake.progress.completedWork.load();
    const uint32_t totalWork = lightmapBake.progress.totalWork.load();
    const float progress = LightmapBakeOverallProgress(phase, completedWork, totalWork);
    const double now = GetTime();
    const double elapsed = (lightmapBake.completedTimeSeconds > 0.0 ? lightmapBake.completedTimeSeconds : now)
            - lightmapBake.startTimeSeconds;

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 145});

    const Rectangle modal{
            (EditorWidth - 620.0f) * 0.5f,
            (EditorHeight - 300.0f) * 0.5f,
            620.0f,
            300.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 248});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    float y = modal.y + 24.0f;
    engine::Text(config, assets, Rectangle{modal.x + 28.0f, y, modal.width - 56.0f, 42.0f}, font, "Baking lightmap", engine::UITextJustify::Left);
    y += 58.0f;

    const char* phaseText = lightmapBake.awaitingAcknowledgement && !lightmapBake.terminalMessage.empty()
            ? lightmapBake.terminalMessage.c_str()
            : (lightmapBake.cancelButtonPressed && running ? "Cancelling bake..." : LightmapBakePhaseText(phase));
    const Color phaseColor = lightmapBake.awaitingAcknowledgement && !lightmapBake.terminalCancelled
            ? config.invalidColor
            : config.textColor;
    engine::Text(config, assets, Rectangle{modal.x + 28.0f, y, modal.width - 56.0f, 38.0f}, font, phaseText, engine::UITextJustify::Left, phaseColor);
    y += 52.0f;

    const Rectangle track{modal.x + 28.0f, y, modal.width - 128.0f, 28.0f};
    DrawRectangleRec(track, config.widgetColor);
    DrawRectangleLinesEx(track, 1.0f, config.borderColor);
    const Rectangle fill{track.x, track.y, track.width * progress, track.height};
    DrawRectangleRec(fill, config.accentColor);
    engine::Text(
            config,
            assets,
            Rectangle{track.x + track.width + 14.0f, y - 4.0f, 72.0f, 36.0f},
            font,
            TextFormat("%d%%", static_cast<int>(std::round(progress * 100.0f))),
            engine::UITextJustify::Right
    );
    y += 56.0f;

    engine::Text(
            config,
            assets,
            Rectangle{modal.x + 28.0f, y, modal.width - 56.0f, 38.0f},
            font,
            TextFormat("Elapsed: %.1fs", std::max(0.0, elapsed)),
            engine::UITextJustify::Left,
            config.mutedTextColor
    );

    const float buttonW = 150.0f;
    const float buttonH = 44.0f;
    const Rectangle button{modal.x + modal.width - buttonW - 28.0f, modal.y + modal.height - buttonH - 24.0f, buttonW, buttonH};
    if (running) {
        if (lightmapBake.cancelButtonPressed) {
            DrawRectangleRec(button, config.disabledColor);
            DrawRectangleLinesEx(button, config.borderThickness, config.borderColor);
            engine::Text(config, assets, button, font, "Cancel", engine::UITextJustify::Center, config.mutedTextColor);
        } else if (engine::Button(ui, config, input, assets, "sector_editor_lightmap_bake_cancel", button, font, "Cancel")) {
            RequestLightmapBakeCancel();
        }
    } else if (lightmapBake.awaitingAcknowledgement) {
        if (engine::Button(ui, config, input, assets, "sector_editor_lightmap_bake_close", button, font, "Close")) {
            lightmapBake.modalOpen = false;
            lightmapBake.awaitingAcknowledgement = false;
            lightmapBake.cancelButtonPressed = false;
            lightmapBake.terminalMessage.clear();
            lightmapBake.temporaryOutputPath.clear();
            lightmapBake.progress.phase.store(SectorLightmapBakePhase::Idle);
        }
    }

    input.ForEachEvent(
            engine::InputEventType::Any,
            true,
            [](engine::InputEvent& event) {
                engine::ConsumeEvent(event);
            }
    );
}

void SectorEditor::DrawStatusPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    (void)ui;
    const Rectangle panel = BuildBottomPanelRect();
    DrawRectangleRec(panel, config.panelColor);
    DrawRectangleLinesEx(panel, config.borderThickness, config.borderColor);

    std::string selectedLabel = "none";
    if (const SectorTopologyStaticPointLight* light = SelectedTopologyLight()) {
        selectedLabel = TextFormat("topology light %d", light->id);
    } else if (const SectorTopologySector* topologySector = SelectedTopologySector()) {
        selectedLabel = topologySector->name.empty()
                ? TextFormat("topology sector %d", topologySector->id)
                : TextFormat("%s (%d)", topologySector->name.c_str(), topologySector->id);
    }

    std::string pendingText;
    if (state.pendingSector.active) {
        pendingText = TextFormat(" | pending %zu pts", state.pendingSector.points.size());
    } else if (state.pendingTopologyLineSplitAtPoint.active) {
        pendingText = " | split at point";
    }
    const std::string shortMapPath = state.hasCurrentLevelPath
            ? state.currentLevelPath
            : std::string{"<untitled>"};
    const char* lightmapText = SectorLightmapStatusText(GetSectorLightmapStatus(state.topologyMap));
    std::string status = statusText.empty() ? "Ready" : statusText;
    if (!state.topologyRenderWarning.empty()) {
        status += " | ";
        status += state.topologyRenderWarning;
    }
    const char* text = TextFormat(
            "%s%s | %s%s | map %s%s | grid %d | %s | selected %s",
            status.c_str(),
            state.topologyDocumentDirty ? " *modified" : "",
            ToolHelpText(state.currentTool),
            pendingText.c_str(),
            shortMapPath.c_str(),
            state.topologyDocumentDirty ? "*" : "",
            state.gridSize,
            lightmapText,
            selectedLabel.c_str()
    );

    engine::Text(
            config,
            assets,
            Rectangle{panel.x + 18.0f, panel.y + 17.0f, panel.width - 36.0f, 44.0f},
            font,
            text,
            engine::UITextJustify::Left
    );
}

void SectorEditor::ResetToBlankMap(engine::AssetManager& assets)
{
    ShutdownLightmapBake();
    preview.Shutdown(assets);
    if (!engine::IsNull(state.editorTextureScope)) {
        assets.UnloadScope(state.editorTextureScope);
    }
    if (!engine::IsNull(state.addMapTexture.previewScope)) {
        assets.UnloadScope(state.addMapTexture.previewScope);
    }

    state = SectorEditorState{};
    uiState = SectorEditorUiState{};
    ResetEditorTopologyDocumentState(state);
    state.viewCenter = Vector2{9.0f, 6.0f};
    state.viewZoom = 48.0f;
    state.gridSize = 8;
    RefreshDefaultTextures();
    RefreshEditorTextureAssets(assets);
    initialized = true;
    statusText = "New blank level";
}

bool SectorEditor::LoadLevel(
        engine::AssetManager& assets,
        const std::string& levelName,
        const std::string& jsonAssetPath)
{
    const bool hadPendingSplit = state.pendingTopologyLineSplitAtPoint.active;
    SectorTopologyMap loaded;
    std::string error;
    const std::string filePath = ResolveEditorAssetPath(jsonAssetPath);
    if (!LoadSectorTopologyMap(filePath.c_str(), loaded, &error)) {
        state.loadLevelModal.errorMessage = error.empty()
                ? "Topology v2 load failed"
                : TextFormat("Topology v2 load failed: %s", error.c_str());
        statusText = state.loadLevelModal.errorMessage;
        return false;
    }

    preview.Shutdown(assets);
    CancelPendingSector(nullptr);
    CancelPendingTopologyVertexMerge(nullptr);
    CancelPendingTopologyLineSplitAtPoint(nullptr);
    CancelPendingTopologySectorCut(nullptr);
    CancelVertexDrag(nullptr);
    CancelLightDrag(nullptr);
    state.topologyMap = std::move(loaded);
    state.topologyDocumentInitialized = true;
    state.topologyDocumentDirty = false;
    state.topologyDocumentStatus = TextFormat("Topology document: loaded %s", jsonAssetPath.c_str());
    state.currentLevelName = levelName;
    state.currentLevelPath = jsonAssetPath;
    state.hasCurrentLevelPath = true;
    state.hasUnsavedChanges = false;
    state.mode = SectorEditorMode::Edit2D;
    state.hasPreviewPose = false;
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.texturePicker = TexturePickerState{};
    state.loadLevelModal = LoadLevelModalState{};
    state.saveLevelModal = SaveLevelModalState{};
    state.confirmationModal = ConfirmationModalState{};
    state.decalTintModal = DecalTintModalState{};
    ClearSelection();
    state.hoveredTopologyLightId = -1;
    state.hasHoveredVertex = false;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    state.pendingSector = PendingSectorDraw{};
    state.vertexDrag = VertexDragState{};
    state.lightDrag = LightDragState{};
    RefreshDefaultTextures();
    RefreshEditorTextureAssets(assets);
    statusText = hadPendingSplit
            ? TextFormat("Loaded topology %s; split at point cancelled", jsonAssetPath.c_str())
            : TextFormat("Loaded topology %s", jsonAssetPath.c_str());
    return true;
}

void SectorEditor::OpenConfirmation(const char* title, const char* message, std::function<void()> onOkay)
{
    state.confirmationModal.open = true;
    state.confirmationModal.title = title == nullptr ? "Confirm" : title;
    state.confirmationModal.message = message == nullptr ? "" : message;
    state.confirmationModal.onOkay = std::move(onOkay);
}

void SectorEditor::OpenNewConfirmation(engine::AssetManager& assets)
{
    OpenConfirmation(
            "New Level",
            "Discard current level and create a new blank map?",
            [this, &assets]() { ResetToBlankMap(assets); }
    );
}

void SectorEditor::OpenReloadConfirmation(engine::AssetManager& assets)
{
    if (!state.hasCurrentLevelPath) {
        statusText = "No saved level to reload.";
        return;
    }
    const std::string name = state.currentLevelName;
    const std::string path = state.currentLevelPath;
    OpenConfirmation(
            "Reload Level",
            "Reload current level from disk and discard unsaved changes?",
            [this, &assets, name, path]() { LoadLevel(assets, name, path); }
    );
}

void SectorEditor::OpenSaveLevelModal()
{
    state.saveLevelModal = SaveLevelModalState{};
    state.saveLevelModal.open = true;
    if (state.hasCurrentLevelPath) {
        std::snprintf(
                state.saveLevelModal.nameBuffer,
                sizeof(state.saveLevelModal.nameBuffer),
                "%s",
                state.currentLevelName.c_str()
        );
    }
}

void SectorEditor::RefreshLevelList()
{
    LoadLevelModalState& modal = state.loadLevelModal;
    modal.levels = ScanLevels(modal.errorMessage);
    modal.optionLabels.clear();
    modal.optionLabels.reserve(modal.levels.size());
    for (const LevelListEntry& level : modal.levels) {
        modal.optionLabels.push_back(level.name.c_str());
    }
    modal.selectedIndex = modal.levels.empty() ? -1 : 0;
    modal.scroll = engine::UIScrollState{};
}

void SectorEditor::OpenLoadLevelModal()
{
    state.loadLevelModal = LoadLevelModalState{};
    state.loadLevelModal.open = true;
    RefreshLevelList();
}

bool SectorEditor::SaveLevelFromModal(bool overwriteConfirmed)
{
    SaveLevelModalState& modal = state.saveLevelModal;
    const std::string name = modal.nameBuffer;
    LevelPaths paths;
    if (!BuildLevelPaths(name, paths, modal.errorMessage)) {
        return false;
    }

    const bool savingToCurrentPath = state.hasCurrentLevelPath
            && state.currentLevelPath == paths.jsonAssetPath;
    std::error_code ec;
    const bool targetExists = std::filesystem::exists(paths.jsonFilePath, ec);
    if (ec) {
        modal.errorMessage = TextFormat("Could not check target level: %s", ec.message().c_str());
        return false;
    }
    if (targetExists && !savingToCurrentPath && !overwriteConfirmed) {
        OpenConfirmation(
                "Overwrite Level",
                "Level already exists. Overwrite it?",
                [this]() { SaveLevelFromModal(true); }
        );
        return false;
    }

    std::filesystem::create_directories(paths.directoryPath, ec);
    if (ec) {
        modal.errorMessage = TextFormat("Could not create level directory: %s", ec.message().c_str());
        return false;
    }

    std::string saveError;
    if (!SaveSectorTopologyMap(paths.jsonFilePath.string().c_str(), state.topologyMap, &saveError)) {
        modal.errorMessage = saveError.empty()
                ? "Could not write topology level JSON"
                : TextFormat("Could not write topology level JSON: %s", saveError.c_str());
        statusText = TextFormat("Save failed: %s", paths.jsonAssetPath.c_str());
        return false;
    }

    state.currentLevelName = name;
    state.currentLevelPath = paths.jsonAssetPath;
    state.hasCurrentLevelPath = true;
    state.hasUnsavedChanges = false;
    state.topologyDocumentInitialized = true;
    state.topologyDocumentDirty = false;
    state.topologyDocumentStatus = TextFormat("Topology document: saved %s", paths.jsonAssetPath.c_str());
    state.saveLevelModal = SaveLevelModalState{};
    state.confirmationModal = ConfirmationModalState{};
    state.decalTintModal = DecalTintModalState{};
    statusText = TextFormat("Saved topology %s", paths.jsonAssetPath.c_str());
    return true;
}

bool SectorEditor::HasDocumentModalOpen() const
{
    return state.saveLevelModal.open
            || state.loadLevelModal.open
            || state.confirmationModal.open
            || state.decalTintModal.open;
}

bool SectorEditor::TryEnterPreview3D(engine::AssetManager& assets, engine::UIContext& ui)
{
    if (!initialized) {
        statusText = "3D mode failed: no map loaded";
        return false;
    }

    CancelPendingSector(nullptr);
    CancelPendingTopologyVertexMerge(nullptr);
    CancelPendingTopologyLineSplitAtPoint(nullptr);
    CancelPendingTopologySectorCut(nullptr);
    CancelVertexDrag(nullptr);
    CancelLightDrag(nullptr);
    ui.hotId = 0;
    ui.activeId = 0;
    ui.openOptionId = 0;
    ui.focusedId = 0;
    uiState.keyboardCaptured = false;

    std::string error;
    if (!preview.Rebuild(assets, state.topologyMap, "sector_editor_preview", error)) {
        state.mode = SectorEditorMode::Edit2D;
        if (StartsWith(error, "Preview failed:")) {
            statusText = std::string{"3D mode failed:"} + error.substr(std::strlen("Preview failed:"));
        } else {
            statusText = error.empty() ? "3D mode failed" : error;
        }
        return false;
    }

    if (state.hasPreviewPose) {
        preview.ApplyPose(state.lastPreviewPose);
    }

    state.mode = SectorEditorMode::Preview3D;
    state.previewUiHidden = false;
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    statusText = TextFormat(
            "3D mode rebuilt: %zu batches, %d triangles",
            preview.BatchCount(),
            preview.TriangleCount()
    );
    return true;
}

void SectorEditor::LeavePreview3D()
{
    state.lastPreviewPose = preview.Pose();
    state.hasPreviewPose = true;
    state.mode = SectorEditorMode::Edit2D;
    state.hoveredSurface3D = SectorSurfaceHit{};
    preview.Leave();
    statusText = "Returned to 2D editor";
}

void SectorEditor::RefreshDefaultTextures()
{
    auto findTexture = [this](const char* preferred, const std::string& fallback = std::string{}) {
        const auto preferredIt = state.topologyMap.texturesById.find(preferred);
        if (preferredIt != state.topologyMap.texturesById.end()) {
            return preferredIt->first;
        }
        if (!fallback.empty()) {
            return fallback;
        }
        const std::vector<std::string> textureIds = SortedSectorTopologyTextureIds(state.topologyMap);
        return textureIds.empty() ? std::string{} : textureIds.front();
    };

    state.defaultWallTextureId = findTexture("wall");
    state.defaultFloorTextureId = findTexture("floor");
    state.defaultCeilingTextureId = findTexture("ceiling");
    state.defaultLowerWallTextureId = findTexture("step_wall", state.defaultWallTextureId);
    state.defaultUpperWallTextureId = findTexture("upper_wall", state.defaultWallTextureId);
}

void SectorEditor::RefreshEditorTextureAssets(engine::AssetManager& assets)
{
    if (!engine::IsNull(state.editorTextureScope)) {
        assets.UnloadScope(state.editorTextureScope);
        state.editorTextureScope = engine::NullAssetScopeHandle();
    }
    state.editorTextureHandlesById.clear();

    if (state.topologyMap.texturesById.empty()) {
        return;
    }

    state.editorTextureScope = assets.CreateScope("sector_editor_textures");
    if (engine::IsNull(state.editorTextureScope)) {
        return;
    }

    for (const std::string& textureId : SortedSectorTopologyTextureIds(state.topologyMap)) {
        const SectorTextureDefinition* texture = FindSectorTopologyTexture(state.topologyMap, textureId);
        if (texture == nullptr) {
            continue;
        }

        const std::string resolvedPath = ResolveEditorAssetPath(texture->path);
        state.editorTextureHandlesById.emplace(
                texture->id,
                assets.RequestTexture(
                        state.editorTextureScope,
                        texture->id.c_str(),
                        resolvedPath.c_str(),
                        SectorTextureLoadFlags(texture->filter)
                )
        );
    }
}

engine::TextureHandle SectorEditor::EditorTextureHandleForId(const std::string& textureId) const
{
    const auto it = state.editorTextureHandlesById.find(textureId);
    return it == state.editorTextureHandlesById.end() ? engine::NullTextureHandle() : it->second;
}

void SectorEditor::OpenAddMapTextureModal(engine::AssetManager& assets)
{
    CloseAddMapTextureModal(assets);
    state.addMapTexture.open = true;
    RefreshAddMapTextureScan();
    SelectAddMapTexturePath(state.addMapTexture.selectedPathIndex);
    statusText = "Add topology texture";
}

void SectorEditor::CloseAddMapTextureModal(engine::AssetManager& assets)
{
    if (!engine::IsNull(state.addMapTexture.previewScope)) {
        assets.UnloadScope(state.addMapTexture.previewScope);
    }
    state.addMapTexture = AddMapTextureState{};
}

void SectorEditor::RefreshAddMapTextureScan()
{
    AddMapTextureState& modalState = state.addMapTexture;
    modalState.paths = ScanAssetImagePngs(modalState.scanMessage);
    modalState.optionLabels.clear();
    modalState.optionLabels.reserve(modalState.paths.size());
    for (const std::string& path : modalState.paths) {
        modalState.optionLabels.push_back(path.c_str());
    }
    modalState.scanned = true;
    modalState.selectedPathIndex = modalState.paths.empty() ? -1 : 0;
}

void SectorEditor::SelectAddMapTexturePath(int pathIndex)
{
    AddMapTextureState& modalState = state.addMapTexture;
    if (pathIndex < 0 || pathIndex >= static_cast<int>(modalState.paths.size())) {
        modalState.selectedPathIndex = -1;
        modalState.textureIdBuffer[0] = '\0';
        return;
    }

    modalState.selectedPathIndex = pathIndex;
    const std::string base = GeneratedTextureIdBase(modalState.paths[static_cast<size_t>(pathIndex)]);
    std::string uniqueId = base;
    int suffix = 1;
    while (FindSectorTopologyTexture(state.topologyMap, uniqueId) != nullptr) {
        char suffixBuffer[16] = {};
        std::snprintf(suffixBuffer, sizeof(suffixBuffer), "_%03d", suffix);
        uniqueId = base + suffixBuffer;
        ++suffix;
    }

    std::snprintf(modalState.textureIdBuffer, sizeof(modalState.textureIdBuffer), "%s", uniqueId.c_str());
    modalState.previewPath.clear();
    modalState.previewTexture = engine::NullTextureHandle();
}

void SectorEditor::RefreshAddMapTexturePreview(engine::AssetManager& assets)
{
    AddMapTextureState& modalState = state.addMapTexture;
    const bool hasSelection = modalState.selectedPathIndex >= 0
            && modalState.selectedPathIndex < static_cast<int>(modalState.paths.size());
    if (!hasSelection) {
        modalState.previewTexture = engine::NullTextureHandle();
        modalState.previewPath.clear();
        return;
    }

    const std::string& path = modalState.paths[static_cast<size_t>(modalState.selectedPathIndex)];
    if (modalState.previewTexture != engine::NullTextureHandle()
            && modalState.previewPath == path
            && modalState.previewFilter == modalState.filter) {
        return;
    }

    if (!engine::IsNull(modalState.previewScope)) {
        assets.UnloadScope(modalState.previewScope);
        modalState.previewScope = engine::NullAssetScopeHandle();
    }
    modalState.previewTexture = engine::NullTextureHandle();
    modalState.previewPath = path;
    modalState.previewFilter = modalState.filter;

    modalState.previewScope = assets.CreateScope("sector_editor_add_texture_preview");
    if (engine::IsNull(modalState.previewScope)) {
        return;
    }

    const std::string resolvedPath = ResolveEditorAssetPath(path);
    modalState.previewTexture = assets.RequestTexture(
            modalState.previewScope,
            "selected_preview",
            resolvedPath.c_str(),
            SectorTextureLoadFlags(modalState.filter)
    );
}

bool SectorEditor::ValidateAddMapTextureId(std::string& error) const
{
    error.clear();
    const AddMapTextureState& modalState = state.addMapTexture;
    if (modalState.selectedPathIndex < 0 || modalState.selectedPathIndex >= static_cast<int>(modalState.paths.size())) {
        error = "Select a PNG file";
        return false;
    }

    const std::string id = modalState.textureIdBuffer;
    if (id.empty()) {
        error = "Texture ID is required";
        return false;
    }
    if (!IsValidTextureId(id)) {
        error = "Texture ID may only contain letters, digits, underscores, and dashes";
        return false;
    }
    return true;
}

bool SectorEditor::AddSelectedMapTexture(engine::AssetManager& assets)
{
    std::string error;
    if (!ValidateAddMapTextureId(error)) {
        state.addMapTexture.validationMessage = error;
        return false;
    }

    AddMapTextureState& modalState = state.addMapTexture;
    const std::string id = modalState.textureIdBuffer;
    const std::string path = modalState.paths[static_cast<size_t>(modalState.selectedPathIndex)];
    const bool replacing = FindSectorTopologyTexture(state.topologyMap, id) != nullptr;

    SectorTextureDefinition definition;
    definition.id = id;
    definition.path = path;
    definition.filter = modalState.filter;
    state.topologyMap.texturesById[id] = std::move(definition);

    RefreshEditorTextureAssets(assets);
    state.hasUnsavedChanges = true;
    state.topologyDocumentDirty = true;
    statusText = TextFormat("%s texture %s", replacing ? "Updated" : "Added", id.c_str());
    CloseAddMapTextureModal(assets);
    return true;
}

bool SectorEditor::PointInTopologyLoop(Vector2 mapPoint, const SectorTopologyLoop& loop) const
{
    std::vector<SectorPoint> points;
    points.reserve(loop.vertexIds.size());
    for (int vertexId : loop.vertexIds) {
        const SectorTopologyVertex* vertex = FindSectorTopologyVertex(state.topologyMap, vertexId);
        if (vertex == nullptr) {
            return false;
        }
        points.push_back(Vector2ToSectorPoint(SectorTopologyVertexToMap(*vertex)));
    }
    const SectorPoint point = Vector2ToSectorPoint(mapPoint);
    return StrictPointInPolygon(point, points) || PointOnPolygonBoundary(point, points);
}

bool SectorEditor::PointInTopologySector(Vector2 mapPoint, const SectorTopologySector& sector) const
{
    SectorTopologyLoopSet loops;
    std::vector<SectorTopologyValidationIssue> loopIssues;
    if (!ExtractSectorTopologyLoops(state.topologyMap, sector.id, loops, &loopIssues)) {
        return false;
    }
    if (!PointInTopologyLoop(mapPoint, loops.outer)) {
        return false;
    }
    for (const SectorTopologyLoop& hole : loops.holes) {
        if (PointInTopologyLoop(mapPoint, hole)) {
            return false;
        }
    }
    return true;
}

int SectorEditor::FindTopologySectorAt(Vector2 mapPoint, bool* outMultipleMatches) const
{
    if (outMultipleMatches != nullptr) {
        *outMultipleMatches = false;
    }

    int selectedId = -1;
    int matchCount = 0;
    for (const SectorTopologySector& sector : state.topologyMap.sectors) {
        if (!PointInTopologySector(mapPoint, sector)) {
            continue;
        }
        ++matchCount;
        if (selectedId < 0 || sector.id < selectedId) {
            selectedId = sector.id;
        }
    }

    if (outMultipleMatches != nullptr) {
        *outMultipleMatches = matchCount > 1;
    }
    return selectedId;
}

int SectorEditor::FindTopologyLightNearScreenPoint(Vector2 screenPoint) const
{
    float bestDistance2 = ScreenLightPickPixels * ScreenLightPickPixels;
    int bestId = -1;
    for (const SectorTopologyStaticPointLight& light : state.topologyMap.staticLights) {
        const Vector2 center = MapToScreen(Vector2{light.position.x, light.position.z});
        const float dx = center.x - screenPoint.x;
        const float dy = center.y - screenPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 < bestDistance2 - 0.001f
                || (std::fabs(distance2 - bestDistance2) <= 0.001f
                        && (bestId < 0 || light.id < bestId))) {
            bestDistance2 = distance2;
            bestId = light.id;
        }
    }
    return bestId;
}

bool SectorEditor::FindTopologyLineNearScreenPoint(
        Vector2 screenPoint,
        Vector2 mapPoint,
        int& outLineDefId,
        int& outSideDefId,
        SectorTopologySideKind& outSide,
        bool& outPreferredMissing) const
{
    outLineDefId = -1;
    outSideDefId = -1;
    outSide = SectorTopologySideKind::Front;
    outPreferredMissing = false;

    float bestDistance = ScreenEdgePickPixels;
    const SectorTopologyLineDef* bestLine = nullptr;
    Vector2 bestStart{};
    Vector2 bestEnd{};

    for (const SectorTopologyLineDef& lineDef : state.topologyMap.lineDefs) {
        const SectorTopologyVertex* start = nullptr;
        const SectorTopologyVertex* end = nullptr;
        if (!GetSectorTopologyLineVertices(state.topologyMap, lineDef, start, end)) {
            continue;
        }

        const Vector2 screenStart = MapToScreen(SectorTopologyVertexToMap(*start));
        const Vector2 screenEnd = MapToScreen(SectorTopologyVertexToMap(*end));
        const float distance = DistancePointToSegment(screenPoint, screenStart, screenEnd);
        if (distance > ScreenEdgePickPixels) {
            continue;
        }
        if (bestLine != nullptr
                && (distance > bestDistance + 0.001f
                        || (std::fabs(distance - bestDistance) <= 0.001f
                                && lineDef.id >= bestLine->id))) {
            continue;
        }

        bestDistance = distance;
        bestLine = &lineDef;
        bestStart = SectorTopologyVertexToMap(*start);
        bestEnd = SectorTopologyVertexToMap(*end);
    }

    if (bestLine == nullptr) {
        return false;
    }

    const bool hasFront = bestLine->frontSideDefId >= 0
            && FindSectorTopologySideDef(state.topologyMap, bestLine->frontSideDefId) != nullptr;
    const bool hasBack = bestLine->backSideDefId >= 0
            && FindSectorTopologySideDef(state.topologyMap, bestLine->backSideDefId) != nullptr;

    SectorTopologySideKind preferredSide = SectorTopologySideKind::Front;
    const float sideCross = Cross(bestStart, bestEnd, mapPoint);
    if (sideCross > GeometryEpsilon) {
        preferredSide = SectorTopologySideKind::Front;
    } else if (sideCross < -GeometryEpsilon) {
        preferredSide = SectorTopologySideKind::Back;
    } else if (!hasFront && hasBack) {
        preferredSide = SectorTopologySideKind::Back;
    }

    const int preferredSideDefId = preferredSide == SectorTopologySideKind::Front
            ? bestLine->frontSideDefId
            : bestLine->backSideDefId;
    const int oppositeSideDefId = preferredSide == SectorTopologySideKind::Front
            ? bestLine->backSideDefId
            : bestLine->frontSideDefId;
    const bool preferredExists = preferredSide == SectorTopologySideKind::Front ? hasFront : hasBack;
    const bool oppositeExists = preferredSide == SectorTopologySideKind::Front ? hasBack : hasFront;

    outLineDefId = bestLine->id;
    if (preferredExists) {
        outSide = preferredSide;
        outSideDefId = preferredSideDefId;
    } else if (oppositeExists) {
        outSide = OppositeSectorTopologySideKind(preferredSide);
        outSideDefId = oppositeSideDefId;
        outPreferredMissing = true;
    } else {
        outSide = preferredSide;
        outSideDefId = -1;
    }
    return true;
}

void SectorEditor::SelectTopologySector(int sectorId)
{
    if (FindSectorTopologySector(state.topologyMap, sectorId) == nullptr) {
        ClearSelection();
        return;
    }

    state.topologySelectionKind = TopologySelectionKind::Sector;
    state.selectedTopologySectorId = sectorId;
    state.selectedTopologyVertexId = -1;
    state.selectedTopologySideDefId = -1;
    state.selectedTopologyLineDefId = -1;
    state.selectedTopologyLightId = -1;
    state.selectedTopologySideKind = SectorTopologySideKind::Front;
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    uiState.idBufferLightIndex = -1;
    uiState.inspectorScroll.offset = Vector2{};
    uiState.floorInput = engine::UIFloatInputState{};
    uiState.ceilingInput = engine::UIFloatInputState{};
    uiState.ambientIntensityInput = engine::UIFloatInputState{};
    uiState.ambientRedInput = engine::UIIntInputState{};
    uiState.ambientGreenInput = engine::UIIntInputState{};
    uiState.ambientBlueInput = engine::UIIntInputState{};
    for (engine::UIFloatInputState& inputState : uiState.topologySectorUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
    SyncSelectedSectorIdBuffer();
    SyncSelectedLightIdBuffer();
}

void SectorEditor::SelectTopologyVertex(int vertexId)
{
    const SectorTopologyVertex* vertex = FindSectorTopologyVertex(state.topologyMap, vertexId);
    if (vertex == nullptr) {
        ClearSelection();
        return;
    }

    state.topologySelectionKind = TopologySelectionKind::Vertex;
    state.selectedTopologySectorId = -1;
    state.selectedTopologyVertexId = vertex->id;
    state.selectedTopologySideDefId = -1;
    state.selectedTopologyLineDefId = -1;
    state.selectedTopologyLightId = -1;
    state.selectedTopologySideKind = SectorTopologySideKind::Front;
    state.inspectedTopologyVertexId = vertex->id;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    uiState.idBufferLightIndex = -1;
    uiState.inspectorScroll.offset = Vector2{};
    SyncSelectedSectorIdBuffer();
    SyncSelectedLightIdBuffer();
}

void SectorEditor::SelectTopologySideDef(int sideDefId, TopologyWallPart wallPart)
{
    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, sideDefId);
    if (sideDef == nullptr) {
        ClearSelection();
        return;
    }
    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(state.topologyMap, sideDef->lineDefId);
    if (lineDef == nullptr) {
        ClearSelection();
        return;
    }

    state.topologySelectionKind = TopologySelectionKind::SideDef;
    state.selectedTopologySectorId = -1;
    state.selectedTopologyVertexId = -1;
    state.selectedTopologySideDefId = sideDef->id;
    state.selectedTopologyLineDefId = lineDef->id;
    state.selectedTopologyLightId = -1;
    state.selectedTopologySideKind = sideDef->side;
    state.selectedTopologyWallPart = wallPart;
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    uiState.idBufferLightIndex = -1;
    uiState.inspectorScroll.offset = Vector2{};
    for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
    SyncSelectedSectorIdBuffer();
    SyncSelectedLightIdBuffer();
}

void SectorEditor::SelectTopologyLineDef(
        int lineDefId,
        SectorTopologySideKind side,
        TopologyWallPart wallPart)
{
    if (FindSectorTopologyLineDef(state.topologyMap, lineDefId) == nullptr) {
        ClearSelection();
        return;
    }

    state.topologySelectionKind = TopologySelectionKind::LineDef;
    state.selectedTopologySectorId = -1;
    state.selectedTopologyVertexId = -1;
    state.selectedTopologySideDefId = -1;
    state.selectedTopologyLineDefId = lineDefId;
    state.selectedTopologyLightId = -1;
    state.selectedTopologySideKind = side;
    state.selectedTopologyWallPart = wallPart;
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    uiState.idBufferLightIndex = -1;
    uiState.inspectorScroll.offset = Vector2{};
    for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
    SyncSelectedSectorIdBuffer();
    SyncSelectedLightIdBuffer();
}

void SectorEditor::SelectTopologyLight(int topologyLightId)
{
    if (FindSectorTopologyStaticLight(state.topologyMap, topologyLightId) == nullptr) {
        ClearSelection();
        return;
    }

    state.selectedTopologyLightId = topologyLightId;
    state.topologySelectionKind = TopologySelectionKind::Light;
    state.selectedTopologySectorId = -1;
    state.selectedTopologyVertexId = -1;
    state.selectedTopologySideDefId = -1;
    state.selectedTopologyLineDefId = -1;
    state.selectedTopologySideKind = SectorTopologySideKind::Front;
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    uiState.inspectorScroll.offset = Vector2{};
    uiState.lightXInput = engine::UIFloatInputState{};
    uiState.lightYInput = engine::UIFloatInputState{};
    uiState.lightZInput = engine::UIFloatInputState{};
    uiState.lightIntensityInput = engine::UIFloatInputState{};
    uiState.lightRadiusInput = engine::UIFloatInputState{};
    uiState.lightSourceRadiusInput = engine::UIFloatInputState{};
    uiState.lightRedInput = engine::UIIntInputState{};
    uiState.lightGreenInput = engine::UIIntInputState{};
    uiState.lightBlueInput = engine::UIIntInputState{};
    SyncSelectedLightIdBuffer();
    SyncSelectedSectorIdBuffer();
}

void SectorEditor::SelectSurface3D(SectorSurfaceRef surface)
{
    const TopologySurfaceEditTarget target = TopologyEditTargetForSurface(surface);
    if (!IsValidSurfaceRef(surface) || !IsValidTopologySurfaceEditTarget(target)) {
        state.selectedSurface3D = SectorSurfaceRef{};
        state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
        return;
    }

    if (!SameSurfaceRef(state.selectedSurface3D, surface)) {
        ResetSurface3DUiState();
    }

    if (IsWallSurface(surface.kind)) {
        SelectTopologySideDef(
                surface.topologySideDefId,
                SurfaceKindToTopologyWallPart(surface.kind));
    } else {
        SelectTopologySector(surface.topologySectorId);
    }
    state.selectedSurface3D = surface;
    state.selectedTopologySurface3D = target;
}

bool SectorEditor::IsValidSurfaceRef(SectorSurfaceRef surface) const
{
    if (surface.kind == SectorSurfaceKind::None) {
        return false;
    }

    if (IsWallSurface(surface.kind)) {
        const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(
                state.topologyMap,
                surface.topologySideDefId);
        return sideDef != nullptr
                && sideDef->lineDefId == surface.topologyLineDefId
                && sideDef->side == surface.topologySide;
    }
    return FindSectorTopologySector(state.topologyMap, surface.topologySectorId) != nullptr;
}

bool SectorEditor::SameSurfaceRef(SectorSurfaceRef a, SectorSurfaceRef b) const
{
    return a.kind == b.kind
            && a.topologySectorId == b.topologySectorId
            && a.topologyLineDefId == b.topologyLineDefId
            && a.topologySideDefId == b.topologySideDefId
            && a.topologySide == b.topologySide;
}

TopologySurfaceEditTarget SectorEditor::TopologyEditTargetForSurface(SectorSurfaceRef surface) const
{
    TopologySurfaceEditTarget target;
    target.kind = SurfaceKindToTopologyEditTargetKind(surface.kind);
    target.sectorId = surface.topologySectorId;
    target.lineDefId = surface.topologyLineDefId;
    target.sideDefId = surface.topologySideDefId;
    target.side = surface.topologySide;
    return target;
}

bool SectorEditor::IsValidTopologySurfaceEditTarget(TopologySurfaceEditTarget target) const
{
    switch (target.kind) {
        case TopologySurfaceEditTargetKind::SectorFloor:
        case TopologySurfaceEditTargetKind::SectorCeiling:
            return FindSectorTopologySector(state.topologyMap, target.sectorId) != nullptr;
        case TopologySurfaceEditTargetKind::SideDefWall:
        case TopologySurfaceEditTargetKind::SideDefLower:
        case TopologySurfaceEditTargetKind::SideDefUpper: {
            const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(
                    state.topologyMap,
                    target.sideDefId);
            return sideDef != nullptr
                    && sideDef->lineDefId == target.lineDefId
                    && sideDef->side == target.side;
        }
        case TopologySurfaceEditTargetKind::None:
            break;
    }
    return false;
}

void SectorEditor::ResetSurface3DUiState()
{
    uiState.surface3DUvScaleUInput = engine::UIFloatInputState{};
    uiState.surface3DUvScaleVInput = engine::UIFloatInputState{};
    uiState.surface3DUvOffsetUInput = engine::UIFloatInputState{};
    uiState.surface3DUvOffsetVInput = engine::UIFloatInputState{};
    uiState.surface3DDecalOpacityInput = engine::UIFloatInputState{};
}

void SectorEditor::ClearSelection()
{
    state.pendingTopologyLineSplitAtPoint = PendingTopologyLineSplitAtPoint{};
    state.pendingTopologyVertexMerge = PendingTopologyVertexMerge{};
    state.pendingTopologySectorCut = PendingTopologySectorCut{};
    state.topologySelectionKind = TopologySelectionKind::None;
    state.selectedTopologySectorId = -1;
    state.selectedTopologyVertexId = -1;
    state.selectedTopologySideDefId = -1;
    state.selectedTopologyLineDefId = -1;
    state.selectedTopologyLightId = -1;
    state.selectedTopologySideKind = SectorTopologySideKind::Front;
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    uiState.ambientIntensityInput = engine::UIFloatInputState{};
    uiState.ambientRedInput = engine::UIIntInputState{};
    uiState.ambientGreenInput = engine::UIIntInputState{};
    uiState.ambientBlueInput = engine::UIIntInputState{};
    uiState.inspectorScroll.offset = Vector2{};
    SyncSelectedSectorIdBuffer();
    SyncSelectedLightIdBuffer();
}

const SectorTopologyDecalLayer* SectorEditor::DecalForSurface(TopologySurfaceEditTarget target) const
{
    if (!IsValidTopologySurfaceEditTarget(target)) {
        return nullptr;
    }

    if (target.kind == TopologySurfaceEditTargetKind::SectorFloor
            || target.kind == TopologySurfaceEditTargetKind::SectorCeiling) {
        const SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, target.sectorId);
        if (sector == nullptr) {
            return nullptr;
        }
        return target.kind == TopologySurfaceEditTargetKind::SectorFloor
                ? &sector->floorDecal
                : &sector->ceilingDecal;
    }

    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, target.sideDefId);
    if (sideDef == nullptr) {
        return nullptr;
    }
    return &TopologyWallPartSettingsFor(*sideDef, TopologyEditTargetWallPart(target.kind)).decal;
}

SectorTopologyDecalLayer* SectorEditor::MutableDecalForSurface(TopologySurfaceEditTarget target)
{
    if (!IsValidTopologySurfaceEditTarget(target)) {
        return nullptr;
    }

    if (target.kind == TopologySurfaceEditTargetKind::SectorFloor
            || target.kind == TopologySurfaceEditTargetKind::SectorCeiling) {
        SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, target.sectorId);
        if (sector == nullptr) {
            return nullptr;
        }
        return target.kind == TopologySurfaceEditTargetKind::SectorFloor
                ? &sector->floorDecal
                : &sector->ceilingDecal;
    }

    SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, target.sideDefId);
    if (sideDef == nullptr) {
        return nullptr;
    }
    return &TopologyWallPartSettingsFor(*sideDef, TopologyEditTargetWallPart(target.kind)).decal;
}

const SectorTopologyUvSettings* SectorEditor::UvForSurface(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer) const
{
    if (!IsValidTopologySurfaceEditTarget(target)) {
        return nullptr;
    }
    if (layer == TopologyMaterialLayer::Decal) {
        const SectorTopologyDecalLayer* decal = DecalForSurface(target);
        return decal == nullptr || decal->textureId.empty() ? nullptr : &decal->uv;
    }

    if (target.kind == TopologySurfaceEditTargetKind::SectorFloor
            || target.kind == TopologySurfaceEditTargetKind::SectorCeiling) {
        const SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, target.sectorId);
        if (sector == nullptr) {
            return nullptr;
        }
        return target.kind == TopologySurfaceEditTargetKind::SectorFloor
                ? &sector->floorUv
                : &sector->ceilingUv;
    }

    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, target.sideDefId);
    if (sideDef == nullptr) {
        return nullptr;
    }
    return &TopologyWallPartSettingsFor(*sideDef, TopologyEditTargetWallPart(target.kind)).uv;
}

SectorTopologyUvSettings* SectorEditor::MutableUvForSurface(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer)
{
    if (!IsValidTopologySurfaceEditTarget(target)) {
        return nullptr;
    }
    if (layer == TopologyMaterialLayer::Decal) {
        SectorTopologyDecalLayer* decal = MutableDecalForSurface(target);
        return decal == nullptr || decal->textureId.empty() ? nullptr : &decal->uv;
    }

    if (target.kind == TopologySurfaceEditTargetKind::SectorFloor
            || target.kind == TopologySurfaceEditTargetKind::SectorCeiling) {
        SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, target.sectorId);
        if (sector == nullptr) {
            return nullptr;
        }
        return target.kind == TopologySurfaceEditTargetKind::SectorFloor
                ? &sector->floorUv
                : &sector->ceilingUv;
    }

    SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, target.sideDefId);
    if (sideDef == nullptr) {
        return nullptr;
    }
    return &TopologyWallPartSettingsFor(*sideDef, TopologyEditTargetWallPart(target.kind)).uv;
}

bool SectorEditor::IsDecalAssigned(TopologySurfaceEditTarget target) const
{
    const SectorTopologyDecalLayer* decal = DecalForSurface(target);
    return decal != nullptr && !decal->textureId.empty();
}

std::string SectorEditor::CurrentTextureForSurface(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer) const
{
    if (!IsValidTopologySurfaceEditTarget(target)) {
        return std::string{"<none>"};
    }
    if (layer == TopologyMaterialLayer::Decal) {
        const SectorTopologyDecalLayer* decal = DecalForSurface(target);
        return decal == nullptr ? std::string{} : decal->textureId;
    }

    if (target.kind == TopologySurfaceEditTargetKind::SectorFloor
            || target.kind == TopologySurfaceEditTargetKind::SectorCeiling) {
        const SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, target.sectorId);
        if (sector == nullptr) {
            return std::string{};
        }
        return target.kind == TopologySurfaceEditTargetKind::SectorFloor
                ? sector->floorTextureId
                : sector->ceilingTextureId;
    }

    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, target.sideDefId);
    if (sideDef == nullptr) {
        return std::string{};
    }
    switch (target.kind) {
        case TopologySurfaceEditTargetKind::SideDefWall:
            return sideDef->wall.textureId;
        case TopologySurfaceEditTargetKind::SideDefLower:
            return sideDef->lower.textureId;
        case TopologySurfaceEditTargetKind::SideDefUpper:
            return sideDef->upper.textureId;
        case TopologySurfaceEditTargetKind::SectorFloor:
        case TopologySurfaceEditTargetKind::SectorCeiling:
        case TopologySurfaceEditTargetKind::None:
            break;
    }
    return std::string{"<none>"};
}

bool SectorEditor::CopyTopologyMaterial(TopologySurfaceEditTarget target)
{
    if (!IsValidTopologySurfaceEditTarget(target)) {
        statusText = "Selected material target is no longer valid.";
        return false;
    }

    TopologyMaterialPayload payload;
    payload.valid = true;
    payload.kind = target.kind;

    if (target.kind == TopologySurfaceEditTargetKind::SectorFloor
            || target.kind == TopologySurfaceEditTargetKind::SectorCeiling) {
        const SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, target.sectorId);
        if (sector == nullptr) {
            statusText = "Selected material target is no longer valid.";
            return false;
        }

        if (target.kind == TopologySurfaceEditTargetKind::SectorFloor) {
            payload.textureId = sector->floorTextureId;
            payload.uv = sector->floorUv;
        } else {
            payload.textureId = sector->ceilingTextureId;
            payload.uv = sector->ceilingUv;
        }
    } else {
        const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, target.sideDefId);
        if (sideDef == nullptr) {
            statusText = "Selected material target is no longer valid.";
            return false;
        }

        const SectorTopologyWallPartSettings& part = TopologyWallPartSettingsFor(
                *sideDef,
                TopologyEditTargetWallPart(target.kind));
        payload.textureId = part.textureId;
        payload.uv = part.uv;
    }

    state.copiedTopologyMaterial = payload;
    statusText = TextFormat("Copied %s material.", TopologyMaterialKindName(payload.kind));
    return true;
}

bool SectorEditor::PasteTopologyMaterial(TopologySurfaceEditTarget target, engine::AssetManager& assets)
{
    if (!state.copiedTopologyMaterial.valid) {
        statusText = "No copied material.";
        return false;
    }

    if (!IsValidTopologySurfaceEditTarget(target)) {
        statusText = "Selected material target is no longer valid.";
        return false;
    }

    if (state.copiedTopologyMaterial.kind != target.kind) {
        statusText = TextFormat(
                "Copied material is for %s; selected target is %s.",
                TopologyMaterialKindName(state.copiedTopologyMaterial.kind),
                TopologyMaterialKindName(target.kind));
        return false;
    }

    if (target.kind == TopologySurfaceEditTargetKind::SectorFloor
            || target.kind == TopologySurfaceEditTargetKind::SectorCeiling) {
        SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, target.sectorId);
        if (sector == nullptr) {
            statusText = "Selected material target is no longer valid.";
            return false;
        }

        if (target.kind == TopologySurfaceEditTargetKind::SectorFloor) {
            sector->floorTextureId = state.copiedTopologyMaterial.textureId;
            sector->floorUv = state.copiedTopologyMaterial.uv;
        } else {
            sector->ceilingTextureId = state.copiedTopologyMaterial.textureId;
            sector->ceilingUv = state.copiedTopologyMaterial.uv;
        }
    } else {
        SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, target.sideDefId);
        if (sideDef == nullptr) {
            statusText = "Selected material target is no longer valid.";
            return false;
        }

        SectorTopologyWallPartSettings& part = TopologyWallPartSettingsFor(
                *sideDef,
                TopologyEditTargetWallPart(target.kind));
        part.textureId = state.copiedTopologyMaterial.textureId;
        part.uv = state.copiedTopologyMaterial.uv;
    }

    state.topologyRenderWarning.clear();
    MarkTopologyDocumentEdited(TextFormat("Pasted %s material.", TopologyMaterialKindName(target.kind)));
    if (state.mode == SectorEditorMode::Preview3D && preview.IsReady()) {
        RebuildPreviewMeshesPreservingView(assets);
    }
    return true;
}

bool SectorEditor::ApplySurface3DUvValue(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        int component,
        float value,
        engine::AssetManager& assets)
{
    if (!IsValidTopologySurfaceEditTarget(target) || !std::isfinite(value)) {
        return false;
    }
    if ((component == 0 || component == 1)
            && (value < TopologyUvScaleMin || value > TopologyUvScaleMax)) {
        return false;
    }

    auto applyComponent = [component, value](auto& uv) {
        switch (component) {
            case 0:
                uv.scale.x = value;
                return true;
            case 1:
                uv.scale.y = value;
                return true;
            case 2:
                uv.offset.x = value;
                return true;
            case 3:
                uv.offset.y = value;
                return true;
            default:
                return false;
        }
    };

    SectorTopologyUvSettings* uv = MutableUvForSurface(target, layer);
    if (uv == nullptr) {
        statusText = layer == TopologyMaterialLayer::Decal
                ? "No decal assigned."
                : "Selected material target is no longer valid.";
        return false;
    }

    const bool changed = applyComponent(*uv);
    if (!changed) {
        return false;
    }

    state.topologyRenderWarning.clear();
    MarkTopologyDocumentEdited(TextFormat(
            "Updated 3D %s %s UV",
            SurfaceKindName(state.selectedSurface3D.kind),
            TopologyMaterialLayerStatusName(layer)));
    return RebuildPreviewMeshesPreservingView(assets);
}

bool SectorEditor::ApplySurfaceDecalOpacity(
        TopologySurfaceEditTarget target,
        float opacity,
        engine::AssetManager* assets)
{
    if (!IsValidTopologySurfaceEditTarget(target) || !std::isfinite(opacity)) {
        return false;
    }
    opacity = std::clamp(opacity, 0.0f, 1.0f);
    SectorTopologyDecalLayer* decal = MutableDecalForSurface(target);
    if (decal == nullptr || decal->textureId.empty()) {
        statusText = "No decal assigned.";
        return false;
    }
    if (decal->opacity == opacity) {
        return false;
    }

    decal->opacity = opacity;
    state.topologyRenderWarning.clear();
    MarkTopologyDocumentEdited(TextFormat("Set %s decal opacity.", TopologyMaterialKindName(target.kind)));
    if (assets != nullptr && state.mode == SectorEditorMode::Preview3D && preview.IsReady()) {
        return RebuildPreviewMeshesPreservingView(*assets);
    }
    return true;
}

bool SectorEditor::ApplySurfaceDecalEmissive(
        TopologySurfaceEditTarget target,
        bool emissive,
        engine::AssetManager* assets)
{
    if (!IsValidTopologySurfaceEditTarget(target)) {
        statusText = "Selected material target is no longer valid.";
        return false;
    }
    SectorTopologyDecalLayer* decal = MutableDecalForSurface(target);
    if (decal == nullptr || decal->textureId.empty()) {
        statusText = "No decal assigned.";
        return false;
    }
    if (decal->emissive == emissive) {
        return false;
    }

    decal->emissive = emissive;
    state.topologyRenderWarning.clear();
    MarkTopologyDocumentEdited(TextFormat("Set %s decal emissive.", TopologyMaterialKindName(target.kind)));
    if (assets != nullptr && state.mode == SectorEditorMode::Preview3D && preview.IsReady()) {
        return RebuildPreviewMeshesPreservingView(*assets);
    }
    return true;
}

bool SectorEditor::ApplySurfaceDecalTint(
        TopologySurfaceEditTarget target,
        Vector3 tint,
        engine::AssetManager* assets)
{
    if (!IsValidTopologySurfaceEditTarget(target)) {
        statusText = "Selected material target is no longer valid.";
        return false;
    }
    if (!IsValidDecalTint(tint)) {
        statusText = "Invalid decal tint.";
        return false;
    }
    SectorTopologyDecalLayer* decal = MutableDecalForSurface(target);
    if (decal == nullptr || decal->textureId.empty()) {
        statusText = "No decal assigned.";
        return false;
    }
    if (SameTint(decal->tint, tint)) {
        return false;
    }

    decal->tint = tint;
    state.topologyRenderWarning.clear();
    MarkTopologyDocumentEdited(TextFormat("Set %s decal tint.", TopologyMaterialKindName(target.kind)));
    if (assets != nullptr && state.mode == SectorEditorMode::Preview3D && preview.IsReady()) {
        return RebuildPreviewMeshesPreservingView(*assets);
    }
    return true;
}

bool SectorEditor::OpenDecalTintModal(TopologySurfaceEditTarget target)
{
    if (!IsValidTopologySurfaceEditTarget(target)) {
        statusText = "Selected material target is no longer valid.";
        return false;
    }
    const SectorTopologyDecalLayer* decal = DecalForSurface(target);
    if (decal == nullptr || decal->textureId.empty()) {
        statusText = "No decal assigned.";
        return false;
    }

    DecalTintModalState modal;
    modal.open = true;
    modal.target = target;
    modal.tint = ClampDecalTint(decal->tint);
    state.decalTintModal = modal;
    return true;
}

bool SectorEditor::ClearSurfaceDecal(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    SectorTopologyDecalLayer* decal = MutableDecalForSurface(target);
    if (decal == nullptr) {
        statusText = "Selected material target is no longer valid.";
        return false;
    }
    if (IsDefaultDecalLayer(*decal)) {
        statusText = "No decal assigned.";
        return false;
    }

    ResetDecalLayer(*decal);
    state.topologyRenderWarning.clear();
    ResetSurface3DUiState();
    for (engine::UIFloatInputState& inputState : uiState.topologySectorUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
    for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
    for (engine::UIFloatInputState& inputState : uiState.topologySectorDecalOpacityInputs) {
        inputState = engine::UIFloatInputState{};
    }
    uiState.topologySideDefDecalOpacityInput = engine::UIFloatInputState{};
    uiState.surface3DDecalOpacityInput = engine::UIFloatInputState{};
    state.decalTintModal = DecalTintModalState{};
    MarkTopologyDocumentEdited(TextFormat("Cleared %s decal.", TopologyMaterialKindName(target.kind)));
    if (assets != nullptr && state.mode == SectorEditorMode::Preview3D && preview.IsReady()) {
        return RebuildPreviewMeshesPreservingView(*assets);
    }
    return true;
}

bool SectorEditor::ResetSurface3DUv(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        engine::AssetManager& assets)
{
    if (!IsValidTopologySurfaceEditTarget(target)) {
        return false;
    }

    auto resetUv = [](SectorTopologyUvSettings& uv) {
        const bool changed = uv.scale.x != 1.0f
                || uv.scale.y != 1.0f
                || uv.offset.x != 0.0f
                || uv.offset.y != 0.0f;
        uv.scale = Vector2{1.0f, 1.0f};
        uv.offset = Vector2{0.0f, 0.0f};
        return changed;
    };

    SectorTopologyUvSettings* uv = MutableUvForSurface(target, layer);
    if (uv == nullptr) {
        statusText = layer == TopologyMaterialLayer::Decal
                ? "No decal assigned."
                : "Selected material target is no longer valid.";
        return false;
    }

    const bool changed = resetUv(*uv);
    if (!changed) {
        return false;
    }

    ResetSurface3DUiState();
    state.topologyRenderWarning.clear();
    MarkTopologyDocumentEdited(TextFormat(
            "Reset 3D %s %s UV",
            SurfaceKindName(state.selectedSurface3D.kind),
            TopologyMaterialLayerStatusName(layer)));
    return RebuildPreviewMeshesPreservingView(assets);
}

bool SectorEditor::FitSelectedDecal(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    if (!IsValidTopologySurfaceEditTarget(target)) {
        statusText = "Selected material target is no longer valid.";
        return false;
    }
    if (!IsDecalAssigned(target)) {
        statusText = "No decal assigned.";
        return false;
    }
    if (target.kind == TopologySurfaceEditTargetKind::SectorFloor
            || target.kind == TopologySurfaceEditTargetKind::SectorCeiling) {
        return FitSelectedFlatDecal(target, assets);
    }
    if (IsWallTopologyEditTarget(target.kind)) {
        return FitSelectedWallMaterial(
                target,
                TopologyUvFitMode::Both,
                assets,
                TopologyMaterialLayer::Decal);
    }

    statusText = "Selected material target cannot be fit.";
    return false;
}

bool SectorEditor::FitSelectedFlatDecal(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    if (target.kind != TopologySurfaceEditTargetKind::SectorFloor
            && target.kind != TopologySurfaceEditTargetKind::SectorCeiling) {
        statusText = "Select a floor or ceiling surface before fitting decal UVs.";
        return false;
    }
    if (!IsDecalAssigned(target)) {
        statusText = "No decal assigned.";
        return false;
    }

    const SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, target.sectorId);
    if (sector == nullptr) {
        statusText = "Selected sector is no longer valid.";
        return false;
    }

    SectorTopologyLoopSet loops;
    if (!ExtractSectorTopologyLoops(state.topologyMap, sector->id, loops)) {
        statusText = "Selected sector has invalid loops.";
        return false;
    }

    bool hasPoint = false;
    float minX = 0.0f;
    float maxX = 0.0f;
    float minY = 0.0f;
    float maxY = 0.0f;
    auto visitLoop = [&](const SectorTopologyLoop& loop) {
        if (loop.vertexIds.empty()) {
            return false;
        }
        for (int vertexId : loop.vertexIds) {
            const SectorTopologyVertex* vertex = FindSectorTopologyVertex(state.topologyMap, vertexId);
            if (vertex == nullptr) {
                return false;
            }
            const Vector2 world = SectorCoordToWorldPosition2(vertex->x, vertex->y);
            if (!std::isfinite(world.x) || !std::isfinite(world.y)) {
                return false;
            }
            if (!hasPoint) {
                minX = maxX = world.x;
                minY = maxY = world.y;
                hasPoint = true;
            } else {
                minX = std::min(minX, world.x);
                maxX = std::max(maxX, world.x);
                minY = std::min(minY, world.y);
                maxY = std::max(maxY, world.y);
            }
        }
        return true;
    };

    if (!visitLoop(loops.outer)) {
        statusText = "Selected sector has invalid outer loop.";
        return false;
    }
    for (const SectorTopologyLoop& hole : loops.holes) {
        if (!visitLoop(hole)) {
            statusText = "Selected sector has invalid hole loop.";
            return false;
        }
    }
    if (!hasPoint) {
        statusText = "Selected sector has no vertices.";
        return false;
    }

    const float widthWorld = maxX - minX;
    const float heightWorld = maxY - minY;
    if (!(widthWorld > 0.0f) || !(heightWorld > 0.0f)
            || !std::isfinite(widthWorld) || !std::isfinite(heightWorld)) {
        statusText = "Selected sector has invalid flat bounds.";
        return false;
    }

    const auto validateScale = [](float scale) {
        return std::isfinite(scale)
                && scale >= TopologyUvScaleMin
                && scale <= TopologyUvScaleMax;
    };
    const Vector2 fittedScale{
            kSectorGeneratedTextureWorldSize / widthWorld,
            kSectorGeneratedTextureWorldSize / heightWorld};
    if (!validateScale(fittedScale.x) || !validateScale(fittedScale.y)) {
        statusText = "Fit decal requires a UV scale outside the editable range.";
        return false;
    }

    SectorTopologyUvSettings* uv = MutableUvForSurface(target, TopologyMaterialLayer::Decal);
    if (uv == nullptr) {
        statusText = "No decal assigned.";
        return false;
    }
    uv->scale = fittedScale;
    uv->offset = Vector2{0.0f, 0.0f};

    state.topologyRenderWarning.clear();
    ResetSurface3DUiState();
    for (engine::UIFloatInputState& inputState : uiState.topologySectorUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
    MarkTopologyDocumentEdited(TextFormat("Fit %s decal.", TopologyMaterialKindName(target.kind)));
    if (assets != nullptr && state.mode == SectorEditorMode::Preview3D && preview.IsReady()) {
        return RebuildPreviewMeshesPreservingView(*assets);
    }
    return true;
}

bool SectorEditor::FitSelectedWallMaterial(
        TopologySurfaceEditTarget target,
        TopologyUvFitMode mode,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer)
{
    if (!IsWallTopologyEditTarget(target.kind) || !IsValidTopologySurfaceEditTarget(target)) {
        statusText = "Select a wall, lower, or upper surface before fitting UVs.";
        return false;
    }
    if (layer == TopologyMaterialLayer::Decal && !IsDecalAssigned(target)) {
        statusText = "No decal assigned.";
        return false;
    }

    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, target.sideDefId);
    if (sideDef == nullptr
            || sideDef->lineDefId != target.lineDefId
            || sideDef->sectorId != target.sectorId
            || sideDef->side != target.side) {
        statusText = "Selected sidedef is no longer valid.";
        return false;
    }

    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(state.topologyMap, sideDef->lineDefId);
    if (lineDef == nullptr) {
        statusText = "Selected sidedef references a missing linedef.";
        return false;
    }

    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (!GetSectorTopologyLineVertices(state.topologyMap, *lineDef, start, end)
            || start == nullptr || end == nullptr) {
        statusText = "Selected linedef endpoints are missing.";
        return false;
    }

    const double dx = static_cast<double>(end->x) - static_cast<double>(start->x);
    const double dy = static_cast<double>(end->y) - static_cast<double>(start->y);
    const double coordLength = std::sqrt(dx * dx + dy * dy);
    const float wallLengthWorld = SectorCoordDistanceToWorldDistance(coordLength);
    if (!(wallLengthWorld > 0.0f) || !std::isfinite(wallLengthWorld)) {
        statusText = "Selected wall has invalid length.";
        return false;
    }

    const SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, sideDef->sectorId);
    if (sector == nullptr) {
        statusText = "Selected sidedef references a missing sector.";
        return false;
    }

    const int oppositeSideDefId = sideDef->side == SectorTopologySideKind::Front
            ? lineDef->backSideDefId
            : lineDef->frontSideDefId;
    const SectorTopologySideDef* opposite = FindOppositeSectorTopologySideDef(state.topologyMap, sideDef->id);
    if (oppositeSideDefId != -1 && opposite == nullptr) {
        statusText = "Selected sidedef's opposite side is no longer valid.";
        return false;
    }

    const SectorTopologySector* oppositeSector = nullptr;
    if (opposite != nullptr) {
        oppositeSector = FindSectorTopologySector(state.topologyMap, opposite->sectorId);
        if (oppositeSector == nullptr) {
            statusText = "Selected sidedef's opposite sector is missing.";
            return false;
        }
    }

    const TopologyWallPart wallPart = TopologyEditTargetWallPart(target.kind);
    float heightAuthoring = 0.0f;
    switch (wallPart) {
        case TopologyWallPart::Wall:
            if (opposite != nullptr) {
                statusText = "Selected wall part has no visible solid wall span.";
                return false;
            }
            heightAuthoring = std::fabs(sector->ceilingZ - sector->floorZ);
            break;
        case TopologyWallPart::Lower:
            if (oppositeSector == nullptr) {
                statusText = "Lower wall fit needs an opposite sector.";
                return false;
            }
            if (!(oppositeSector->floorZ > sector->floorZ)) {
                statusText = "Selected lower wall has no visible height span.";
                return false;
            }
            heightAuthoring = oppositeSector->floorZ - sector->floorZ;
            break;
        case TopologyWallPart::Upper:
            if (oppositeSector == nullptr) {
                statusText = "Upper wall fit needs an opposite sector.";
                return false;
            }
            if (!(sector->ceilingZ > oppositeSector->ceilingZ)) {
                statusText = "Selected upper wall has no visible height span.";
                return false;
            }
            heightAuthoring = sector->ceilingZ - oppositeSector->ceilingZ;
            break;
    }

    const float wallHeightWorld = SectorAuthoringToWorldDistance(std::fabs(heightAuthoring));
    const bool fitWidth = mode == TopologyUvFitMode::Width || mode == TopologyUvFitMode::Both;
    const bool fitHeight = mode == TopologyUvFitMode::Height || mode == TopologyUvFitMode::Both;
    if (fitHeight && (!(wallHeightWorld > 0.0f) || !std::isfinite(wallHeightWorld))) {
        statusText = "Selected wall has invalid height.";
        return false;
    }

    const auto validateScale = [](float scale) {
        return std::isfinite(scale)
                && scale >= TopologyUvScaleMin
                && scale <= TopologyUvScaleMax;
    };
    const float widthScale = kSectorGeneratedTextureWorldSize / wallLengthWorld;
    const float heightScale = fitHeight
            ? kSectorGeneratedTextureWorldSize / wallHeightWorld
            : 1.0f;
    if (fitWidth && !validateScale(widthScale)) {
        statusText = "Fit width requires a UV scale outside the editable range.";
        return false;
    }
    if (fitHeight && !validateScale(heightScale)) {
        statusText = "Fit height requires a UV scale outside the editable range.";
        return false;
    }

    SectorTopologyUvSettings* uv = MutableUvForSurface(target, layer);
    if (uv == nullptr) {
        statusText = layer == TopologyMaterialLayer::Decal
                ? "No decal assigned."
                : "Selected material target is no longer valid.";
        return false;
    }
    if (fitWidth) {
        uv->scale.x = widthScale;
        uv->offset.x = 0.0f;
    }
    if (fitHeight) {
        uv->scale.y = heightScale;
        uv->offset.y = 0.0f;
    }

    state.topologyRenderWarning.clear();
    ResetSurface3DUiState();
    for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }

    MarkTopologyDocumentEdited(TextFormat(
            "Fit %s %s %s.",
            TopologyWallPartStatusName(wallPart),
            TopologyMaterialLayerStatusName(layer),
            TopologyUvFitModeStatusName(mode)));
    if (assets != nullptr && state.mode == SectorEditorMode::Preview3D && preview.IsReady()) {
        return RebuildPreviewMeshesPreservingView(*assets);
    }
    return true;
}

bool SectorEditor::AlignSelectedWallMaterialVertical(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer)
{
    if (!IsWallTopologyEditTarget(target.kind) || !IsValidTopologySurfaceEditTarget(target)) {
        statusText = "Select a wall, lower, or upper surface before aligning UVs.";
        return false;
    }
    if (layer == TopologyMaterialLayer::Decal && !IsDecalAssigned(target)) {
        statusText = "No decal assigned.";
        return false;
    }

    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, target.sideDefId);
    if (sideDef == nullptr
            || sideDef->lineDefId != target.lineDefId
            || sideDef->sectorId != target.sectorId
            || sideDef->side != target.side) {
        statusText = "Selected sidedef is no longer valid.";
        return false;
    }

    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(state.topologyMap, sideDef->lineDefId);
    if (lineDef == nullptr) {
        statusText = "Selected sidedef references a missing linedef.";
        return false;
    }

    const SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, sideDef->sectorId);
    if (sector == nullptr) {
        statusText = "Selected sidedef references a missing sector.";
        return false;
    }

    const int oppositeSideDefId = sideDef->side == SectorTopologySideKind::Front
            ? lineDef->backSideDefId
            : lineDef->frontSideDefId;
    const SectorTopologySideDef* opposite = FindOppositeSectorTopologySideDef(state.topologyMap, sideDef->id);
    if (oppositeSideDefId != -1 && opposite == nullptr) {
        statusText = "Selected sidedef's opposite side is no longer valid.";
        return false;
    }

    const SectorTopologySector* oppositeSector = nullptr;
    if (opposite != nullptr) {
        oppositeSector = FindSectorTopologySector(state.topologyMap, opposite->sectorId);
        if (oppositeSector == nullptr) {
            statusText = "Selected sidedef's opposite sector is missing.";
            return false;
        }
    }

    const TopologyWallPart wallPart = TopologyEditTargetWallPart(target.kind);
    float spanBottomAuthoring = 0.0f;
    float spanTopAuthoring = 0.0f;
    switch (wallPart) {
        case TopologyWallPart::Wall:
            if (opposite != nullptr) {
                statusText = "Selected wall part has no visible solid wall span.";
                return false;
            }
            spanBottomAuthoring = sector->floorZ;
            spanTopAuthoring = sector->ceilingZ;
            break;
        case TopologyWallPart::Lower:
            if (oppositeSector == nullptr) {
                statusText = "Lower wall alignment needs an opposite sector.";
                return false;
            }
            if (!(oppositeSector->floorZ > sector->floorZ)) {
                statusText = "Selected lower wall has no visible height span.";
                return false;
            }
            spanBottomAuthoring = sector->floorZ;
            spanTopAuthoring = oppositeSector->floorZ;
            break;
        case TopologyWallPart::Upper:
            if (oppositeSector == nullptr) {
                statusText = "Upper wall alignment needs an opposite sector.";
                return false;
            }
            if (!(sector->ceilingZ > oppositeSector->ceilingZ)) {
                statusText = "Selected upper wall has no visible height span.";
                return false;
            }
            spanBottomAuthoring = oppositeSector->ceilingZ;
            spanTopAuthoring = sector->ceilingZ;
            break;
    }

    const float spanBottomWorld = SectorAuthoringToWorldDistance(spanBottomAuthoring);
    const float spanTopWorld = SectorAuthoringToWorldDistance(spanTopAuthoring);
    const float spanHeightWorld = spanTopWorld - spanBottomWorld;
    if (!(spanHeightWorld > 0.0f)
            || !std::isfinite(spanBottomWorld)
            || !std::isfinite(spanTopWorld)
            || !std::isfinite(spanHeightWorld)) {
        statusText = "Selected wall has invalid height.";
        return false;
    }

    SectorTopologyUvSettings* uv = MutableUvForSurface(target, layer);
    if (uv == nullptr) {
        statusText = layer == TopologyMaterialLayer::Decal
                ? "No decal assigned."
                : "Selected material target is no longer valid.";
        return false;
    }
    if (!std::isfinite(uv->scale.y)) {
        statusText = "Selected wall has invalid V scale.";
        return false;
    }

    const float alignedOffsetY = -(spanTopWorld / kSectorGeneratedTextureWorldSize) * uv->scale.y;
    if (!std::isfinite(alignedOffsetY)) {
        statusText = "Aligned V offset is invalid.";
        return false;
    }

    uv->offset.y = alignedOffsetY;
    state.topologyRenderWarning.clear();
    ResetSurface3DUiState();
    for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }

    MarkTopologyDocumentEdited(TextFormat(
            "Aligned %s %s vertically.",
            TopologyWallPartStatusName(wallPart),
            TopologyMaterialLayerStatusName(layer)));
    if (assets != nullptr && state.mode == SectorEditorMode::Preview3D && preview.IsReady()) {
        return RebuildPreviewMeshesPreservingView(*assets);
    }
    return true;
}

bool SectorEditor::AlignSelectedWallMaterialU(
        TopologySurfaceEditTarget target,
        TopologyUAlignDirection direction,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer)
{
    if (!IsWallTopologyEditTarget(target.kind) || !IsValidTopologySurfaceEditTarget(target)) {
        statusText = "Select a wall, lower, or upper surface before aligning U.";
        return false;
    }
    if (layer == TopologyMaterialLayer::Decal && !IsDecalAssigned(target)) {
        statusText = "No decal assigned.";
        return false;
    }

    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, target.sideDefId);
    if (sideDef == nullptr
            || sideDef->lineDefId != target.lineDefId
            || sideDef->sectorId != target.sectorId
            || sideDef->side != target.side) {
        statusText = "Selected sidedef is no longer valid.";
        return false;
    }

    const SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, sideDef->sectorId);
    if (sector == nullptr) {
        statusText = "Selected sidedef references a missing sector.";
        return false;
    }

    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(state.topologyMap, sideDef->lineDefId);
    if (lineDef == nullptr) {
        statusText = "Selected sidedef references a missing linedef.";
        return false;
    }

    float selectedLengthWorld = 0.0f;
    if (!SectorTopologyWallLengthWorld(state.topologyMap, *lineDef, selectedLengthWorld)) {
        statusText = "Selected wall has invalid length.";
        return false;
    }

    SectorTopologyLoopSet loops;
    if (!ExtractSectorTopologyLoops(state.topologyMap, sideDef->sectorId, loops)) {
        statusText = "Could not extract selected sector boundary loop.";
        return false;
    }

    const SectorTopologyLoop* selectedLoop = nullptr;
    size_t selectedEdgeIndex = 0;
    if (!FindUniqueSectorLoopEdgeForSideDef(loops, sideDef->id, selectedLoop, selectedEdgeIndex)
            || selectedLoop == nullptr
            || selectedLoop->edges.empty()) {
        statusText = "Selected sidedef was not found in exactly one sector loop.";
        return false;
    }

    const size_t edgeCount = selectedLoop->edges.size();
    if (edgeCount < 2) {
        statusText = "Selected sector loop has no neighboring wall segment.";
        return false;
    }

    const TopologyWallPart wallPart = TopologyEditTargetWallPart(target.kind);
    const SectorTopologyLoopEdge& selectedEdge = selectedLoop->edges[selectedEdgeIndex];

    if (selectedEdge.sideDefId != sideDef->id
            || selectedEdge.lineDefId != sideDef->lineDefId
            || selectedEdge.side != sideDef->side) {
        statusText = "Selected sidedef loop edge is stale.";
        return false;
    }

    if (!TopologyWallPartHasVisibleSpan(state.topologyMap, *sideDef, wallPart)) {
        switch (wallPart) {
            case TopologyWallPart::Wall:
                statusText = "Selected wall part has no visible solid wall span.";
                break;
            case TopologyWallPart::Lower:
                statusText = "Selected lower wall has no visible height span.";
                break;
            case TopologyWallPart::Upper:
                statusText = "Selected upper wall has no visible height span.";
                break;
        }
        return false;
    }

    std::string neighborError;
    const SectorTopologyLoopEdge* neighborEdge = FindVisibleTopologyWallPartNeighborEdge(
            state.topologyMap,
            *selectedLoop,
            selectedEdgeIndex,
            sideDef->sectorId,
            direction,
            wallPart,
            layer,
            neighborError);
    if (neighborEdge == nullptr) {
        if (!neighborError.empty()) {
            statusText = neighborError;
        } else {
            statusText = TextFormat(
                    "No %s visible %s %s in this sector loop.",
                    TopologyUAlignDirectionStatusName(direction),
                    TopologyWallPartSurfaceStatusName(wallPart),
                    TopologyMaterialLayerStatusName(layer));
        }
        return false;
    }

    const SectorTopologySideDef* neighborSideDef = FindSectorTopologySideDef(
            state.topologyMap,
            neighborEdge->sideDefId);
    if (neighborSideDef == nullptr
            || neighborSideDef->lineDefId != neighborEdge->lineDefId
            || neighborSideDef->sectorId != sideDef->sectorId
            || neighborSideDef->side != neighborEdge->side) {
        statusText = TextFormat(
                "%s sidedef is no longer valid.",
                direction == TopologyUAlignDirection::Previous ? "Previous" : "Next");
        return false;
    }

    const SectorTopologyLineDef* neighborLineDef = FindSectorTopologyLineDef(
            state.topologyMap,
            neighborSideDef->lineDefId);
    if (neighborLineDef == nullptr) {
        statusText = TextFormat(
                "%s sidedef references a missing linedef.",
                direction == TopologyUAlignDirection::Previous ? "Previous" : "Next");
        return false;
    }

    float neighborLengthWorld = 0.0f;
    if (!SectorTopologyWallLengthWorld(state.topologyMap, *neighborLineDef, neighborLengthWorld)) {
        statusText = TextFormat(
                "%s wall has invalid length.",
                direction == TopologyUAlignDirection::Previous ? "Previous" : "Next");
        return false;
    }

    const SectorTopologyWallPartSettings& neighborPart = TopologyWallPartSettingsFor(*neighborSideDef, wallPart);
    const SectorTopologyUvSettings& neighborUv = layer == TopologyMaterialLayer::Decal
            ? neighborPart.decal.uv
            : neighborPart.uv;
    SectorTopologyUvSettings* selectedUv = MutableUvForSurface(target, layer);
    if (selectedUv == nullptr) {
        statusText = layer == TopologyMaterialLayer::Decal
                ? "No decal assigned."
                : "Selected material target is no longer valid.";
        return false;
    }

    if (!std::isfinite(selectedUv->scale.x)
            || !std::isfinite(selectedUv->offset.x)
            || !std::isfinite(neighborUv.scale.x)
            || !std::isfinite(neighborUv.offset.x)) {
        statusText = "Wall U alignment requires finite U scale and offset values.";
        return false;
    }

    float alignedOffsetX = 0.0f;
    if (direction == TopologyUAlignDirection::Previous) {
        const float previousBaseEndU = neighborLengthWorld / kSectorGeneratedTextureWorldSize;
        alignedOffsetX = previousBaseEndU * neighborUv.scale.x + neighborUv.offset.x;
    } else {
        const float selectedBaseEndU = selectedLengthWorld / kSectorGeneratedTextureWorldSize;
        alignedOffsetX = neighborUv.offset.x - selectedBaseEndU * selectedUv->scale.x;
    }

    if (!std::isfinite(alignedOffsetX)) {
        statusText = "Aligned U offset is invalid.";
        return false;
    }

    selectedUv->offset.x = alignedOffsetX;
    state.topologyRenderWarning.clear();
    ResetSurface3DUiState();
    for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }

    MarkTopologyDocumentEdited(TextFormat(
            "Aligned %s %s U from %s.",
            TopologyWallPartStatusName(wallPart),
            TopologyMaterialLayerStatusName(layer),
            TopologyUAlignDirectionStatusName(direction)));
    if (assets != nullptr && state.mode == SectorEditorMode::Preview3D && preview.IsReady()) {
        return RebuildPreviewMeshesPreservingView(*assets);
    }
    return true;
}

bool SectorEditor::RebuildPreviewMeshesPreservingView(engine::AssetManager& assets)
{
    if (!preview.IsReady()) {
        return false;
    }

    const SectorMeshPreviewPose pose = preview.Pose();
    const bool mouseLook = preview.IsMouseLookEnabled();
    const SectorSurfaceRef selected = state.selectedSurface3D;
    const TopologySurfaceEditTarget selectedTarget = state.selectedTopologySurface3D;

    std::string error;
    if (!preview.Rebuild(assets, state.topologyMap, "sector_editor_preview", error)) {
        if (StartsWith(error, "Preview failed:")) {
            statusText = std::string{"3D mode failed:"} + error.substr(std::strlen("Preview failed:"));
        } else {
            statusText = error.empty() ? "3D mode rebuild failed" : error;
        }
        state.mode = SectorEditorMode::Edit2D;
        return false;
    }

    preview.ApplyPose(pose);
    preview.SetMouseLookEnabled(mouseLook);
    const bool selectedStillValid = IsValidSurfaceRef(selected);
    state.selectedSurface3D = selectedStillValid ? selected : SectorSurfaceRef{};
    state.selectedTopologySurface3D = selectedStillValid && IsValidTopologySurfaceEditTarget(selectedTarget)
            ? selectedTarget
            : TopologySurfaceEditTarget{};
    return true;
}

bool SectorEditor::SplitSelectedTopologyLineDef()
{
    const SectorTopologyLineDef* lineDef = SelectedTopologyLineDef();
    if (lineDef == nullptr) {
        ClearStaleTopologySelection();
        statusText = "Select a topology linedef before splitting.";
        return false;
    }

    const int originalLineDefId = lineDef->id;
    const TopologySelectionKind previousSelectionKind = state.topologySelectionKind;
    const SectorTopologySideKind previousSide = state.selectedTopologySideKind;
    const TopologyWallPart previousWallPart = state.selectedTopologyWallPart;

    SectorTopologySplitLineResult split;
    std::string error;
    if (!SplitSectorTopologyLineDef(state.topologyMap, originalLineDefId, &split, &error)) {
        statusText = error.empty() ? "Cannot split topology linedef" : error;
        return false;
    }

    state.pendingTopologyLineSplitAtPoint = PendingTopologyLineSplitAtPoint{};
    state.pendingTopologySectorCut = PendingTopologySectorCut{};
    state.hasHoveredVertex = false;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }

    if (previousSelectionKind == TopologySelectionKind::SideDef) {
        const int secondSideDefId = previousSide == SectorTopologySideKind::Front
                ? split.secondFrontSideDefId
                : split.secondBackSideDefId;
        if (FindSectorTopologySideDef(state.topologyMap, secondSideDefId) != nullptr) {
            SelectTopologySideDef(secondSideDefId, previousWallPart);
        } else {
            SelectTopologyLineDef(split.secondLineDefId, previousSide, previousWallPart);
        }
    } else {
        SelectTopologyLineDef(split.secondLineDefId, previousSide, previousWallPart);
    }

    state.topologyDocumentDirty = true;
    state.topologyRenderWarning.clear();
    state.hasUnsavedChanges = true;
    statusText = TextFormat(
            "Split topology linedef %d; selected linedef %d",
            originalLineDefId,
            split.secondLineDefId);
    return true;
}

bool SectorEditor::DissolveSelectedTopologyVertex()
{
    const SectorTopologyVertex* vertex = SelectedTopologyVertex();
    if (vertex == nullptr) {
        ClearStaleTopologySelection();
        statusText = "Select a topology vertex before dissolving.";
        return false;
    }

    const int vertexId = vertex->id;
    SectorTopologyDissolveVertexResult dissolve;
    std::string error;
    if (!DissolveSectorTopologyVertex(state.topologyMap, vertexId, &dissolve, &error)) {
        statusText = error.empty() ? "Cannot dissolve topology vertex" : error;
        return false;
    }

    state.pendingTopologyLineSplitAtPoint = PendingTopologyLineSplitAtPoint{};
    state.pendingTopologyVertexMerge = PendingTopologyVertexMerge{};
    state.pendingTopologySectorCut = PendingTopologySectorCut{};
    state.hasHoveredVertex = false;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    state.inspectedTopologyVertexId = -1;
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }

    SelectTopologyLineDef(
            dissolve.replacementLineDefId,
            SectorTopologySideKind::Front,
            state.selectedTopologyWallPart);
    state.topologyDocumentDirty = true;
    state.topologyRenderWarning.clear();
    state.hasUnsavedChanges = true;
    statusText = TextFormat(
            "Dissolved topology vertex %d; selected linedef %d",
            dissolve.removedVertexId,
            dissolve.replacementLineDefId);
    return true;
}

bool SectorEditor::JoinSelectedTopologySectors()
{
    const SectorTopologyLineDef* lineDef = SelectedTopologyLineDef();
    if (lineDef == nullptr) {
        ClearStaleTopologySelection();
        statusText = "Select a two-sided topology portal before joining sectors.";
        return false;
    }

    const SectorTopologySideDef* winnerSideDef = SelectedTopologySideDef();
    if (winnerSideDef == nullptr) {
        const int sideDefId = state.selectedTopologySideKind == SectorTopologySideKind::Front
                ? lineDef->frontSideDefId
                : lineDef->backSideDefId;
        winnerSideDef = FindSectorTopologySideDef(state.topologyMap, sideDefId);
    }
    if (winnerSideDef == nullptr || winnerSideDef->lineDefId != lineDef->id) {
        statusText = "Join Sectors requires a selected side of a two-sided topology portal.";
        return false;
    }

    const SectorTopologySideDef* otherSideDef = FindOppositeSectorTopologySideDef(
            state.topologyMap,
            winnerSideDef->id);
    if (otherSideDef == nullptr) {
        statusText = "Join Sectors requires a two-sided topology portal.";
        return false;
    }
    if (winnerSideDef->sectorId == otherSideDef->sectorId) {
        statusText = "Join Sectors requires two different adjacent sectors.";
        return false;
    }

    SectorTopologyJoinSectorsResult join;
    std::string error;
    if (!JoinSectorTopologySectors(
                state.topologyMap,
                winnerSideDef->sectorId,
                otherSideDef->sectorId,
                &join,
                &error)) {
        statusText = error.empty() ? "Join Sectors rejected." : error;
        return false;
    }

    CancelPendingSector(nullptr);
    CancelVertexDrag(nullptr);
    CancelLightDrag(nullptr);
    state.pendingTopologyLineSplitAtPoint = PendingTopologyLineSplitAtPoint{};
    state.pendingTopologyVertexMerge = PendingTopologyVertexMerge{};
    state.pendingTopologySectorCut = PendingTopologySectorCut{};
    state.hasHoveredVertex = false;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    state.hoveredTopologyLightId = -1;
    ClearTransientTopologyEditStateAfterGeometryChange();
    SelectTopologySector(join.survivingSectorId);
    MarkTopologyDocumentEdited(TextFormat(
            "Joined topology sectors %d and %d; kept sector %d.",
            join.survivingSectorId,
            join.removedSectorId,
            join.survivingSectorId));
    return true;
}

std::string SectorEditor::CurrentTextureForPickerTarget() const
{
    if (state.texturePicker.topologyTargetKind == TopologyTexturePickerTargetKind::SideDef) {
        const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(
                state.topologyMap,
                state.texturePicker.topologySideDefId);
        if (sideDef == nullptr) {
            return std::string{};
        }
        const SectorTopologyWallPartSettings& part = TopologyWallPartSettingsFor(
                *sideDef,
                state.texturePicker.topologyWallPart);
        return state.texturePicker.topologyLayer == TopologyMaterialLayer::Decal
                ? part.decal.textureId
                : part.textureId;
    }

    const SectorTopologySector* sector = FindSectorTopologySector(
            state.topologyMap,
            state.texturePicker.topologySectorId);
    if (sector == nullptr) {
        return std::string{};
    }

    switch (state.texturePicker.topologyField) {
        case TopologySectorTextureField::Floor:
            return state.texturePicker.topologyLayer == TopologyMaterialLayer::Decal
                    ? sector->floorDecal.textureId
                    : sector->floorTextureId;
        case TopologySectorTextureField::Ceiling:
            return state.texturePicker.topologyLayer == TopologyMaterialLayer::Decal
                    ? sector->ceilingDecal.textureId
                    : sector->ceilingTextureId;
        case TopologySectorTextureField::DefaultWall: return sector->defaultWall.textureId;
        case TopologySectorTextureField::DefaultLower: return sector->defaultLower.textureId;
        case TopologySectorTextureField::DefaultUpper: return sector->defaultUpper.textureId;
        case TopologySectorTextureField::None: break;
    }
    return std::string{};
}

void SectorEditor::OpenTopologyTexturePicker(
        int sectorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer)
{
    TexturePickerState& picker = state.texturePicker;
    if (FindSectorTopologySector(state.topologyMap, sectorId) == nullptr
            || field == TopologySectorTextureField::None
            || (layer == TopologyMaterialLayer::Decal
                    && field != TopologySectorTextureField::Floor
                    && field != TopologySectorTextureField::Ceiling)) {
        picker = TexturePickerState{};
        statusText = "No topology sector texture target";
        return;
    }

    picker.open = true;
    picker.rebuildPreviewOnApply = false;
    picker.topologyTargetKind = TopologyTexturePickerTargetKind::Sector;
    picker.topologyLayer = layer;
    picker.topologySectorId = sectorId;
    picker.topologyField = field;
    picker.topologySideDefId = -1;
    picker.topologyWallPart = TopologyWallPart::Wall;
    picker.selectedTextureIndex = 0;
    picker.scroll = engine::UIScrollState{};
    picker.textureIds.clear();
    picker.optionLabels.clear();

    const std::vector<std::string> textureIds = SortedSectorTopologyTextureIds(state.topologyMap);
    picker.textureIds.insert(picker.textureIds.end(), textureIds.begin(), textureIds.end());

    const std::string currentTexture = CurrentTextureForPickerTarget();
    for (size_t i = 0; i < picker.textureIds.size(); ++i) {
        picker.optionLabels.push_back(picker.textureIds[i].c_str());
        if (picker.textureIds[i] == currentTexture) {
            picker.selectedTextureIndex = static_cast<int>(i);
        }
    }
}

void SectorEditor::OpenTopologySideDefTexturePicker(
        int sideDefId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    TexturePickerState& picker = state.texturePicker;
    if (FindSectorTopologySideDef(state.topologyMap, sideDefId) == nullptr) {
        picker = TexturePickerState{};
        statusText = "No topology sidedef texture target";
        return;
    }

    picker.open = true;
    picker.rebuildPreviewOnApply = false;
    picker.topologyTargetKind = TopologyTexturePickerTargetKind::SideDef;
    picker.topologyLayer = layer;
    picker.topologySectorId = -1;
    picker.topologyField = TopologySectorTextureField::None;
    picker.topologySideDefId = sideDefId;
    picker.topologyWallPart = wallPart;
    picker.selectedTextureIndex = 0;
    picker.scroll = engine::UIScrollState{};
    picker.textureIds.clear();
    picker.optionLabels.clear();

    const std::vector<std::string> textureIds = SortedSectorTopologyTextureIds(state.topologyMap);
    picker.textureIds.insert(picker.textureIds.end(), textureIds.begin(), textureIds.end());

    const std::string currentTexture = CurrentTextureForPickerTarget();
    for (size_t i = 0; i < picker.textureIds.size(); ++i) {
        picker.optionLabels.push_back(picker.textureIds[i].c_str());
        if (picker.textureIds[i] == currentTexture) {
            picker.selectedTextureIndex = static_cast<int>(i);
        }
    }
}

void SectorEditor::ApplyTexturePickerSelection(engine::AssetManager& assets)
{
    TexturePickerState& picker = state.texturePicker;
    if (!picker.open
            || picker.topologyTargetKind == TopologyTexturePickerTargetKind::None
            || picker.selectedTextureIndex < 0
            || picker.selectedTextureIndex >= static_cast<int>(picker.textureIds.size())) {
        picker = TexturePickerState{};
        return;
    }

    const std::string selectedTexture = picker.textureIds[static_cast<size_t>(picker.selectedTextureIndex)];
    const bool rebuildPreviewOnApply = picker.rebuildPreviewOnApply;
    bool changed = false;

    auto assignTexture = [&](std::string& field) {
        if (field != selectedTexture) {
            field = selectedTexture;
            changed = true;
        }
    };
    auto assignDecalTexture = [&](SectorTopologyDecalLayer& decal) {
        if (decal.textureId.empty()) {
            ResetTopologyUv(decal.uv);
            decal.opacity = 1.0f;
            decal.emissive = false;
            decal.tint = Vector3{1.0f, 1.0f, 1.0f};
        }
        assignTexture(decal.textureId);
    };

    std::string status;
    if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::Sector) {
        SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, picker.topologySectorId);
        if (sector == nullptr) {
            picker = TexturePickerState{};
            return;
        }

        switch (picker.topologyField) {
            case TopologySectorTextureField::Floor:
                if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                    assignDecalTexture(sector->floorDecal);
                } else {
                    assignTexture(sector->floorTextureId);
                }
                break;
            case TopologySectorTextureField::Ceiling:
                if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                    assignDecalTexture(sector->ceilingDecal);
                } else {
                    assignTexture(sector->ceilingTextureId);
                }
                break;
            case TopologySectorTextureField::DefaultWall:
                assignTexture(sector->defaultWall.textureId);
                break;
            case TopologySectorTextureField::DefaultLower:
                assignTexture(sector->defaultLower.textureId);
                break;
            case TopologySectorTextureField::DefaultUpper:
                assignTexture(sector->defaultUpper.textureId);
                break;
            case TopologySectorTextureField::None:
                break;
        }
        status = picker.topologyLayer == TopologyMaterialLayer::Decal
                ? TextFormat("Selected %s decal texture.", picker.topologyField == TopologySectorTextureField::Floor ? "floor" : "ceiling")
                : TextFormat("Changed %s", TopologySectorTextureFieldLabel(picker.topologyField));
    } else if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::SideDef) {
        SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, picker.topologySideDefId);
        if (sideDef == nullptr) {
            picker = TexturePickerState{};
            return;
        }
        SectorTopologyWallPartSettings& part = TopologyWallPartSettingsFor(*sideDef, picker.topologyWallPart);
        if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
            assignDecalTexture(part.decal);
            status = TextFormat(
                    "Selected %s decal texture.",
                    TopologyWallPartStatusName(picker.topologyWallPart));
        } else {
            assignTexture(part.textureId);
            status = TextFormat(
                    "Changed topology sidedef %d %s texture",
                    sideDef->id,
                    TopologyWallPartStatusName(picker.topologyWallPart));
        }
    }

    if (changed) {
        state.topologyRenderWarning.clear();
        MarkTopologyDocumentEdited(status.c_str());
        if (rebuildPreviewOnApply && state.mode == SectorEditorMode::Preview3D && preview.IsReady()) {
            RebuildPreviewMeshesPreservingView(assets);
        }
    }

    picker = TexturePickerState{};
}

} // namespace game
