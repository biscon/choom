#pragma once

#include "engine/ecs/World.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorLightmapTypes.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <cstdint>
#include <string>
#include <vector>

namespace engine {
class AssetManager;
class AudioSystem;
}

namespace game {

struct SectorObject;
struct SectorObjectTransform;
struct SectorReceiverBounds;
struct SectorBakedObjectLightProbeRuntimeData;

constexpr float kSectorDoorPortalBlockEpsilon = 0.001f;
constexpr float kSectorDoorAutoOpenFallbackDistance = 2.0f;
// Shadow-only aperture seal margins for procedural door spotlight casters.
// These counter shadow bias peter-panning at closed doorway edges without changing visuals or collision.
constexpr float kSectorDoorShadowCasterVerticalSealMarginWorld = 0.08f;
constexpr float kSectorDoorShadowCasterHorizontalSealMarginWorld = 0.04f;

struct SectorDynamicDoorCollider {
    int placedObjectId = 0;
    engine::Entity entity = engine::NullEntity();
    Vector2 center = {};
    Vector2 tangent = {1.0f, 0.0f};
    Vector2 normal = {0.0f, -1.0f};
    Vector2 halfExtents = {};
    float bottom = 0.0f;
    float top = 0.0f;
};

struct SectorDoorShadowCaster {
    int placedObjectId = 0;
    engine::Entity entity = engine::NullEntity();
    Matrix model = {};
    Vector3 position = {};
    float width = 0.0f;
    float height = 0.0f;
    float thickness = 0.0f;
};

struct SectorDoorModelShadowCaster {
    int placedObjectId = 0;
    engine::Entity entity = engine::NullEntity();
    engine::ModelHandle model = engine::NullModelHandle();
    Matrix transform = {};
};

struct SectorDoor {
    int placedObjectId = 0;
    bool enabled = true;
};

struct SectorDoorResolvedAnchor {
    int lineDefId = 0;
    int frontSectorId = 0;
    int backSectorId = 0;
    int frontSideDefId = 0;
    int backSideDefId = 0;
    Vector2 endpointA = {};
    Vector2 endpointB = {};
    Vector2 midpoint = {};
    // Tangent is the resolved portal front-side direction; normal points front sector -> back sector.
    Vector2 tangent = {1.0f, 0.0f};
    Vector2 normal = {0.0f, -1.0f};
    float openBottom = 0.0f;
    float openTop = 0.0f;
    float portalWidth = 0.0f;
    float portalHeight = 0.0f;
};

struct SectorDoorMotion {
    SectorDoorMotionType motion = SectorDoorMotionType::SlideVertical;
    float openFraction = 0.0f;
    float targetOpenFraction = 0.0f;
    // Slides store world units; swings store radians.
    float travelAmount = 0.0f;
    // Slides store world units/second; swings store radians/second.
    float travelSpeed = 1.5f;
    SectorDoorHinge hinge = SectorDoorHinge::Start;
    SectorDoorSwingSide swingSide = SectorDoorSwingSide::Front;
};

enum class SectorDoorAudioEvent {
    None,
    Open,
    Close
};

struct SectorDoorAudio {
    std::string openSoundId;
    std::string closeSoundId;
    engine::SoundHandle openSound = engine::NullSoundHandle();
    engine::SoundHandle closeSound = engine::NullSoundHandle();
    bool targetWasOpen = false;
    SectorDoorAudioEvent pendingEvent = SectorDoorAudioEvent::None;
};

struct SectorDoorInteraction {
    bool autoOpen = false;
    float interactionDistance = 1.5f;
    float autoOpenDistance = kSectorDoorAutoOpenFallbackDistance;
};

struct SectorDoorRender {
    float width = 0.0f;
    float height = 0.0f;
    float thickness = 0.25f;
    // Positive values move the closed slab from the front sector toward the back sector.
    float normalOffset = 0.0f;
    std::string textureId;
    SectorDoorFaceUvSet faceUvs;
    Color tint = WHITE;
    bool visible = true;
    // Derived current leaf axes. They are updated from the same pose as collision.
    Vector2 widthAxis = {};
    Vector2 thicknessAxis = {};
    // Optional fixed-frame alignment, stored in already-scaled world units.
    float leafHingeToFrameCenter = 0.0f;
    float leafBottomOffset = 0.0f;
    bool alignLeafToFrame = false;
};

enum class SectorDoorModelFallbackReason {
    None,
    ProceduralVisual,
    CatalogUnavailable,
    MissingCatalogAsset,
    InvalidFit,
    AssetScopeUnavailable,
    LeafRequestFailed
};

struct SectorDoorModelRender {
    engine::ModelHandle leafModel = engine::NullModelHandle();
    engine::ModelHandle frameModel = engine::NullModelHandle();
    float effectiveScale = 1.0f;
    float nominalWidth = 0.0f;
    float nominalHeight = 0.0f;
    float nominalThickness = 0.0f;
    float actualWidth = 0.0f;
    float actualHeight = 0.0f;
    float actualThickness = 0.0f;
    float frameOuterWidth = 0.0f;
    float frameOuterHeight = 0.0f;
    float leafHingeToFrameCenter = 0.0f;
    float leafBottomOffset = 0.0f;
    Matrix leafMatrix = {};
    Matrix frameMatrix = {};
    BoundingBox analyticReceiverBounds = {};
    BoundingBox receiverBounds = {};
    BoundingBox leafLocalBounds = {};
    BoundingBox frameLocalBounds = {};
    Vector3 containingSectorAmbient = {0.15f, 0.15f, 0.15f};
    float environmentExposure = 0.15f;
    SectorDoorModelFallbackReason fallbackReason =
            SectorDoorModelFallbackReason::ProceduralVisual;
    bool modelVisualRequested = false;
    bool catalogResolved = false;
    bool frameDeclared = false;
    bool leafReady = false;
    bool leafFailed = false;
    bool frameReady = false;
    bool frameFailed = false;
    bool leafBoundsReady = false;
    bool frameBoundsReady = false;
    bool leafFailureReported = false;
    bool frameFailureReported = false;
};

struct SectorDoorModelDrawPolicy {
    bool drawProcedural = true;
    bool drawLeaf = false;
    bool drawFrame = false;
};

struct SectorDoorPlayerObstacle {
    Vector3 feetPosition = {};
    float radius = 0.25f;
    float height = 1.6f;
};

struct SectorDoorSwingPose {
    Vector3 hingePosition = {};
    Vector3 center = {};
    Vector2 widthAxis = {1.0f, 0.0f};
    Vector2 thicknessAxis = {0.0f, -1.0f};
    float bottom = 0.0f;
    float top = 0.0f;
    float angleRadians = 0.0f;
    Matrix leafMatrix = {};
    Matrix frameMatrix = {};
};

struct SectorDoorCollider {
    Vector2 center = {};
    Vector2 tangent = {1.0f, 0.0f};
    Vector2 normal = {0.0f, -1.0f};
    Vector2 halfExtents = {};
    float bottom = 0.0f;
    float top = 0.0f;
    bool enabled = true;
};

struct SectorDoorPortalBlocker {
    int lineDefId = 0;
    int frontSectorId = 0;
    int backSectorId = 0;
    int frontSideDefId = 0;
    int backSideDefId = 0;
    bool blocksPortal = true;
};

struct SectorDoorSlabGeometry {
    Vector3 tangent = {1.0f, 0.0f, 0.0f};
    Vector3 normal = {0.0f, 0.0f, -1.0f};
    Vector3 bottomFrontLeft = {};
    Vector3 bottomFrontRight = {};
    Vector3 bottomBackRight = {};
    Vector3 bottomBackLeft = {};
    Vector3 topFrontLeft = {};
    Vector3 topFrontRight = {};
    Vector3 topBackRight = {};
    Vector3 topBackLeft = {};
};

struct SectorDoorSlabMeshVertex {
    Vector3 position = {};
    Vector3 normal = {};
    Vector2 uv = {};
    Color color = WHITE;
};

struct SectorDoorSlabMeshData {
    std::vector<SectorDoorSlabMeshVertex> vertices;
    std::vector<uint16_t> indices;
};

enum class SectorDoorUvFitMode {
    Width,
    Height,
    Both
};

const char* SectorDoorFaceName(SectorDoorFace face);
int SectorDoorFaceIndex(SectorDoorFace face);
SectorDoorFace SectorDoorFaceFromIndex(int index);
SectorDoorFaceUv& DoorFaceUv(SectorDoorFaceUvSet& uvs, SectorDoorFace face);
const SectorDoorFaceUv& DoorFaceUv(const SectorDoorFaceUvSet& uvs, SectorDoorFace face);
bool IsDefaultSectorDoorFaceUv(const SectorDoorFaceUv& uv);
bool IsDefaultSectorDoorFaceUvSet(const SectorDoorFaceUvSet& uvs);
bool SameSectorDoorFaceUv(const SectorDoorFaceUv& a, const SectorDoorFaceUv& b);
bool SameSectorDoorFaceUvSet(const SectorDoorFaceUvSet& a, const SectorDoorFaceUvSet& b);
bool IsValidSectorDoorUvScale(float scale);
bool IsValidSectorDoorUvOffset(float offset);
Vector2 SectorDoorFaceDimensions(const SectorDoorRender& render, SectorDoorFace face);
Vector2 SectorDoorFaceBaseUvSpan(const SectorDoorRender& render, SectorDoorFace face);
bool FitSectorDoorFaceUv(
        SectorDoorFaceUvSet& uvs,
        SectorDoorFace face,
        SectorDoorUvFitMode mode,
        const SectorDoorRender& render,
        std::string* outError = nullptr);
bool ResetSectorDoorFaceUv(SectorDoorFaceUvSet& uvs, SectorDoorFace face);
bool CopySectorDoorFaceUv(
        SectorDoorFaceUvSet& uvs,
        SectorDoorFace source,
        SectorDoorFace target);
bool ApplySectorDoorFaceUvToAll(SectorDoorFaceUvSet& uvs, SectorDoorFace source);

SectorDoorSlabGeometry BuildSectorDoorSlabGeometry(
        const SectorObjectTransform& transform,
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorRender& render);

SectorDoorSlabMeshData BuildSectorDoorSlabMeshData(const SectorDoorRender& render);

Matrix BuildSectorDoorSlabModelMatrix(
        const SectorObjectTransform& transform,
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorRender& render);

Matrix BuildSectorDoorShadowCasterModelMatrix(
        const SectorDoorShadowCaster& caster,
        float visualWidth,
        float visualHeight);

Vector3 EvaluateSectorObjectAmbientCubeLighting(
        const BakedObjectLightingSample& sample,
        Vector3 worldNormal);

bool BuildSectorDoorStaticLightingColors(
        const SectorDoorSlabMeshData& meshData,
        const SectorObjectTransform& transform,
        const SectorObject& object,
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorRender& render,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
        const SectorTopologyMap* mapForFallback,
        std::vector<Vector3>& outLighting);

bool AppendSectorDoorReceiverBounds(
        const SectorObjectTransform& transform,
        const SectorObject& object,
        const SectorDoor& door,
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorRender& render,
        std::vector<SectorReceiverBounds>& outBounds);

void CollectSectorDoorReceiverBounds(
        engine::World& world,
        std::vector<SectorReceiverBounds>& outBounds);

SectorDoorModelDrawPolicy ResolveSectorDoorModelDrawPolicy(
        const SectorDoorModelRender& model,
        bool leafAssetAvailable,
        bool frameAssetAvailable);

bool ShouldDrawSectorDoorForVisibility(
        const SectorDoorResolvedAnchor& anchor,
        const RuntimePortalVisibilityResult& visibility);

int ResolveSectorDoorAdjacentLightingSector(
        const SectorDoorResolvedAnchor& anchor,
        Vector3 leafCenter,
        int containingSectorId);

BoundingBox TransformSectorDoorModelBounds(
        BoundingBox localBounds,
        Matrix transform);

BoundingBox UnionSectorDoorModelBounds(
        BoundingBox first,
        BoundingBox second);

void CollectSectorDoorModelShadowCasters(
        engine::World& world,
        engine::AssetManager& assets,
        std::vector<SectorDoorModelShadowCaster>& outCasters);

bool AppendSectorDoorModelShadowCasters(
        engine::Entity entity,
        const SectorObject& object,
        const SectorDoor& door,
        const SectorDoorRender& render,
        const SectorDoorModelRender& model,
        const SectorDoorModelDrawPolicy& policy,
        std::vector<SectorDoorModelShadowCaster>& outCasters);

bool AppendSectorDoorShadowCaster(
        engine::Entity entity,
        const SectorObjectTransform& transform,
        const SectorObject& object,
        const SectorDoor& door,
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorRender& render,
        std::vector<SectorDoorShadowCaster>& outCasters);

void CollectSectorDoorShadowCasters(
        engine::World& world,
        std::vector<SectorDoorShadowCaster>& outCasters);

float SectorDoorResolvedOpenDistance(
        const SectorResolvedDoorAnchor& resolved,
        const SectorPlacedDoor& door);

float SectorDoorAnchorYawRadians(const SectorResolvedDoorAnchor& resolved);

Vector3 SectorDoorMotionOffset(
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorMotion& motion);

Vector3 SectorDoorClosedCenter(
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorRender& render);

SectorDoorResolvedAnchor ToSectorRuntimeDoorAnchor(const SectorResolvedDoorAnchor& resolved);

float SectorDoorSwingSign(
        const SectorDoorResolvedAnchor& anchor,
        SectorDoorHinge hinge,
        SectorDoorSwingSide swingSide);

SectorDoorSwingPose BuildSectorDoorSwingPose(
        const SectorDoorResolvedAnchor& anchor,
        const SectorDoorMotion& motion,
        const SectorDoorRender& render,
        float effectiveScale,
        float openFraction);

SectorDoorCollider BuildSectorDoorSwingCollider(
        const SectorDoorSwingPose& pose,
        float width,
        float thickness,
        bool enabled = true);

void UpdateSectorDoorAutoOpenSystem(
        engine::World& world,
        const Vector3& playerPosition);

bool ToggleTargetedSectorDoorInteractionSystem(
        engine::World& world,
        const Vector3& playerPosition,
        const Vector3& playerForward);

bool AdvanceSectorDoorMotionSystem(
        engine::World& world,
        float dt,
        const SectorDoorPlayerObstacle* playerObstacle = nullptr);

bool RefreshSectorDoorModelReadinessSystem(
        engine::World& world,
        engine::AssetManager& assets);

SectorDoorAudioEvent UpdateSectorDoorAudioTransition(
        SectorDoorAudio& audio,
        const SectorDoorMotion& motion);

bool IsSectorDoorAudioEventReady(
        SectorDoorAudioEvent event,
        const SectorDoorMotion& motion);

void UpdateSectorDoorAudioSystem(
        engine::World& world,
        engine::AssetManager& assets,
        engine::AudioSystem& audio);

void UpdateSectorDoorDerivedStateSystem(engine::World& world);

void CollectSectorDoorDynamicColliders(
        engine::World& world,
        std::vector<SectorDynamicDoorCollider>& colliders);

bool SectorDoorDynamicCollidersAllowPlayerHeight(
        Vector2 positionXZ,
        float feetY,
        float radius,
        float playerHeight,
        const std::vector<SectorDynamicDoorCollider>& colliders);

void CollectSectorDoorDynamicPortalBlockers(
        engine::World& world,
        std::vector<RuntimePortalDynamicBlocker>& blockers);

SectorCollisionMoveResult ResolveSectorDoorDynamicCollidersForPlayerMovement(
        const SectorCollisionMoveState& moveState,
        const SectorCollisionMoveResult& staticResult,
        const SectorCollisionMoveConfig& config,
        const std::vector<SectorDynamicDoorCollider>& colliders);

} // namespace game
