#include "engine/particles/ParticleSystem.h"
#include "sector_demo/particles/SectorParticleJson.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <raymath.h>

namespace {
engine::ParticlePool MakePool(size_t capacity = 256)
{
    engine::ParticlePool p;
    engine::InitializeParticlePool(p, capacity, 1);
    auto& d = p.emitters[0].definition;
    d.layers[0].rate = 60; d.layers[0].lifetime = 2; d.seed = 42;
    engine::ResetParticlePool(p);
    return p;
}
void Simulation()
{
    auto a = MakePool(), b = MakePool();
    const auto capacity = a.particles.capacity();
    for (int i = 0; i < 120; ++i) engine::UpdateParticles(a,1.0f/60,{});
    for (int i = 0; i < 60; ++i) engine::UpdateParticles(b,1.0f/30,{});
    assert(a.count == b.count && a.count > 60);
    for (size_t i = 0; i < a.count; ++i) {
        assert(a.particles[i].position.x == b.particles[i].position.x);
        assert(a.particles[i].position.y == b.particles[i].position.y);
    }
    assert(a.particles.capacity() == capacity);
    auto stalled = MakePool();
    engine::UpdateParticles(stalled,1000,{});
    assert(stalled.count <= 4);
    engine::UpdateParticles(stalled,std::numeric_limits<float>::quiet_NaN(),{});
    assert(stalled.count <= 4);
    a.emitters[0].enabled = false;
    assert(!engine::TriggerParticleBurst(a,0,1));
    for (int i = 0; i < 200; ++i) engine::UpdateParticles(a,1.0f/60,{});
    assert(a.count == 0);
    auto limited = MakePool(2);
    for (int i = 0; i < 30; ++i) engine::UpdateParticles(limited,1.0f/60,{});
    assert(limited.count == 2 && limited.diagnostics.droppedBirths > 0);
    auto burst = MakePool(); burst.emitters[0].definition.emission = engine::ParticleEmission::TriggeredBurst;
    engine::PrewarmParticles(burst,{}); assert(burst.count == 0);
    assert(engine::TriggerParticleBurst(burst,0,1));
    engine::UpdateParticles(burst,1.0f/60,{}); assert(burst.count == 20);
    engine::UpdateParticles(burst,1.0f/60,{}); assert(burst.count == 20);
    assert(!engine::TriggerParticleBurst(burst,0,-1));
    assert(engine::ParticleFlipbookFrame(0.5f,10,4,0) == 1);
    assert(engine::ParticleFlipbookFrame(0,0,4,0.75f) == 3);
    engine::Particle p; engine::ParticleLayer l;
    p.age = p.lifetime;
    assert(engine::ParticleOpacity(p,l) == 0);
}
void Collision()
{
    auto p = MakePool(16); p.collisionCapacity = 2;
    auto& d = p.emitters[0].definition;
    d.collision = engine::ParticleCollision::Bounce;
    d.direction = {0,-1,0}; d.position = {0,0.02f,0}; d.shape = engine::ParticleShape::Point;
    engine::ParticleUpdateContext context;
    context.trace = [](void*,Vector3 a,Vector3 b) -> engine::ParticleHit {
        if (b.y < 0) return {true,{a.x,0,a.z},{0,1,0}};
        return {};
    };
    for (int i = 0; i < 10; ++i) engine::UpdateParticles(p,1.0f/60,context);
    assert(p.count <= 2 && p.collidingCount == p.count && p.diagnostics.collisionQueries <= 2);
    for (size_t i = 0; i < p.count; ++i) assert(p.particles[i].position.y >= 0);
    d.collision = engine::ParticleCollision::Die;
    for (int i = 0; i < 10; ++i) engine::UpdateParticles(p,1.0f/60,context);
    for (size_t i = 0; i < p.count; ++i) assert(p.particles[i].position.y >= 0);
}
void Advance(engine::ParticlePool& pool, int ticks, const engine::ParticleUpdateContext& context = {})
{
    for (int i = 0; i < ticks; ++i) engine::UpdateParticles(pool, 1.0f / 60, context);
}
void Playback()
{
    // Identical prewarmed snapshots include established trajectories rather than
    // comparing particles born on different discrete emission boundaries.
    auto normal = MakePool(1024);
    game::SectorCompiledParticleEmitter source;
    source.settings = game::MakeSectorParticlePreset(game::SectorParticlePreset::MistSwirl);
    source.settings.drag = 3.9f;
    source.settings.swirl = 0.2f;
    source.settings.drift = {0.1f, 0.05f, -0.1f};
    source.settings.gravity = -0.1f;
    normal.emitters[0].definition = game::CompileSectorParticleDefinition(source);
    engine::ResetParticlePool(normal);
    engine::PrewarmParticles(normal, {}, 1);
    auto slow = normal;
    normal.emitters[0].enabled = slow.emitters[0].enabled = false;
    slow.emitters[0].definition.timeScale = 0.5f;
    Advance(normal, 60); Advance(slow, 120);
    assert(normal.count == slow.count && normal.count > 0);
    assert(std::fabs(normal.emitters[0].time - slow.emitters[0].time) < 0.0001f);
    for (size_t i = 0; i < normal.count; ++i) {
        const auto& a = normal.particles[i]; const auto& b = slow.particles[i];
        const auto& layer = normal.emitters[0].definition.layers[a.layer];
        assert(Vector3Distance(a.position, b.position) < 0.01f);
        assert(Vector3Distance(a.velocity, b.velocity) < 0.01f);
        assert(std::fabs(a.rotation - b.rotation) < 0.0001f);
        assert(std::fabs(a.age - b.age) < 0.0001f && a.lifetime == b.lifetime);
        assert(std::fabs(engine::ParticleOpacity(a, layer) - engine::ParticleOpacity(b, layer)) < 0.0001f);
        assert(std::fabs(engine::ParticleSize(a, layer) - engine::ParticleSize(b, layer)) < 0.0001f);
        assert(engine::ParticleFlipbookFrame(a.age, 7, 16, a.phase / (2 * PI))
                == engine::ParticleFlipbookFrame(b.age, 7, 16, b.phase / (2 * PI)));
    }

    auto births = MakePool(1024), slowBirths = MakePool(1024), fastBirths = MakePool(1024);
    slowBirths.emitters[0].definition.timeScale = 0.5f;
    fastBirths.emitters[0].definition.timeScale = 2;
    Advance(births, 60); Advance(slowBirths, 120); Advance(fastBirths, 30);
    assert(births.count == slowBirths.count && births.count == fastBirths.count);
    assert(births.emitters[0].randomState == slowBirths.emitters[0].randomState);
    for (size_t i = 0; i < births.count; ++i) {
        assert(Vector3Distance(births.particles[i].position, slowBirths.particles[i].position) < 0.02f);
        assert(Vector3Distance(births.particles[i].position, fastBirths.particles[i].position) == 0);
    }

    // Two clocks in one pool must not affect each other's motion or births.
    auto mixed = MakePool(1024);
    mixed.emitters.resize(2, mixed.emitters[0]);
    mixed.emitters[0].definition.timeScale = 0.5f;
    mixed.emitters[1].definition.timeScale = 2;
    engine::ResetParticlePool(mixed);
    const auto capacity = mixed.particles.capacity();
    Advance(mixed, 30);
    assert(std::fabs(mixed.emitters[0].time - 0.25f) < 0.0001f);
    assert(std::fabs(mixed.emitters[1].time - 1.0f) < 0.0001f);
    size_t counts[2]{};
    for (size_t i = 0; i < mixed.count; ++i) ++counts[mixed.particles[i].emitter];
    assert(counts[0] == 15 && counts[1] == 60);
    assert(mixed.particles.capacity() == capacity);

    auto frozen = births;
    frozen.emitters[0].definition.timeScale = 0;
    assert(engine::TriggerParticleBurst(frozen, 0, 1));
    Advance(frozen, 120);
    assert(frozen.count == births.count && frozen.emitters[0].time == births.emitters[0].time);
    assert(frozen.emitters[0].pendingBurst == 1);
    assert(frozen.emitters[0].randomState == births.emitters[0].randomState);
    assert(frozen.emitters[0].emissionRemainders == births.emitters[0].emissionRemainders);
    for (size_t i = 0; i < frozen.count; ++i) {
        assert(frozen.particles[i].age == births.particles[i].age);
        assert(frozen.particles[i].rotation == births.particles[i].rotation);
        assert(Vector3Distance(frozen.particles[i].position, births.particles[i].position) == 0);
    }
    frozen.emitters[0].definition.timeScale = 1;
    Advance(frozen, 1);
    assert(frozen.emitters[0].pendingBurst == 0 && frozen.count == births.count + 21);
    frozen.emitters[0].definition.timeScale = 0;
    assert(engine::TriggerParticleBurst(frozen, 0, 1));
    frozen.emitters[0].enabled = false;
    Advance(frozen, 1);
    assert(frozen.emitters[0].pendingBurst == 0);
    frozen.emitters[0].definition.timeScale = 0.5f;
    Advance(frozen, 360);
    assert(frozen.count == 0);

    // Prewarming is independent of playback, including a frozen authored effect.
    for (float scale : {0.0f, 0.5f, 1.0f, 4.0f}) {
        auto a = MakePool(), b = MakePool();
        b.emitters[0].definition.timeScale = scale;
        engine::PrewarmParticles(a, {}, 1); engine::PrewarmParticles(b, {}, 1);
        assert(a.count == b.count && a.emitters[0].time == b.emitters[0].time);
        for (size_t i = 0; i < a.count; ++i)
            assert(Vector3Distance(a.particles[i].position, b.particles[i].position) == 0);
        engine::ResetParticlePool(b);
        assert(b.emitters[0].time == 0 && b.count == 0);
    }

    for (auto emission : {engine::ParticleEmission::RepeatingBurst, engine::ParticleEmission::TriggeredBurst}) {
        auto a = MakePool(1024), b = MakePool(1024);
        a.emitters[0].definition.emission = b.emitters[0].definition.emission = emission;
        a.emitters[0].definition.burstInterval = b.emitters[0].definition.burstInterval = 0.5f;
        a.emitters[0].definition.layers[0].lifetime = b.emitters[0].definition.layers[0].lifetime = 10;
        b.emitters[0].definition.timeScale = 0.5f;
        if (emission == engine::ParticleEmission::TriggeredBurst) {
            assert(engine::TriggerParticleBurst(a, 0, 1));
            assert(engine::TriggerParticleBurst(b, 0, 1));
        }
        Advance(a, 50); Advance(b, 100);
        assert(a.count == b.count && a.count > 0);
        assert(a.emitters[0].pendingBurst == 0 && b.emitters[0].pendingBurst == 0);
    }

    // Accelerated playback still traces bounded integration segments.
    auto fast = MakePool(16), reference = MakePool(16);
    fast.emitters[0].definition.collision = reference.emitters[0].definition.collision = engine::ParticleCollision::Bounce;
    fast.emitters[0].definition.direction = reference.emitters[0].definition.direction = {0, -1, 0};
    fast.emitters[0].definition.position = reference.emitters[0].definition.position = {0, 0.02f, 0};
    fast.emitters[0].definition.timeScale = 4;
    engine::ParticleUpdateContext context;
    context.trace = [](void*, Vector3 a, Vector3 b) -> engine::ParticleHit {
        if (b.y < 0) return {true, {a.x, 0, a.z}, {0, 1, 0}};
        return {};
    };
    Advance(fast, 10, context); Advance(reference, 40, context);
    assert(fast.count == reference.count && fast.diagnostics.droppedBirths > 0);
    for (size_t i = 0; i < fast.count; ++i)
        assert(Vector3Distance(fast.particles[i].position, reference.particles[i].position) == 0);
    assert(fast.diagnostics.collisionQueries <= 4 * fast.count);
    auto stalled = MakePool(1024); stalled.emitters[0].definition.timeScale = 4;
    engine::UpdateParticles(stalled, 1000, {});
    assert(stalled.count <= 16);
}
void Settings()
{
    for (int i = 0; i < 6; ++i) {
        auto s = game::MakeSectorParticlePreset(static_cast<game::SectorParticlePreset>(i));
        assert(s.timeScale == 1);
        s.textures[1] = {"assets/particles/custom.png",4,4,16,24,true};
        auto j = game::WriteSectorParticleSettings(s);
        assert(game::WriteSectorParticleSettings(game::ReadSectorParticleSettings(j)) == j);
        j.erase("timeScale");
        assert(game::ReadSectorParticleSettings(j).timeScale == 1);
        for (float scale : {0.0f, 0.5f, 1.0f, 4.0f}) {
            s.timeScale = scale;
            const auto restored = game::ReadSectorParticleSettings(game::WriteSectorParticleSettings(s));
            assert(restored.timeScale == scale);
            game::SectorCompiledParticleEmitter emitter; emitter.settings = restored;
            assert(game::CompileSectorParticleDefinition(emitter).timeScale == scale);
        }
        for (float scale : {-1.0f, 4.01f, std::numeric_limits<float>::infinity(),
                std::numeric_limits<float>::quiet_NaN()}) {
            s.timeScale = scale;
            std::string error; assert(!game::ValidateSectorParticleSettings(s, error));
            j["timeScale"] = scale;
            bool rejected = false;
            try { game::ReadSectorParticleSettings(j); } catch (const std::exception&) { rejected = true; }
            assert(rejected);
        }
        s.timeScale = 1;
        s.scale = std::numeric_limits<float>::infinity();
        std::string error; assert(!game::ValidateSectorParticleSettings(s,error));
    }
    std::string error;
    auto s = game::MakeSectorParticlePreset(game::SectorParticlePreset::Smoke);
    s.textures[0].path = "assets/../../outside.png";
    assert(!game::ValidateSectorParticleSettings(s,error));
    s.textures[0].path.clear(); s.textures[0].frames = 2;
    assert(!game::ValidateSectorParticleSettings(s,error));
    const auto path = std::filesystem::temp_directory_path() / "engine_particle_presets_test.json";
    std::filesystem::remove(path);
    s = game::MakeSectorParticlePreset(game::SectorParticlePreset::Fire);
    assert(!game::SaveSectorParticlePreset(path.string(),s,error));
    s.presetName = "Furnace"; s.timeScale = 0.5f;
    assert(game::SaveSectorParticlePreset(path.string(),s,error));
    auto placement = s; s.intensity = 3;
    assert(game::SaveSectorParticlePreset(path.string(),s,error));
    std::vector<game::SectorParticleSettings> presets;
    assert(game::LoadSectorParticlePresets(path.string(),presets,error));
    assert(presets.size() == 7 && presets.back().intensity == 3 && placement.intensity == 1);
    assert(presets.back().timeScale == 0.5f);
    std::filesystem::remove(path);
}
void Benchmark()
{
    auto p = MakePool(8192); p.emitters[0].definition.layers[0].rate = 3000;
    p.emitters[0].definition.layers[0].lifetime = 5;
    engine::PrewarmParticles(p,{},3);
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 300; ++i) engine::UpdateParticles(p,1.0f/60,{});
    const double ms = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/300;
    std::cout << "8192-particle simulation benchmark: " << ms << " ms/step, " << p.count << " active\n";
}
}
int main(int argc, char**)
{
    Simulation(); Collision(); Playback(); Settings();
    if (argc > 1) Benchmark();
    std::cout << "Particle tests passed\n";
}
