#include "sector_demo/SectorDynamicModelShadowCasters.h"

#include "engine/components/AnimatedModel.h"
#include "engine/ecs/World.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorStaticModelTransform.h"

#include <array>
#include <cstdio>

namespace game {
namespace {

uint64_t HashBytes(uint64_t hash, const void* data, size_t size)
{
    constexpr uint64_t prime = 1099511628211ull;
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= prime;
    }
    return hash;
}

uint64_t HashMatrix(uint64_t hash, const Matrix& matrix)
{
    const std::array<float, 16> values{
            matrix.m0, matrix.m1, matrix.m2, matrix.m3,
            matrix.m4, matrix.m5, matrix.m6, matrix.m7,
            matrix.m8, matrix.m9, matrix.m10, matrix.m11,
            matrix.m12, matrix.m13, matrix.m14, matrix.m15};
    return HashBytes(hash, values.data(), values.size() * sizeof(float));
}

uint64_t CasterFingerprint(
        engine::Entity entity,
        int placedObjectId,
        engine::ModelHandle model,
        const Matrix& transform,
        const engine::AnimatedModelInstance& instance)
{
    uint64_t hash = 1469598103934665603ull;
    hash = HashBytes(hash, &entity.index, sizeof(entity.index));
    hash = HashBytes(hash, &entity.generation, sizeof(entity.generation));
    hash = HashBytes(hash, &placedObjectId, sizeof(placedObjectId));
    hash = HashBytes(hash, &model.index, sizeof(model.index));
    hash = HashBytes(hash, &model.generation, sizeof(model.generation));
    hash = HashMatrix(hash, transform);
    const uint64_t boneCount = static_cast<uint64_t>(instance.boneMatrices.size());
    hash = HashBytes(hash, &boneCount, sizeof(boneCount));
    for (const Matrix& boneMatrix : instance.boneMatrices) {
        hash = HashMatrix(hash, boneMatrix);
    }
    const uint64_t meshBoneCount =
            static_cast<uint64_t>(instance.meshBoneMatrices.size());
    hash = HashBytes(hash, &meshBoneCount, sizeof(meshBoneCount));
    for (const Matrix& boneMatrix : instance.meshBoneMatrices) {
        hash = HashMatrix(hash, boneMatrix);
    }
    const uint64_t meshNodeCount =
            static_cast<uint64_t>(instance.meshNodeMatrices.size());
    hash = HashBytes(hash, &meshNodeCount, sizeof(meshNodeCount));
    for (const Matrix& meshNodeMatrix : instance.meshNodeMatrices) {
        hash = HashMatrix(hash, meshNodeMatrix);
    }
    return hash;
}

uint64_t StaticPoseCasterFingerprint(
        engine::Entity entity,
        int placedObjectId,
        engine::ModelHandle model,
        const Matrix& transform)
{
    uint64_t hash = 1469598103934665603ull;
    hash = HashBytes(hash, &entity.index, sizeof(entity.index));
    hash = HashBytes(hash, &entity.generation, sizeof(entity.generation));
    hash = HashBytes(hash, &placedObjectId, sizeof(placedObjectId));
    hash = HashBytes(hash, &model.index, sizeof(model.index));
    hash = HashBytes(hash, &model.generation, sizeof(model.generation));
    return HashMatrix(hash, transform);
}

uint64_t CollectionFingerprint(
        const std::vector<SectorDynamicModelShadowCaster>& casters)
{
    uint64_t hash = 1469598103934665603ull;
    const uint64_t count = static_cast<uint64_t>(casters.size());
    hash = HashBytes(hash, &count, sizeof(count));
    for (const SectorDynamicModelShadowCaster& caster : casters) {
        hash = HashBytes(
                hash,
                &caster.contentFingerprint,
                sizeof(caster.contentFingerprint));
    }
    return hash;
}

void RefreshRevision(SectorDynamicModelShadowCasterCollection& collection)
{
    const uint64_t fingerprint = CollectionFingerprint(collection.casters);
    if (!collection.fingerprintInitialized
            || fingerprint != collection.fingerprint) {
        collection.fingerprint = fingerprint;
        ++collection.revision;
        collection.fingerprintInitialized = true;
    }
}

} // namespace

void ReserveSectorDynamicModelShadowCasters(
        SectorDynamicModelShadowCasterCollection& collection,
        size_t capacity)
{
    collection.casters.reserve(capacity);
    collection.capacityWarningPrinted = false;
}

void UpdateSectorDynamicModelShadowCasters(
        SectorDynamicModelShadowCasterCollection& collection,
        engine::World* runtimeObjectWorld)
{
    collection.casters.clear();
    if (runtimeObjectWorld != nullptr) {
        runtimeObjectWorld->ForEach<
                SectorObjectTransform,
                SectorObject,
                SectorDynamicModel,
                engine::AnimatedModelInstance>(
                [&collection, runtimeObjectWorld](
                        engine::Entity entity,
                        SectorObjectTransform& transform,
                        SectorObject& object,
                        SectorDynamicModel& dynamicModel,
                        engine::AnimatedModelInstance& instance) {
                    if (!object.visible
                            || dynamicModel.shadowMode
                                    != SectorDynamicModelShadowMode::Dynamic
                            || !instance.poseReady
                            || instance.poseFailed
                            || engine::IsNull(instance.model)) {
                        return;
                    }
                    if (collection.casters.size()
                                    == collection.casters.capacity()
                            && !collection.capacityWarningPrinted) {
                        std::fprintf(
                                stderr,
                                "[SectorMeshRenderer WARNING] Dynamic prop/NPC shadow caster capacity exceeded; frame-time allocation may occur\n");
                        collection.capacityWarningPrinted = true;
                    }
                    Vector3 renderPosition = transform.position;
                    if (runtimeObjectWorld->Has<SectorObjectVisualOffset>(entity)) {
                        renderPosition = Vector3Add(
                                renderPosition,
                                runtimeObjectWorld
                                        ->Get<SectorObjectVisualOffset>(entity)
                                        .position);
                    }
                    const Matrix authoredTransform =
                            BuildSectorStaticModelAuthoredTransform(
                                    renderPosition,
                                    transform.rotationXRadians,
                                    transform.yawRadians,
                                    transform.rotationZRadians,
                                    dynamicModel.scale);
                    const uint64_t contentFingerprint = CasterFingerprint(
                            entity,
                            dynamicModel.placedObjectId,
                            instance.model,
                            authoredTransform,
                            instance);
                    collection.casters.push_back(
                            SectorDynamicModelShadowCaster{
                                    entity,
                                    dynamicModel.placedObjectId,
                                    instance.model,
                                    authoredTransform,
                                    contentFingerprint,
                                    true});
                });
        runtimeObjectWorld->ForEach<
                SectorObjectTransform,
                SectorObject,
                SectorItem>(
                [&collection, runtimeObjectWorld](
                        engine::Entity entity,
                        SectorObjectTransform& transform,
                        SectorObject& object,
                        SectorItem& item) {
                    if (!object.visible
                            || item.shadowMode
                                    != SectorDynamicModelShadowMode::Dynamic
                            || engine::IsNull(item.model)) {
                        return;
                    }
                    if (collection.casters.size()
                                    == collection.casters.capacity()
                            && !collection.capacityWarningPrinted) {
                        std::fprintf(
                                stderr,
                                "[SectorMeshRenderer WARNING] Dynamic prop/NPC/item shadow caster capacity exceeded; frame-time allocation may occur\n");
                        collection.capacityWarningPrinted = true;
                    }
                    const Matrix authoredTransform =
                            BuildSectorStaticModelAuthoredTransform(
                                    runtimeObjectWorld->Has<SectorObjectVisualOffset>(entity)
                                            ? Vector3Add(
                                                    transform.position,
                                                    runtimeObjectWorld
                                                            ->Get<SectorObjectVisualOffset>(entity)
                                                            .position)
                                            : transform.position,
                                    transform.rotationXRadians,
                                    transform.yawRadians,
                                    transform.rotationZRadians,
                                    item.scale
                                            * item.presentation.scaleMultiplier);
                    collection.casters.push_back(
                            SectorDynamicModelShadowCaster{
                                    entity,
                                    item.placedObjectId,
                                    item.model,
                                    authoredTransform,
                                    StaticPoseCasterFingerprint(
                                            entity,
                                            item.placedObjectId,
                                            item.model,
                                            authoredTransform),
                                    false});
                });
    }
    RefreshRevision(collection);
}

void ClearSectorDynamicModelShadowCasters(
        SectorDynamicModelShadowCasterCollection& collection)
{
    if (collection.casters.empty()) {
        return;
    }
    collection.casters.clear();
    RefreshRevision(collection);
}

} // namespace game
