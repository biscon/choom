#include "sector_demo/renderer/SectorImpactParticleSystem.h"

#include "engine/assets/AssetManager.h"
#include "engine/components/AnimatedModel.h"
#include "engine/ecs/World.h"
#include "engine/systems/AnimatedModelRaycast.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorStaticModelTransform.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace game {
namespace {

constexpr float BloodEmissionSeconds = 0.06f;
constexpr float Pi = 3.14159265358979323846f;

Vector3 AttachedRenderPosition(
        engine::World& world,
        engine::Entity entity,
        const SectorObjectTransform& transform)
{
    return world.Has<SectorObjectVisualOffset>(entity)
            ? Vector3Add(
                    transform.position,
                    world.Get<SectorObjectVisualOffset>(entity).position)
            : transform.position;
}

Matrix AttachedAuthoredTransform(
        engine::World& world,
        engine::Entity entity,
        const SectorObjectTransform& transform,
        const SectorDynamicModel& model)
{
    return BuildSectorStaticModelAuthoredTransform(
            AttachedRenderPosition(world, entity, transform),
            transform.rotationXRadians,
            transform.yawRadians,
            transform.rotationZRadians,
            model.scale);
}

Color FadeColor(Color color, float alpha)
{
    color.a = static_cast<unsigned char>(std::clamp(
            alpha * static_cast<float>(color.a), 0.0f, 255.0f));
    return color;
}

void DrawLayeredDiamond(
        Vector3 center,
        Vector3 axis,
        Vector3 perpendicular,
        float halfLength,
        float halfWidth,
        Color outer,
        Color middle,
        Color core)
{
    const auto drawDiamond = [&](float scale, Color color) {
        const Vector3 top = Vector3Add(center, Vector3Scale(axis, halfLength * scale));
        const Vector3 bottom = Vector3Subtract(center, Vector3Scale(axis, halfLength * scale));
        const Vector3 left = Vector3Subtract(center, Vector3Scale(perpendicular, halfWidth * scale));
        const Vector3 right = Vector3Add(center, Vector3Scale(perpendicular, halfWidth * scale));
        DrawTriangle3D(top, left, right, color);
        DrawTriangle3D(bottom, right, left, color);
    };
    drawDiamond(1.0f, outer);
    drawDiamond(0.68f, middle);
    drawDiamond(0.31f, core);
}

} // namespace

void SectorImpactParticleSystem::Clear()
{
    emitters = {};
    particles = {};
    emitterOverflowWarned = false;
    particleOverflowWarned = false;
}

float SectorImpactParticleSystem::Random01(uint32_t& state) const
{
    state ^= state << 13u;
    state ^= state >> 17u;
    state ^= state << 5u;
    return static_cast<float>(state & 0x00ffffffu)
            / static_cast<float>(0x01000000u);
}

void SectorImpactParticleSystem::Spawn(const WeaponImpactEvent& event)
{
    if (event.kind == WeaponImpactKind::None
            || !event.particles.enabled
            || event.particles.particleCount <= 0) return;
    for (Emitter& emitter : emitters) {
        if (emitter.active) continue;
        emitter = {};
        emitter.kind = event.kind;
        emitter.position = event.position;
        emitter.normal = event.normal;
        emitter.localPosition = event.localPosition;
        emitter.sectorId = event.sectorId;
        emitter.attachedEntity = event.attachedEntity;
        emitter.surfaceAnchor = event.surfaceAnchor;
        emitter.settings = event.particles;
        emitter.randomState = nextSeed;
        nextSeed = nextSeed * 1664525u + 1013904223u;
        emitter.active = true;
        if (event.kind == WeaponImpactKind::SurfaceDebris) {
            for (int index = 0; index < event.particles.particleCount; ++index) {
                EmitParticle(emitter, index % 3 == 0);
                ++emitter.emittedCount;
            }
            emitter.active = false;
        } else {
            const int immediate = std::min(6, event.particles.particleCount);
            for (int index = 0; index < immediate; ++index) {
                EmitParticle(emitter, false);
                ++emitter.emittedCount;
            }
        }
        return;
    }
    if (!emitterOverflowWarned) {
        std::fprintf(stderr,
                "[Impact Particles WARNING] Emitter capacity exceeded; impact was dropped.\n");
        emitterOverflowWarned = true;
    }
}

void SectorImpactParticleSystem::EmitParticle(Emitter& emitter, bool dust)
{
    Particle* target = nullptr;
    for (Particle& particle : particles) {
        if (!particle.active) {
            target = &particle;
            break;
        }
    }
    if (target == nullptr) {
        if (!particleOverflowWarned) {
            std::fprintf(stderr,
                    "[Impact Particles WARNING] Particle capacity exceeded; particles were dropped.\n");
            particleOverflowWarned = true;
        }
        return;
    }
    const float azimuth = Random01(emitter.randomState) * Pi * 2.0f;
    const float elevation = 0.18f + Random01(emitter.randomState) * 0.72f;
    Vector3 tangent = Vector3Normalize(Vector3CrossProduct(
            std::fabs(emitter.normal.y) < 0.95f
                    ? Vector3{0.0f, 1.0f, 0.0f}
                    : Vector3{1.0f, 0.0f, 0.0f},
            emitter.normal));
    Vector3 bitangent = Vector3Normalize(Vector3CrossProduct(
            emitter.normal, tangent));
    Vector3 spread = Vector3Add(
            Vector3Scale(tangent, std::cos(azimuth)),
            Vector3Scale(bitangent, std::sin(azimuth)));
    Vector3 direction = Vector3Normalize(Vector3Add(
            Vector3Scale(emitter.normal, elevation),
            Vector3Scale(spread, 1.0f - elevation * 0.35f)));
    const bool blood = emitter.kind == WeaponImpactKind::Blood;
    const float speed = (blood ? 1.4f : (dust ? 0.28f : 1.8f))
            * emitter.settings.intensity
            * (0.55f + Random01(emitter.randomState) * 0.9f);
    *target = {};
    target->kind = emitter.kind;
    target->position = Vector3Add(
            emitter.position,
            Vector3Scale(emitter.normal, 0.008f));
    target->velocity = Vector3Scale(direction, speed);
    target->lifetimeSeconds = blood
            ? 0.28f + Random01(emitter.randomState) * 0.42f
            : (dust
                    ? 0.32f + Random01(emitter.randomState) * 0.35f
                    : 0.22f + Random01(emitter.randomState) * 0.42f);
    target->sizeWorld = (blood ? 0.018f : (dust ? 0.055f : 0.022f))
            * emitter.settings.sizeScale
            * (0.65f + Random01(emitter.randomState) * 0.7f);
    target->rotationRadians = Random01(emitter.randomState) * Pi * 2.0f;
    target->sectorId = emitter.sectorId;
    target->dust = dust;
    target->active = true;
}

void SectorImpactParticleSystem::Update(
        engine::World& world,
        const engine::AssetManager* assets,
        float rawDt)
{
    const float dt = std::isfinite(rawDt) ? std::max(0.0f, rawDt) : 0.0f;
    for (Emitter& emitter : emitters) {
        if (!emitter.active) continue;
        if (!engine::IsNull(emitter.attachedEntity)
                && world.IsAlive(emitter.attachedEntity)
                && world.Has<SectorObjectTransform>(emitter.attachedEntity)) {
            const SectorObjectTransform& transform =
                    world.Get<SectorObjectTransform>(emitter.attachedEntity);
            bool resolvedSurfaceAnchor = false;
            if (assets != nullptr
                    && emitter.surfaceAnchor.valid
                    && world.Has<SectorDynamicModel>(emitter.attachedEntity)
                    && world.Has<engine::AnimatedModelInstance>(
                            emitter.attachedEntity)) {
                const SectorDynamicModel& model = world.Get<SectorDynamicModel>(
                        emitter.attachedEntity);
                const engine::AnimatedModelInstance& instance =
                        world.Get<engine::AnimatedModelInstance>(
                                emitter.attachedEntity);
                const engine::ModelAsset* asset = assets->GetModelAsset(
                        instance.model);
                if (asset != nullptr) {
                    resolvedSurfaceAnchor =
                            engine::ResolveAnimatedModelSurfaceAnchor(
                                    *asset,
                                    instance,
                                    emitter.surfaceAnchor,
                                    AttachedAuthoredTransform(
                                            world,
                                            emitter.attachedEntity,
                                            transform,
                                            model),
                                    emitter.position);
                }
            }
            if (!resolvedSurfaceAnchor) {
                if (world.Has<SectorDynamicModel>(emitter.attachedEntity)) {
                    emitter.position = Vector3Transform(
                            emitter.localPosition,
                            AttachedAuthoredTransform(
                                    world,
                                    emitter.attachedEntity,
                                    transform,
                                    world.Get<SectorDynamicModel>(
                                            emitter.attachedEntity)));
                } else {
                    emitter.position = Vector3Add(
                            transform.position, emitter.localPosition);
                }
            }
            if (world.Has<SectorObject>(emitter.attachedEntity)) {
                emitter.sectorId = world.Get<SectorObject>(
                        emitter.attachedEntity).currentSectorId;
            }
        }
        emitter.ageSeconds += dt;
        const float progress = std::clamp(
                emitter.ageSeconds / BloodEmissionSeconds, 0.0f, 1.0f);
        const int desired = std::min(
                emitter.settings.particleCount,
                static_cast<int>(std::ceil(
                        progress * emitter.settings.particleCount)));
        while (emitter.emittedCount < desired) {
            EmitParticle(emitter, false);
            ++emitter.emittedCount;
        }
        if (emitter.ageSeconds >= BloodEmissionSeconds
                || emitter.emittedCount >= emitter.settings.particleCount) {
            emitter.active = false;
        }
    }

    for (Particle& particle : particles) {
        if (!particle.active) continue;
        particle.ageSeconds += dt;
        if (particle.ageSeconds >= particle.lifetimeSeconds) {
            particle.active = false;
            continue;
        }
        const float gravity = particle.kind == WeaponImpactKind::Blood
                ? 7.0f : (particle.dust ? 0.35f : 9.8f);
        particle.velocity.y -= gravity * dt;
        const float drag = particle.kind == WeaponImpactKind::Blood
                ? 1.8f : (particle.dust ? 3.5f : 0.8f);
        particle.velocity = Vector3Scale(
                particle.velocity, std::exp(-drag * dt));
        particle.position = Vector3Add(
                particle.position, Vector3Scale(particle.velocity, dt));
        particle.rotationRadians += dt * (particle.dust ? 0.8f : 7.0f);
    }
}

void SectorImpactParticleSystem::Draw(
        const Camera3D& camera,
        const RuntimePortalVisibilityResult& visibility) const
{
    const Vector3 cameraForward = Vector3Normalize(
            Vector3Subtract(camera.target, camera.position));
    const Vector3 cameraRight = Vector3Normalize(
            Vector3CrossProduct(cameraForward, camera.up));
    const Vector3 cameraUp = Vector3Normalize(Vector3CrossProduct(
            cameraRight, cameraForward));
    rlEnableColorBlend();
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    for (const Particle& particle : particles) {
        if (!particle.active
                || !ShouldDrawRuntimeSectorForVisibility(
                        particle.sectorId, visibility)) continue;
        const float life = std::clamp(
                1.0f - particle.ageSeconds / particle.lifetimeSeconds,
                0.0f,
                1.0f);
        if (particle.kind == WeaponImpactKind::Blood) {
            Vector3 projectedVelocity = Vector3Subtract(
                    particle.velocity,
                    Vector3Scale(
                            cameraForward,
                            Vector3DotProduct(particle.velocity, cameraForward)));
            Vector3 axis = Vector3LengthSqr(projectedVelocity) > 0.00001f
                    ? Vector3Normalize(projectedVelocity)
                    : cameraUp;
            Vector3 perpendicular = Vector3Normalize(
                    Vector3CrossProduct(cameraForward, axis));
            const float stretch = 1.3f + std::min(
                    3.0f, Vector3Length(particle.velocity) * 0.8f);
            DrawLayeredDiamond(
                    particle.position,
                    axis,
                    perpendicular,
                    particle.sizeWorld * stretch,
                    particle.sizeWorld * 0.7f,
                    FadeColor(Color{42, 2, 5, 170}, life),
                    FadeColor(Color{126, 8, 15, 205}, life),
                    FadeColor(Color{220, 42, 38, 230}, life));
        } else if (particle.dust) {
            const Vector3 axis = Vector3Normalize(Vector3Add(
                    Vector3Scale(cameraUp, std::cos(particle.rotationRadians)),
                    Vector3Scale(cameraRight, std::sin(particle.rotationRadians))));
            const Vector3 perpendicular = Vector3Normalize(
                    Vector3CrossProduct(cameraForward, axis));
            DrawLayeredDiamond(
                    particle.position,
                    axis,
                    perpendicular,
                    particle.sizeWorld * (1.2f + particle.ageSeconds),
                    particle.sizeWorld * (1.0f + particle.ageSeconds),
                    FadeColor(Color{72, 66, 58, 30}, life * life),
                    FadeColor(Color{132, 122, 105, 34}, life * life),
                    FadeColor(Color{190, 180, 158, 22}, life * life));
        } else {
            const Vector3 axis = Vector3Normalize(Vector3Add(
                    Vector3Scale(cameraUp, std::cos(particle.rotationRadians)),
                    Vector3Scale(cameraRight, std::sin(particle.rotationRadians))));
            const Vector3 perpendicular = Vector3Normalize(
                    Vector3CrossProduct(cameraForward, axis));
            DrawLayeredDiamond(
                    particle.position,
                    axis,
                    perpendicular,
                    particle.sizeWorld,
                    particle.sizeWorld * 0.58f,
                    FadeColor(Color{45, 42, 38, 230}, life),
                    FadeColor(Color{105, 96, 82, 235}, life),
                    FadeColor(Color{176, 163, 138, 220}, life));
        }
    }
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    rlEnableColorBlend();
    rlSetBlendMode(BLEND_ALPHA);
}

size_t SectorImpactParticleSystem::ActiveParticleCount() const
{
    return static_cast<size_t>(std::count_if(
            particles.begin(), particles.end(),
            [](const Particle& particle) { return particle.active; }));
}

} // namespace game
