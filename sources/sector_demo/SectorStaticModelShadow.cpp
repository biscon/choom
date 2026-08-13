#include "sector_demo/SectorStaticModelShadow.h"

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

uint64_t Fingerprint(
        const std::vector<SectorStaticModelShadowCaster>& casters)
{
    uint64_t hash = 1469598103934665603ull;
    const uint64_t count = static_cast<uint64_t>(casters.size());
    hash = HashBytes(hash, &count, sizeof(count));
    for (const SectorStaticModelShadowCaster& caster : casters) {
        hash = HashBytes(
                hash,
                &caster.placedObjectId,
                sizeof(caster.placedObjectId));
        hash = HashBytes(hash, &caster.model.index, sizeof(caster.model.index));
        hash = HashBytes(
                hash,
                &caster.model.generation,
                sizeof(caster.model.generation));
        const std::array<float, 16> matrixValues{
                caster.transform.m0, caster.transform.m1,
                caster.transform.m2, caster.transform.m3,
                caster.transform.m4, caster.transform.m5,
                caster.transform.m6, caster.transform.m7,
                caster.transform.m8, caster.transform.m9,
                caster.transform.m10, caster.transform.m11,
                caster.transform.m12, caster.transform.m13,
                caster.transform.m14, caster.transform.m15};
        hash = HashBytes(
                hash,
                matrixValues.data(),
                matrixValues.size() * sizeof(float));
    }
    return hash;
}

void RefreshRevision(SectorStaticModelShadowCasterCollection& collection)
{
    const uint64_t fingerprint = Fingerprint(collection.casters);
    if (!collection.fingerprintInitialized
            || fingerprint != collection.fingerprint) {
        collection.fingerprint = fingerprint;
        ++collection.revision;
        collection.fingerprintInitialized = true;
    }
}

} // namespace

void ReserveSectorStaticModelShadowCasters(
        SectorStaticModelShadowCasterCollection& collection,
        size_t capacity)
{
    collection.casters.reserve(capacity);
    collection.capacityWarningPrinted = false;
}

void UpdateSectorStaticModelShadowCasters(
        SectorStaticModelShadowCasterCollection& collection,
        engine::World* runtimeObjectWorld)
{
    collection.casters.clear();
    if (runtimeObjectWorld != nullptr) {
        runtimeObjectWorld->ForEach<
                SectorObjectTransform,
                SectorObject,
                SectorStaticModel>(
                [&collection](
                        engine::Entity,
                        SectorObjectTransform& transform,
                        SectorObject& object,
                        SectorStaticModel& staticModel) {
                    if (!object.visible || engine::IsNull(staticModel.model)) {
                        return;
                    }
                    if (collection.casters.size()
                                    == collection.casters.capacity()
                            && !collection.capacityWarningPrinted) {
                        std::fprintf(
                                stderr,
                                "[SectorMeshRenderer WARNING] Static prop spotlight shadow caster capacity exceeded; frame-time allocation may occur\n");
                        collection.capacityWarningPrinted = true;
                    }
                    collection.casters.push_back(
                            SectorStaticModelShadowCaster{
                                    staticModel.placedObjectId,
                                    staticModel.model,
                                    BuildSectorStaticModelAuthoredTransform(
                                            transform.position,
                                            transform.rotationXRadians,
                                            transform.yawRadians,
                                            transform.rotationZRadians,
                                            staticModel.scale)});
                });
    }
    RefreshRevision(collection);
}

void ClearSectorStaticModelShadowCasters(
        SectorStaticModelShadowCasterCollection& collection)
{
    if (collection.casters.empty()) {
        return;
    }
    collection.casters.clear();
    RefreshRevision(collection);
}

} // namespace game
