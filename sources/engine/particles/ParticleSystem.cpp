#include "engine/particles/ParticleSystem.h"

#include <raymath.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace engine {
namespace {
constexpr float Step = 1.0f / 60.0f;
float Random(uint32_t& state)
{
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return static_cast<float>(state & 0xffffffu) / 16777216.0f;
}
void Emit(ParticlePool& pool, size_t emitterIndex, int layerIndex, int count)
{
    auto& emitter = pool.emitters[emitterIndex];
    const auto& d = emitter.definition;
    const auto& layer = d.layers[layerIndex];
    // Collision admission is bounded separately; never silently make a colliding
    // particle pass through geometry when its collision budget is exhausted.
    const bool collision = d.collision != ParticleCollision::None;
    for (int n = 0; n < count; ++n) {
        if (pool.count == pool.particles.size()
                || (collision && pool.collidingCount >= pool.collisionCapacity)) {
            pool.diagnostics.droppedBirths += static_cast<uint64_t>(count - n);
            if (!pool.overflowWarned) {
                std::fprintf(stderr, "[Particles WARNING] Particle/collision capacity reached; dropping births.\n");
                pool.overflowWarned = true;
            }
            break;
        }
        auto& rng = emitter.randomState;
        const Vector3 direction = Vector3Normalize(d.direction);
        const Vector3 right = Vector3Normalize(Vector3CrossProduct(direction,
                std::fabs(direction.y) < 0.95f ? Vector3{0, 1, 0} : Vector3{0, 0, 1}));
        const Vector3 up = Vector3CrossProduct(right, direction);
        Vector3 offset{};
        if (d.shape == ParticleShape::Box) {
            offset = {(Random(rng) - 0.5f) * d.dimensions.x,
                    (Random(rng) - 0.5f) * d.dimensions.y,
                    (Random(rng) - 0.5f) * d.dimensions.z};
        } else if (d.shape == ParticleShape::Disc) {
            const float angle = Random(rng) * 2 * PI;
            const float radius = std::sqrt(Random(rng)) * 0.5f;
            offset = Vector3Add(Vector3Scale(right, std::cos(angle) * radius * d.dimensions.x),
                    Vector3Scale(up, std::sin(angle) * radius * d.dimensions.z));
        }
        const float angle = Random(rng) * 2 * PI;
        const float spread = std::sqrt(Random(rng)) * d.spread;
        const Vector3 radial = Vector3Add(Vector3Scale(right, std::cos(angle)),
                Vector3Scale(up, std::sin(angle)));
        Particle p;
        p.position = Vector3Add(d.position, offset);
        p.velocity = Vector3Scale(Vector3Normalize(Vector3Add(direction,
                Vector3Scale(radial, spread))), layer.speed * (0.7f + Random(rng) * 0.6f));
        p.lifetime = std::max(0.02f, layer.lifetime * (0.7f + Random(rng) * 0.6f));
        p.size = layer.size * (0.7f + Random(rng) * 0.6f);
        p.rotation = layer.textureRole == 0 ? (Random(rng) - 0.5f) * 0.5f : Random(rng) * 2 * PI;
        p.phase = Random(rng) * 2 * PI;
        p.emitter = static_cast<uint32_t>(emitterIndex);
        p.layer = static_cast<uint8_t>(layerIndex);
        p.collision = collision;
        pool.particles[pool.count++] = p;
        if (collision) ++pool.collidingCount;
    }
}
float SimulationScale(const ParticleEmitterDefinition& definition, bool prewarm)
{
    // Prewarm describes simulation seconds, so playback does not change initial fullness.
    if (prewarm || !std::isfinite(definition.timeScale)) return 1.0f;
    return std::clamp(definition.timeScale, 0.0f, 4.0f);
}
float SimulationStep(const ParticleEmitterDefinition& definition, bool prewarm, int substep)
{
    return Step * std::clamp(SimulationScale(definition, prewarm) - substep, 0.0f, 1.0f);
}
void TickSubstep(ParticlePool& pool, const ParticleUpdateContext& context, bool prewarm, int substep)
{
    for (auto& emitter : pool.emitters)
        emitter.time += SimulationStep(emitter.definition, prewarm, substep);
    for (size_t i = 0; i < pool.count;) {
        Particle& p = pool.particles[i];
        const auto& emitter = pool.emitters[p.emitter];
        const auto& d = emitter.definition;
        const float dt = SimulationStep(d, prewarm, substep);
        if (dt == 0) { ++i; continue; }
        const auto& layer = d.layers[p.layer];
        p.age += dt;
        p.lightingAge += dt;
        const float t = emitter.time + p.phase;
        const Vector3 offset = Vector3Subtract(p.position, d.position);
        const Vector3 swirl = Vector3Scale(Vector3CrossProduct(d.direction, offset),
                d.swirl / (1.0f + Vector3Length(offset)));
        const Vector3 noise{std::sin(t * 1.73f + p.position.z),
                std::sin(t * 1.31f + p.position.x) * 0.35f,
                std::cos(t * 1.91f + p.position.y)};
        const Vector3 force = Vector3Add(Vector3Scale(noise, d.turbulence), swirl);
        p.velocity = Vector3Add(p.velocity, Vector3Scale(force, dt));
        p.velocity.y += layer.gravity * dt;
        p.velocity = Vector3Scale(p.velocity, std::exp(-layer.drag * dt));
        Vector3 next = Vector3Add(p.position, Vector3Scale(Vector3Add(p.velocity, d.drift), dt));
        if (p.collision && context.trace != nullptr && p.age < p.lifetime) {
            ++pool.diagnostics.collisionQueries;
            const auto hit = context.trace(context.user, p.position, next);
            if (hit.hit) {
                if (d.collision == ParticleCollision::Die) p.age = p.lifetime;
                else {
                    p.velocity = Vector3Scale(Vector3Subtract(p.velocity,
                            Vector3Scale(hit.normal, 2 * Vector3DotProduct(p.velocity, hit.normal))), d.restitution);
                    next = Vector3Add(hit.position, Vector3Scale(hit.normal, 0.003f));
                }
            }
        }
        p.position = next;
        p.rotation += layer.spin * dt;
        if (p.age >= p.lifetime) {
            if (p.collision) --pool.collidingCount;
            p = pool.particles[--pool.count];
        } else ++i;
    }
    for (size_t i = 0; i < pool.emitters.size(); ++i) {
        auto& emitter = pool.emitters[i];
        const auto& d = emitter.definition;
        if (!emitter.enabled) { emitter.pendingBurst = 0; continue; }
        const float dt = SimulationStep(d, prewarm, substep);
        if (dt == 0) continue;
        float lod = 1;
        if (context.distanceLod) {
            const float distance = Vector3Distance(context.camera, d.position);
            lod = std::clamp((d.maxDistance - distance) / std::max(1.0f, d.maxDistance * 0.4f), 0.0f, 1.0f);
        }
        float burst = prewarm ? 0 : emitter.pendingBurst;
        if (!prewarm) emitter.pendingBurst = 0;
        if (!prewarm && d.emission == ParticleEmission::RepeatingBurst) {
            emitter.burstTimer -= dt;
            if (emitter.burstTimer <= 0) { burst += 1; emitter.burstTimer = std::max(Step, d.burstInterval); }
        }
        for (int layer = 0; layer < d.layerCount; ++layer) {
            const float rate = d.layers[layer].rate;
            float births = d.emission == ParticleEmission::Continuous ? rate * dt : 0;
            births += burst * d.burstCount * rate / std::max(1.0f, d.layers[0].rate);
            births *= emitter.intensity * lod;
            float& remainder = emitter.emissionRemainders[layer];
            remainder += std::clamp(births, 0.0f, 32768.0f);
            const int count = static_cast<int>(remainder);
            remainder -= static_cast<float>(count);
            Emit(pool, i, layer, count);
        }
    }
}
void Tick(ParticlePool& pool, const ParticleUpdateContext& context, bool prewarm)
{
    pool.time += Step;
    int substeps = 1;
    for (const auto& emitter : pool.emitters)
        substeps = std::max(substeps, static_cast<int>(std::ceil(SimulationScale(emitter.definition, prewarm))));
    // At most four passes, retaining particle-before-emission ordering and the
    // original integration step at normal speed. Slow playback still updates every tick.
    for (int substep = 0; substep < substeps; ++substep)
        TickSubstep(pool, context, prewarm, substep);
}
} // namespace

void InitializeParticlePool(ParticlePool& pool, size_t capacity, size_t emitterCount)
{
    pool = {};
    pool.particles.resize(capacity);
    pool.emitters.resize(emitterCount);
}
void ResetParticlePool(ParticlePool& pool)
{
    pool.count = pool.collidingCount = 0;
    pool.accumulator = pool.time = 0;
    pool.diagnostics = {};
    pool.overflowWarned = false;
    for (auto& emitter : pool.emitters) {
        emitter.randomState = emitter.definition.seed ? emitter.definition.seed : 1;
        emitter.emissionRemainders = {};
        emitter.burstTimer = emitter.pendingBurst = 0;
        emitter.time = 0;
    }
}
bool TriggerParticleBurst(ParticlePool& pool, size_t index, float scale)
{
    if (index >= pool.emitters.size() || !pool.emitters[index].enabled
            || !std::isfinite(scale) || scale <= 0 || scale > 100) return false;
    auto& burst = pool.emitters[index].pendingBurst;
    burst = std::min(100.0f, burst + scale);
    return true;
}
void UpdateParticles(ParticlePool& pool, float dt, const ParticleUpdateContext& context)
{
    const auto start = std::chrono::steady_clock::now();
    pool.diagnostics.collisionQueries = 0;
    if (std::isfinite(dt) && dt > 0) {
        pool.accumulator += std::min(dt, Step * 4);
        int steps = 0;
        while (pool.accumulator >= Step && steps++ < 4) {
            Tick(pool, context, false);
            pool.accumulator -= Step;
        }
    }
    pool.diagnostics.active = pool.count;
    pool.diagnostics.updateMilliseconds = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
}
void PrewarmParticles(ParticlePool& pool, const ParticleUpdateContext& context, float seconds)
{
    for (int i = 0; i < static_cast<int>(std::clamp(seconds, 0.0f, 10.0f) / Step); ++i)
        Tick(pool, context, true);
    pool.diagnostics.active = pool.count;
}
float ParticleOpacity(const Particle& p, const ParticleLayer& layer)
{
    const float t = p.age / p.lifetime;
    return layer.opacity * std::min(1.0f, t * 8.0f) * std::pow(std::max(0.0f, 1 - t), 1.2f);
}
float ParticleSize(const Particle& p, const ParticleLayer& layer)
{
    return p.size * (1 + layer.growth * p.age / p.lifetime);
}
int ParticleFlipbookFrame(float age, float fps, int frames, float phase)
{
    if (frames <= 1 || !std::isfinite(age) || !std::isfinite(fps) || !std::isfinite(phase)) return 0;
    const double frame = std::floor(std::max(0.0, static_cast<double>(age) * fps + phase * frames));
    return static_cast<int>(std::fmod(frame, frames));
}
} // namespace engine
