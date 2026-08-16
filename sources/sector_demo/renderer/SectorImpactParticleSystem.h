#pragma once

#include "engine/ecs/Entity.h"
#include "game/npc/NpcCombatSystem.h"
#include "sector_demo/SectorPortalVisibility.h"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace engine {
class AssetManager;
class World;
}

namespace game {

class SectorImpactParticleSystem {
public:
    static constexpr size_t MaximumEmitters = 64;
    static constexpr size_t MaximumParticles = 1024;

    void Clear();
    void Spawn(const WeaponImpactEvent& event);
    void Update(
            engine::World& world,
            const engine::AssetManager* assets,
            float dt);
    void Draw(
            const Camera3D& camera,
            const RuntimePortalVisibilityResult& visibility) const;

    size_t ActiveParticleCount() const;

private:
    struct Emitter {
        WeaponImpactKind kind = WeaponImpactKind::None;
        Vector3 position{};
        Vector3 normal{};
        Vector3 localPosition{};
        int sectorId = 0;
        engine::Entity attachedEntity = engine::NullEntity();
        engine::AnimatedModelSurfaceAnchor surfaceAnchor;
        FpsWeaponImpactParticlesDefinition settings;
        float ageSeconds = 0.0f;
        int emittedCount = 0;
        uint32_t randomState = 1;
        bool active = false;
    };

    struct Particle {
        WeaponImpactKind kind = WeaponImpactKind::None;
        Vector3 position{};
        Vector3 velocity{};
        float ageSeconds = 0.0f;
        float lifetimeSeconds = 0.0f;
        float sizeWorld = 0.0f;
        float rotationRadians = 0.0f;
        int sectorId = 0;
        bool dust = false;
        bool active = false;
    };

    void EmitParticle(Emitter& emitter, bool dust);
    float Random01(uint32_t& state) const;

    std::array<Emitter, MaximumEmitters> emitters{};
    std::array<Particle, MaximumParticles> particles{};
    uint32_t nextSeed = 0x83d2e11bu;
    bool emitterOverflowWarned = false;
    bool particleOverflowWarned = false;
};

} // namespace game
