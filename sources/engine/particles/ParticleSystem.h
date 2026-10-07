#pragma once

#include <raylib.h>
#include <array>
#include <cstdint>
#include <vector>

namespace engine {

enum class ParticleShape { Point, Disc, Box };
enum class ParticleCollision { None, Die, Bounce };
enum class ParticleEmission { Continuous, RepeatingBurst, TriggeredBurst };

struct ParticleLayer {
    float rate = 20.0f;
    float lifetime = 1.0f;
    float speed = 1.0f;
    float size = 0.5f;
    float growth = 1.0f;
    float opacity = 0.5f;
    float emissive = 0.0f;
    float gravity = 0.0f;
    float drag = 0.5f;
    float spin = 0.5f;
    Vector3 color{1, 1, 1};
    int textureRole = 0;
    bool streak = false;
};

struct ParticleEmitterDefinition {
    Vector3 position{};
    Vector3 direction{0, 1, 0};
    Vector3 dimensions{0.2f, 0.2f, 0.2f};
    Vector3 drift{};
    ParticleShape shape = ParticleShape::Disc;
    ParticleCollision collision = ParticleCollision::None;
    ParticleEmission emission = ParticleEmission::Continuous;
    float spread = 0.3f;
    float turbulence = 0.3f;
    float swirl = 0.0f;
    float burstInterval = 1.0f;
    float burstCount = 20.0f;
    float restitution = 0.35f;
    float maxDistance = 50.0f;
    int layerCount = 1;
    std::array<ParticleLayer, 3> layers{};
    uint32_t seed = 1;
};

struct ParticleEmitterState {
    ParticleEmitterDefinition definition;
    bool enabled = true;
    float intensity = 1.0f;
    std::array<float, 3> emissionRemainders{};
    float burstTimer = 0.0f;
    uint32_t randomState = 1;
    // A bounded command accumulator; consumed by the next simulation step.
    float pendingBurst = 0.0f;
};

struct Particle {
    Vector3 position{};
    Vector3 velocity{};
    Vector3 lighting{1, 1, 1};
    float age = 0.0f;
    float lifetime = 1.0f;
    float size = 1.0f;
    float rotation = 0.0f;
    float phase = 0.0f;
    float lightingAge = 1.0f;
    uint32_t emitter = 0;
    uint8_t layer = 0;
    int sectorId = -1;
    bool collision = false;
};

struct ParticleDiagnostics {
    uint64_t droppedBirths = 0;
    uint64_t collisionQueries = 0;
    size_t active = 0;
    size_t visible = 0;
    size_t drawCalls = 0;
    double updateMilliseconds = 0.0;
    double drawCpuMilliseconds = 0.0;
};

struct ParticlePool {
    std::vector<Particle> particles;
    std::vector<ParticleEmitterState> emitters;
    size_t count = 0;
    size_t collidingCount = 0;
    size_t collisionCapacity = 128;
    float accumulator = 0.0f;
    float time = 0.0f;
    bool overflowWarned = false;
    ParticleDiagnostics diagnostics;
};

struct ParticleHit { bool hit = false; Vector3 position{}; Vector3 normal{}; };
struct ParticleUpdateContext {
    void* user = nullptr;
    ParticleHit (*trace)(void*, Vector3 from, Vector3 to) = nullptr;
    // Explicit context, shared by world and isolated previews; no global services.
    Vector3 camera{};
    bool distanceLod = false;
};

void InitializeParticlePool(ParticlePool& pool, size_t capacity, size_t emitterCount);
void ResetParticlePool(ParticlePool& pool);
bool TriggerParticleBurst(ParticlePool& pool, size_t emitter, float scale);
void UpdateParticles(ParticlePool& pool, float dt, const ParticleUpdateContext& context);
void PrewarmParticles(ParticlePool& pool, const ParticleUpdateContext& context, float seconds = 3.0f);
float ParticleOpacity(const Particle& particle, const ParticleLayer& layer);
float ParticleSize(const Particle& particle, const ParticleLayer& layer);
int ParticleFlipbookFrame(float age, float fps, int frames, float randomPhase);

} // namespace engine
