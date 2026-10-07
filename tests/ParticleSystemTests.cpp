#include "engine/particles/ParticleSystem.h"
#include "sector_demo/particles/SectorParticleJson.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>

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
void Settings()
{
    for (int i = 0; i < 6; ++i) {
        auto s = game::MakeSectorParticlePreset(static_cast<game::SectorParticlePreset>(i));
        s.textures[1] = {"assets/particles/custom.png",4,4,16,24,true};
        auto j = game::WriteSectorParticleSettings(s);
        assert(game::WriteSectorParticleSettings(game::ReadSectorParticleSettings(j)) == j);
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
    s.presetName = "Furnace"; assert(game::SaveSectorParticlePreset(path.string(),s,error));
    auto placement = s; s.intensity = 3;
    assert(game::SaveSectorParticlePreset(path.string(),s,error));
    std::vector<game::SectorParticleSettings> presets;
    assert(game::LoadSectorParticlePresets(path.string(),presets,error));
    assert(presets.size() == 7 && presets.back().intensity == 3 && placement.intensity == 1);
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
    Simulation(); Collision(); Settings();
    if (argc > 1) Benchmark();
    std::cout << "Particle tests passed\n";
}
