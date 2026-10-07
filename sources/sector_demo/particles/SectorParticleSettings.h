#pragma once

#include "engine/particles/ParticleSystem.h"
#include "sector_demo/SectorTopologyUnits.h"
#include <array>
#include <string>
#include <vector>

namespace game {
enum class SectorParticlePreset { Fire, Smoke, Vapor, MistSwirl, SteamJet, ElectricSparks };
struct SectorParticleTexture {
    std::string path;
    int columns = 1;
    int rows = 1;
    int frames = 1;
    float fps = 12.0f;
    bool randomStart = true;
};
struct SectorParticleSettings {
    SectorParticlePreset preset = SectorParticlePreset::Fire;
    std::string presetName = "Fire";
    float scale = 1;
    float intensity = 1;
    Color tint = WHITE;
    float speed = 1;
    float lifetime = 1;
    float spread = 0.3f;
    engine::ParticleShape shape = engine::ParticleShape::Disc;
    Vector3 dimensions{0.4f, 0.1f, 0.4f};
    Vector3 drift{};
    float turbulence = 0.5f;
    float swirl = 0.0f;
    float gravity = 0.0f;
    float drag = 0.3f;
    float smokeAmount = 0.5f;
    float emberAmount = 0.3f;
    engine::ParticleEmission emission = engine::ParticleEmission::Continuous;
    float burstInterval = 0.75f;
    float burstCount = 20;
    engine::ParticleCollision collision = engine::ParticleCollision::None;
    float restitution = 0.35f;
    float maxDistance = 50;
    uint32_t seed = 1;
    std::array<SectorParticleTexture, 4> textures;
};
struct SectorAuthoringParticleEmitter {
    int id = -1;
    std::string referenceId;
    SectorCoord x = 0, z = 0;
    float heightWorld = 0.05f;
    float yawDegrees = 0, pitchDegrees = 90;
    bool enabled = true;
    SectorParticleSettings settings;
};
struct SectorCompiledParticleEmitter {
    int sourceAuthoringEmitterId = -1;
    std::string id;
    Vector3 positionWorld{};
    float yawDegrees = 0, pitchDegrees = 90;
    int sectorId = -1;
    bool enabled = true;
    SectorParticleSettings settings;
    // Runtime-only overrides and bounded command state, never level-serialized.
    float runtimeIntensity = 1;
    mutable float pendingBurst = 0;
};
const char* SectorParticlePresetName(SectorParticlePreset preset);
const char* SectorParticleTexturePath(int role);
SectorParticleSettings MakeSectorParticlePreset(SectorParticlePreset preset);
bool ValidateSectorParticleSettings(const SectorParticleSettings& settings, std::string& error);
engine::ParticleEmitterDefinition CompileSectorParticleDefinition(const SectorCompiledParticleEmitter& emitter);
bool LoadSectorParticlePresets(const std::string& path, std::vector<SectorParticleSettings>& presets, std::string& error);
bool SaveSectorParticlePreset(const std::string& path, const SectorParticleSettings& preset, std::string& error);
} // namespace game
