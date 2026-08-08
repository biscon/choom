#pragma once

#include "engine/ecs/Entity.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/SectorPortalVisibility.h"

#include <raylib.h>
#include <raymath.h>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace engine {
class AssetManager;
class World;
}

namespace game {

class SectorCollisionWorld;
struct SectorTopologyMap;

constexpr std::size_t MaxDynamicModelProjectedShadows = 8;
constexpr int DynamicModelProjectedShadowResolution = 256;

struct SectorDynamicModelShadowDrawContext {
    using TextureResolver = const Texture2D* (*)(
            void* userData,
            engine::AssetManager& assets,
            const std::string& textureId);

    engine::AssetManager* assets = nullptr;
    engine::World* world = nullptr;
    const Camera3D* camera = nullptr;
    const SectorCollisionWorld* collisionWorld = nullptr;
    const RuntimePortalVisibilityResult* visibility = nullptr;
    const std::vector<SectorMeshBatch>* sectorDrawRecords = nullptr;
    void* textureResolverUserData = nullptr;
    TextureResolver textureResolver = nullptr;
};

class SectorDynamicModelShadowRenderer {
public:
    bool Load();
    void Shutdown();
    void RebuildSources(const SectorTopologyMap& map, const SectorCollisionWorld* collisionWorld);
    void RenderShadowMaps(const SectorDynamicModelShadowDrawContext& context);
    void Draw(const SectorDynamicModelShadowDrawContext& context);

    bool IsLoaded() const { return loaded; }
    std::size_t ActiveProjectedShadowCount() const { return activeSlotCount; }

private:
    enum class LightKind { Point, Spot };

    struct LightSource {
        int id = 0;
        int sectorId = -1;
        LightKind kind = LightKind::Point;
        Vector3 position = {};
        Vector3 direction = {0.0f, -1.0f, 0.0f};
        Vector3 color = {1.0f, 1.0f, 1.0f};
        float intensity = 1.0f;
        float range = 0.0f;
        float innerConeCos = 1.0f;
        float outerConeCos = -1.0f;
    };

    struct Candidate {
        engine::Entity entity = engine::NullEntity();
        int placedObjectId = 0;
        int sectorId = -1;
        float priority = 0.0f;
    };

    struct Slot {
        RenderTexture2D target = {};
        engine::Entity entity = engine::NullEntity();
        int placedObjectId = 0;
        int lightSourceId = -1;
        LightKind lightSourceKind = LightKind::Point;
        int sectorId = -1;
        Matrix lightViewProjection = MatrixIdentity();
        Vector3 lightPosition = {};
        Vector3 lightDirection = {0.0f, -1.0f, 0.0f};
        Vector3 casterCenter = {};
        float maximumDistance = 3.0f;
        bool directional = false;
        bool active = false;
    };

    bool IsObjectAssigned(engine::Entity entity) const;
    void ResetBorrowedReceiverTextures();
    void DrawContactShadows(const SectorDynamicModelShadowDrawContext& context);
    void DrawProjectedShadows(const SectorDynamicModelShadowDrawContext& context);

    std::vector<LightSource> lightSources;
    std::vector<Candidate> candidates;
    std::array<Slot, MaxDynamicModelProjectedShadows> slots{};
    std::array<int, MaxDynamicModelProjectedShadows> previousPlacedObjectIds{};
    std::array<int, MaxDynamicModelProjectedShadows> previousLightSourceIds{};
    std::array<LightKind, MaxDynamicModelProjectedShadows> previousLightSourceKinds{};
    Material casterMaterial = {};
    Material receiverMaterial = {};
    Material contactMaterial = {};
    Mesh contactMesh = {};
    Texture2D receiverDefaultTexture = {};
    Vector3 fallbackDirectionToLight = {-0.35f, 0.80f, -0.25f};
    bool loaded = false;
    bool projectedLoaded = false;
    std::size_t activeSlotCount = 0;
    int casterUseSkinningLoc = -1;
    int casterLightViewProjectionLoc = -1;
    int receiverLightViewProjectionLoc = -1;
    int receiverLightPositionLoc = -1;
    int receiverLightDirectionLoc = -1;
    int receiverCasterCenterLoc = -1;
    int receiverDirectionalLoc = -1;
    int receiverMaximumDistanceLoc = -1;
    int receiverAlphaTestLoc = -1;
    int receiverAlphaCutoffLoc = -1;
    int contactOpacityLoc = -1;
};

} // namespace game
