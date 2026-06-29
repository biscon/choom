#include "sector_editor/SectorEditorHelpers.h"

#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <raylib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <system_error>
#include <utility>

namespace game {

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

const char* PreviewControlModeName(SectorPreviewControlMode mode)
{
    switch (mode) {
        case SectorPreviewControlMode::FreeFly:
            return "FreeFly";
        case SectorPreviewControlMode::Gameplay:
            return "Gameplay";
    }
    return "Unknown";
}

const char* VerticalTransitionName(SectorFpsVerticalTransition transition)
{
    switch (transition) {
        case SectorFpsVerticalTransition::None:
            return "none";
        case SectorFpsVerticalTransition::StayedGrounded:
            return "ground";
        case SectorFpsVerticalTransition::SteppedUp:
            return "step up";
        case SectorFpsVerticalTransition::SnappedDown:
            return "snap down";
        case SectorFpsVerticalTransition::StartedDrop:
            return "drop";
        case SectorFpsVerticalTransition::Landed:
            return "landed";
        case SectorFpsVerticalTransition::CeilingBonk:
            return "ceiling";
        case SectorFpsVerticalTransition::BlockedStep:
            return "blocked step";
        case SectorFpsVerticalTransition::CannotFit:
            return "cannot fit";
    }
    return "unknown";
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
        case SectorEditorTool::AuthoringLine: return "Authoring Line";
        case SectorEditorTool::AuthoringRectangle: return "Rectangle";
        case SectorEditorTool::AuthoringInsertVertex: return "Insert Vertex";
        case SectorEditorTool::AuthoringMove: return "Move Vertex";
        case SectorEditorTool::StaticLight: return "Static Light";
        case SectorEditorTool::DynamicLight: return "Dynamic Light";
        case SectorEditorTool::DynamicSpotLight: return "Dynamic Spot";
        case SectorEditorTool::Move: return "Move";
    }
    return "Unknown";
}

bool IsGraphAuthoringTool(SectorEditorTool tool)
{
    return tool == SectorEditorTool::Select
            || tool == SectorEditorTool::AuthoringLine
            || tool == SectorEditorTool::AuthoringRectangle
            || tool == SectorEditorTool::AuthoringInsertVertex
            || tool == SectorEditorTool::AuthoringMove;
}

bool IsLegacyTopologyMutationTool(SectorEditorTool tool)
{
    return tool == SectorEditorTool::Move;
}

bool IsToolAvailableInGraphAuthoritativeMode(SectorEditorTool tool)
{
    return !IsLegacyTopologyMutationTool(tool);
}

bool IsSectorEditorGraphAuthoritativeMode()
{
    return true;
}

const char* LegacyTopologyMutationUnavailableMessage()
{
    return "Legacy direct-topology tools are unavailable in graph-authoritative mode; use authoring lines and vertices instead.";
}

const char* TopologyWallPartName(TopologyWallPart part)
{
    switch (part) {
        case TopologyWallPart::Wall: return "Wall";
        case TopologyWallPart::Lower: return "Lower";
        case TopologyWallPart::Upper: return "Upper";
        case TopologyWallPart::Middle: return "Middle";
    }
    return "Wall";
}

const char* TopologyWallPartStatusName(TopologyWallPart part)
{
    switch (part) {
        case TopologyWallPart::Wall: return "wall";
        case TopologyWallPart::Lower: return "lower";
        case TopologyWallPart::Upper: return "upper";
        case TopologyWallPart::Middle: return "middle";
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
        case TopologyWallPart::Middle: return "middle";
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
        case SectorSurfaceKind::Middle: return "Middle";
        case SectorSurfaceKind::None: break;
    }
    return "None";
}

bool IsWallSurface(SectorSurfaceKind kind)
{
    return kind == SectorSurfaceKind::Wall
            || kind == SectorSurfaceKind::LowerWall
            || kind == SectorSurfaceKind::UpperWall
            || kind == SectorSurfaceKind::Middle;
}

SectorSurfaceKind ToEditorSurfaceKind(SectorGeneratedSurfaceKind kind)
{
    switch (kind) {
        case SectorGeneratedSurfaceKind::Floor: return SectorSurfaceKind::Floor;
        case SectorGeneratedSurfaceKind::Ceiling: return SectorSurfaceKind::Ceiling;
        case SectorGeneratedSurfaceKind::Wall: return SectorSurfaceKind::Wall;
        case SectorGeneratedSurfaceKind::LowerWall: return SectorSurfaceKind::LowerWall;
        case SectorGeneratedSurfaceKind::UpperWall: return SectorSurfaceKind::UpperWall;
        case SectorGeneratedSurfaceKind::Middle: return SectorSurfaceKind::Middle;
    }
    return SectorSurfaceKind::None;
}

TopologyWallPart SurfaceKindToTopologyWallPart(SectorSurfaceKind kind)
{
    switch (kind) {
        case SectorSurfaceKind::LowerWall: return TopologyWallPart::Lower;
        case SectorSurfaceKind::UpperWall: return TopologyWallPart::Upper;
        case SectorSurfaceKind::Middle: return TopologyWallPart::Middle;
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
        case SectorSurfaceKind::Middle: return TopologySurfaceEditTargetKind::SideDefMiddle;
        case SectorSurfaceKind::None: break;
    }
    return TopologySurfaceEditTargetKind::None;
}

TopologyWallPart TopologyEditTargetWallPart(TopologySurfaceEditTargetKind kind)
{
    switch (kind) {
        case TopologySurfaceEditTargetKind::SideDefLower: return TopologyWallPart::Lower;
        case TopologySurfaceEditTargetKind::SideDefUpper: return TopologyWallPart::Upper;
        case TopologySurfaceEditTargetKind::SideDefMiddle: return TopologyWallPart::Middle;
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
        case TopologyWallPart::Middle: return TopologySurfaceEditTargetKind::SideDefMiddle;
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
        case TopologySurfaceEditTargetKind::SideDefMiddle: return "middle";
        case TopologySurfaceEditTargetKind::None:
            break;
    }
    return "none";
}

bool IsWallTopologyEditTarget(TopologySurfaceEditTargetKind kind)
{
    return kind == TopologySurfaceEditTargetKind::SideDefWall
            || kind == TopologySurfaceEditTargetKind::SideDefLower
            || kind == TopologySurfaceEditTargetKind::SideDefUpper
            || kind == TopologySurfaceEditTargetKind::SideDefMiddle;
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

float ClampDecalBloomIntensity(float value)
{
    if (!std::isfinite(value)) {
        return 1.0f;
    }
    return std::clamp(value, 0.0f, 10.0f);
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
            && IsWhiteTint(decal.tint)
            && decal.bloomIntensity == 1.0f;
}

bool IsDefaultWallPartSettings(const SectorTopologyWallPartSettings& part)
{
    return part.textureId.empty()
            && IsDefaultTopologyUv(part.uv)
            && IsDefaultDecalLayer(part.decal);
}

void ResetDecalLayer(SectorTopologyDecalLayer& decal)
{
    decal.textureId.clear();
    ResetTopologyUv(decal.uv);
    decal.opacity = 1.0f;
    decal.emissive = false;
    decal.tint = Vector3{1.0f, 1.0f, 1.0f};
    decal.bloomIntensity = 1.0f;
}

TopologySectorTextureField TopologyEditTargetSectorTextureField(TopologySurfaceEditTargetKind kind)
{
    switch (kind) {
        case TopologySurfaceEditTargetKind::SectorFloor: return TopologySectorTextureField::Floor;
        case TopologySurfaceEditTargetKind::SectorCeiling: return TopologySectorTextureField::Ceiling;
        case TopologySurfaceEditTargetKind::SideDefWall:
        case TopologySurfaceEditTargetKind::SideDefLower:
        case TopologySurfaceEditTargetKind::SideDefUpper:
        case TopologySurfaceEditTargetKind::SideDefMiddle:
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
        case SectorEditorTool::Select: return "Select: click authoring lines or vertices";
        case SectorEditorTool::AuthoringLine: return "Authoring line: click snapped points to draw a continuous line chain, right click/Esc stops chain";
        case SectorEditorTool::AuthoringRectangle: return "Rectangle: click first corner, then opposite corner, right click/Esc cancels";
        case SectorEditorTool::AuthoringInsertVertex: return "Insert Vertex: click an authoring line to split it, right click/Esc cancels";
        case SectorEditorTool::AuthoringMove: return "Move Vertex: drag authoring vertices";
        case SectorEditorTool::StaticLight: return "Static Light: click inside a sector to place, or drag an existing baked point light";
        case SectorEditorTool::DynamicLight: return "Dynamic Light: click inside a sector to place, or drag an existing runtime point light";
        case SectorEditorTool::DynamicSpotLight: return "Dynamic Spot: click inside a sector to place, edit target and cone in Inspector";
        case SectorEditorTool::Move: return "Legacy move: unavailable in graph-authoritative mode; use Move Vertex for authoring vertices";
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
    if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::MapSky) {
        return "sky texture";
    }
    if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::SideDef
            || picker.topologyTargetKind == TopologyTexturePickerTargetKind::AuthoringSide) {
        const char* sideTarget = picker.topologyTargetKind == TopologyTexturePickerTargetKind::AuthoringSide
                ? "authoring side"
                : "sidedef";
        return picker.topologyLayer == TopologyMaterialLayer::Decal
                ? TextFormat("%s %s decal texture", TopologyWallPartStatusName(picker.topologyWallPart), sideTarget)
                : TextFormat("%s %s texture", TopologyWallPartStatusName(picker.topologyWallPart), sideTarget);
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

bool SameSkySettings(const SectorTopologySkySettings& left, const SectorTopologySkySettings& right)
{
    const SectorTopologySkySettings a = NormalizeSectorTopologySkySettings(left);
    const SectorTopologySkySettings b = NormalizeSectorTopologySkySettings(right);
    return a.textureId == b.textureId
            && a.yawOffsetDegrees == b.yawOffsetDegrees
            && a.verticalOffset == b.verticalOffset
            && a.verticalScale == b.verticalScale
            && a.topColor.r == b.topColor.r
            && a.topColor.g == b.topColor.g
            && a.topColor.b == b.topColor.b
            && a.topColor.a == b.topColor.a;
}

bool SameDirectionalLightSettings(
        const SectorTopologyDirectionalLightSettings& left,
        const SectorTopologyDirectionalLightSettings& right)
{
    const SectorTopologyDirectionalLightSettings a =
            NormalizeSectorTopologyDirectionalLightSettings(left);
    const SectorTopologyDirectionalLightSettings b =
            NormalizeSectorTopologyDirectionalLightSettings(right);
    return a.enabled == b.enabled
            && a.directionToLight.x == b.directionToLight.x
            && a.directionToLight.y == b.directionToLight.y
            && a.directionToLight.z == b.directionToLight.z
            && a.color.r == b.color.r
            && a.color.g == b.color.g
            && a.color.b == b.color.b
            && a.color.a == b.color.a
            && a.intensity == b.intensity;
}

bool SamePreviewSettings(const SectorPreviewSettings& left, const SectorPreviewSettings& right)
{
    const SectorPreviewSettings a = NormalizeSectorPreviewSettings(left);
    const SectorPreviewSettings b = NormalizeSectorPreviewSettings(right);
    return a.walkSpeed == b.walkSpeed
            && a.runSpeed == b.runSpeed
            && a.mouseSensitivity == b.mouseSensitivity
            && a.eyeHeight == b.eyeHeight
            && a.gravity == b.gravity
            && a.playerRadius == b.playerRadius
            && a.playerHeight == b.playerHeight
            && a.stepHeight == b.stepHeight
            && a.jumpHeight == b.jumpHeight
            && a.headBobStrength == b.headBobStrength
            && a.headBobFrequency == b.headBobFrequency;
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
        case TopologyWallPart::Middle: return sideDef.middle;
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
        case TopologyWallPart::Middle: return sideDef.middle;
    }
    return sideDef.wall;
}

SectorTopologyWallPartSettings& TopologyWallPartSettingsFor(
        SectorAuthoringLineSide& side,
        TopologyWallPart part)
{
    switch (part) {
        case TopologyWallPart::Wall: return side.wall;
        case TopologyWallPart::Lower: return side.lower;
        case TopologyWallPart::Upper: return side.upper;
        case TopologyWallPart::Middle: return side.middle;
    }
    return side.wall;
}

const SectorTopologyWallPartSettings& TopologyWallPartSettingsFor(
        const SectorAuthoringLineSide& side,
        TopologyWallPart part)
{
    switch (part) {
        case TopologyWallPart::Wall: return side.wall;
        case TopologyWallPart::Lower: return side.lower;
        case TopologyWallPart::Upper: return side.upper;
        case TopologyWallPart::Middle: return side.middle;
    }
    return side.wall;
}

bool IsTopologyMiddleEligible(
        const SectorTopologyMap& map,
        const SectorTopologySideDef* sideDef)
{
    if (sideDef == nullptr) {
        return false;
    }
    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(map, sideDef->lineDefId);
    if (lineDef == nullptr
            || lineDef->frontSideDefId == -1
            || lineDef->backSideDefId == -1) {
        return false;
    }
    const int expectedSideDefId = sideDef->side == SectorTopologySideKind::Front
            ? lineDef->frontSideDefId
            : lineDef->backSideDefId;
    if (expectedSideDefId != sideDef->id) {
        return false;
    }
    const SectorTopologySideDef* front = FindSectorTopologySideDef(map, lineDef->frontSideDefId);
    const SectorTopologySideDef* back = FindSectorTopologySideDef(map, lineDef->backSideDefId);
    if (front == nullptr || back == nullptr) {
        return false;
    }
    const SectorTopologySideDef* opposite = sideDef->side == SectorTopologySideKind::Front
            ? back
            : front;
    return FindSectorTopologySector(map, sideDef->sectorId) != nullptr
            && FindSectorTopologySector(map, opposite->sectorId) != nullptr;
}

TopologyWallPart ValidTopologyWallPartForSideDef(
        const SectorTopologyMap& map,
        const SectorTopologySideDef* sideDef,
        TopologyWallPart wallPart)
{
    if (wallPart == TopologyWallPart::Middle
            && !IsTopologyMiddleEligible(map, sideDef)) {
        return TopologyWallPart::Wall;
    }
    return wallPart;
}

bool IsMiddleTopologyEditTarget(TopologySurfaceEditTargetKind kind)
{
    return kind == TopologySurfaceEditTargetKind::SideDefMiddle;
}

TopologyMaterialLayer EffectiveTopologyMaterialLayer(
        TopologySurfaceEditTargetKind kind,
        TopologyMaterialLayer layer)
{
    return IsMiddleTopologyEditTarget(kind) ? TopologyMaterialLayer::Base : layer;
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
        case TopologyWallPart::Middle:
            return oppositeSector != nullptr
                    && std::min(sector->ceilingZ, oppositeSector->ceilingZ)
                            > std::max(sector->floorZ, oppositeSector->floorZ)
                    && std::isfinite(sector->floorZ)
                    && std::isfinite(sector->ceilingZ)
                    && std::isfinite(oppositeSector->floorZ)
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

} // namespace game
