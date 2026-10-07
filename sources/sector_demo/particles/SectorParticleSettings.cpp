#include "sector_demo/particles/SectorParticleJson.h"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace game {
const char* SectorParticlePresetName(SectorParticlePreset preset)
{
    static constexpr const char* Names[]{"Fire", "Smoke", "Vapor", "Mist Swirl", "Steam Jet", "Electric Sparks"};
    const int i = static_cast<int>(preset);
    return i >= 0 && i < 6 ? Names[i] : "Unknown";
}
const char* SectorParticleTexturePath(int role)
{
    static constexpr const char* Paths[]{"assets/particles/flame.png", "assets/particles/smoke.png",
            "assets/particles/vapor.png", "assets/particles/spark.png"};
    return Paths[std::clamp(role, 0, 3)];
}
SectorParticleSettings MakeSectorParticlePreset(SectorParticlePreset preset)
{
    SectorParticleSettings s;
    s.preset = preset;
    s.presetName = SectorParticlePresetName(preset);
    switch (preset) {
        case SectorParticlePreset::Fire: break;
        case SectorParticlePreset::Smoke: s.tint = {125, 130, 137, 255}; s.turbulence = 0.25f; break;
        case SectorParticlePreset::Vapor: s.turbulence = 0.2f; s.swirl = 0.3f; break;
        case SectorParticlePreset::MistSwirl:
            s.shape = engine::ParticleShape::Box; s.dimensions = {3, 0.3f, 3};
            s.speed = 0.2f; s.swirl = 1.2f; s.turbulence = 0.2f; break;
        case SectorParticlePreset::SteamJet: s.spread = 0.08f; s.dimensions = {0.05f, 0.05f, 0.05f}; break;
        case SectorParticlePreset::ElectricSparks:
            s.emission = engine::ParticleEmission::RepeatingBurst; s.spread = 0.8f;
            s.gravity = -5; s.drag = 0.1f; s.collision = engine::ParticleCollision::Bounce;
            s.shape = engine::ParticleShape::Point; break;
    }
    return s;
}
bool ValidateSectorParticleSettings(const SectorParticleSettings& s, std::string& error)
{
    const auto range = [](float n, float lo, float hi) { return std::isfinite(n) && n >= lo && n <= hi; };
    if (static_cast<int>(s.preset) < 0 || static_cast<int>(s.preset) > 5
            || static_cast<int>(s.shape) < 0 || static_cast<int>(s.shape) > 2
            || static_cast<int>(s.emission) < 0 || static_cast<int>(s.emission) > 2
            || static_cast<int>(s.collision) < 0 || static_cast<int>(s.collision) > 2
            || s.presetName.empty() || s.presetName.size() > 63
            || !range(s.scale, 0.01f, 100) || !range(s.intensity, 0, 100)
            || !range(s.speed, 0, 100) || !range(s.lifetime, 0.01f, 20)
            || !range(s.spread, 0, 10) || !range(s.turbulence, 0, 20)
            || !range(s.swirl, -20, 20) || !range(s.gravity, -100, 100)
            || !range(s.drag, 0, 20) || !range(s.smokeAmount, 0, 10)
            || !range(s.emberAmount, 0, 10) || !range(s.burstInterval, 0.02f, 3600)
            || !range(s.burstCount, 1, 1024) || !range(s.restitution, 0, 1)
            || !range(s.maxDistance, 1, 1000)
            || !range(s.dimensions.x, 0, 100) || !range(s.dimensions.y, 0, 100)
            || !range(s.dimensions.z, 0, 100) || !range(s.drift.x, -100, 100)
            || !range(s.drift.y, -100, 100) || !range(s.drift.z, -100, 100)) {
        error = "Particle settings contain an invalid value or out-of-range parameter"; return false;
    }
    for (const auto& t : s.textures) {
        if (t.columns < 1 || t.columns > 64 || t.rows < 1 || t.rows > 64
                || t.frames < 1 || t.frames > t.columns * t.rows || !range(t.fps, 0, 240)) {
            error = "Flipbook frames must fit a 1-64 column/row grid; FPS must be 0-240"; return false;
        }
        if (!t.path.empty()) {
            const std::filesystem::path p(t.path);
            if (t.path.rfind("assets/", 0) != 0 || p.is_absolute()
                    || p.lexically_normal().generic_string() != t.path
                    || t.path.find('\\') != std::string::npos || t.path.size() > 511) {
                error = "Particle texture paths must be normalized paths below assets/"; return false;
            }
        }
    }
    error.clear(); return true;
}
engine::ParticleEmitterDefinition CompileSectorParticleDefinition(const SectorCompiledParticleEmitter& e)
{
    const auto& s = e.settings;
    engine::ParticleEmitterDefinition d;
    d.position = e.positionWorld;
    const float yaw = e.yawDegrees * DEG2RAD, pitch = e.pitchDegrees * DEG2RAD;
    d.direction = {std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
    d.dimensions = Vector3Scale(s.dimensions, s.scale);
    d.shape = s.shape; d.collision = s.collision; d.emission = s.emission;
    d.drift = s.drift; d.spread = s.spread; d.turbulence = s.turbulence;
    d.swirl = s.swirl; d.burstInterval = s.burstInterval; d.burstCount = s.burstCount;
    d.restitution = s.restitution; d.maxDistance = s.maxDistance;
    d.seed = s.seed ^ (static_cast<uint32_t>(e.sourceAuthoringEmitterId) * 2654435761u);
    auto& p = d.layers[0];
    p.gravity = s.gravity; p.drag = s.drag;
    switch (s.preset) {
        case SectorParticlePreset::Fire:
            p.rate = 24; p.lifetime = 0.8f; p.speed = 1.0f; p.size = 0.65f;
            p.growth = -0.4f; p.opacity = 0.8f; p.emissive = 2.5f; p.spin = 0.08f;
            d.layerCount = 3;
            d.layers[1] = p; d.layers[1].textureRole = 1; d.layers[1].rate = 12 * s.smokeAmount;
            d.layers[1].lifetime = 2.8f; d.layers[1].size = 0.5f; d.layers[1].growth = 2;
            d.layers[1].opacity = 0.23f; d.layers[1].emissive = 0;
            d.layers[1].color = {0.32f, 0.33f, 0.35f}; d.layers[1].spin = 0.25f;
            d.layers[2] = p; d.layers[2].textureRole = 3; d.layers[2].rate = 10 * s.emberAmount;
            d.layers[2].size = 0.05f; d.layers[2].speed = 1.7f; d.layers[2].lifetime = 1.2f;
            d.layers[2].streak = true; break;
        case SectorParticlePreset::Smoke:
            p.textureRole = 1; p.rate = 12; p.lifetime = 4; p.speed = 0.65f;
            p.size = 0.7f; p.growth = 2; p.opacity = 0.35f; break;
        case SectorParticlePreset::Vapor:
            p.textureRole = 2; p.rate = 14; p.lifetime = 2.6f; p.speed = 0.5f;
            p.size = 0.8f; p.growth = 1.4f; p.opacity = 0.25f; break;
        case SectorParticlePreset::MistSwirl:
            p.textureRole = 2; p.rate = 18; p.lifetime = 5; p.speed = 0.3f;
            p.size = 1.2f; p.growth = 0.8f; p.opacity = 0.16f; break;
        case SectorParticlePreset::SteamJet:
            p.textureRole = 2; p.rate = 40; p.lifetime = 1.1f; p.speed = 3.0f;
            p.size = 0.25f; p.growth = 3; p.opacity = 0.35f; break;
        case SectorParticlePreset::ElectricSparks:
            p.textureRole = 3; p.rate = 25; p.lifetime = 0.65f; p.speed = 4;
            p.size = 0.08f; p.growth = -0.5f; p.opacity = 1; p.emissive = 5;
            p.streak = true; break;
    }
    const Vector3 tint{std::pow(s.tint.r / 255.0f, 2.2f), std::pow(s.tint.g / 255.0f, 2.2f), std::pow(s.tint.b / 255.0f, 2.2f)};
    for (int i = 0; i < d.layerCount; ++i) {
        auto& layer = d.layers[i];
        layer.size *= s.scale; layer.speed *= s.speed * s.scale;
        layer.lifetime *= s.lifetime;
        layer.color = Vector3Multiply(layer.color, tint);
        layer.opacity *= s.tint.a / 255.0f;
    }
    return d;
}
nlohmann::ordered_json WriteSectorParticleSettings(const SectorParticleSettings& s)
{
    std::string error;
    if (!ValidateSectorParticleSettings(s, error)) throw std::runtime_error(error);
    nlohmann::ordered_json j;
    j["preset"] = static_cast<int>(s.preset); j["presetName"] = s.presetName;
    j["tint"] = {s.tint.r, s.tint.g, s.tint.b, s.tint.a};
    j["shape"] = static_cast<int>(s.shape); j["emission"] = static_cast<int>(s.emission);
    j["collision"] = static_cast<int>(s.collision);
    j["dimensions"] = {s.dimensions.x, s.dimensions.y, s.dimensions.z};
    j["drift"] = {s.drift.x, s.drift.y, s.drift.z};
#define WRITE_FIELD(name) j[#name] = s.name
    WRITE_FIELD(scale); WRITE_FIELD(intensity); WRITE_FIELD(speed); WRITE_FIELD(lifetime);
    WRITE_FIELD(spread); WRITE_FIELD(turbulence); WRITE_FIELD(swirl); WRITE_FIELD(gravity);
    WRITE_FIELD(drag); WRITE_FIELD(smokeAmount); WRITE_FIELD(emberAmount);
    WRITE_FIELD(burstInterval); WRITE_FIELD(burstCount); WRITE_FIELD(restitution);
    WRITE_FIELD(maxDistance); WRITE_FIELD(seed);
#undef WRITE_FIELD
    j["textures"] = nlohmann::ordered_json::array();
    for (const auto& t : s.textures) j["textures"].push_back({{"path", t.path},
            {"columns", t.columns}, {"rows", t.rows}, {"frames", t.frames},
            {"fps", t.fps}, {"randomStart", t.randomStart}});
    return j;
}
SectorParticleSettings ReadSectorParticleSettings(const nlohmann::ordered_json& j)
{
    if (!j.is_object()) throw std::runtime_error("Particle settings must be an object");
    const int preset = j.value("preset", 0);
    if (preset < 0 || preset > 5) throw std::runtime_error("Unknown particle preset type");
    auto s = MakeSectorParticlePreset(static_cast<SectorParticlePreset>(preset));
    s.presetName = j.value("presetName", s.presetName);
    if (j.contains("tint")) {
        const auto& t = j.at("tint");
        if (!t.is_array() || t.size() != 4) throw std::runtime_error("Particle tint needs RGBA");
        unsigned char* channels[]{&s.tint.r, &s.tint.g, &s.tint.b, &s.tint.a};
        for (int i = 0; i < 4; ++i) {
            const int n = t.at(i).get<int>();
            if (n < 0 || n > 255) throw std::runtime_error("Particle tint channel outside 0-255");
            *channels[i] = static_cast<unsigned char>(n);
        }
    }
    s.shape = static_cast<engine::ParticleShape>(j.value("shape", static_cast<int>(s.shape)));
    s.emission = static_cast<engine::ParticleEmission>(j.value("emission", static_cast<int>(s.emission)));
    s.collision = static_cast<engine::ParticleCollision>(j.value("collision", static_cast<int>(s.collision)));
    const auto vector = [&](const char* name, Vector3 fallback) {
        if (!j.contains(name)) return fallback;
        const auto& v = j.at(name);
        if (!v.is_array() || v.size() != 3) throw std::runtime_error("Particle vector needs three values");
        return Vector3{v.at(0).get<float>(), v.at(1).get<float>(), v.at(2).get<float>()};
    };
    s.dimensions = vector("dimensions", s.dimensions); s.drift = vector("drift", s.drift);
#define READ_FIELD(name) s.name = j.value(#name, s.name)
    READ_FIELD(scale); READ_FIELD(intensity); READ_FIELD(speed); READ_FIELD(lifetime);
    READ_FIELD(spread); READ_FIELD(turbulence); READ_FIELD(swirl); READ_FIELD(gravity);
    READ_FIELD(drag); READ_FIELD(smokeAmount); READ_FIELD(emberAmount);
    READ_FIELD(burstInterval); READ_FIELD(burstCount); READ_FIELD(restitution);
    READ_FIELD(maxDistance); READ_FIELD(seed);
#undef READ_FIELD
    if (j.contains("textures")) {
        const auto& ts = j.at("textures");
        if (!ts.is_array() || ts.size() > 4) throw std::runtime_error("Particle textures supports four roles");
        for (size_t i = 0; i < ts.size(); ++i) {
            const auto& v = ts.at(i); auto& t = s.textures[i];
            t.path = v.value("path", t.path); t.columns = v.value("columns", t.columns);
            t.rows = v.value("rows", t.rows); t.frames = v.value("frames", t.frames);
            t.fps = v.value("fps", t.fps); t.randomStart = v.value("randomStart", t.randomStart);
        }
    }
    std::string error;
    if (!ValidateSectorParticleSettings(s, error)) throw std::runtime_error(error);
    return s;
}
bool LoadSectorParticlePresets(const std::string& path, std::vector<SectorParticleSettings>& presets, std::string& error)
{
    try {
        std::vector<SectorParticleSettings> candidate;
        for (int i = 0; i < 6; ++i) candidate.push_back(MakeSectorParticlePreset(static_cast<SectorParticlePreset>(i)));
        if (std::filesystem::exists(path)) {
            std::ifstream input(path);
            auto j = nlohmann::ordered_json::parse(input);
            if (!j.is_array()) throw std::runtime_error("Preset library must be an array");
            for (const auto& value : j) {
                auto preset = ReadSectorParticleSettings(value);
                const auto duplicate = std::find_if(candidate.begin(), candidate.end(), [&](const auto& p) { return p.presetName == preset.presetName; });
                if (duplicate != candidate.end()) throw std::runtime_error("Duplicate or reserved preset name: " + preset.presetName);
                candidate.push_back(std::move(preset));
            }
        }
        presets = std::move(candidate); error.clear(); return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
bool SaveSectorParticlePreset(const std::string& path, const SectorParticleSettings& preset, std::string& error)
{
    std::vector<SectorParticleSettings> presets;
    if (!LoadSectorParticlePresets(path, presets, error)) return false;
    for (int i = 0; i < 6; ++i) if (preset.presetName == SectorParticlePresetName(static_cast<SectorParticlePreset>(i))) {
        error = "Choose a custom name; built-in presets are read-only"; return false;
    }
    if (!ValidateSectorParticleSettings(preset, error)) return false;
    const auto existing = std::find_if(presets.begin() + 6, presets.end(), [&](const auto& p) { return p.presetName == preset.presetName; });
    if (existing == presets.end()) presets.push_back(preset); else *existing = preset;
    try {
        auto j = nlohmann::ordered_json::array();
        for (size_t i = 6; i < presets.size(); ++i) j.push_back(WriteSectorParticleSettings(presets[i]));
        const std::filesystem::path target(path), temp(path + ".tmp");
        if (!target.parent_path().empty()) std::filesystem::create_directories(target.parent_path());
        { std::ofstream output(temp); output << j.dump(2) << '\n'; output.close();
          if (!output) throw std::runtime_error("Could not write particle presets"); }
        std::filesystem::rename(temp, target);
        error.clear(); return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
} // namespace game
