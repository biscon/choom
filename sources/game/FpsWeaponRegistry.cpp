#include "game/FpsWeaponRegistry.h"

#include "util/json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace game {
namespace {

using Json = nlohmann::ordered_json;

[[noreturn]] void Fail(const std::string& message) { throw std::runtime_error(message); }

const Json& Require(const Json& object, const char* name, const std::string& context)
{
    const auto it = object.find(name);
    if (it == object.end()) Fail(context + " is missing required field '" + name + "'");
    return *it;
}

std::string String(const Json& object, const char* name, const std::string& context)
{
    const Json& value = Require(object, name, context);
    if (!value.is_string()) Fail(context + "." + name + " must be a string");
    const std::string result = value.get<std::string>();
    if (result.empty()) Fail(context + "." + name + " must not be empty");
    return result;
}

float Number(const Json& object, const char* name, const std::string& context)
{
    const Json& value = Require(object, name, context);
    if (!value.is_number()) Fail(context + "." + name + " must be a number");
    const double result = value.get<double>();
    if (!std::isfinite(result) || std::abs(result) > std::numeric_limits<float>::max()) {
        Fail(context + "." + name + " must be a finite float");
    }
    return static_cast<float>(result);
}

int Integer(const Json& object, const char* name, const std::string& context)
{
    const Json& value = Require(object, name, context);
    if (!value.is_number_integer()) Fail(context + "." + name + " must be an integer");
    return value.get<int>();
}

bool Boolean(const Json& object, const char* name, const std::string& context)
{
    const Json& value = Require(object, name, context);
    if (!value.is_boolean()) Fail(context + "." + name + " must be a boolean");
    return value.get<bool>();
}

unsigned char ColorChannel(
        const Json& object,
        const char* name,
        const std::string& context)
{
    const int value = Integer(object, name, context);
    if (value < 0 || value > 255) {
        Fail(context + "." + name + " must be between 0 and 255");
    }
    return static_cast<unsigned char>(value);
}

Color ReadColor(const Json& value, const std::string& context)
{
    if (!value.is_object()) Fail(context + " must be an object");
    return Color{
            ColorChannel(value, "r", context),
            ColorChannel(value, "g", context),
            ColorChannel(value, "b", context),
            ColorChannel(value, "a", context)};
}

Vector3 Vector(const Json& object, const char* name, const std::string& context)
{
    const Json& value = Require(object, name, context);
    if (!value.is_array() || value.size() != 3) Fail(context + "." + name + " must contain 3 numbers");
    Vector3 result{};
    float* components[] = {&result.x, &result.y, &result.z};
    for (size_t i = 0; i < 3; ++i) {
        if (!value[i].is_number()) Fail(context + "." + name + " must contain only numbers");
        const double number = value[i].get<double>();
        if (!std::isfinite(number)) Fail(context + "." + name + " must contain finite numbers");
        *components[i] = static_cast<float>(number);
    }
    return result;
}

bool ValidAssetPath(const std::string& path)
{
    const std::filesystem::path parsed(path);
    return path.rfind("assets/", 0) == 0
            && !parsed.is_absolute()
            && path.find("..") == std::string::npos
            && (parsed.extension() == ".glb" || parsed.extension() == ".gltf");
}

bool ValidAudioPath(const std::string& path)
{
    if (path.empty()) return false;
    const std::filesystem::path parsed(path);
    const bool windowsDrivePath = path.size() >= 2
            && std::isalpha(static_cast<unsigned char>(path[0]))
            && path[1] == ':';
    if (parsed.is_absolute()
            || windowsDrivePath
            || path.front() == '\\'
            || path.find('\\') != std::string::npos
            || path.find("..") != std::string::npos) {
        return false;
    }
    std::string extension = parsed.extension().generic_string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
    return extension == ".ogg" || extension == ".wav" || extension == ".mp3";
}

bool ValidLevelName(const std::string& name)
{
    if (name.empty()) {
        return false;
    }
    for (char ch : name) {
        const bool asciiLetter = (ch >= 'A' && ch <= 'Z')
                || (ch >= 'a' && ch <= 'z');
        const bool asciiDigit = ch >= '0' && ch <= '9';
        if (!(asciiLetter || asciiDigit || ch == '_' || ch == '-')) {
            return false;
        }
    }
    return true;
}

bool ValidSoundSetId(const std::string& id)
{
    if (id.empty() || id.front() == '/' || id.front() == '\\') return false;
    std::string segment;
    for (const char character : id) {
        if (character == '\\') return false;
        if (character == '/') {
            if (segment.empty() || segment == "." || segment == "..") return false;
            segment.clear();
            continue;
        }
        const unsigned char value = static_cast<unsigned char>(character);
        if (!std::isalnum(value) && character != '_' && character != '-' && character != '.') {
            return false;
        }
        segment.push_back(character);
    }
    return !segment.empty() && segment != "." && segment != "..";
}

void ValidatePresentation(const FpsViewmodelPresentation& value, const std::string& context)
{
    const float values[] = {value.position.x, value.position.y, value.position.z,
            value.rotationDegrees.x, value.rotationDegrees.y, value.rotationDegrees.z,
            value.scale, value.verticalFovDegrees};
    for (float component : values) if (!std::isfinite(component)) Fail(context + " contains a non-finite value");
    if (value.scale <= 0.0f) Fail(context + ".scale must be greater than zero");
    if (value.verticalFovDegrees <= 1.0f || value.verticalFovDegrees >= 179.0f) {
        Fail(context + ".verticalFovDegrees must be between 1 and 179");
    }
}

void ValidateHolsterTransition(
        const FpsViewmodelHolsterTransition& value,
        const std::string& context)
{
    const float values[] = {
            value.holsterDurationSeconds,
            value.unholsterDurationSeconds,
            value.hiddenTranslation.x,
            value.hiddenTranslation.y,
            value.hiddenTranslation.z,
            value.hiddenRotationDegrees.x,
            value.hiddenRotationDegrees.y,
            value.hiddenRotationDegrees.z};
    for (float component : values) {
        if (!std::isfinite(component)) {
            Fail(context + " contains a non-finite value");
        }
    }
    if (value.holsterDurationSeconds <= 0.0f) {
        Fail(context + ".holsterDurationSeconds must be greater than zero");
    }
    if (value.unholsterDurationSeconds <= 0.0f) {
        Fail(context + ".unholsterDurationSeconds must be greater than zero");
    }
}

void ValidateGripCorrection(
        const FpsViewmodelGripCorrection& value,
        const std::string& context)
{
    const float values[] = {
            value.translation.x, value.translation.y, value.translation.z,
            value.rotationDegrees.x, value.rotationDegrees.y,
            value.rotationDegrees.z, value.scale};
    for (float component : values) {
        if (!std::isfinite(component)) {
            Fail(context + " contains a non-finite value");
        }
    }
    if (value.scale <= 0.0f) {
        Fail(context + ".scale must be greater than zero");
    }
}

FpsViewmodelPresentation ReadPresentation(const Json& object, const std::string& context)
{
    FpsViewmodelPresentation result;
    result.position = Vector(object, "position", context);
    result.rotationDegrees = Vector(object, "rotationDegrees", context);
    result.scale = Number(object, "scale", context);
    result.verticalFovDegrees = Number(object, "verticalFovDegrees", context);
    ValidatePresentation(result, context);
    return result;
}

FpsViewmodelHolsterTransition ReadHolsterTransition(
        const Json& object,
        const std::string& context)
{
    if (!object.is_object()) Fail(context + " must be an object");
    FpsViewmodelHolsterTransition result;
    result.holsterDurationSeconds = Number(
            object, "holsterDurationSeconds", context);
    result.unholsterDurationSeconds = Number(
            object, "unholsterDurationSeconds", context);
    result.hiddenTranslation = Vector(object, "hiddenTranslation", context);
    result.hiddenRotationDegrees = Vector(
            object, "hiddenRotationDegrees", context);
    ValidateHolsterTransition(result, context);
    return result;
}

FpsViewmodelMaterialOverride ReadMaterialOverride(
        const Json& object,
        const std::string& context)
{
    if (!object.is_object()) Fail(context + " must be an object");
    FpsViewmodelMaterialOverride result;
    result.enabled = true;
    result.metallicFactor = Number(object, "metallicFactor", context);
    result.roughnessFactor = Number(object, "roughnessFactor", context);
    result.useMetallicRoughnessTexture = Boolean(
            object,
            "useMetallicRoughnessTexture",
            context);
    if (result.metallicFactor < 0.0f || result.metallicFactor > 1.0f) {
        Fail(context + ".metallicFactor must be between 0 and 1");
    }
    if (result.roughnessFactor < 0.045f || result.roughnessFactor > 1.0f) {
        Fail(context + ".roughnessFactor must be between 0.045 and 1");
    }
    return result;
}

FpsViewmodelGripCorrection ReadGripCorrection(
        const Json& object,
        const std::string& context)
{
    if (!object.is_object()) Fail(context + " must be an object");
    FpsViewmodelGripCorrection result;
    result.translation = Vector(object, "translation", context);
    result.rotationDegrees = Vector(object, "rotationDegrees", context);
    result.scale = Number(object, "scale", context);
    ValidateGripCorrection(result, context);
    return result;
}

void ValidateCrosshair(
        const FpsWeaponCrosshairDefinition& value,
        const std::string& context)
{
    const float dimensions[] = {
            value.centerGapPixels,
            value.segmentLengthPixels,
            value.innerThicknessPixels,
            value.outlineThicknessPixels};
    for (float dimension : dimensions) {
        if (!std::isfinite(dimension) || dimension <= 0.0f) {
            Fail(context + " dimensions must be finite and greater than zero");
        }
    }
}

FpsWeaponCrosshairDefinition ReadCrosshair(
        const Json& object,
        const std::string& context)
{
    if (!object.is_object()) Fail(context + " must be an object");
    FpsWeaponCrosshairDefinition result;
    result.enabled = Boolean(object, "enabled", context);
    const auto innerColor = object.find("innerColor");
    if (innerColor != object.end()) {
        result.innerColor = ReadColor(*innerColor, context + ".innerColor");
    }
    const auto outlineColor = object.find("outlineColor");
    if (outlineColor != object.end()) {
        result.outlineColor = ReadColor(
                *outlineColor,
                context + ".outlineColor");
    }
    const auto readOptionalDimension = [&](const char* name, float current) {
        return object.find(name) == object.end()
                ? current
                : Number(object, name, context);
    };
    result.centerGapPixels = readOptionalDimension(
            "centerGapPixels", result.centerGapPixels);
    result.segmentLengthPixels = readOptionalDimension(
            "segmentLengthPixels", result.segmentLengthPixels);
    result.innerThicknessPixels = readOptionalDimension(
            "innerThicknessPixels", result.innerThicknessPixels);
    result.outlineThicknessPixels = readOptionalDimension(
            "outlineThicknessPixels", result.outlineThicknessPixels);
    ValidateCrosshair(result, context);
    return result;
}

FpsWeaponImpactParticlesDefinition ReadImpactParticles(
        const Json& object,
        const std::string& context)
{
    if (!object.is_object()) Fail(context + " must be an object");
    FpsWeaponImpactParticlesDefinition result;
    result.enabled = Boolean(object, "enabled", context);
    result.particleCount = Integer(object, "particleCount", context);
    result.sizeScale = Number(object, "sizeScale", context);
    result.intensity = Number(object, "intensity", context);
    return result;
}

void ValidateFiring(
        const FpsWeaponFiringDefinition& value,
        const std::string& context)
{
    const float values[] = {
            value.shotIntervalSeconds, value.maximumRangeWorld,
            value.noiseRadiusWorld,
            value.pellets.spreadHalfAngleDegrees,
            value.recoil.translationImpulse.x, value.recoil.translationImpulse.y,
            value.recoil.translationImpulse.z,
            value.recoil.rotationImpulseDegrees.x,
            value.recoil.rotationImpulseDegrees.y,
            value.recoil.rotationImpulseDegrees.z,
            value.recoil.rollVariationDegrees,
            value.recoil.springFrequencyHz, value.recoil.dampingRatio,
            value.recoil.maximumTranslation.x, value.recoil.maximumTranslation.y,
            value.recoil.maximumTranslation.z,
            value.recoil.maximumRotationDegrees.x,
            value.recoil.maximumRotationDegrees.y,
            value.recoil.maximumRotationDegrees.z,
            value.cameraRecoil.pitchKickDegrees,
            value.cameraRecoil.pitchVariationDegrees,
            value.cameraRecoil.yawVariationDegrees,
            value.cameraRecoil.rollVariationDegrees,
            value.cameraRecoil.springFrequencyHz,
            value.cameraRecoil.springDampingRatio,
            value.cameraRecoil.maxPitchDegrees,
            value.cameraRecoil.maxYawDegrees,
            value.cameraRecoil.maxRollDegrees,
            value.muzzleSocket.position.x, value.muzzleSocket.position.y,
            value.muzzleSocket.position.z,
            value.muzzleSocket.rotationDegrees.x,
            value.muzzleSocket.rotationDegrees.y,
            value.muzzleSocket.rotationDegrees.z,
            value.muzzleFlash.lifetimeSeconds, value.muzzleFlash.sizeWorld,
            value.muzzleFlash.sizeVariation, value.muzzleFlash.irregularity,
            value.muzzleFlash.forwardStretch,
            value.muzzleFlash.rearSuppression,
            value.muzzleFlash.edgeSoftness,
            value.muzzleFlash.radianceStrength,
            value.muzzleLight.intensity,
            value.muzzleLight.radiusWorld, value.muzzleLight.lifetimeSeconds,
            value.muzzleLight.decayExponent,
            value.impact.staggerSeconds,
            value.impact.knockbackImpulseWorldPerSecond,
            value.impact.blood.sizeScale,
            value.impact.blood.intensity,
            value.impact.surfaceDebris.sizeScale,
            value.impact.surfaceDebris.intensity};
    for (float component : values) {
        if (!std::isfinite(component)) Fail(context + " contains a non-finite value");
    }
    if (value.shotIntervalSeconds <= 0.0f || value.maximumRangeWorld <= 0.0f) {
        Fail(context + " shot interval and maximum range must be greater than zero");
    }
    if (value.noiseRadiusWorld < 0.0f || value.noiseRadiusWorld > 10000.0f) {
        Fail(context + ".noiseRadiusWorld must be between 0 and 10000");
    }
    if (value.pellets.count < 1
            || value.pellets.count > MaxFpsWeaponPellets
            || value.pellets.spreadHalfAngleDegrees < 0.0f
            || value.pellets.spreadHalfAngleDegrees > 45.0f) {
        Fail(context + ".pellets contains an invalid count or spread half-angle");
    }
    if (value.recoil.rollVariationDegrees < 0.0f
            || value.recoil.springFrequencyHz <= 0.0f
            || value.recoil.dampingRatio <= 0.0f
            || value.recoil.maximumTranslation.x < 0.0f
            || value.recoil.maximumTranslation.y < 0.0f
            || value.recoil.maximumTranslation.z < 0.0f
            || value.recoil.maximumRotationDegrees.x < 0.0f
            || value.recoil.maximumRotationDegrees.y < 0.0f
            || value.recoil.maximumRotationDegrees.z < 0.0f) {
        Fail(context + ".recoil contains an invalid response or limit");
    }
    if (value.cameraRecoil.pitchKickDegrees < 0.0f
            || value.cameraRecoil.pitchVariationDegrees < 0.0f
            || value.cameraRecoil.yawVariationDegrees < 0.0f
            || value.cameraRecoil.rollVariationDegrees < 0.0f
            || value.cameraRecoil.springFrequencyHz <= 0.0f
            || value.cameraRecoil.springDampingRatio <= 0.0f
            || value.cameraRecoil.maxPitchDegrees < 0.0f
            || value.cameraRecoil.maxYawDegrees < 0.0f
            || value.cameraRecoil.maxRollDegrees < 0.0f) {
        Fail(context + ".cameraRecoil contains an invalid kick, response, or limit");
    }
    if (value.muzzleFlash.lifetimeSeconds <= 0.0f
            || value.muzzleFlash.sizeWorld <= 0.0f
            || value.muzzleFlash.sizeVariation < 0.0f
            || value.muzzleFlash.sizeVariation > 0.5f
            || value.muzzleFlash.irregularity < 0.0f
            || value.muzzleFlash.irregularity > 1.0f
            || value.muzzleFlash.forwardStretch < 1.0f
            || value.muzzleFlash.forwardStretch > 4.0f
            || value.muzzleFlash.minimumLobeCount < 3
            || value.muzzleFlash.maximumLobeCount
                    > MaxFpsMuzzleFlashLobes
            || value.muzzleFlash.minimumLobeCount
                    > value.muzzleFlash.maximumLobeCount
            || value.muzzleFlash.rearSuppression < 0.0f
            || value.muzzleFlash.rearSuppression > 1.0f
            || value.muzzleFlash.edgeSoftness < 0.01f
            || value.muzzleFlash.edgeSoftness > 1.0f
            || value.muzzleFlash.radianceStrength < 0.0f
            || value.muzzleFlash.radianceStrength > 64.0f) {
        Fail(context + ".muzzleFlash contains an invalid lifetime, size, shape, lobe range, or edge softness");
    }
    if (value.muzzleLight.intensity < 0.0f
            || value.muzzleLight.radiusWorld <= 0.0f
            || value.muzzleLight.lifetimeSeconds <= 0.0f
            || value.muzzleLight.decayExponent <= 0.0f) {
        Fail(context + ".muzzleLight contains an invalid intensity, radius, lifetime, or decay");
    }
    const auto validateParticles = [&](
            const FpsWeaponImpactParticlesDefinition& particles,
            const char* name) {
        if (particles.particleCount < 0 || particles.particleCount > 256
                || (particles.enabled && particles.particleCount == 0)
                || particles.sizeScale < 0.05f || particles.sizeScale > 10.0f
                || particles.intensity < 0.0f || particles.intensity > 10.0f) {
            Fail(context + ".impact." + name
                    + " contains an invalid count, size scale, or intensity");
        }
    };
    if (value.impact.damage < 0 || value.impact.damage > 1000000
            || value.impact.staggerSeconds < 0.0f
            || value.impact.staggerSeconds > 10.0f
            || value.impact.knockbackImpulseWorldPerSecond < 0.0f
            || value.impact.knockbackImpulseWorldPerSecond > 100.0f) {
        Fail(context + ".impact contains invalid damage, stagger, or knockback values");
    }
    validateParticles(value.impact.blood, "blood");
    validateParticles(value.impact.surfaceDebris, "surfaceDebris");
}

void ValidateReload(
        const FpsWeaponReloadDefinition& value,
        const std::string& context)
{
    if (value.magazineSize < 1 || value.magazineSize > 1000000) {
        Fail(context + ".magazineSize must be between 1 and 1000000");
    }
    if (!std::isfinite(value.durationSeconds)
            || value.durationSeconds <= 0.0f
            || value.durationSeconds > 60.0f) {
        Fail(context + ".durationSeconds must be greater than zero and at most 60");
    }
    if (!value.dryFireSoundPath.empty()
            && !ValidAudioPath(value.dryFireSoundPath)) {
        Fail(context + ".dryFireSound must be a relative .ogg, .wav, or .mp3 path beneath assets/audio");
    }
    if (!value.reloadSoundPath.empty()
            && !ValidAudioPath(value.reloadSoundPath)) {
        Fail(context + ".reloadSound must be a relative .ogg, .wav, or .mp3 path beneath assets/audio");
    }
}

FpsWeaponFiringDefinition ReadFiring(const Json& object, const std::string& context)
{
    if (!object.is_object()) Fail(context + " must be an object");
    FpsWeaponFiringDefinition result;
    result.shotIntervalSeconds = Number(object, "shotIntervalSeconds", context);
    result.maximumRangeWorld = Number(object, "maximumRangeWorld", context);
    if (const auto noiseRadius = object.find("noiseRadiusWorld");
            noiseRadius != object.end()) {
        if (!noiseRadius->is_number()) {
            Fail(context + ".noiseRadiusWorld must be a number");
        }
        result.noiseRadiusWorld = noiseRadius->get<float>();
    }
    const auto pellets = object.find("pellets");
    if (pellets != object.end()) {
        const std::string pelletsContext = context + ".pellets";
        if (!pellets->is_object()) Fail(pelletsContext + " must be an object");
        result.pellets.enabled = Boolean(*pellets, "enabled", pelletsContext);
        result.pellets.count = Integer(*pellets, "count", pelletsContext);
        result.pellets.spreadHalfAngleDegrees = Number(
                *pellets, "spreadHalfAngleDegrees", pelletsContext);
    }
    const auto shootSound = object.find("shootSound");
    if (shootSound != object.end()) {
        if (!shootSound->is_string()) {
            Fail(context + ".shootSound must be a string");
        }
        result.shootSoundPath = shootSound->get<std::string>();
        if (!ValidAudioPath(result.shootSoundPath)) {
            Fail(context + ".shootSound must be a relative .ogg, .wav, or .mp3 path beneath assets/audio");
        }
    }

    const Json& recoil = Require(object, "recoil", context);
    const std::string recoilContext = context + ".recoil";
    result.recoil.translationImpulse = Vector(recoil, "translationImpulse", recoilContext);
    result.recoil.rotationImpulseDegrees = Vector(recoil, "rotationImpulseDegrees", recoilContext);
    result.recoil.rollVariationDegrees = Number(recoil, "rollVariationDegrees", recoilContext);
    result.recoil.springFrequencyHz = Number(recoil, "springFrequencyHz", recoilContext);
    result.recoil.dampingRatio = Number(recoil, "dampingRatio", recoilContext);
    result.recoil.maximumTranslation = Vector(recoil, "maximumTranslation", recoilContext);
    result.recoil.maximumRotationDegrees = Vector(recoil, "maximumRotationDegrees", recoilContext);

    const auto cameraRecoil = object.find("cameraRecoil");
    if (cameraRecoil != object.end()) {
        const std::string cameraRecoilContext = context + ".cameraRecoil";
        if (!cameraRecoil->is_object()) {
            Fail(cameraRecoilContext + " must be an object");
        }
        result.cameraRecoil.enabled = Boolean(
                *cameraRecoil, "enabled", cameraRecoilContext);
        result.cameraRecoil.pitchKickDegrees = Number(
                *cameraRecoil, "pitchKickDegrees", cameraRecoilContext);
        result.cameraRecoil.pitchVariationDegrees = Number(
                *cameraRecoil, "pitchVariationDegrees", cameraRecoilContext);
        result.cameraRecoil.yawVariationDegrees = Number(
                *cameraRecoil, "yawVariationDegrees", cameraRecoilContext);
        result.cameraRecoil.rollVariationDegrees = Number(
                *cameraRecoil, "rollVariationDegrees", cameraRecoilContext);
        result.cameraRecoil.springFrequencyHz = Number(
                *cameraRecoil, "springFrequencyHz", cameraRecoilContext);
        result.cameraRecoil.springDampingRatio = Number(
                *cameraRecoil, "springDampingRatio", cameraRecoilContext);
        result.cameraRecoil.maxPitchDegrees = Number(
                *cameraRecoil, "maxPitchDegrees", cameraRecoilContext);
        result.cameraRecoil.maxYawDegrees = Number(
                *cameraRecoil, "maxYawDegrees", cameraRecoilContext);
        result.cameraRecoil.maxRollDegrees = Number(
                *cameraRecoil, "maxRollDegrees", cameraRecoilContext);
    }

    const Json& socket = Require(object, "muzzleSocket", context);
    const std::string socketContext = context + ".muzzleSocket";
    result.muzzleSocket.position = Vector(socket, "position", socketContext);
    result.muzzleSocket.rotationDegrees = Vector(socket, "rotationDegrees", socketContext);

    const Json& flash = Require(object, "muzzleFlash", context);
    const std::string flashContext = context + ".muzzleFlash";
    result.muzzleFlash.enabled = Boolean(flash, "enabled", flashContext);
    result.muzzleFlash.lifetimeSeconds = Number(flash, "lifetimeSeconds", flashContext);
    result.muzzleFlash.sizeWorld = Number(flash, "sizeWorld", flashContext);
    result.muzzleFlash.sizeVariation = Number(flash, "sizeVariation", flashContext);
    result.muzzleFlash.irregularity = Number(flash, "irregularity", flashContext);
    result.muzzleFlash.forwardStretch = Number(flash, "forwardStretch", flashContext);
    result.muzzleFlash.minimumLobeCount = Integer(flash, "minimumLobeCount", flashContext);
    result.muzzleFlash.maximumLobeCount = Integer(flash, "maximumLobeCount", flashContext);
    result.muzzleFlash.rearSuppression = Number(flash, "rearSuppression", flashContext);
    result.muzzleFlash.coreColor = ReadColor(
            Require(flash, "coreColor", flashContext),
            flashContext + ".coreColor");
    result.muzzleFlash.hotColor = ReadColor(
            Require(flash, "hotColor", flashContext),
            flashContext + ".hotColor");
    result.muzzleFlash.warmColor = ReadColor(
            Require(flash, "warmColor", flashContext),
            flashContext + ".warmColor");
    result.muzzleFlash.edgeColor = ReadColor(
            Require(flash, "edgeColor", flashContext),
            flashContext + ".edgeColor");
    result.muzzleFlash.edgeSoftness = Number(
            flash, "edgeSoftness", flashContext);
    if (flash.find("radianceStrength") != flash.end()) {
        result.muzzleFlash.radianceStrength = Number(
                flash, "radianceStrength", flashContext);
    }

    const Json& light = Require(object, "muzzleLight", context);
    const std::string lightContext = context + ".muzzleLight";
    result.muzzleLight.enabled = Boolean(light, "enabled", lightContext);
    result.muzzleLight.color = ReadColor(Require(light, "color", lightContext), lightContext + ".color");
    result.muzzleLight.intensity = Number(light, "intensity", lightContext);
    result.muzzleLight.radiusWorld = Number(light, "radiusWorld", lightContext);
    result.muzzleLight.lifetimeSeconds = Number(light, "lifetimeSeconds", lightContext);
    result.muzzleLight.decayExponent = Number(light, "decayExponent", lightContext);

    const auto impact = object.find("impact");
    if (impact != object.end()) {
        const std::string impactContext = context + ".impact";
        if (!impact->is_object()) Fail(impactContext + " must be an object");
        result.impact.damage = Integer(*impact, "damage", impactContext);
        result.impact.staggerSeconds = Number(
                *impact, "staggerSeconds", impactContext);
        result.impact.knockbackImpulseWorldPerSecond = Number(
                *impact,
                "knockbackImpulseWorldPerSecond",
                impactContext);
        result.impact.blood = ReadImpactParticles(
                Require(*impact, "blood", impactContext),
                impactContext + ".blood");
        result.impact.surfaceDebris = ReadImpactParticles(
                Require(*impact, "surfaceDebris", impactContext),
                impactContext + ".surfaceDebris");
    }
    ValidateFiring(result, context);
    return result;
}

FpsWeaponReloadDefinition ReadReload(
        const Json& object,
        const std::string& context)
{
    if (!object.is_object()) Fail(context + " must be an object");
    FpsWeaponReloadDefinition result;
    result.magazineSize = Integer(object, "magazineSize", context);
    result.durationSeconds = Number(object, "durationSeconds", context);
    const auto readSound = [&](const char* field, std::string& destination) {
        const auto value = object.find(field);
        if (value == object.end()) return;
        if (!value->is_string()) {
            Fail(context + "." + field + " must be a string");
        }
        destination = value->get<std::string>();
    };
    readSound("dryFireSound", result.dryFireSoundPath);
    readSound("reloadSound", result.reloadSoundPath);
    ValidateReload(result, context);
    return result;
}

std::optional<Vector3> OptionalVector(const Json& object, const char* name, const std::string& context)
{
    if (object.find(name) == object.end()) return std::nullopt;
    return Vector(object, name, context);
}

std::optional<float> OptionalNumber(const Json& object, const char* name, const std::string& context)
{
    if (object.find(name) == object.end()) return std::nullopt;
    return Number(object, name, context);
}

std::optional<int> OptionalInteger(const Json& object, const char* name, const std::string& context)
{
    if (object.find(name) == object.end()) return std::nullopt;
    return Integer(object, name, context);
}

std::optional<bool> OptionalBoolean(
        const Json& object,
        const char* name,
        const std::string& context)
{
    if (object.find(name) == object.end()) return std::nullopt;
    return Boolean(object, name, context);
}

std::string PlayerStaminaSettingsError(
        const PlayerStaminaApplicationSettings& settings)
{
    const auto finite = [](float value) { return std::isfinite(value); };
    if (!finite(settings.maximum) || settings.maximum <= 0.0f) {
        return "maximum must be greater than zero";
    }
    if (!finite(settings.sprintDrainPerSecond)
            || settings.sprintDrainPerSecond < 0.0f) {
        return "sprintDrainPerSecond must be non-negative";
    }
    if (!finite(settings.jumpCost)
            || settings.jumpCost < 0.0f
            || settings.jumpCost > settings.maximum) {
        return "jumpCost must be between zero and maximum";
    }
    if (!finite(settings.regenerationPerSecond)
            || settings.regenerationPerSecond < 0.0f) {
        return "regenerationPerSecond must be non-negative";
    }
    if (!finite(settings.exhaustedRecoveryRatio)
            || settings.exhaustedRecoveryRatio < 0.0f
            || settings.exhaustedRecoveryRatio > 1.0f) {
        return "exhaustedRecoveryRatio must be between 0 and 1";
    }
    const PlayerWindedCameraApplicationSettings& camera =
            settings.windedCamera;
    if (!finite(camera.startThresholdRatio)
            || camera.startThresholdRatio < 0.0f
            || camera.startThresholdRatio > 1.0f) {
        return "windedCamera.startThresholdRatio must be between 0 and 1";
    }
    if (!finite(camera.verticalAmplitudeWorld)
            || camera.verticalAmplitudeWorld < 0.0f) {
        return "windedCamera.verticalAmplitudeWorld must be non-negative";
    }
    if (!finite(camera.pitchAmplitudeDegrees)
            || camera.pitchAmplitudeDegrees < 0.0f) {
        return "windedCamera.pitchAmplitudeDegrees must be non-negative";
    }
    if (!finite(camera.frequencyHz) || camera.frequencyHz < 0.0f) {
        return "windedCamera.frequencyHz must be non-negative";
    }
    if (!finite(camera.responseSeconds) || camera.responseSeconds <= 0.0f) {
        return "windedCamera.responseSeconds must be greater than zero";
    }
    const PlayerBreathingAudioApplicationSettings& breathing =
            settings.breathingAudio;
    if (!finite(breathing.thresholdRatio)
            || breathing.thresholdRatio < 0.0f
            || breathing.thresholdRatio > 1.0f) {
        return "breathingAudio.thresholdRatio must be between 0 and 1";
    }
    if (!finite(breathing.volume)
            || breathing.volume < 0.0f
            || breathing.volume > 1.0f) {
        return "breathingAudio.volume must be between 0 and 1";
    }
    if (!finite(breathing.fadeOutSeconds)
            || breathing.fadeOutSeconds <= 0.0f) {
        return "breathingAudio.fadeOutSeconds must be greater than zero";
    }
    return {};
}

void SetError(std::string* output, const std::string& message) { if (output) *output = message; }

bool NearlyEqual(float a, float b) { return std::abs(a - b) <= 0.0001f; }
bool Same(Vector3 a, Vector3 b) { return NearlyEqual(a.x,b.x) && NearlyEqual(a.y,b.y) && NearlyEqual(a.z,b.z); }

Json Vec(Vector3 value) { return Json::array({value.x, value.y, value.z}); }

Json ColorValue(Color value)
{
    return Json{{"r", value.r}, {"g", value.g}, {"b", value.b}, {"a", value.a}};
}

Json MaterialOverrideValue(const FpsViewmodelMaterialOverride& value)
{
    return Json{
            {"metallicFactor", value.metallicFactor},
            {"roughnessFactor", value.roughnessFactor},
            {"useMetallicRoughnessTexture", value.useMetallicRoughnessTexture}};
}

Json ImpactParticlesValue(const FpsWeaponImpactParticlesDefinition& value)
{
    return Json{
            {"enabled", value.enabled},
            {"particleCount", value.particleCount},
            {"sizeScale", value.sizeScale},
            {"intensity", value.intensity}};
}

Json FiringValue(const FpsWeaponFiringDefinition& value)
{
    Json firing{
            {"shotIntervalSeconds", value.shotIntervalSeconds},
            {"maximumRangeWorld", value.maximumRangeWorld},
            {"noiseRadiusWorld", value.noiseRadiusWorld},
            {"recoil", {
                    {"translationImpulse", Vec(value.recoil.translationImpulse)},
                    {"rotationImpulseDegrees", Vec(value.recoil.rotationImpulseDegrees)},
                    {"rollVariationDegrees", value.recoil.rollVariationDegrees},
                    {"springFrequencyHz", value.recoil.springFrequencyHz},
                    {"dampingRatio", value.recoil.dampingRatio},
                    {"maximumTranslation", Vec(value.recoil.maximumTranslation)},
                    {"maximumRotationDegrees", Vec(value.recoil.maximumRotationDegrees)}}},
            {"cameraRecoil", {
                    {"enabled", value.cameraRecoil.enabled},
                    {"pitchKickDegrees", value.cameraRecoil.pitchKickDegrees},
                    {"pitchVariationDegrees", value.cameraRecoil.pitchVariationDegrees},
                    {"yawVariationDegrees", value.cameraRecoil.yawVariationDegrees},
                    {"rollVariationDegrees", value.cameraRecoil.rollVariationDegrees},
                    {"springFrequencyHz", value.cameraRecoil.springFrequencyHz},
                    {"springDampingRatio", value.cameraRecoil.springDampingRatio},
                    {"maxPitchDegrees", value.cameraRecoil.maxPitchDegrees},
                    {"maxYawDegrees", value.cameraRecoil.maxYawDegrees},
                    {"maxRollDegrees", value.cameraRecoil.maxRollDegrees}}},
            {"muzzleSocket", {
                    {"position", Vec(value.muzzleSocket.position)},
                    {"rotationDegrees", Vec(value.muzzleSocket.rotationDegrees)}}},
            {"muzzleFlash", {
                    {"enabled", value.muzzleFlash.enabled},
                    {"lifetimeSeconds", value.muzzleFlash.lifetimeSeconds},
                    {"sizeWorld", value.muzzleFlash.sizeWorld},
                    {"sizeVariation", value.muzzleFlash.sizeVariation},
                    {"irregularity", value.muzzleFlash.irregularity},
                    {"forwardStretch", value.muzzleFlash.forwardStretch},
                    {"minimumLobeCount", value.muzzleFlash.minimumLobeCount},
                    {"maximumLobeCount", value.muzzleFlash.maximumLobeCount},
                    {"rearSuppression", value.muzzleFlash.rearSuppression},
                    {"coreColor", ColorValue(value.muzzleFlash.coreColor)},
                    {"hotColor", ColorValue(value.muzzleFlash.hotColor)},
                    {"warmColor", ColorValue(value.muzzleFlash.warmColor)},
                    {"edgeColor", ColorValue(value.muzzleFlash.edgeColor)},
                    {"edgeSoftness", value.muzzleFlash.edgeSoftness},
                    {"radianceStrength", value.muzzleFlash.radianceStrength}}},
            {"muzzleLight", {
                    {"enabled", value.muzzleLight.enabled},
                    {"color", ColorValue(value.muzzleLight.color)},
                    {"intensity", value.muzzleLight.intensity},
                    {"radiusWorld", value.muzzleLight.radiusWorld},
                    {"lifetimeSeconds", value.muzzleLight.lifetimeSeconds},
                    {"decayExponent", value.muzzleLight.decayExponent}}},
            {"impact", {
                    {"damage", value.impact.damage},
                    {"staggerSeconds", value.impact.staggerSeconds},
                    {"knockbackImpulseWorldPerSecond", value.impact.knockbackImpulseWorldPerSecond},
                    {"blood", ImpactParticlesValue(value.impact.blood)},
                    {"surfaceDebris", ImpactParticlesValue(value.impact.surfaceDebris)}}}};
    const FpsWeaponPelletDefinition defaultPellets;
    if (value.pellets.enabled != defaultPellets.enabled
            || value.pellets.count != defaultPellets.count
            || value.pellets.spreadHalfAngleDegrees
                    != defaultPellets.spreadHalfAngleDegrees) {
        firing["pellets"] = {
                {"enabled", value.pellets.enabled},
                {"count", value.pellets.count},
                {"spreadHalfAngleDegrees",
                        value.pellets.spreadHalfAngleDegrees}};
    }
    if (!value.shootSoundPath.empty()) {
        firing["shootSound"] = value.shootSoundPath;
    }
    return firing;
}

Json WeaponValue(const FpsWeaponDefinition& value)
{
    Json viewmodel{
            {"modelPath", value.viewmodel.modelPath},
            {"idleAnimation", value.viewmodel.idleAnimation},
            {"sourceFps", value.viewmodel.sourceFps},
            {"firstFrame", value.viewmodel.firstFrame},
            {"lastFrame", value.viewmodel.lastFrame},
            {"playbackSpeed", value.viewmodel.playbackSpeed},
            {"position", Vec(value.viewmodel.presentation.position)},
            {"rotationDegrees", Vec(value.viewmodel.presentation.rotationDegrees)},
            {"scale", value.viewmodel.presentation.scale},
            {"verticalFovDegrees", value.viewmodel.presentation.verticalFovDegrees},
            {"holsterTransition", {
                    {"holsterDurationSeconds", value.viewmodel.holsterTransition.holsterDurationSeconds},
                    {"unholsterDurationSeconds", value.viewmodel.holsterTransition.unholsterDurationSeconds},
                    {"hiddenTranslation", Vec(value.viewmodel.holsterTransition.hiddenTranslation)},
                    {"hiddenRotationDegrees", Vec(value.viewmodel.holsterTransition.hiddenRotationDegrees)}}},
            {"brightnessAdjustment", value.viewmodel.brightnessAdjustment}};
    if (value.viewmodel.materialOverride.enabled) {
        viewmodel["materialOverride"] = MaterialOverrideValue(
                value.viewmodel.materialOverride);
    }
    viewmodel["attachment"] = {
            {"modelPath", value.viewmodel.attachment.modelPath},
            {"boneName", value.viewmodel.attachment.boneName},
            {"translation", Vec(value.viewmodel.attachment.gripCorrection.translation)},
            {"rotationDegrees", Vec(value.viewmodel.attachment.gripCorrection.rotationDegrees)},
            {"scale", value.viewmodel.attachment.gripCorrection.scale},
            {"brightnessAdjustment", value.viewmodel.attachment.lighting.brightnessAdjustment},
            {"materialOverride", MaterialOverrideValue(
                    value.viewmodel.attachment.lighting.materialOverride)}};
    Json reload{
            {"magazineSize", value.reload.magazineSize},
            {"durationSeconds", value.reload.durationSeconds}};
    if (!value.reload.dryFireSoundPath.empty()) {
        reload["dryFireSound"] = value.reload.dryFireSoundPath;
    }
    if (!value.reload.reloadSoundPath.empty()) {
        reload["reloadSound"] = value.reload.reloadSoundPath;
    }
    Json weapon{
            {"id", value.id},
            {"crosshair", {
                    {"enabled", value.crosshair.enabled},
                    {"innerColor", ColorValue(value.crosshair.innerColor)},
                    {"outlineColor", ColorValue(value.crosshair.outlineColor)},
                    {"centerGapPixels", value.crosshair.centerGapPixels},
                    {"segmentLengthPixels", value.crosshair.segmentLengthPixels},
                    {"innerThicknessPixels", value.crosshair.innerThicknessPixels},
                    {"outlineThicknessPixels", value.crosshair.outlineThicknessPixels}}},
            {"firing", FiringValue(value.firing)},
            {"reload", std::move(reload)},
            {"viewmodel", std::move(viewmodel)}};
    if (value.weaponSlot != 0) {
        weapon["slot"] = value.weaponSlot;
    }
    return weapon;
}

Json RegistryValue(const FpsWeaponRegistry& registry)
{
    Json weapons = Json::array();
    for (const FpsWeaponDefinition& weapon : registry.weapons) {
        weapons.push_back(WeaponValue(weapon));
    }
    return Json{
            {"version", 3},
            {"weapons", std::move(weapons)}};
}

} // namespace

const char* FpsShadowQualityName(FpsShadowQuality quality)
{
    switch (quality) {
        case FpsShadowQuality::Off: return "off";
        case FpsShadowQuality::Low: return "low";
        case FpsShadowQuality::Medium: return "medium";
        case FpsShadowQuality::High: return "high";
    }
    return "high";
}

FpsGraphicsSettings NormalizeFpsGraphicsSettings(FpsGraphicsSettings settings)
{
    if (!std::isfinite(settings.renderScale)) {
        settings.renderScale = 1.5f;
    }
    settings.renderScale = std::clamp(settings.renderScale, 0.5f, 2.0f);
    settings.horizontalFovDegrees = std::clamp(
            settings.horizontalFovDegrees,
            MinFpsHorizontalFovDegrees,
            MaxFpsHorizontalFovDegrees);
    settings.maxDynamicLights = std::clamp(
            settings.maxDynamicLights,
            MinFpsDynamicLights,
            MaxFpsDynamicLights);
    settings.maxShadowLightUpdatesPerFrame = std::clamp(
            settings.maxShadowLightUpdatesPerFrame,
            MinFpsShadowLightUpdatesPerFrame,
            MaxFpsShadowLightUpdatesPerFrame);
    if (!std::isfinite(settings.dynamicLightFadeInSeconds)) {
        settings.dynamicLightFadeInSeconds =
                DefaultFpsDynamicLightFadeInSeconds;
    }
    settings.dynamicLightFadeInSeconds = std::clamp(
            settings.dynamicLightFadeInSeconds,
            MinFpsDynamicLightFadeInSeconds,
            MaxFpsDynamicLightFadeInSeconds);
    return settings;
}

float FpsVerticalFovDegrees(int horizontalFovDegrees, float aspectRatio)
{
    const int normalizedHorizontal = std::clamp(
            horizontalFovDegrees,
            MinFpsHorizontalFovDegrees,
            MaxFpsHorizontalFovDegrees);
    if (!std::isfinite(aspectRatio) || aspectRatio <= 0.0f) {
        aspectRatio = 16.0f / 9.0f;
    }
    constexpr float DegreesToRadians = 3.14159265358979323846f / 180.0f;
    constexpr float RadiansToDegrees = 180.0f / 3.14159265358979323846f;
    const float horizontalRadians =
            static_cast<float>(normalizedHorizontal) * DegreesToRadians;
    return 2.0f * std::atan(
            std::tan(horizontalRadians * 0.5f) / aspectRatio)
            * RadiansToDegrees;
}

bool ParseFpsWeaponRegistry(std::string_view text, FpsWeaponRegistry& output, std::string* error)
{
    try {
        const Json root = Json::parse(text.begin(), text.end());
        if (!root.is_object()) Fail("weapon registry root must be an object");
        FpsWeaponRegistry parsed;
        const int sourceVersion = Integer(root, "version", "weapon registry");
        if (sourceVersion != 1 && sourceVersion != 2 && sourceVersion != 3) {
            Fail("weapon registry version must be 1, 2, or 3");
        }
        parsed.version = 3;
        const std::string legacyInitialWeaponId = sourceVersion == 1
                ? String(root, "initialWeaponId", "weapon registry")
                : std::string{};
        const Json& weapons = Require(root, "weapons", "weapon registry");
        if (!weapons.is_array() || weapons.empty()) Fail("weapon registry.weapons must be a non-empty array");
        std::unordered_set<std::string> ids;
        std::unordered_set<int> weaponSlots;
        for (size_t i = 0; i < weapons.size(); ++i) {
            const Json& object = weapons[i];
            const std::string context = "weapon registry.weapons[" + std::to_string(i) + "]";
            if (!object.is_object()) Fail(context + " must be an object");
            FpsWeaponDefinition definition;
            definition.id = String(object, "id", context);
            if (!ids.insert(definition.id).second) Fail("duplicate weapon id '" + definition.id + "'");
            if (sourceVersion >= 2) {
                const std::optional<int> weaponSlot = OptionalInteger(
                        object, "slot", context);
                if (weaponSlot
                        && (*weaponSlot < MinFpsWeaponSlot
                            || *weaponSlot > MaxFpsWeaponSlot)) {
                    Fail(context + ".slot must be between 1 and 6 when present");
                }
                definition.weaponSlot = weaponSlot.value_or(0);
                if (definition.weaponSlot != 0
                        && !weaponSlots.insert(definition.weaponSlot).second) {
                    Fail("duplicate weapon slot "
                            + std::to_string(definition.weaponSlot));
                }
            }
            const auto crosshair = object.find("crosshair");
            if (crosshair != object.end()) {
                definition.crosshair = ReadCrosshair(
                        *crosshair,
                        context + ".crosshair");
            }
            definition.firing = ReadFiring(
                    Require(object, "firing", context),
                    context + ".firing");
            const auto reload = object.find("reload");
            if (reload != object.end()) {
                definition.reload = ReadReload(*reload, context + ".reload");
            }
            const Json& viewmodel = Require(object, "viewmodel", context);
            if (!viewmodel.is_object()) Fail(context + ".viewmodel must be an object");
            const std::string vm = context + ".viewmodel";
            definition.viewmodel.modelPath = String(viewmodel, "modelPath", vm);
            if (!ValidAssetPath(definition.viewmodel.modelPath)) Fail(vm + ".modelPath must be an assets/ .glb or .gltf path without traversal");
            definition.viewmodel.idleAnimation = String(viewmodel, "idleAnimation", vm);
            definition.viewmodel.sourceFps = Number(viewmodel, "sourceFps", vm);
            definition.viewmodel.firstFrame = Integer(viewmodel, "firstFrame", vm);
            definition.viewmodel.lastFrame = Integer(viewmodel, "lastFrame", vm);
            definition.viewmodel.playbackSpeed = Number(viewmodel, "playbackSpeed", vm);
            definition.viewmodel.presentation = ReadPresentation(viewmodel, vm);
            definition.viewmodel.holsterTransition = ReadHolsterTransition(
                    Require(viewmodel, "holsterTransition", vm),
                    vm + ".holsterTransition");
            definition.viewmodel.brightnessAdjustment =
                    OptionalNumber(viewmodel, "brightnessAdjustment", vm).value_or(0.0f);
            if (definition.viewmodel.brightnessAdjustment < -1.0f
                    || definition.viewmodel.brightnessAdjustment > 1.0f) {
                Fail(vm + ".brightnessAdjustment must be between -1 and 1");
            }
            const auto materialOverride = viewmodel.find("materialOverride");
            if (materialOverride != viewmodel.end()) {
                definition.viewmodel.materialOverride = ReadMaterialOverride(
                        *materialOverride,
                        vm + ".materialOverride");
            }
            const Json& attachment = Require(viewmodel, "attachment", vm);
            if (!attachment.is_object()) Fail(vm + ".attachment must be an object");
            const std::string attachmentContext = vm + ".attachment";
            definition.viewmodel.attachment.modelPath = String(
                    attachment, "modelPath", attachmentContext);
            if (!ValidAssetPath(definition.viewmodel.attachment.modelPath)) {
                Fail(attachmentContext
                        + ".modelPath must be an assets/ .glb or .gltf path without traversal");
            }
            definition.viewmodel.attachment.boneName = String(
                    attachment, "boneName", attachmentContext);
            if (definition.viewmodel.attachment.boneName.size() >= sizeof(BoneInfo::name)) {
                Fail(attachmentContext + ".boneName must contain at most 31 characters");
            }
            definition.viewmodel.attachment.gripCorrection = ReadGripCorrection(
                    attachment, attachmentContext);
            definition.viewmodel.attachment.lighting.brightnessAdjustment =
                    Number(attachment, "brightnessAdjustment", attachmentContext);
            if (definition.viewmodel.attachment.lighting.brightnessAdjustment < -1.0f
                    || definition.viewmodel.attachment.lighting.brightnessAdjustment > 1.0f) {
                Fail(attachmentContext
                        + ".brightnessAdjustment must be between -1 and 1");
            }
            definition.viewmodel.attachment.lighting.materialOverride =
                    ReadMaterialOverride(
                            Require(
                                    attachment,
                                    "materialOverride",
                                    attachmentContext),
                            attachmentContext + ".materialOverride");
            if (definition.viewmodel.sourceFps <= 0.0f) Fail(vm + ".sourceFps must be greater than zero");
            if (definition.viewmodel.playbackSpeed <= 0.0f) Fail(vm + ".playbackSpeed must be greater than zero");
            if (definition.viewmodel.firstFrame < 0 || definition.viewmodel.lastFrame <= definition.viewmodel.firstFrame) {
                Fail(vm + " requires lastFrame > firstFrame >= 0");
            }
            ValidateReload(definition.reload, context + ".reload");
            parsed.weapons.push_back(std::move(definition));
        }
        if (sourceVersion == 1) {
            auto legacyInitial = std::find_if(
                    parsed.weapons.begin(),
                    parsed.weapons.end(),
                    [&legacyInitialWeaponId](const FpsWeaponDefinition& weapon) {
                        return weapon.id == legacyInitialWeaponId;
                    });
            if (legacyInitial == parsed.weapons.end()) {
                Fail("initial weapon id '" + legacyInitialWeaponId
                        + "' has no definition");
            }
            legacyInitial->weaponSlot = MinFpsWeaponSlot;
        }
        output = std::move(parsed);
        SetError(error, {});
        return true;
    } catch (const std::exception& exception) {
        SetError(error, exception.what());
        return false;
    }
}

FpsWeaponDefinition MakeDefaultFpsWeaponDefinition()
{
    FpsWeaponDefinition definition;
    definition.id = "new_weapon";
    definition.viewmodel.attachment.lighting.materialOverride.enabled = true;
    return definition;
}

bool SerializeFpsWeaponRegistryJson(
        const FpsWeaponRegistry& registry,
        std::string& outJson,
        std::string* error)
{
    try {
        const std::string text = RegistryValue(registry).dump(2) + '\n';
        FpsWeaponRegistry validated;
        std::string validationError;
        if (!ParseFpsWeaponRegistry(text, validated, &validationError)) {
            SetError(error, validationError);
            return false;
        }
        outJson = text;
        SetError(error, {});
        return true;
    } catch (const std::exception& exception) {
        SetError(error, exception.what());
        return false;
    }
}

bool ValidateFpsWeaponRegistry(
        const FpsWeaponRegistry& registry,
        std::string* error)
{
    std::string ignored;
    return SerializeFpsWeaponRegistryJson(registry, ignored, error);
}

bool LoadFpsWeaponRegistry(const std::string& path, FpsWeaponRegistry& output, std::string* error)
{
    std::ifstream input(path);
    if (!input) { SetError(error, "could not open weapon registry: " + path); return false; }
    std::ostringstream text; text << input.rdbuf();
    return ParseFpsWeaponRegistry(text.str(), output, error);
}

bool SaveFpsWeaponRegistry(
        const std::string& path,
        const FpsWeaponRegistry& registry,
        std::string* error)
{
    std::string text;
    if (!SerializeFpsWeaponRegistryJson(registry, text, error)) return false;
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        SetError(error, "could not write weapon registry: " + path);
        return false;
    }
    output << text;
    if (!output) {
        SetError(error, "failed writing weapon registry: " + path);
        return false;
    }
    SetError(error, {});
    return true;
}

const FpsWeaponDefinition* FindFpsWeaponDefinition(const FpsWeaponRegistry& registry, std::string_view id)
{
    const auto it = std::find_if(registry.weapons.begin(), registry.weapons.end(),
            [id](const FpsWeaponDefinition& value) { return value.id == id; });
    return it == registry.weapons.end() ? nullptr : &*it;
}

const FpsWeaponDefinition* FindFpsWeaponDefinitionForSlot(
        const FpsWeaponRegistry& registry,
        int weaponSlot)
{
    if (weaponSlot < MinFpsWeaponSlot || weaponSlot > MaxFpsWeaponSlot) {
        return nullptr;
    }
    const auto it = std::find_if(
            registry.weapons.begin(),
            registry.weapons.end(),
            [weaponSlot](const FpsWeaponDefinition& weapon) {
                return weapon.weaponSlot == weaponSlot;
            });
    return it == registry.weapons.end() ? nullptr : &*it;
}

int FpsWeaponSlotFromKey(int key)
{
    if (key < KEY_ONE || key > KEY_SIX) {
        return 0;
    }
    return key - KEY_ONE + MinFpsWeaponSlot;
}

void ApplyFpsApplicationWeaponOverrides(
        FpsWeaponRegistry& registry,
        const FpsApplicationSettings& settings)
{
    for (FpsWeaponDefinition& weapon : registry.weapons) {
        weapon.viewmodel.presentation = ResolveFpsViewmodelPresentation(
                weapon.viewmodel.presentation,
                FindFpsViewmodelOverride(settings, weapon.id));
        weapon.viewmodel.holsterTransition = ResolveFpsViewmodelHolsterTransition(
                weapon.viewmodel.holsterTransition,
                FindFpsViewmodelHolsterTransitionOverride(settings, weapon.id));
        weapon.viewmodel.attachment.gripCorrection = ResolveFpsViewmodelGripCorrection(
                weapon.viewmodel.attachment.gripCorrection,
                FindFpsViewmodelGripCorrectionOverride(settings, weapon.id));
        weapon.viewmodel.attachment.lighting = ResolveFpsViewmodelAttachmentLighting(
                weapon.viewmodel.attachment.lighting,
                FindFpsViewmodelAttachmentLightingOverride(settings, weapon.id));
        weapon.firing = ResolveFpsWeaponFiringDefinition(
                weapon.firing,
                FindFpsWeaponFiringOverride(settings, weapon.id));
    }
}

bool ParseFpsApplicationSettings(std::string_view text, FpsApplicationSettings& output, std::string* error)
{
    try {
        const Json root = Json::parse(text.begin(), text.end());
        if (!root.is_object()) Fail("application settings root must be an object");
        FpsApplicationSettings parsed;
        parsed.version = Integer(root, "version", "application settings");
        if (parsed.version != 1) Fail("application settings version must be 1");
        const auto firstLevel = root.find("firstLevel");
        if (firstLevel != root.end()) {
            if (!firstLevel->is_string()) {
                Fail("application settings.firstLevel must be a string");
            }
            parsed.firstLevel = firstLevel->get<std::string>();
            if (!ValidLevelName(parsed.firstLevel)) {
                Fail("application settings.firstLevel must use only letters, digits, underscore, or dash");
            }
        }
        const auto consoleEnabled = root.find("consoleEnabled");
        if (consoleEnabled != root.end()) {
            if (!consoleEnabled->is_boolean()) {
                Fail("application settings.consoleEnabled must be a boolean");
            }
            parsed.consoleEnabled = consoleEnabled->get<bool>();
        }
        const auto playerInventory = root.find("playerInventory");
        if (playerInventory != root.end()) {
            if (!playerInventory->is_object()) {
                Fail("application settings.playerInventory must be an object");
            }
            const auto maxCarryWeightKg = playerInventory->find(
                    "maxCarryWeightKg");
            if (maxCarryWeightKg != playerInventory->end()) {
                if (!maxCarryWeightKg->is_number()) {
                    Fail("application settings.playerInventory.maxCarryWeightKg must be a number");
                }
                const double value = maxCarryWeightKg->get<double>();
                if (!std::isfinite(value) || value <= 0.0
                        || value > std::numeric_limits<float>::max()) {
                    Fail("application settings.playerInventory.maxCarryWeightKg must be finite and positive");
                }
                parsed.playerInventory.maxCarryWeightKg =
                        static_cast<float>(value);
            }
            const auto maxSlots = playerInventory->find("maxSlots");
            if (maxSlots != playerInventory->end()) {
                if (!maxSlots->is_number_integer()) {
                    Fail("application settings.playerInventory.maxSlots must be an integer");
                }
                parsed.playerInventory.maxSlots = maxSlots->get<int>();
                if (parsed.playerInventory.maxSlots < 1
                        || parsed.playerInventory.maxSlots > 1024) {
                    Fail("application settings.playerInventory.maxSlots must be between 1 and 1024");
                }
            }
            const auto pickupVacuumDurationSeconds = playerInventory->find(
                    "pickupVacuumDurationSeconds");
            if (pickupVacuumDurationSeconds != playerInventory->end()) {
                if (!pickupVacuumDurationSeconds->is_number()) {
                    Fail("application settings.playerInventory.pickupVacuumDurationSeconds must be a number");
                }
                const double value = pickupVacuumDurationSeconds->get<double>();
                if (!std::isfinite(value) || value <= 0.0
                        || value > std::numeric_limits<float>::max()) {
                    Fail("application settings.playerInventory.pickupVacuumDurationSeconds must be finite and positive");
                }
                parsed.playerInventory.pickupVacuumDurationSeconds =
                        static_cast<float>(value);
            }
            const auto pickupVacuumTargetHeightWorld = playerInventory->find(
                    "pickupVacuumTargetHeightWorld");
            if (pickupVacuumTargetHeightWorld != playerInventory->end()) {
                if (!pickupVacuumTargetHeightWorld->is_number()) {
                    Fail("application settings.playerInventory.pickupVacuumTargetHeightWorld must be a number");
                }
                const double value = pickupVacuumTargetHeightWorld->get<double>();
                if (!std::isfinite(value) || value < 0.0
                        || value > std::numeric_limits<float>::max()) {
                    Fail("application settings.playerInventory.pickupVacuumTargetHeightWorld must be finite and non-negative");
                }
                parsed.playerInventory.pickupVacuumTargetHeightWorld =
                        static_cast<float>(value);
            }
        }
        const auto playerSneak = root.find("playerSneak");
        if (playerSneak != root.end()) {
            const std::string sneakContext =
                    "application settings.playerSneak";
            if (!playerSneak->is_object()) {
                Fail(sneakContext + " must be an object");
            }
            parsed.playerSneak.fullVisibilityLightLevel = OptionalNumber(
                    *playerSneak,
                    "fullVisibilityLightLevel",
                    sneakContext).value_or(
                            parsed.playerSneak.fullVisibilityLightLevel);
            parsed.playerSneak.darknessCutoffNormalized = OptionalNumber(
                    *playerSneak,
                    "darknessCutoffNormalized",
                    sneakContext).value_or(
                            parsed.playerSneak.darknessCutoffNormalized);
            const std::optional<double> lightHalfResponseRange =
                    OptionalNumber(
                            *playerSneak,
                            "lightHalfResponseRangeNormalized",
                            sneakContext);
            if (lightHalfResponseRange.has_value()) {
                parsed.playerSneak.lightHalfResponseRangeNormalized =
                        static_cast<float>(*lightHalfResponseRange);
            } else {
                parsed.playerSneak.lightHalfResponseRangeNormalized = std::min(
                        parsed.playerSneak.lightHalfResponseRangeNormalized,
                        (1.0f - parsed.playerSneak.darknessCutoffNormalized)
                                * 0.5f);
            }
            parsed.playerSneak.visualDetectionBuildSeconds = OptionalNumber(
                    *playerSneak,
                    "visualDetectionBuildSeconds",
                    sneakContext).value_or(
                            parsed.playerSneak.visualDetectionBuildSeconds);
            parsed.playerSneak.visualDetectionDecaySeconds = OptionalNumber(
                    *playerSneak,
                    "visualDetectionDecaySeconds",
                    sneakContext).value_or(
                            parsed.playerSneak.visualDetectionDecaySeconds);
            parsed.playerSneak.darknessProximityRangeWorld = OptionalNumber(
                    *playerSneak,
                    "darknessProximityRangeWorld",
                    sneakContext).value_or(
                            parsed.playerSneak.darknessProximityRangeWorld);
            parsed.playerSneak.crouchVisualDetectionMultiplier = OptionalNumber(
                    *playerSneak,
                    "crouchVisualDetectionMultiplier",
                    sneakContext).value_or(
                            parsed.playerSneak.crouchVisualDetectionMultiplier);
            parsed.playerSneak.crouchMovementNoiseMultiplier = OptionalNumber(
                    *playerSneak,
                    "crouchMovementNoiseMultiplier",
                    sneakContext).value_or(
                            parsed.playerSneak.crouchMovementNoiseMultiplier);
            const std::string sneakError = PlayerSneakSettingsError(
                    parsed.playerSneak);
            if (!sneakError.empty()) {
                Fail(sneakContext + "." + sneakError);
            }
        }
        const auto playerFlashlight = root.find("playerFlashlight");
        if (playerFlashlight != root.end()) {
            const std::string flashlightContext =
                    "application settings.playerFlashlight";
            if (!playerFlashlight->is_object()) {
                Fail(flashlightContext + " must be an object");
            }
            PlayerFlashlightApplicationSettings& flashlight =
                    parsed.playerFlashlight;
            flashlight.intensity = OptionalNumber(
                    *playerFlashlight, "intensity", flashlightContext)
                    .value_or(flashlight.intensity);
            flashlight.reachWorld = OptionalNumber(
                    *playerFlashlight, "reachWorld", flashlightContext)
                    .value_or(flashlight.reachWorld);
            flashlight.coneRadiusWorld = OptionalNumber(
                    *playerFlashlight, "coneRadiusWorld", flashlightContext)
                    .value_or(flashlight.coneRadiusWorld);
            const auto tint = playerFlashlight->find("tint");
            if (tint != playerFlashlight->end()) {
                flashlight.tint = ReadColor(*tint, flashlightContext + ".tint");
            }
            flashlight.hotspotRadiusRatio = OptionalNumber(
                    *playerFlashlight, "hotspotRadiusRatio", flashlightContext)
                    .value_or(flashlight.hotspotRadiusRatio);
            flashlight.spillBrightness = OptionalNumber(
                    *playerFlashlight, "spillBrightness", flashlightContext)
                    .value_or(flashlight.spillBrightness);
            flashlight.edgeSoftness = OptionalNumber(
                    *playerFlashlight, "edgeSoftness", flashlightContext)
                    .value_or(flashlight.edgeSoftness);
            flashlight.beamHaze = OptionalNumber(
                    *playerFlashlight, "beamHaze", flashlightContext)
                    .value_or(flashlight.beamHaze);
            flashlight.shadowSoftness = OptionalNumber(
                    *playerFlashlight, "shadowSoftness", flashlightContext)
                    .value_or(flashlight.shadowSoftness);
            flashlight.shadowContactOffsetWorld = OptionalNumber(
                    *playerFlashlight,
                    "shadowContactOffsetWorld",
                    flashlightContext)
                    .value_or(flashlight.shadowContactOffsetWorld);
            flashlight.heightAboveEyeWorld = OptionalNumber(
                    *playerFlashlight, "heightAboveEyeWorld", flashlightContext)
                    .value_or(flashlight.heightAboveEyeWorld);
            flashlight.lateralOffsetWorld = OptionalNumber(
                    *playerFlashlight, "lateralOffsetWorld", flashlightContext)
                    .value_or(flashlight.lateralOffsetWorld);
            flashlight.aimConvergenceDistanceWorld = OptionalNumber(
                    *playerFlashlight,
                    "aimConvergenceDistanceWorld",
                    flashlightContext)
                    .value_or(flashlight.aimConvergenceDistanceWorld);
            flashlight.aimResponseSeconds = OptionalNumber(
                    *playerFlashlight, "aimResponseSeconds", flashlightContext)
                    .value_or(flashlight.aimResponseSeconds);
            const std::string flashlightError =
                    PlayerFlashlightSettingsError(flashlight);
            if (!flashlightError.empty()) {
                Fail(flashlightContext + "." + flashlightError);
            }
        }
        const auto playerHealth = root.find("playerHealth");
        if (playerHealth != root.end()) {
            const std::string healthContext = "application settings.playerHealth";
            if (!playerHealth->is_object()) {
                Fail(healthContext + " must be an object");
            }
            const auto lowHealthVisual = playerHealth->find("lowHealthVisual");
            if (lowHealthVisual != playerHealth->end()) {
                const std::string visualContext =
                        healthContext + ".lowHealthVisual";
                if (!lowHealthVisual->is_object()) {
                    Fail(visualContext + " must be an object");
                }
                PlayerLowHealthVisualApplicationSettings& visual =
                        parsed.playerHealth.lowHealthVisual;
                visual.enabled = OptionalBoolean(
                        *lowHealthVisual,
                        "enabled",
                        visualContext).value_or(visual.enabled);
                visual.thresholdRatio = OptionalNumber(
                        *lowHealthVisual,
                        "thresholdRatio",
                        visualContext).value_or(visual.thresholdRatio);
                const auto vignetteColor = lowHealthVisual->find(
                        "vignetteColor");
                if (vignetteColor != lowHealthVisual->end()) {
                    visual.vignetteColor = ReadColor(
                            *vignetteColor,
                            visualContext + ".vignetteColor");
                }
                visual.vignetteInnerRadius = OptionalNumber(
                        *lowHealthVisual,
                        "vignetteInnerRadius",
                        visualContext).value_or(visual.vignetteInnerRadius);
                visual.vignetteOuterRadius = OptionalNumber(
                        *lowHealthVisual,
                        "vignetteOuterRadius",
                        visualContext).value_or(visual.vignetteOuterRadius);
                visual.maximumVignetteOpacity = OptionalNumber(
                        *lowHealthVisual,
                        "maximumVignetteOpacity",
                        visualContext).value_or(
                                visual.maximumVignetteOpacity);
                visual.maximumDesaturation = OptionalNumber(
                        *lowHealthVisual,
                        "maximumDesaturation",
                        visualContext).value_or(
                                visual.maximumDesaturation);
            }
            const auto heartbeatAudio = playerHealth->find("heartbeatAudio");
            if (heartbeatAudio != playerHealth->end()) {
                const std::string heartbeatContext =
                        healthContext + ".heartbeatAudio";
                if (!heartbeatAudio->is_object()) {
                    Fail(heartbeatContext + " must be an object");
                }
                PlayerHeartbeatAudioApplicationSettings& heartbeat =
                        parsed.playerHealth.heartbeatAudio;
                heartbeat.enabled = OptionalBoolean(
                        *heartbeatAudio,
                        "enabled",
                        heartbeatContext).value_or(heartbeat.enabled);
                heartbeat.startThresholdRatio = OptionalNumber(
                        *heartbeatAudio,
                        "startThresholdRatio",
                        heartbeatContext).value_or(
                                heartbeat.startThresholdRatio);
                heartbeat.fullEffectRatio = OptionalNumber(
                        *heartbeatAudio,
                        "fullEffectRatio",
                        heartbeatContext).value_or(
                                heartbeat.fullEffectRatio);
                heartbeat.maximumVolume = OptionalNumber(
                        *heartbeatAudio,
                        "maximumVolume",
                        heartbeatContext).value_or(heartbeat.maximumVolume);
                heartbeat.startPitch = OptionalNumber(
                        *heartbeatAudio,
                        "startPitch",
                        heartbeatContext).value_or(heartbeat.startPitch);
                heartbeat.maximumPitch = OptionalNumber(
                        *heartbeatAudio,
                        "maximumPitch",
                        heartbeatContext).value_or(heartbeat.maximumPitch);
                heartbeat.responseSeconds = OptionalNumber(
                        *heartbeatAudio,
                        "responseSeconds",
                        heartbeatContext).value_or(
                                heartbeat.responseSeconds);
            }
            const auto lowHealthMovement = playerHealth->find(
                    "lowHealthMovement");
            if (lowHealthMovement != playerHealth->end()) {
                const std::string movementContext =
                        healthContext + ".lowHealthMovement";
                if (!lowHealthMovement->is_object()) {
                    Fail(movementContext + " must be an object");
                }
                PlayerLowHealthMovementApplicationSettings& movement =
                        parsed.playerHealth.lowHealthMovement;
                movement.enabled = OptionalBoolean(
                        *lowHealthMovement,
                        "enabled",
                        movementContext).value_or(movement.enabled);
                movement.startThresholdRatio = OptionalNumber(
                        *lowHealthMovement,
                        "startThresholdRatio",
                        movementContext).value_or(
                                movement.startThresholdRatio);
                movement.minimumSpeedScale = OptionalNumber(
                        *lowHealthMovement,
                        "minimumSpeedScale",
                        movementContext).value_or(
                                movement.minimumSpeedScale);
                movement.minimumSprintSpeedScale = OptionalNumber(
                        *lowHealthMovement,
                        "minimumSprintSpeedScale",
                        movementContext).value_or(
                                movement.minimumSpeedScale);
            }
            const auto lowHealthCamera = playerHealth->find(
                    "lowHealthCamera");
            if (lowHealthCamera != playerHealth->end()) {
                const std::string cameraContext =
                        healthContext + ".lowHealthCamera";
                if (!lowHealthCamera->is_object()) {
                    Fail(cameraContext + " must be an object");
                }
                PlayerLowHealthCameraApplicationSettings& camera =
                        parsed.playerHealth.lowHealthCamera;
                camera.enabled = OptionalBoolean(
                        *lowHealthCamera,
                        "enabled",
                        cameraContext).value_or(camera.enabled);
                camera.startThresholdRatio = OptionalNumber(
                        *lowHealthCamera,
                        "startThresholdRatio",
                        cameraContext).value_or(
                                camera.startThresholdRatio);
                camera.fullEffectRatio = OptionalNumber(
                        *lowHealthCamera,
                        "fullEffectRatio",
                        cameraContext).value_or(camera.fullEffectRatio);
                camera.lateralAmplitudeWorld = OptionalNumber(
                        *lowHealthCamera,
                        "lateralAmplitudeWorld",
                        cameraContext).value_or(
                                camera.lateralAmplitudeWorld);
                camera.verticalAmplitudeWorld = OptionalNumber(
                        *lowHealthCamera,
                        "verticalAmplitudeWorld",
                        cameraContext).value_or(
                                camera.verticalAmplitudeWorld);
                camera.pitchAmplitudeDegrees = OptionalNumber(
                        *lowHealthCamera,
                        "pitchAmplitudeDegrees",
                        cameraContext).value_or(
                                camera.pitchAmplitudeDegrees);
                camera.yawAmplitudeDegrees = OptionalNumber(
                        *lowHealthCamera,
                        "yawAmplitudeDegrees",
                        cameraContext).value_or(
                                camera.yawAmplitudeDegrees);
                camera.rollAmplitudeDegrees = OptionalNumber(
                        *lowHealthCamera,
                        "rollAmplitudeDegrees",
                        cameraContext).value_or(
                                camera.rollAmplitudeDegrees);
                camera.frequencyHz = OptionalNumber(
                        *lowHealthCamera,
                        "frequencyHz",
                        cameraContext).value_or(camera.frequencyHz);
                camera.responseSeconds = OptionalNumber(
                        *lowHealthCamera,
                        "responseSeconds",
                        cameraContext).value_or(camera.responseSeconds);
            }
            const std::string healthError = PlayerHealthSettingsError(
                    parsed.playerHealth);
            if (!healthError.empty()) {
                Fail(healthContext + "." + healthError);
            }
        }
        const auto graphics = root.find("graphics");
        if (graphics != root.end()) {
            if (!graphics->is_object()) {
                Fail("application settings.graphics must be an object");
            }
            const auto renderScale = graphics->find("renderScale");
            if (renderScale != graphics->end()) {
                if (!renderScale->is_number()) {
                    Fail("application settings.graphics.renderScale must be a number");
                }
                const double value = renderScale->get<double>();
                if (!std::isfinite(value) || value < 0.5 || value > 2.0) {
                    Fail("application settings.graphics.renderScale must be between 0.5 and 2.0");
                }
                parsed.graphics.renderScale = static_cast<float>(value);
            }
            const auto fxaa = graphics->find("fxaa");
            if (fxaa != graphics->end()) {
                if (!fxaa->is_boolean()) {
                    Fail("application settings.graphics.fxaa must be a boolean");
                }
                parsed.graphics.fxaa = fxaa->get<bool>();
            }
            const auto dynamicLightFadeIn =
                    graphics->find("dynamicLightFadeInSeconds");
            if (dynamicLightFadeIn != graphics->end()) {
                if (!dynamicLightFadeIn->is_number()) {
                    Fail("application settings.graphics.dynamicLightFadeInSeconds must be a number");
                }
                const double value = dynamicLightFadeIn->get<double>();
                if (!std::isfinite(value)
                        || value < MinFpsDynamicLightFadeInSeconds
                        || value > MaxFpsDynamicLightFadeInSeconds) {
                    Fail("application settings.graphics.dynamicLightFadeInSeconds must be between 0.0 and 2.0");
                }
                parsed.graphics.dynamicLightFadeInSeconds =
                        static_cast<float>(value);
            }
            const auto performanceOverlay = graphics->find("performanceOverlay");
            if (performanceOverlay != graphics->end()) {
                if (!performanceOverlay->is_boolean()) {
                    Fail("application settings.graphics.performanceOverlay must be a boolean");
                }
                parsed.graphics.performanceOverlay = performanceOverlay->get<bool>();
            }
            const auto showFpsCounter = graphics->find("showFpsCounter");
            if (showFpsCounter != graphics->end()) {
                if (!showFpsCounter->is_boolean()) {
                    Fail("application settings.graphics.showFpsCounter must be a boolean");
                }
                parsed.graphics.showFpsCounter = showFpsCounter->get<bool>();
            }
            const auto vsync = graphics->find("vsync");
            if (vsync != graphics->end()) {
                if (!vsync->is_boolean()) {
                    Fail("application settings.graphics.vsync must be a boolean");
                }
                parsed.graphics.vsync = vsync->get<bool>();
            }
            const auto horizontalFov = graphics->find("horizontalFovDegrees");
            if (horizontalFov != graphics->end()) {
                if (!horizontalFov->is_number_integer()) {
                    Fail("application settings.graphics.horizontalFovDegrees must be an integer");
                }
                const int value = horizontalFov->get<int>();
                if (value < MinFpsHorizontalFovDegrees
                        || value > MaxFpsHorizontalFovDegrees) {
                    Fail("application settings.graphics.horizontalFovDegrees must be between 70 and 120");
                }
                parsed.graphics.horizontalFovDegrees = value;
            }
            const auto parseQuality = [](const Json& value, const char* context) {
                if (!value.is_string()) Fail(std::string(context) + " must be a string");
                const std::string name = value.get<std::string>();
                if (name == "off") return 0;
                if (name == "low") return 1;
                if (name == "medium") return 2;
                if (name == "high") return 3;
                Fail(std::string(context) + " must be off, low, medium, or high");
            };
            const auto shadows = graphics->find("shadowQuality");
            if (shadows != graphics->end()) {
                parsed.graphics.shadowQuality = static_cast<FpsShadowQuality>(
                        parseQuality(*shadows, "application settings.graphics.shadowQuality"));
            }
            const auto maxDynamicLights = graphics->find("maxDynamicLights");
            if (maxDynamicLights != graphics->end()) {
                if (!maxDynamicLights->is_number_integer()) {
                    Fail("application settings.graphics.maxDynamicLights must be an integer");
                }
                const int value = maxDynamicLights->get<int>();
                if (value < MinFpsDynamicLights || value > MaxFpsDynamicLights) {
                    Fail("application settings.graphics.maxDynamicLights must be between 0 and 32");
                }
                parsed.graphics.maxDynamicLights = value;
            }
            const auto maxShadowLightUpdates =
                    graphics->find("maxShadowLightUpdatesPerFrame");
            if (maxShadowLightUpdates != graphics->end()) {
                if (!maxShadowLightUpdates->is_number_integer()) {
                    Fail("application settings.graphics.maxShadowLightUpdatesPerFrame must be an integer");
                }
                const int value = maxShadowLightUpdates->get<int>();
                if (value < MinFpsShadowLightUpdatesPerFrame
                        || value > MaxFpsShadowLightUpdatesPerFrame) {
                    Fail("application settings.graphics.maxShadowLightUpdatesPerFrame must be between 0 and 32");
                }
                parsed.graphics.maxShadowLightUpdatesPerFrame = value;
            }
            const auto depthPrepass = graphics->find("depthPrepass");
            if (depthPrepass != graphics->end()) {
                if (!depthPrepass->is_boolean()) {
                    Fail("application settings.graphics.depthPrepass must be a boolean");
                }
                parsed.graphics.depthPrepass = depthPrepass->get<bool>();
            }
            parsed.graphics = NormalizeFpsGraphicsSettings(parsed.graphics);
        }
        const auto toneMapping = root.find("toneMapping");
        if (toneMapping != root.end()) {
            const std::string context = "application settings.toneMapping";
            if (!toneMapping->is_object()) {
                Fail(context + " must be an object");
            }
            const auto toneMapper = toneMapping->find("operator");
            if (toneMapper != toneMapping->end()) {
                if (!toneMapper->is_string()) {
                    Fail(context + ".operator must be a string");
                }
                const std::string name = toneMapper->get<std::string>();
                if (name == "khronosPbrNeutral") {
                    parsed.toneMapping.toneMapper =
                            engine::ToneMappingOperator::KhronosPbrNeutral;
                } else if (name == "acesFilmicFitted") {
                    parsed.toneMapping.toneMapper =
                            engine::ToneMappingOperator::AcesFilmicFitted;
                } else {
                    Fail(context + ".operator must be khronosPbrNeutral or acesFilmicFitted");
                }
            }
            const auto exposure = toneMapping->find(
                    "exposureCompensationEv");
            if (exposure != toneMapping->end()) {
                if (!exposure->is_number()) {
                    Fail(context + ".exposureCompensationEv must be a number");
                }
                const double value = exposure->get<double>();
                if (!std::isfinite(value)
                        || value < engine::MinimumToneMappingExposureEv
                        || value > engine::MaximumToneMappingExposureEv) {
                    Fail(context + ".exposureCompensationEv must be between -8 and 8");
                }
                parsed.toneMapping.exposureCompensationEv =
                        static_cast<float>(value);
            }
            parsed.toneMapping = engine::NormalizeToneMappingSettings(
                    parsed.toneMapping);
        }
        const auto hdrBloom = root.find("hdrBloom");
        if (hdrBloom != root.end()) {
            if (!hdrBloom->is_object()) {
                Fail("application settings.hdrBloom must be an object");
            }
            const auto readBloomNumber = [&](const char* name, float current,
                                             float minimum, float maximum) {
                const auto it = hdrBloom->find(name);
                if (it == hdrBloom->end()) return current;
                if (!it->is_number()) {
                    Fail(std::string("application settings.hdrBloom.") + name
                            + " must be a number");
                }
                const double value = it->get<double>();
                if (!std::isfinite(value) || value < minimum || value > maximum) {
                    Fail(std::string("application settings.hdrBloom.") + name
                            + " is outside its finite supported range");
                }
                return static_cast<float>(value);
            };
            const auto enabled = hdrBloom->find("enabled");
            if (enabled != hdrBloom->end()) {
                if (!enabled->is_boolean()) {
                    Fail("application settings.hdrBloom.enabled must be a boolean");
                }
                parsed.hdrBloom.enabled = enabled->get<bool>();
            }
            parsed.hdrBloom.threshold = readBloomNumber(
                    "threshold", parsed.hdrBloom.threshold,
                    0.0f, engine::Rgba16fMaximumFinite);
            parsed.hdrBloom.softKnee = readBloomNumber(
                    "softKnee", parsed.hdrBloom.softKnee, 0.0f, 1.0f);
            parsed.hdrBloom.intensity = readBloomNumber(
                    "intensity", parsed.hdrBloom.intensity, 0.0f, 16.0f);
            parsed.hdrBloom.radius = readBloomNumber(
                    "radius", parsed.hdrBloom.radius, 0.25f, 4.0f);
        }
        const auto footsteps = root.find("footsteps");
        if (footsteps != root.end()) {
            if (!footsteps->is_object()) {
                Fail("application settings.footsteps must be an object");
            }
            const auto defaultSet = footsteps->find("defaultSet");
            if (defaultSet != footsteps->end()) {
                if (!defaultSet->is_string()) {
                    Fail("application settings.footsteps.defaultSet must be a string");
                }
                parsed.footsteps.defaultSet = defaultSet->get<std::string>();
                if (!ValidSoundSetId(parsed.footsteps.defaultSet)) {
                    Fail("application settings.footsteps.defaultSet must be a safe relative footstep set id");
                }
            }
            const auto volume = footsteps->find("volume");
            if (volume != footsteps->end()) {
                if (!volume->is_number()) {
                    Fail("application settings.footsteps.volume must be a number");
                }
                const double value = volume->get<double>();
                if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
                    Fail("application settings.footsteps.volume must be between 0 and 1");
                }
                parsed.footsteps.volume = static_cast<float>(value);
            }
            const auto impactMultiplier = footsteps->find(
                    "landingImpactVolumeMultiplier");
            if (impactMultiplier != footsteps->end()) {
                if (!impactMultiplier->is_number()) {
                    Fail("application settings.footsteps.landingImpactVolumeMultiplier must be a number");
                }
                const double value = impactMultiplier->get<double>();
                if (!std::isfinite(value)
                        || value < 0.0
                        || value > std::numeric_limits<float>::max()) {
                    Fail("application settings.footsteps.landingImpactVolumeMultiplier must be non-negative");
                }
                parsed.footsteps.landingImpactVolumeMultiplier =
                        static_cast<float>(value);
            }
            const auto readNoiseRadius = [&](const char* name, float& output) {
                const auto member = footsteps->find(name);
                if (member == footsteps->end()) return;
                if (!member->is_number()) {
                    Fail(std::string("application settings.footsteps.") + name
                            + " must be a number");
                }
                const double value = member->get<double>();
                if (!std::isfinite(value) || value < 0.0 || value > 10000.0) {
                    Fail(std::string("application settings.footsteps.") + name
                            + " must be between 0 and 10000");
                }
                output = static_cast<float>(value);
            };
            readNoiseRadius("noiseRadiusWorld", parsed.footsteps.noiseRadiusWorld);
            readNoiseRadius(
                    "landingNoiseRadiusWorld",
                    parsed.footsteps.landingNoiseRadiusWorld);
        }
        const auto playerSounds = root.find("playerSounds");
        if (playerSounds != root.end()) {
            if (!playerSounds->is_object()) {
                Fail("application settings.playerSounds must be an object");
            }
            const auto events = playerSounds->find("events");
            if (events != playerSounds->end()) {
                if (!events->is_object()) {
                    Fail("application settings.playerSounds.events must be an object");
                }
                parsed.playerSounds.events.clear();
                parsed.playerSounds.events.reserve(events->size());
                for (auto it = events->begin(); it != events->end(); ++it) {
                    const std::string context =
                            "application settings.playerSounds.events.'"
                            + it.key() + "'";
                    if (!ValidSoundSetId(it.key())) {
                        Fail(context + " must use a safe event id");
                    }
                    if (!it.value().is_object()) {
                        Fail(context + " must be an object");
                    }
                    PlayerSoundEventSettings event;
                    event.id = it.key();
                    event.set = String(it.value(), "set", context);
                    if (!ValidSoundSetId(event.set)) {
                        Fail(context + ".set must be a safe relative player sound set id");
                    }
                    const auto volume = it.value().find("volume");
                    if (volume != it.value().end()) {
                        if (!volume->is_number()) {
                            Fail(context + ".volume must be a number");
                        }
                        const double value = volume->get<double>();
                        if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
                            Fail(context + ".volume must be between 0 and 1");
                        }
                        event.volume = static_cast<float>(value);
                    }
                    parsed.playerSounds.events.push_back(std::move(event));
                }
            }
        }
        const auto playerStamina = root.find("playerStamina");
        if (playerStamina != root.end()) {
            const std::string staminaContext = "application settings.playerStamina";
            if (!playerStamina->is_object()) {
                Fail(staminaContext + " must be an object");
            }
            parsed.playerStamina.maximum = OptionalNumber(
                    *playerStamina,
                    "maximum",
                    staminaContext).value_or(parsed.playerStamina.maximum);
            parsed.playerStamina.sprintDrainPerSecond = OptionalNumber(
                    *playerStamina,
                    "sprintDrainPerSecond",
                    staminaContext).value_or(
                            parsed.playerStamina.sprintDrainPerSecond);
            parsed.playerStamina.jumpCost = OptionalNumber(
                    *playerStamina,
                    "jumpCost",
                    staminaContext).value_or(parsed.playerStamina.jumpCost);
            parsed.playerStamina.regenerationPerSecond = OptionalNumber(
                    *playerStamina,
                    "regenerationPerSecond",
                    staminaContext).value_or(
                            parsed.playerStamina.regenerationPerSecond);
            parsed.playerStamina.exhaustedRecoveryRatio = OptionalNumber(
                    *playerStamina,
                    "exhaustedRecoveryRatio",
                    staminaContext).value_or(
                            parsed.playerStamina.exhaustedRecoveryRatio);

            const auto windedCamera = playerStamina->find("windedCamera");
            if (windedCamera != playerStamina->end()) {
                const std::string cameraContext = staminaContext
                        + ".windedCamera";
                if (!windedCamera->is_object()) {
                    Fail(cameraContext + " must be an object");
                }
                parsed.playerStamina.windedCamera.enabled = OptionalBoolean(
                        *windedCamera,
                        "enabled",
                        cameraContext).value_or(
                                parsed.playerStamina.windedCamera.enabled);
                parsed.playerStamina.windedCamera.startThresholdRatio =
                        OptionalNumber(
                                *windedCamera,
                                "startThresholdRatio",
                                cameraContext).value_or(
                                        parsed.playerStamina.windedCamera
                                                .startThresholdRatio);
                parsed.playerStamina.windedCamera.verticalAmplitudeWorld =
                        OptionalNumber(
                                *windedCamera,
                                "verticalAmplitudeWorld",
                                cameraContext).value_or(
                                        parsed.playerStamina.windedCamera
                                                .verticalAmplitudeWorld);
                parsed.playerStamina.windedCamera.pitchAmplitudeDegrees =
                        OptionalNumber(
                                *windedCamera,
                                "pitchAmplitudeDegrees",
                                cameraContext).value_or(
                                        parsed.playerStamina.windedCamera
                                                .pitchAmplitudeDegrees);
                parsed.playerStamina.windedCamera.frequencyHz = OptionalNumber(
                        *windedCamera,
                        "frequencyHz",
                        cameraContext).value_or(
                                parsed.playerStamina.windedCamera.frequencyHz);
                parsed.playerStamina.windedCamera.responseSeconds =
                        OptionalNumber(
                                *windedCamera,
                                "responseSeconds",
                                cameraContext).value_or(
                                        parsed.playerStamina.windedCamera
                                                .responseSeconds);
            }

            const auto breathingAudio = playerStamina->find("breathingAudio");
            if (breathingAudio != playerStamina->end()) {
                const std::string breathingContext = staminaContext
                        + ".breathingAudio";
                if (!breathingAudio->is_object()) {
                    Fail(breathingContext + " must be an object");
                }
                parsed.playerStamina.breathingAudio.thresholdRatio =
                        OptionalNumber(
                                *breathingAudio,
                                "thresholdRatio",
                                breathingContext).value_or(
                                        parsed.playerStamina.breathingAudio
                                                .thresholdRatio);
                parsed.playerStamina.breathingAudio.volume = OptionalNumber(
                        *breathingAudio,
                        "volume",
                        breathingContext).value_or(
                                parsed.playerStamina.breathingAudio.volume);
                parsed.playerStamina.breathingAudio.fadeOutSeconds =
                        OptionalNumber(
                                *breathingAudio,
                                "fadeOutSeconds",
                                breathingContext).value_or(
                                        parsed.playerStamina.breathingAudio
                                                .fadeOutSeconds);
            }
            const std::string staminaError = PlayerStaminaSettingsError(
                    parsed.playerStamina);
            if (!staminaError.empty()) {
                Fail(staminaContext + "." + staminaError);
            }
        }
        const auto overrides = root.find("viewmodelOverrides");
        if (overrides != root.end()) {
            if (!overrides->is_object()) Fail("application settings.viewmodelOverrides must be an object");
            for (auto it = overrides->begin(); it != overrides->end(); ++it) {
                if (!it.value().is_object()) Fail("viewmodel override '" + it.key() + "' must be an object");
                FpsApplicationSettingsEntry entry;
                entry.weaponId = it.key();
                entry.viewmodel.position = OptionalVector(it.value(), "position", "viewmodel override '" + it.key() + "'");
                entry.viewmodel.rotationDegrees = OptionalVector(it.value(), "rotationDegrees", "viewmodel override '" + it.key() + "'");
                entry.viewmodel.scale = OptionalNumber(it.value(), "scale", "viewmodel override '" + it.key() + "'");
                entry.viewmodel.verticalFovDegrees = OptionalNumber(it.value(), "verticalFovDegrees", "viewmodel override '" + it.key() + "'");
                const auto holsterTransition =
                        it.value().find("holsterTransition");
                if (holsterTransition != it.value().end()) {
                    const std::string transitionContext =
                            "viewmodel override '" + it.key()
                            + "'.holsterTransition";
                    if (!holsterTransition->is_object()) {
                        Fail(transitionContext + " must be an object");
                    }
                    entry.holsterTransition.holsterDurationSeconds =
                            OptionalNumber(
                                    *holsterTransition,
                                    "holsterDurationSeconds",
                                    transitionContext);
                    entry.holsterTransition.unholsterDurationSeconds =
                            OptionalNumber(
                                    *holsterTransition,
                                    "unholsterDurationSeconds",
                                    transitionContext);
                    entry.holsterTransition.hiddenTranslation =
                            OptionalVector(
                                    *holsterTransition,
                                    "hiddenTranslation",
                                    transitionContext);
                    entry.holsterTransition.hiddenRotationDegrees =
                            OptionalVector(
                                    *holsterTransition,
                                    "hiddenRotationDegrees",
                                    transitionContext);
                }
                const auto gripCorrection = it.value().find("gripCorrection");
                if (gripCorrection != it.value().end()) {
                    const std::string gripContext = "viewmodel override '" + it.key()
                            + "'.gripCorrection";
                    if (!gripCorrection->is_object()) {
                        Fail(gripContext + " must be an object");
                    }
                    entry.gripCorrection.translation = OptionalVector(
                            *gripCorrection, "translation", gripContext);
                    entry.gripCorrection.rotationDegrees = OptionalVector(
                            *gripCorrection, "rotationDegrees", gripContext);
                    entry.gripCorrection.scale = OptionalNumber(
                            *gripCorrection, "scale", gripContext);
                }
                const auto attachmentLighting =
                        it.value().find("attachmentLighting");
                if (attachmentLighting != it.value().end()) {
                    const std::string lightingContext = "viewmodel override '"
                            + it.key() + "'.attachmentLighting";
                    if (!attachmentLighting->is_object()) {
                        Fail(lightingContext + " must be an object");
                    }
                    entry.attachmentLighting.brightnessAdjustment =
                            OptionalNumber(
                                    *attachmentLighting,
                                    "brightnessAdjustment",
                                    lightingContext);
                    entry.attachmentLighting.metallicFactor = OptionalNumber(
                            *attachmentLighting,
                            "metallicFactor",
                            lightingContext);
                    entry.attachmentLighting.roughnessFactor = OptionalNumber(
                            *attachmentLighting,
                            "roughnessFactor",
                            lightingContext);
                }
                const auto firing = it.value().find("firing");
                if (firing != it.value().end()) {
                    const std::string firingContext = "viewmodel override '"
                            + it.key() + "'.firing";
                    if (!firing->is_object()) Fail(firingContext + " must be an object");
                    entry.firing.shotIntervalSeconds = OptionalNumber(*firing, "shotIntervalSeconds", firingContext);
                    entry.firing.recoilTranslationImpulse = OptionalVector(*firing, "recoilTranslationImpulse", firingContext);
                    entry.firing.recoilRotationImpulseDegrees = OptionalVector(*firing, "recoilRotationImpulseDegrees", firingContext);
                    entry.firing.recoilRollVariationDegrees = OptionalNumber(*firing, "recoilRollVariationDegrees", firingContext);
                    entry.firing.recoilSpringFrequencyHz = OptionalNumber(*firing, "recoilSpringFrequencyHz", firingContext);
                    entry.firing.recoilDampingRatio = OptionalNumber(*firing, "recoilDampingRatio", firingContext);
                    entry.firing.cameraRecoilEnabled = OptionalBoolean(*firing, "cameraRecoilEnabled", firingContext);
                    entry.firing.cameraRecoilPitchKickDegrees = OptionalNumber(*firing, "cameraRecoilPitchKickDegrees", firingContext);
                    entry.firing.cameraRecoilPitchVariationDegrees = OptionalNumber(*firing, "cameraRecoilPitchVariationDegrees", firingContext);
                    entry.firing.cameraRecoilYawVariationDegrees = OptionalNumber(*firing, "cameraRecoilYawVariationDegrees", firingContext);
                    entry.firing.cameraRecoilRollVariationDegrees = OptionalNumber(*firing, "cameraRecoilRollVariationDegrees", firingContext);
                    entry.firing.cameraRecoilSpringFrequencyHz = OptionalNumber(*firing, "cameraRecoilSpringFrequencyHz", firingContext);
                    entry.firing.cameraRecoilSpringDampingRatio = OptionalNumber(*firing, "cameraRecoilSpringDampingRatio", firingContext);
                    entry.firing.cameraRecoilMaxPitchDegrees = OptionalNumber(*firing, "cameraRecoilMaxPitchDegrees", firingContext);
                    entry.firing.cameraRecoilMaxYawDegrees = OptionalNumber(*firing, "cameraRecoilMaxYawDegrees", firingContext);
                    entry.firing.cameraRecoilMaxRollDegrees = OptionalNumber(*firing, "cameraRecoilMaxRollDegrees", firingContext);
                    entry.firing.muzzlePosition = OptionalVector(*firing, "muzzlePosition", firingContext);
                    entry.firing.muzzleRotationDegrees = OptionalVector(*firing, "muzzleRotationDegrees", firingContext);
                    entry.firing.flashLifetimeSeconds = OptionalNumber(*firing, "flashLifetimeSeconds", firingContext);
                    entry.firing.flashSizeWorld = OptionalNumber(*firing, "flashSizeWorld", firingContext);
                    entry.firing.flashSizeVariation = OptionalNumber(*firing, "flashSizeVariation", firingContext);
                    entry.firing.flashIrregularity = OptionalNumber(*firing, "flashIrregularity", firingContext);
                    entry.firing.flashForwardStretch = OptionalNumber(*firing, "flashForwardStretch", firingContext);
                    entry.firing.flashMinimumLobeCount = OptionalInteger(*firing, "flashMinimumLobeCount", firingContext);
                    entry.firing.flashMaximumLobeCount = OptionalInteger(*firing, "flashMaximumLobeCount", firingContext);
                    entry.firing.flashRearSuppression = OptionalNumber(*firing, "flashRearSuppression", firingContext);
                    entry.firing.flashEdgeSoftness = OptionalNumber(*firing, "flashEdgeSoftness", firingContext);
                    entry.firing.flashRadianceStrength = OptionalNumber(*firing, "flashRadianceStrength", firingContext);
                    entry.firing.muzzleLightIntensity = OptionalNumber(*firing, "muzzleLightIntensity", firingContext);
                    entry.firing.muzzleLightRadiusWorld = OptionalNumber(*firing, "muzzleLightRadiusWorld", firingContext);
                    entry.firing.muzzleLightLifetimeSeconds = OptionalNumber(*firing, "muzzleLightLifetimeSeconds", firingContext);
                }
                parsed.weapons.push_back(std::move(entry));
            }
        }
        output = std::move(parsed);
        SetError(error, {});
        return true;
    } catch (const std::exception& exception) { SetError(error, exception.what()); return false; }
}

bool LoadFpsApplicationSettings(const std::string& path, FpsApplicationSettings& output, std::string* error)
{
    std::ifstream input(path);
    if (!input) { output = {}; SetError(error, {}); return true; }
    std::ostringstream text; text << input.rdbuf();
    return ParseFpsApplicationSettings(text.str(), output, error);
}

bool SaveFpsApplicationSettings(const std::string& path, const FpsApplicationSettings& settings, std::string* error)
{
    if (!ValidLevelName(settings.firstLevel)) {
        SetError(error, "application settings first level is invalid");
        return false;
    }
    if (!ValidSoundSetId(settings.footsteps.defaultSet)) {
        SetError(error, "application settings default footstep set is invalid");
        return false;
    }
    if (!std::isfinite(settings.footsteps.volume)
            || settings.footsteps.volume < 0.0f
            || settings.footsteps.volume > 1.0f) {
        SetError(error, "application settings footstep volume must be between 0 and 1");
        return false;
    }
    if (!std::isfinite(settings.footsteps.landingImpactVolumeMultiplier)
            || settings.footsteps.landingImpactVolumeMultiplier < 0.0f) {
        SetError(error, "application settings landing footstep volume multiplier must be non-negative");
        return false;
    }
    if (!std::isfinite(settings.footsteps.noiseRadiusWorld)
            || settings.footsteps.noiseRadiusWorld < 0.0f
            || settings.footsteps.noiseRadiusWorld > 10000.0f
            || !std::isfinite(settings.footsteps.landingNoiseRadiusWorld)
            || settings.footsteps.landingNoiseRadiusWorld < 0.0f
            || settings.footsteps.landingNoiseRadiusWorld > 10000.0f) {
        SetError(error, "application settings footstep noise radii must be between 0 and 10000");
        return false;
    }
    if (!std::isfinite(settings.playerInventory.maxCarryWeightKg)
            || settings.playerInventory.maxCarryWeightKg <= 0.0f) {
        SetError(error, "application settings playerInventory.maxCarryWeightKg must be finite and positive");
        return false;
    }
    if (settings.playerInventory.maxSlots < 1
            || settings.playerInventory.maxSlots > 1024) {
        SetError(error, "application settings playerInventory.maxSlots must be between 1 and 1024");
        return false;
    }
    if (!std::isfinite(
                settings.playerInventory.pickupVacuumDurationSeconds)
            || settings.playerInventory.pickupVacuumDurationSeconds <= 0.0f) {
        SetError(error, "application settings playerInventory.pickupVacuumDurationSeconds must be finite and positive");
        return false;
    }
    if (!std::isfinite(
                settings.playerInventory.pickupVacuumTargetHeightWorld)
            || settings.playerInventory.pickupVacuumTargetHeightWorld < 0.0f) {
        SetError(error, "application settings playerInventory.pickupVacuumTargetHeightWorld must be finite and non-negative");
        return false;
    }
    const std::string healthError = PlayerHealthSettingsError(
            settings.playerHealth);
    if (!healthError.empty()) {
        SetError(error, "application settings playerHealth." + healthError);
        return false;
    }
    const std::string staminaError = PlayerStaminaSettingsError(
            settings.playerStamina);
    if (!staminaError.empty()) {
        SetError(error, "application settings playerStamina." + staminaError);
        return false;
    }
    const std::string sneakError = PlayerSneakSettingsError(
            settings.playerSneak);
    if (!sneakError.empty()) {
        SetError(error, "application settings playerSneak." + sneakError);
        return false;
    }
    const std::string flashlightError = PlayerFlashlightSettingsError(
            settings.playerFlashlight);
    if (!flashlightError.empty()) {
        SetError(error,
                "application settings playerFlashlight." + flashlightError);
        return false;
    }
    const engine::ToneMappingSettings normalizedToneMapping =
            engine::NormalizeToneMappingSettings(settings.toneMapping);
    if (settings.toneMapping.toneMapper != normalizedToneMapping.toneMapper
            || !std::isfinite(settings.toneMapping.exposureCompensationEv)
            || settings.toneMapping.exposureCompensationEv
                    < engine::MinimumToneMappingExposureEv
            || settings.toneMapping.exposureCompensationEv
                    > engine::MaximumToneMappingExposureEv) {
        SetError(error, "application settings tone mapping is invalid");
        return false;
    }
    std::unordered_set<std::string> playerSoundEventIds;
    for (const PlayerSoundEventSettings& event : settings.playerSounds.events) {
        if (!ValidSoundSetId(event.id)
                || !ValidSoundSetId(event.set)) {
            SetError(error, "application settings player sound event has an invalid id or set");
            return false;
        }
        if (!std::isfinite(event.volume)
                || event.volume < 0.0f
                || event.volume > 1.0f) {
            SetError(error, "application settings player sound event volume must be between 0 and 1");
            return false;
        }
        if (!playerSoundEventIds.insert(event.id).second) {
            SetError(error, "application settings player sound event ids must be unique");
            return false;
        }
    }
    Json root = {
            {"version", 1},
            {"firstLevel", settings.firstLevel},
            {"consoleEnabled", settings.consoleEnabled},
            {"footsteps", {
                    {"defaultSet", settings.footsteps.defaultSet},
                    {"volume", settings.footsteps.volume},
                    {"landingImpactVolumeMultiplier",
                            settings.footsteps.landingImpactVolumeMultiplier},
                    {"noiseRadiusWorld", settings.footsteps.noiseRadiusWorld},
                    {"landingNoiseRadiusWorld",
                            settings.footsteps.landingNoiseRadiusWorld}}}};
    const FpsGraphicsSettings graphics =
            NormalizeFpsGraphicsSettings(settings.graphics);
    root["graphics"] = {
            {"renderScale", graphics.renderScale},
            {"fxaa", graphics.fxaa},
            {"shadowQuality", FpsShadowQualityName(graphics.shadowQuality)},
            {"maxDynamicLights", graphics.maxDynamicLights},
            {"maxShadowLightUpdatesPerFrame",
                    graphics.maxShadowLightUpdatesPerFrame},
            {"dynamicLightFadeInSeconds",
                    graphics.dynamicLightFadeInSeconds},
            {"depthPrepass", graphics.depthPrepass},
            {"showFpsCounter", graphics.showFpsCounter},
            {"performanceOverlay", graphics.performanceOverlay},
            {"vsync", graphics.vsync},
            {"horizontalFovDegrees", graphics.horizontalFovDegrees}};
    root["toneMapping"] = {
            {"operator", engine::ToneMappingOperatorName(
                    normalizedToneMapping.toneMapper)},
            {"exposureCompensationEv",
                    normalizedToneMapping.exposureCompensationEv}};
    root["playerInventory"] = {
            {"maxCarryWeightKg", settings.playerInventory.maxCarryWeightKg},
            {"maxSlots", settings.playerInventory.maxSlots},
            {"pickupVacuumDurationSeconds",
                    settings.playerInventory.pickupVacuumDurationSeconds},
            {"pickupVacuumTargetHeightWorld",
                    settings.playerInventory.pickupVacuumTargetHeightWorld}};
    root["playerSneak"] = {
            {"fullVisibilityLightLevel",
                    settings.playerSneak.fullVisibilityLightLevel},
            {"darknessCutoffNormalized",
                    settings.playerSneak.darknessCutoffNormalized},
            {"lightHalfResponseRangeNormalized",
                    settings.playerSneak.lightHalfResponseRangeNormalized},
            {"visualDetectionBuildSeconds",
                    settings.playerSneak.visualDetectionBuildSeconds},
            {"visualDetectionDecaySeconds",
                    settings.playerSneak.visualDetectionDecaySeconds},
            {"darknessProximityRangeWorld",
                    settings.playerSneak.darknessProximityRangeWorld},
            {"crouchVisualDetectionMultiplier",
                    settings.playerSneak.crouchVisualDetectionMultiplier},
            {"crouchMovementNoiseMultiplier",
                    settings.playerSneak.crouchMovementNoiseMultiplier}};
    root["playerFlashlight"] = {
            {"intensity", settings.playerFlashlight.intensity},
            {"reachWorld", settings.playerFlashlight.reachWorld},
            {"coneRadiusWorld", settings.playerFlashlight.coneRadiusWorld},
            {"tint", ColorValue(settings.playerFlashlight.tint)},
            {"hotspotRadiusRatio",
                    settings.playerFlashlight.hotspotRadiusRatio},
            {"spillBrightness", settings.playerFlashlight.spillBrightness},
            {"edgeSoftness", settings.playerFlashlight.edgeSoftness},
            {"beamHaze", settings.playerFlashlight.beamHaze},
            {"shadowSoftness", settings.playerFlashlight.shadowSoftness},
            {"shadowContactOffsetWorld",
                    settings.playerFlashlight.shadowContactOffsetWorld},
            {"heightAboveEyeWorld",
                    settings.playerFlashlight.heightAboveEyeWorld},
            {"lateralOffsetWorld",
                    settings.playerFlashlight.lateralOffsetWorld},
            {"aimConvergenceDistanceWorld",
                    settings.playerFlashlight.aimConvergenceDistanceWorld},
            {"aimResponseSeconds",
                    settings.playerFlashlight.aimResponseSeconds}};
    const PlayerLowHealthVisualApplicationSettings& lowHealthVisual =
            settings.playerHealth.lowHealthVisual;
    const PlayerHeartbeatAudioApplicationSettings& heartbeatAudio =
            settings.playerHealth.heartbeatAudio;
    const PlayerLowHealthMovementApplicationSettings& lowHealthMovement =
            settings.playerHealth.lowHealthMovement;
    const PlayerLowHealthCameraApplicationSettings& lowHealthCamera =
            settings.playerHealth.lowHealthCamera;
    root["playerHealth"] = {
        {"lowHealthVisual", {
            {"enabled", lowHealthVisual.enabled},
            {"thresholdRatio", lowHealthVisual.thresholdRatio},
            {"vignetteColor", ColorValue(lowHealthVisual.vignetteColor)},
            {"vignetteInnerRadius", lowHealthVisual.vignetteInnerRadius},
            {"vignetteOuterRadius", lowHealthVisual.vignetteOuterRadius},
            {"maximumVignetteOpacity",
                    lowHealthVisual.maximumVignetteOpacity},
            {"maximumDesaturation",
                    lowHealthVisual.maximumDesaturation}}},
        {"heartbeatAudio", {
            {"enabled", heartbeatAudio.enabled},
            {"startThresholdRatio", heartbeatAudio.startThresholdRatio},
            {"fullEffectRatio", heartbeatAudio.fullEffectRatio},
            {"maximumVolume", heartbeatAudio.maximumVolume},
            {"startPitch", heartbeatAudio.startPitch},
            {"maximumPitch", heartbeatAudio.maximumPitch},
            {"responseSeconds", heartbeatAudio.responseSeconds}}},
        {"lowHealthMovement", {
            {"enabled", lowHealthMovement.enabled},
            {"startThresholdRatio", lowHealthMovement.startThresholdRatio},
            {"minimumSpeedScale", lowHealthMovement.minimumSpeedScale},
            {"minimumSprintSpeedScale",
                    lowHealthMovement.minimumSprintSpeedScale}}},
        {"lowHealthCamera", {
            {"enabled", lowHealthCamera.enabled},
            {"startThresholdRatio", lowHealthCamera.startThresholdRatio},
            {"fullEffectRatio", lowHealthCamera.fullEffectRatio},
            {"lateralAmplitudeWorld",
                    lowHealthCamera.lateralAmplitudeWorld},
            {"verticalAmplitudeWorld",
                    lowHealthCamera.verticalAmplitudeWorld},
            {"pitchAmplitudeDegrees",
                    lowHealthCamera.pitchAmplitudeDegrees},
            {"yawAmplitudeDegrees", lowHealthCamera.yawAmplitudeDegrees},
            {"rollAmplitudeDegrees", lowHealthCamera.rollAmplitudeDegrees},
            {"frequencyHz", lowHealthCamera.frequencyHz},
            {"responseSeconds", lowHealthCamera.responseSeconds}}}};
    const engine::HdrBloomSettings hdrBloom =
            engine::NormalizeHdrBloomSettings(settings.hdrBloom);
    root["hdrBloom"] = {
            {"enabled", hdrBloom.enabled},
            {"threshold", hdrBloom.threshold},
            {"softKnee", hdrBloom.softKnee},
            {"intensity", hdrBloom.intensity},
            {"radius", hdrBloom.radius}};
    Json playerSoundEvents = Json::object();
    for (const PlayerSoundEventSettings& event : settings.playerSounds.events) {
        playerSoundEvents[event.id] = {
                {"set", event.set},
                {"volume", event.volume}};
    }
    root["playerSounds"] = {{"events", std::move(playerSoundEvents)}};
    root["playerStamina"] = {
            {"maximum", settings.playerStamina.maximum},
            {"sprintDrainPerSecond",
                    settings.playerStamina.sprintDrainPerSecond},
            {"jumpCost", settings.playerStamina.jumpCost},
            {"regenerationPerSecond",
                    settings.playerStamina.regenerationPerSecond},
            {"exhaustedRecoveryRatio",
                    settings.playerStamina.exhaustedRecoveryRatio},
            {"windedCamera", {
                    {"enabled", settings.playerStamina.windedCamera.enabled},
                    {"startThresholdRatio",
                            settings.playerStamina.windedCamera
                                    .startThresholdRatio},
                    {"verticalAmplitudeWorld",
                            settings.playerStamina.windedCamera
                                    .verticalAmplitudeWorld},
                    {"pitchAmplitudeDegrees",
                            settings.playerStamina.windedCamera
                                    .pitchAmplitudeDegrees},
                    {"frequencyHz",
                            settings.playerStamina.windedCamera.frequencyHz},
                    {"responseSeconds",
                            settings.playerStamina.windedCamera
                                    .responseSeconds}}},
            {"breathingAudio", {
                    {"thresholdRatio",
                            settings.playerStamina.breathingAudio
                                    .thresholdRatio},
                    {"volume", settings.playerStamina.breathingAudio.volume},
                    {"fadeOutSeconds",
                            settings.playerStamina.breathingAudio
                                    .fadeOutSeconds}}}};
    Json overrides = Json::object();
    for (const auto& entry : settings.weapons) {
        Json value = Json::object();
        if (entry.viewmodel.position) value["position"] = Vec(*entry.viewmodel.position);
        if (entry.viewmodel.rotationDegrees) value["rotationDegrees"] = Vec(*entry.viewmodel.rotationDegrees);
        if (entry.viewmodel.scale) value["scale"] = *entry.viewmodel.scale;
        if (entry.viewmodel.verticalFovDegrees) value["verticalFovDegrees"] = *entry.viewmodel.verticalFovDegrees;
        Json holsterTransition = Json::object();
        if (entry.holsterTransition.holsterDurationSeconds) {
            holsterTransition["holsterDurationSeconds"] =
                    *entry.holsterTransition.holsterDurationSeconds;
        }
        if (entry.holsterTransition.unholsterDurationSeconds) {
            holsterTransition["unholsterDurationSeconds"] =
                    *entry.holsterTransition.unholsterDurationSeconds;
        }
        if (entry.holsterTransition.hiddenTranslation) {
            holsterTransition["hiddenTranslation"] = Vec(
                    *entry.holsterTransition.hiddenTranslation);
        }
        if (entry.holsterTransition.hiddenRotationDegrees) {
            holsterTransition["hiddenRotationDegrees"] = Vec(
                    *entry.holsterTransition.hiddenRotationDegrees);
        }
        if (!holsterTransition.empty()) {
            value["holsterTransition"] = std::move(holsterTransition);
        }
        Json gripCorrection = Json::object();
        if (entry.gripCorrection.translation) {
            gripCorrection["translation"] = Vec(*entry.gripCorrection.translation);
        }
        if (entry.gripCorrection.rotationDegrees) {
            gripCorrection["rotationDegrees"] = Vec(
                    *entry.gripCorrection.rotationDegrees);
        }
        if (entry.gripCorrection.scale) {
            gripCorrection["scale"] = *entry.gripCorrection.scale;
        }
        if (!gripCorrection.empty()) {
            value["gripCorrection"] = std::move(gripCorrection);
        }
        Json attachmentLighting = Json::object();
        if (entry.attachmentLighting.brightnessAdjustment) {
            attachmentLighting["brightnessAdjustment"] =
                    *entry.attachmentLighting.brightnessAdjustment;
        }
        if (entry.attachmentLighting.metallicFactor) {
            attachmentLighting["metallicFactor"] =
                    *entry.attachmentLighting.metallicFactor;
        }
        if (entry.attachmentLighting.roughnessFactor) {
            attachmentLighting["roughnessFactor"] =
                    *entry.attachmentLighting.roughnessFactor;
        }
        if (!attachmentLighting.empty()) {
            value["attachmentLighting"] = std::move(attachmentLighting);
        }
        Json firing = Json::object();
        if (entry.firing.shotIntervalSeconds) firing["shotIntervalSeconds"] = *entry.firing.shotIntervalSeconds;
        if (entry.firing.recoilTranslationImpulse) firing["recoilTranslationImpulse"] = Vec(*entry.firing.recoilTranslationImpulse);
        if (entry.firing.recoilRotationImpulseDegrees) firing["recoilRotationImpulseDegrees"] = Vec(*entry.firing.recoilRotationImpulseDegrees);
        if (entry.firing.recoilRollVariationDegrees) firing["recoilRollVariationDegrees"] = *entry.firing.recoilRollVariationDegrees;
        if (entry.firing.recoilSpringFrequencyHz) firing["recoilSpringFrequencyHz"] = *entry.firing.recoilSpringFrequencyHz;
        if (entry.firing.recoilDampingRatio) firing["recoilDampingRatio"] = *entry.firing.recoilDampingRatio;
        if (entry.firing.cameraRecoilEnabled) firing["cameraRecoilEnabled"] = *entry.firing.cameraRecoilEnabled;
        if (entry.firing.cameraRecoilPitchKickDegrees) firing["cameraRecoilPitchKickDegrees"] = *entry.firing.cameraRecoilPitchKickDegrees;
        if (entry.firing.cameraRecoilPitchVariationDegrees) firing["cameraRecoilPitchVariationDegrees"] = *entry.firing.cameraRecoilPitchVariationDegrees;
        if (entry.firing.cameraRecoilYawVariationDegrees) firing["cameraRecoilYawVariationDegrees"] = *entry.firing.cameraRecoilYawVariationDegrees;
        if (entry.firing.cameraRecoilRollVariationDegrees) firing["cameraRecoilRollVariationDegrees"] = *entry.firing.cameraRecoilRollVariationDegrees;
        if (entry.firing.cameraRecoilSpringFrequencyHz) firing["cameraRecoilSpringFrequencyHz"] = *entry.firing.cameraRecoilSpringFrequencyHz;
        if (entry.firing.cameraRecoilSpringDampingRatio) firing["cameraRecoilSpringDampingRatio"] = *entry.firing.cameraRecoilSpringDampingRatio;
        if (entry.firing.cameraRecoilMaxPitchDegrees) firing["cameraRecoilMaxPitchDegrees"] = *entry.firing.cameraRecoilMaxPitchDegrees;
        if (entry.firing.cameraRecoilMaxYawDegrees) firing["cameraRecoilMaxYawDegrees"] = *entry.firing.cameraRecoilMaxYawDegrees;
        if (entry.firing.cameraRecoilMaxRollDegrees) firing["cameraRecoilMaxRollDegrees"] = *entry.firing.cameraRecoilMaxRollDegrees;
        if (entry.firing.muzzlePosition) firing["muzzlePosition"] = Vec(*entry.firing.muzzlePosition);
        if (entry.firing.muzzleRotationDegrees) firing["muzzleRotationDegrees"] = Vec(*entry.firing.muzzleRotationDegrees);
        if (entry.firing.flashLifetimeSeconds) firing["flashLifetimeSeconds"] = *entry.firing.flashLifetimeSeconds;
        if (entry.firing.flashSizeWorld) firing["flashSizeWorld"] = *entry.firing.flashSizeWorld;
        if (entry.firing.flashSizeVariation) firing["flashSizeVariation"] = *entry.firing.flashSizeVariation;
        if (entry.firing.flashIrregularity) firing["flashIrregularity"] = *entry.firing.flashIrregularity;
        if (entry.firing.flashForwardStretch) firing["flashForwardStretch"] = *entry.firing.flashForwardStretch;
        if (entry.firing.flashMinimumLobeCount) firing["flashMinimumLobeCount"] = *entry.firing.flashMinimumLobeCount;
        if (entry.firing.flashMaximumLobeCount) firing["flashMaximumLobeCount"] = *entry.firing.flashMaximumLobeCount;
        if (entry.firing.flashRearSuppression) firing["flashRearSuppression"] = *entry.firing.flashRearSuppression;
        if (entry.firing.flashEdgeSoftness) firing["flashEdgeSoftness"] = *entry.firing.flashEdgeSoftness;
        if (entry.firing.flashRadianceStrength) firing["flashRadianceStrength"] = *entry.firing.flashRadianceStrength;
        if (entry.firing.muzzleLightIntensity) firing["muzzleLightIntensity"] = *entry.firing.muzzleLightIntensity;
        if (entry.firing.muzzleLightRadiusWorld) firing["muzzleLightRadiusWorld"] = *entry.firing.muzzleLightRadiusWorld;
        if (entry.firing.muzzleLightLifetimeSeconds) firing["muzzleLightLifetimeSeconds"] = *entry.firing.muzzleLightLifetimeSeconds;
        if (!firing.empty()) value["firing"] = std::move(firing);
        if (!value.empty()) overrides[entry.weaponId] = std::move(value);
    }
    if (!overrides.empty()) {
        root["viewmodelOverrides"] = std::move(overrides);
    }
    std::ofstream output(path, std::ios::trunc);
    if (!output) { SetError(error, "could not write application settings: " + path); return false; }
    output << root.dump(2) << '\n';
    if (!output) { SetError(error, "failed writing application settings: " + path); return false; }
    SetError(error, {}); return true;
}

const FpsViewmodelPresentationOverride* FindFpsViewmodelOverride(const FpsApplicationSettings& settings, std::string_view id)
{
    const auto it = std::find_if(settings.weapons.begin(), settings.weapons.end(),
            [id](const FpsApplicationSettingsEntry& value) { return value.weaponId == id; });
    return it == settings.weapons.end()
                    || FpsViewmodelOverrideEmpty(it->viewmodel)
            ? nullptr
            : &it->viewmodel;
}

void SetFpsViewmodelOverride(FpsApplicationSettings& settings, std::string id, const FpsViewmodelPresentationOverride& value)
{
    for (auto& entry : settings.weapons) if (entry.weaponId == id) { entry.viewmodel = value; return; }
    FpsApplicationSettingsEntry entry;
    entry.weaponId = std::move(id);
    entry.viewmodel = value;
    settings.weapons.push_back(std::move(entry));
}

void ClearFpsViewmodelOverride(FpsApplicationSettings& settings, std::string_view id)
{
    for (auto& entry : settings.weapons) {
        if (entry.weaponId == id) {
            entry.viewmodel = {};
            break;
        }
    }
    settings.weapons.erase(std::remove_if(
            settings.weapons.begin(), settings.weapons.end(),
            [](const FpsApplicationSettingsEntry& value) {
                return FpsViewmodelOverrideEmpty(value.viewmodel)
                        && FpsViewmodelHolsterTransitionOverrideEmpty(
                                value.holsterTransition)
                        && FpsViewmodelGripCorrectionOverrideEmpty(
                                value.gripCorrection)
                        && FpsViewmodelAttachmentLightingOverrideEmpty(
                                value.attachmentLighting)
                        && FpsWeaponFiringOverrideEmpty(value.firing);
    }), settings.weapons.end());
}

const FpsViewmodelHolsterTransitionOverride*
FindFpsViewmodelHolsterTransitionOverride(
        const FpsApplicationSettings& settings,
        std::string_view id)
{
    const auto it = std::find_if(
            settings.weapons.begin(),
            settings.weapons.end(),
            [id](const FpsApplicationSettingsEntry& value) {
                return value.weaponId == id;
            });
    return it == settings.weapons.end()
                    || FpsViewmodelHolsterTransitionOverrideEmpty(
                            it->holsterTransition)
            ? nullptr
            : &it->holsterTransition;
}

void SetFpsViewmodelHolsterTransitionOverride(
        FpsApplicationSettings& settings,
        std::string id,
        const FpsViewmodelHolsterTransitionOverride& value)
{
    for (auto& entry : settings.weapons) {
        if (entry.weaponId == id) {
            entry.holsterTransition = value;
            return;
        }
    }
    FpsApplicationSettingsEntry entry;
    entry.weaponId = std::move(id);
    entry.holsterTransition = value;
    settings.weapons.push_back(std::move(entry));
}

void ClearFpsViewmodelHolsterTransitionOverride(
        FpsApplicationSettings& settings,
        std::string_view id)
{
    for (auto& entry : settings.weapons) {
        if (entry.weaponId == id) {
            entry.holsterTransition = {};
            break;
        }
    }
    settings.weapons.erase(std::remove_if(
            settings.weapons.begin(), settings.weapons.end(),
            [](const FpsApplicationSettingsEntry& value) {
                return FpsViewmodelOverrideEmpty(value.viewmodel)
                        && FpsViewmodelHolsterTransitionOverrideEmpty(
                                value.holsterTransition)
                        && FpsViewmodelGripCorrectionOverrideEmpty(
                                value.gripCorrection)
                        && FpsViewmodelAttachmentLightingOverrideEmpty(
                                value.attachmentLighting)
                        && FpsWeaponFiringOverrideEmpty(value.firing);
            }), settings.weapons.end());
}

const FpsViewmodelGripCorrectionOverride* FindFpsViewmodelGripCorrectionOverride(
        const FpsApplicationSettings& settings,
        std::string_view id)
{
    const auto it = std::find_if(settings.weapons.begin(), settings.weapons.end(),
            [id](const FpsApplicationSettingsEntry& value) {
                return value.weaponId == id;
            });
    return it == settings.weapons.end()
                    || FpsViewmodelGripCorrectionOverrideEmpty(
                            it->gripCorrection)
            ? nullptr
            : &it->gripCorrection;
}

void SetFpsViewmodelGripCorrectionOverride(
        FpsApplicationSettings& settings,
        std::string id,
        const FpsViewmodelGripCorrectionOverride& value)
{
    for (auto& entry : settings.weapons) {
        if (entry.weaponId == id) {
            entry.gripCorrection = value;
            return;
        }
    }
    FpsApplicationSettingsEntry entry;
    entry.weaponId = std::move(id);
    entry.gripCorrection = value;
    settings.weapons.push_back(std::move(entry));
}

void ClearFpsViewmodelGripCorrectionOverride(
        FpsApplicationSettings& settings,
        std::string_view id)
{
    for (auto& entry : settings.weapons) {
        if (entry.weaponId == id) {
            entry.gripCorrection = {};
            break;
        }
    }
    settings.weapons.erase(std::remove_if(
            settings.weapons.begin(), settings.weapons.end(),
            [](const FpsApplicationSettingsEntry& value) {
                return FpsViewmodelOverrideEmpty(value.viewmodel)
                        && FpsViewmodelHolsterTransitionOverrideEmpty(
                                value.holsterTransition)
                        && FpsViewmodelGripCorrectionOverrideEmpty(
                                value.gripCorrection)
                        && FpsViewmodelAttachmentLightingOverrideEmpty(
                                value.attachmentLighting)
                        && FpsWeaponFiringOverrideEmpty(value.firing);
            }), settings.weapons.end());
}

const FpsViewmodelAttachmentLightingOverride*
FindFpsViewmodelAttachmentLightingOverride(
        const FpsApplicationSettings& settings,
        std::string_view id)
{
    const auto it = std::find_if(
            settings.weapons.begin(),
            settings.weapons.end(),
            [id](const FpsApplicationSettingsEntry& value) {
                return value.weaponId == id;
            });
    return it == settings.weapons.end()
                    || FpsViewmodelAttachmentLightingOverrideEmpty(
                            it->attachmentLighting)
            ? nullptr
            : &it->attachmentLighting;
}

void SetFpsViewmodelAttachmentLightingOverride(
        FpsApplicationSettings& settings,
        std::string id,
        const FpsViewmodelAttachmentLightingOverride& value)
{
    for (auto& entry : settings.weapons) {
        if (entry.weaponId == id) {
            entry.attachmentLighting = value;
            return;
        }
    }
    FpsApplicationSettingsEntry entry;
    entry.weaponId = std::move(id);
    entry.attachmentLighting = value;
    settings.weapons.push_back(std::move(entry));
}

void ClearFpsViewmodelAttachmentLightingOverride(
        FpsApplicationSettings& settings,
        std::string_view id)
{
    for (auto& entry : settings.weapons) {
        if (entry.weaponId == id) {
            entry.attachmentLighting = {};
            break;
        }
    }
    settings.weapons.erase(std::remove_if(
            settings.weapons.begin(), settings.weapons.end(),
            [](const FpsApplicationSettingsEntry& value) {
                return FpsViewmodelOverrideEmpty(value.viewmodel)
                        && FpsViewmodelHolsterTransitionOverrideEmpty(
                                value.holsterTransition)
                        && FpsViewmodelGripCorrectionOverrideEmpty(
                                value.gripCorrection)
                        && FpsViewmodelAttachmentLightingOverrideEmpty(
                                value.attachmentLighting)
                        && FpsWeaponFiringOverrideEmpty(value.firing);
            }), settings.weapons.end());
}

FpsViewmodelPresentation ClampFpsViewmodelPresentation(FpsViewmodelPresentation value)
{
    value.position.x = std::clamp(value.position.x, -10.0f, 10.0f);
    value.position.y = std::clamp(value.position.y, -10.0f, 10.0f);
    value.position.z = std::clamp(value.position.z, -10.0f, 10.0f);
    value.rotationDegrees.x = std::clamp(value.rotationDegrees.x, -360.0f, 360.0f);
    value.rotationDegrees.y = std::clamp(value.rotationDegrees.y, -360.0f, 360.0f);
    value.rotationDegrees.z = std::clamp(value.rotationDegrees.z, -360.0f, 360.0f);
    value.scale = std::clamp(value.scale, 0.01f, 10.0f);
    value.verticalFovDegrees = std::clamp(value.verticalFovDegrees, 20.0f, 120.0f);
    return value;
}

FpsViewmodelPresentation ResolveFpsViewmodelPresentation(const FpsViewmodelPresentation& defaults, const FpsViewmodelPresentationOverride* value)
{
    FpsViewmodelPresentation result = defaults;
    if (value) {
        if (value->position) result.position = *value->position;
        if (value->rotationDegrees) result.rotationDegrees = *value->rotationDegrees;
        if (value->scale) result.scale = *value->scale;
        if (value->verticalFovDegrees) result.verticalFovDegrees = *value->verticalFovDegrees;
    }
    return ClampFpsViewmodelPresentation(result);
}

FpsViewmodelPresentationOverride BuildFpsViewmodelOverride(const FpsViewmodelPresentation& defaults, const FpsViewmodelPresentation& effective)
{
    const FpsViewmodelPresentation clean = ClampFpsViewmodelPresentation(effective);
    FpsViewmodelPresentationOverride result;
    if (!Same(defaults.position, clean.position)) result.position = clean.position;
    if (!Same(defaults.rotationDegrees, clean.rotationDegrees)) result.rotationDegrees = clean.rotationDegrees;
    if (!NearlyEqual(defaults.scale, clean.scale)) result.scale = clean.scale;
    if (!NearlyEqual(defaults.verticalFovDegrees, clean.verticalFovDegrees)) result.verticalFovDegrees = clean.verticalFovDegrees;
    return result;
}

bool FpsViewmodelOverrideEmpty(const FpsViewmodelPresentationOverride& value)
{
    return !value.position && !value.rotationDegrees && !value.scale && !value.verticalFovDegrees;
}

FpsViewmodelHolsterTransition ClampFpsViewmodelHolsterTransition(
        FpsViewmodelHolsterTransition value)
{
    value.holsterDurationSeconds = std::clamp(
            value.holsterDurationSeconds, 0.05f, 2.0f);
    value.unholsterDurationSeconds = std::clamp(
            value.unholsterDurationSeconds, 0.05f, 2.0f);
    value.hiddenTranslation.x = std::clamp(
            value.hiddenTranslation.x, -10.0f, 10.0f);
    value.hiddenTranslation.y = std::clamp(
            value.hiddenTranslation.y, -10.0f, 10.0f);
    value.hiddenTranslation.z = std::clamp(
            value.hiddenTranslation.z, -10.0f, 10.0f);
    value.hiddenRotationDegrees.x = std::clamp(
            value.hiddenRotationDegrees.x, -360.0f, 360.0f);
    value.hiddenRotationDegrees.y = std::clamp(
            value.hiddenRotationDegrees.y, -360.0f, 360.0f);
    value.hiddenRotationDegrees.z = std::clamp(
            value.hiddenRotationDegrees.z, -360.0f, 360.0f);
    return value;
}

FpsViewmodelHolsterTransition ResolveFpsViewmodelHolsterTransition(
        const FpsViewmodelHolsterTransition& defaults,
        const FpsViewmodelHolsterTransitionOverride* value)
{
    FpsViewmodelHolsterTransition result = defaults;
    if (value != nullptr) {
        if (value->holsterDurationSeconds) {
            result.holsterDurationSeconds = *value->holsterDurationSeconds;
        }
        if (value->unholsterDurationSeconds) {
            result.unholsterDurationSeconds =
                    *value->unholsterDurationSeconds;
        }
        if (value->hiddenTranslation) {
            result.hiddenTranslation = *value->hiddenTranslation;
        }
        if (value->hiddenRotationDegrees) {
            result.hiddenRotationDegrees = *value->hiddenRotationDegrees;
        }
    }
    return ClampFpsViewmodelHolsterTransition(result);
}

FpsViewmodelHolsterTransitionOverride BuildFpsViewmodelHolsterTransitionOverride(
        const FpsViewmodelHolsterTransition& defaults,
        const FpsViewmodelHolsterTransition& effective)
{
    const FpsViewmodelHolsterTransition clean =
            ClampFpsViewmodelHolsterTransition(effective);
    FpsViewmodelHolsterTransitionOverride result;
    if (!NearlyEqual(
                defaults.holsterDurationSeconds,
                clean.holsterDurationSeconds)) {
        result.holsterDurationSeconds = clean.holsterDurationSeconds;
    }
    if (!NearlyEqual(
                defaults.unholsterDurationSeconds,
                clean.unholsterDurationSeconds)) {
        result.unholsterDurationSeconds = clean.unholsterDurationSeconds;
    }
    if (!Same(defaults.hiddenTranslation, clean.hiddenTranslation)) {
        result.hiddenTranslation = clean.hiddenTranslation;
    }
    if (!Same(
                defaults.hiddenRotationDegrees,
                clean.hiddenRotationDegrees)) {
        result.hiddenRotationDegrees = clean.hiddenRotationDegrees;
    }
    return result;
}

bool FpsViewmodelHolsterTransitionOverrideEmpty(
        const FpsViewmodelHolsterTransitionOverride& value)
{
    return !value.holsterDurationSeconds
            && !value.unholsterDurationSeconds
            && !value.hiddenTranslation
            && !value.hiddenRotationDegrees;
}

FpsViewmodelGripCorrection ClampFpsViewmodelGripCorrection(
        FpsViewmodelGripCorrection value)
{
    value.translation.x = std::clamp(value.translation.x, -1.0f, 1.0f);
    value.translation.y = std::clamp(value.translation.y, -1.0f, 1.0f);
    value.translation.z = std::clamp(value.translation.z, -1.0f, 1.0f);
    value.rotationDegrees.x = std::clamp(
            value.rotationDegrees.x, -360.0f, 360.0f);
    value.rotationDegrees.y = std::clamp(
            value.rotationDegrees.y, -360.0f, 360.0f);
    value.rotationDegrees.z = std::clamp(
            value.rotationDegrees.z, -360.0f, 360.0f);
    value.scale = std::clamp(value.scale, 0.01f, 10.0f);
    return value;
}

FpsViewmodelGripCorrection ResolveFpsViewmodelGripCorrection(
        const FpsViewmodelGripCorrection& defaults,
        const FpsViewmodelGripCorrectionOverride* value)
{
    FpsViewmodelGripCorrection result = defaults;
    if (value != nullptr) {
        if (value->translation) result.translation = *value->translation;
        if (value->rotationDegrees) {
            result.rotationDegrees = *value->rotationDegrees;
        }
        if (value->scale) result.scale = *value->scale;
    }
    return ClampFpsViewmodelGripCorrection(result);
}

FpsViewmodelGripCorrectionOverride BuildFpsViewmodelGripCorrectionOverride(
        const FpsViewmodelGripCorrection& defaults,
        const FpsViewmodelGripCorrection& effective)
{
    const FpsViewmodelGripCorrection clean =
            ClampFpsViewmodelGripCorrection(effective);
    FpsViewmodelGripCorrectionOverride result;
    if (!Same(defaults.translation, clean.translation)) {
        result.translation = clean.translation;
    }
    if (!Same(defaults.rotationDegrees, clean.rotationDegrees)) {
        result.rotationDegrees = clean.rotationDegrees;
    }
    if (!NearlyEqual(defaults.scale, clean.scale)) result.scale = clean.scale;
    return result;
}

bool FpsViewmodelGripCorrectionOverrideEmpty(
        const FpsViewmodelGripCorrectionOverride& value)
{
    return !value.translation && !value.rotationDegrees && !value.scale;
}

FpsViewmodelAttachmentLighting ClampFpsViewmodelAttachmentLighting(
        FpsViewmodelAttachmentLighting value)
{
    value.brightnessAdjustment = std::clamp(
            value.brightnessAdjustment, -1.0f, 1.0f);
    value.materialOverride.metallicFactor = std::clamp(
            value.materialOverride.metallicFactor, 0.0f, 1.0f);
    value.materialOverride.roughnessFactor = std::clamp(
            value.materialOverride.roughnessFactor, 0.045f, 1.0f);
    return value;
}

FpsViewmodelAttachmentLighting ResolveFpsViewmodelAttachmentLighting(
        const FpsViewmodelAttachmentLighting& defaults,
        const FpsViewmodelAttachmentLightingOverride* value)
{
    FpsViewmodelAttachmentLighting result = defaults;
    if (value != nullptr) {
        if (value->brightnessAdjustment) {
            result.brightnessAdjustment = *value->brightnessAdjustment;
        }
        if (value->metallicFactor) {
            result.materialOverride.metallicFactor = *value->metallicFactor;
        }
        if (value->roughnessFactor) {
            result.materialOverride.roughnessFactor = *value->roughnessFactor;
        }
    }
    return ClampFpsViewmodelAttachmentLighting(result);
}

FpsViewmodelAttachmentLightingOverride
BuildFpsViewmodelAttachmentLightingOverride(
        const FpsViewmodelAttachmentLighting& defaults,
        const FpsViewmodelAttachmentLighting& effective)
{
    const FpsViewmodelAttachmentLighting clean =
            ClampFpsViewmodelAttachmentLighting(effective);
    FpsViewmodelAttachmentLightingOverride result;
    if (!NearlyEqual(
                defaults.brightnessAdjustment,
                clean.brightnessAdjustment)) {
        result.brightnessAdjustment = clean.brightnessAdjustment;
    }
    if (!NearlyEqual(
                defaults.materialOverride.metallicFactor,
                clean.materialOverride.metallicFactor)) {
        result.metallicFactor = clean.materialOverride.metallicFactor;
    }
    if (!NearlyEqual(
                defaults.materialOverride.roughnessFactor,
                clean.materialOverride.roughnessFactor)) {
        result.roughnessFactor = clean.materialOverride.roughnessFactor;
    }
    return result;
}

bool FpsViewmodelAttachmentLightingOverrideEmpty(
        const FpsViewmodelAttachmentLightingOverride& value)
{
    return !value.brightnessAdjustment
            && !value.metallicFactor
            && !value.roughnessFactor;
}

const FpsWeaponFiringOverride* FindFpsWeaponFiringOverride(
        const FpsApplicationSettings& settings,
        std::string_view id)
{
    const auto it = std::find_if(
            settings.weapons.begin(), settings.weapons.end(),
            [id](const FpsApplicationSettingsEntry& value) {
                return value.weaponId == id;
            });
    return it == settings.weapons.end() || FpsWeaponFiringOverrideEmpty(it->firing)
            ? nullptr
            : &it->firing;
}

void SetFpsWeaponFiringOverride(
        FpsApplicationSettings& settings,
        std::string id,
        const FpsWeaponFiringOverride& value)
{
    for (auto& entry : settings.weapons) {
        if (entry.weaponId == id) {
            entry.firing = value;
            return;
        }
    }
    FpsApplicationSettingsEntry entry;
    entry.weaponId = std::move(id);
    entry.firing = value;
    settings.weapons.push_back(std::move(entry));
}

void ClearFpsWeaponFiringOverride(
        FpsApplicationSettings& settings,
        std::string_view id)
{
    for (auto& entry : settings.weapons) {
        if (entry.weaponId == id) entry.firing = {};
    }
    settings.weapons.erase(std::remove_if(
            settings.weapons.begin(), settings.weapons.end(),
            [](const FpsApplicationSettingsEntry& value) {
                return FpsViewmodelOverrideEmpty(value.viewmodel)
                        && FpsViewmodelHolsterTransitionOverrideEmpty(value.holsterTransition)
                        && FpsViewmodelGripCorrectionOverrideEmpty(value.gripCorrection)
                        && FpsViewmodelAttachmentLightingOverrideEmpty(value.attachmentLighting)
                        && FpsWeaponFiringOverrideEmpty(value.firing);
            }), settings.weapons.end());
}

FpsWeaponCameraRecoilDefinition ClampFpsWeaponCameraRecoilDefinition(
        FpsWeaponCameraRecoilDefinition value)
{
    const FpsWeaponCameraRecoilDefinition defaults;
    const auto finiteOr = [](float candidate, float fallback) {
        return std::isfinite(candidate) ? candidate : fallback;
    };
    value.pitchKickDegrees = std::clamp(
            finiteOr(value.pitchKickDegrees, defaults.pitchKickDegrees),
            0.0f,
            45.0f);
    value.pitchVariationDegrees = std::clamp(
            finiteOr(value.pitchVariationDegrees,
                    defaults.pitchVariationDegrees),
            0.0f,
            45.0f);
    value.yawVariationDegrees = std::clamp(
            finiteOr(value.yawVariationDegrees, defaults.yawVariationDegrees),
            0.0f,
            45.0f);
    value.rollVariationDegrees = std::clamp(
            finiteOr(value.rollVariationDegrees,
                    defaults.rollVariationDegrees),
            0.0f,
            45.0f);
    value.springFrequencyHz = std::clamp(
            finiteOr(value.springFrequencyHz, defaults.springFrequencyHz),
            0.5f,
            40.0f);
    value.springDampingRatio = std::clamp(
            finiteOr(value.springDampingRatio,
                    defaults.springDampingRatio),
            0.1f,
            3.0f);
    value.maxPitchDegrees = std::clamp(
            finiteOr(value.maxPitchDegrees, defaults.maxPitchDegrees),
            0.0f,
            90.0f);
    value.maxYawDegrees = std::clamp(
            finiteOr(value.maxYawDegrees, defaults.maxYawDegrees),
            0.0f,
            90.0f);
    value.maxRollDegrees = std::clamp(
            finiteOr(value.maxRollDegrees, defaults.maxRollDegrees),
            0.0f,
            90.0f);
    return value;
}

FpsWeaponFiringDefinition ClampFpsWeaponFiringDefinition(
        FpsWeaponFiringDefinition value)
{
    value.shotIntervalSeconds = std::clamp(value.shotIntervalSeconds, 0.03f, 5.0f);
    value.maximumRangeWorld = std::clamp(value.maximumRangeWorld, 1.0f, 10000.0f);
    value.noiseRadiusWorld = std::isfinite(value.noiseRadiusWorld)
            ? std::clamp(value.noiseRadiusWorld, 0.0f, 10000.0f)
            : FpsWeaponFiringDefinition{}.noiseRadiusWorld;
    value.pellets.count = std::clamp(
            value.pellets.count, 1, MaxFpsWeaponPellets);
    value.pellets.spreadHalfAngleDegrees = std::isfinite(
            value.pellets.spreadHalfAngleDegrees)
            ? std::clamp(value.pellets.spreadHalfAngleDegrees, 0.0f, 45.0f)
            : FpsWeaponPelletDefinition{}.spreadHalfAngleDegrees;
    const auto clampVector = [](Vector3& vector, float minimum, float maximum) {
        vector.x = std::clamp(vector.x, minimum, maximum);
        vector.y = std::clamp(vector.y, minimum, maximum);
        vector.z = std::clamp(vector.z, minimum, maximum);
    };
    clampVector(value.recoil.translationImpulse, -1.0f, 1.0f);
    clampVector(value.recoil.rotationImpulseDegrees, -45.0f, 45.0f);
    value.recoil.rollVariationDegrees = std::clamp(value.recoil.rollVariationDegrees, 0.0f, 10.0f);
    value.recoil.springFrequencyHz = std::clamp(value.recoil.springFrequencyHz, 0.5f, 40.0f);
    value.recoil.dampingRatio = std::clamp(value.recoil.dampingRatio, 0.1f, 3.0f);
    clampVector(value.recoil.maximumTranslation, 0.0f, 2.0f);
    clampVector(value.recoil.maximumRotationDegrees, 0.0f, 90.0f);
    value.cameraRecoil = ClampFpsWeaponCameraRecoilDefinition(
            value.cameraRecoil);
    clampVector(value.muzzleSocket.position, -2.0f, 2.0f);
    clampVector(value.muzzleSocket.rotationDegrees, -360.0f, 360.0f);
    value.muzzleFlash.lifetimeSeconds = std::clamp(value.muzzleFlash.lifetimeSeconds, 0.005f, 60.0f);
    value.muzzleFlash.sizeWorld = std::clamp(value.muzzleFlash.sizeWorld, 0.005f, 2.0f);
    value.muzzleFlash.sizeVariation = std::clamp(value.muzzleFlash.sizeVariation, 0.0f, 0.5f);
    value.muzzleFlash.irregularity = std::clamp(value.muzzleFlash.irregularity, 0.0f, 1.0f);
    value.muzzleFlash.forwardStretch = std::clamp(value.muzzleFlash.forwardStretch, 1.0f, 4.0f);
    value.muzzleFlash.minimumLobeCount = std::clamp(
            value.muzzleFlash.minimumLobeCount, 3, MaxFpsMuzzleFlashLobes);
    value.muzzleFlash.maximumLobeCount = std::clamp(
            value.muzzleFlash.maximumLobeCount,
            value.muzzleFlash.minimumLobeCount,
            MaxFpsMuzzleFlashLobes);
    value.muzzleFlash.rearSuppression = std::clamp(value.muzzleFlash.rearSuppression, 0.0f, 1.0f);
    value.muzzleFlash.edgeSoftness = std::clamp(value.muzzleFlash.edgeSoftness, 0.01f, 1.0f);
    value.muzzleFlash.radianceStrength = std::isfinite(
            value.muzzleFlash.radianceStrength)
            ? std::clamp(value.muzzleFlash.radianceStrength, 0.0f, 64.0f)
            : FpsWeaponMuzzleFlashDefinition{}.radianceStrength;
    value.muzzleLight.intensity = std::clamp(value.muzzleLight.intensity, 0.0f, 100.0f);
    value.muzzleLight.radiusWorld = std::clamp(value.muzzleLight.radiusWorld, 0.05f, 100.0f);
    value.muzzleLight.lifetimeSeconds = std::clamp(value.muzzleLight.lifetimeSeconds, 0.005f, 2.0f);
    value.muzzleLight.decayExponent = std::clamp(value.muzzleLight.decayExponent, 0.1f, 10.0f);
    value.impact.damage = std::clamp(value.impact.damage, 0, 1000000);
    value.impact.staggerSeconds = std::isfinite(value.impact.staggerSeconds)
            ? std::clamp(value.impact.staggerSeconds, 0.0f, 10.0f)
            : 0.0f;
    value.impact.knockbackImpulseWorldPerSecond = std::isfinite(
            value.impact.knockbackImpulseWorldPerSecond)
            ? std::clamp(
                    value.impact.knockbackImpulseWorldPerSecond,
                    0.0f,
                    100.0f)
            : 0.0f;
    const auto clampParticles = [](FpsWeaponImpactParticlesDefinition& particles) {
        particles.particleCount = std::clamp(particles.particleCount, 0, 256);
        particles.sizeScale = std::isfinite(particles.sizeScale)
                ? std::clamp(particles.sizeScale, 0.05f, 10.0f)
                : 1.0f;
        particles.intensity = std::isfinite(particles.intensity)
                ? std::clamp(particles.intensity, 0.0f, 10.0f)
                : 1.0f;
        if (particles.particleCount == 0) particles.enabled = false;
    };
    clampParticles(value.impact.blood);
    clampParticles(value.impact.surfaceDebris);
    return value;
}

FpsWeaponFiringDefinition ResolveFpsWeaponFiringDefinition(
        const FpsWeaponFiringDefinition& defaults,
        const FpsWeaponFiringOverride* value)
{
    FpsWeaponFiringDefinition result = defaults;
    if (value != nullptr) {
        if (value->shotIntervalSeconds) result.shotIntervalSeconds = *value->shotIntervalSeconds;
        if (value->recoilTranslationImpulse) result.recoil.translationImpulse = *value->recoilTranslationImpulse;
        if (value->recoilRotationImpulseDegrees) result.recoil.rotationImpulseDegrees = *value->recoilRotationImpulseDegrees;
        if (value->recoilRollVariationDegrees) result.recoil.rollVariationDegrees = *value->recoilRollVariationDegrees;
        if (value->recoilSpringFrequencyHz) result.recoil.springFrequencyHz = *value->recoilSpringFrequencyHz;
        if (value->recoilDampingRatio) result.recoil.dampingRatio = *value->recoilDampingRatio;
        if (value->cameraRecoilEnabled) result.cameraRecoil.enabled = *value->cameraRecoilEnabled;
        if (value->cameraRecoilPitchKickDegrees) result.cameraRecoil.pitchKickDegrees = *value->cameraRecoilPitchKickDegrees;
        if (value->cameraRecoilPitchVariationDegrees) result.cameraRecoil.pitchVariationDegrees = *value->cameraRecoilPitchVariationDegrees;
        if (value->cameraRecoilYawVariationDegrees) result.cameraRecoil.yawVariationDegrees = *value->cameraRecoilYawVariationDegrees;
        if (value->cameraRecoilRollVariationDegrees) result.cameraRecoil.rollVariationDegrees = *value->cameraRecoilRollVariationDegrees;
        if (value->cameraRecoilSpringFrequencyHz) result.cameraRecoil.springFrequencyHz = *value->cameraRecoilSpringFrequencyHz;
        if (value->cameraRecoilSpringDampingRatio) result.cameraRecoil.springDampingRatio = *value->cameraRecoilSpringDampingRatio;
        if (value->cameraRecoilMaxPitchDegrees) result.cameraRecoil.maxPitchDegrees = *value->cameraRecoilMaxPitchDegrees;
        if (value->cameraRecoilMaxYawDegrees) result.cameraRecoil.maxYawDegrees = *value->cameraRecoilMaxYawDegrees;
        if (value->cameraRecoilMaxRollDegrees) result.cameraRecoil.maxRollDegrees = *value->cameraRecoilMaxRollDegrees;
        if (value->muzzlePosition) result.muzzleSocket.position = *value->muzzlePosition;
        if (value->muzzleRotationDegrees) result.muzzleSocket.rotationDegrees = *value->muzzleRotationDegrees;
        if (value->flashLifetimeSeconds) result.muzzleFlash.lifetimeSeconds = *value->flashLifetimeSeconds;
        if (value->flashSizeWorld) result.muzzleFlash.sizeWorld = *value->flashSizeWorld;
        if (value->flashSizeVariation) result.muzzleFlash.sizeVariation = *value->flashSizeVariation;
        if (value->flashIrregularity) result.muzzleFlash.irregularity = *value->flashIrregularity;
        if (value->flashForwardStretch) result.muzzleFlash.forwardStretch = *value->flashForwardStretch;
        if (value->flashMinimumLobeCount) result.muzzleFlash.minimumLobeCount = *value->flashMinimumLobeCount;
        if (value->flashMaximumLobeCount) result.muzzleFlash.maximumLobeCount = *value->flashMaximumLobeCount;
        if (value->flashRearSuppression) result.muzzleFlash.rearSuppression = *value->flashRearSuppression;
        if (value->flashEdgeSoftness) result.muzzleFlash.edgeSoftness = *value->flashEdgeSoftness;
        if (value->flashRadianceStrength) result.muzzleFlash.radianceStrength = *value->flashRadianceStrength;
        if (value->muzzleLightIntensity) result.muzzleLight.intensity = *value->muzzleLightIntensity;
        if (value->muzzleLightRadiusWorld) result.muzzleLight.radiusWorld = *value->muzzleLightRadiusWorld;
        if (value->muzzleLightLifetimeSeconds) result.muzzleLight.lifetimeSeconds = *value->muzzleLightLifetimeSeconds;
    }
    return ClampFpsWeaponFiringDefinition(result);
}

FpsWeaponFiringOverride BuildFpsWeaponFiringOverride(
        const FpsWeaponFiringDefinition& defaults,
        const FpsWeaponFiringDefinition& effective)
{
    const FpsWeaponFiringDefinition clean = ClampFpsWeaponFiringDefinition(effective);
    FpsWeaponFiringOverride result;
    if (!NearlyEqual(defaults.shotIntervalSeconds, clean.shotIntervalSeconds)) result.shotIntervalSeconds = clean.shotIntervalSeconds;
    if (!Same(defaults.recoil.translationImpulse, clean.recoil.translationImpulse)) result.recoilTranslationImpulse = clean.recoil.translationImpulse;
    if (!Same(defaults.recoil.rotationImpulseDegrees, clean.recoil.rotationImpulseDegrees)) result.recoilRotationImpulseDegrees = clean.recoil.rotationImpulseDegrees;
    if (!NearlyEqual(defaults.recoil.rollVariationDegrees, clean.recoil.rollVariationDegrees)) result.recoilRollVariationDegrees = clean.recoil.rollVariationDegrees;
    if (!NearlyEqual(defaults.recoil.springFrequencyHz, clean.recoil.springFrequencyHz)) result.recoilSpringFrequencyHz = clean.recoil.springFrequencyHz;
    if (!NearlyEqual(defaults.recoil.dampingRatio, clean.recoil.dampingRatio)) result.recoilDampingRatio = clean.recoil.dampingRatio;
    if (defaults.cameraRecoil.enabled != clean.cameraRecoil.enabled) result.cameraRecoilEnabled = clean.cameraRecoil.enabled;
    if (!NearlyEqual(defaults.cameraRecoil.pitchKickDegrees, clean.cameraRecoil.pitchKickDegrees)) result.cameraRecoilPitchKickDegrees = clean.cameraRecoil.pitchKickDegrees;
    if (!NearlyEqual(defaults.cameraRecoil.pitchVariationDegrees, clean.cameraRecoil.pitchVariationDegrees)) result.cameraRecoilPitchVariationDegrees = clean.cameraRecoil.pitchVariationDegrees;
    if (!NearlyEqual(defaults.cameraRecoil.yawVariationDegrees, clean.cameraRecoil.yawVariationDegrees)) result.cameraRecoilYawVariationDegrees = clean.cameraRecoil.yawVariationDegrees;
    if (!NearlyEqual(defaults.cameraRecoil.rollVariationDegrees, clean.cameraRecoil.rollVariationDegrees)) result.cameraRecoilRollVariationDegrees = clean.cameraRecoil.rollVariationDegrees;
    if (!NearlyEqual(defaults.cameraRecoil.springFrequencyHz, clean.cameraRecoil.springFrequencyHz)) result.cameraRecoilSpringFrequencyHz = clean.cameraRecoil.springFrequencyHz;
    if (!NearlyEqual(defaults.cameraRecoil.springDampingRatio, clean.cameraRecoil.springDampingRatio)) result.cameraRecoilSpringDampingRatio = clean.cameraRecoil.springDampingRatio;
    if (!NearlyEqual(defaults.cameraRecoil.maxPitchDegrees, clean.cameraRecoil.maxPitchDegrees)) result.cameraRecoilMaxPitchDegrees = clean.cameraRecoil.maxPitchDegrees;
    if (!NearlyEqual(defaults.cameraRecoil.maxYawDegrees, clean.cameraRecoil.maxYawDegrees)) result.cameraRecoilMaxYawDegrees = clean.cameraRecoil.maxYawDegrees;
    if (!NearlyEqual(defaults.cameraRecoil.maxRollDegrees, clean.cameraRecoil.maxRollDegrees)) result.cameraRecoilMaxRollDegrees = clean.cameraRecoil.maxRollDegrees;
    if (!Same(defaults.muzzleSocket.position, clean.muzzleSocket.position)) result.muzzlePosition = clean.muzzleSocket.position;
    if (!Same(defaults.muzzleSocket.rotationDegrees, clean.muzzleSocket.rotationDegrees)) result.muzzleRotationDegrees = clean.muzzleSocket.rotationDegrees;
    if (!NearlyEqual(defaults.muzzleFlash.lifetimeSeconds, clean.muzzleFlash.lifetimeSeconds)) result.flashLifetimeSeconds = clean.muzzleFlash.lifetimeSeconds;
    if (!NearlyEqual(defaults.muzzleFlash.sizeWorld, clean.muzzleFlash.sizeWorld)) result.flashSizeWorld = clean.muzzleFlash.sizeWorld;
    if (!NearlyEqual(defaults.muzzleFlash.sizeVariation, clean.muzzleFlash.sizeVariation)) result.flashSizeVariation = clean.muzzleFlash.sizeVariation;
    if (!NearlyEqual(defaults.muzzleFlash.irregularity, clean.muzzleFlash.irregularity)) result.flashIrregularity = clean.muzzleFlash.irregularity;
    if (!NearlyEqual(defaults.muzzleFlash.forwardStretch, clean.muzzleFlash.forwardStretch)) result.flashForwardStretch = clean.muzzleFlash.forwardStretch;
    if (defaults.muzzleFlash.minimumLobeCount != clean.muzzleFlash.minimumLobeCount) result.flashMinimumLobeCount = clean.muzzleFlash.minimumLobeCount;
    if (defaults.muzzleFlash.maximumLobeCount != clean.muzzleFlash.maximumLobeCount) result.flashMaximumLobeCount = clean.muzzleFlash.maximumLobeCount;
    if (!NearlyEqual(defaults.muzzleFlash.rearSuppression, clean.muzzleFlash.rearSuppression)) result.flashRearSuppression = clean.muzzleFlash.rearSuppression;
    if (!NearlyEqual(defaults.muzzleFlash.edgeSoftness, clean.muzzleFlash.edgeSoftness)) result.flashEdgeSoftness = clean.muzzleFlash.edgeSoftness;
    if (!NearlyEqual(defaults.muzzleFlash.radianceStrength, clean.muzzleFlash.radianceStrength)) result.flashRadianceStrength = clean.muzzleFlash.radianceStrength;
    if (!NearlyEqual(defaults.muzzleLight.intensity, clean.muzzleLight.intensity)) result.muzzleLightIntensity = clean.muzzleLight.intensity;
    if (!NearlyEqual(defaults.muzzleLight.radiusWorld, clean.muzzleLight.radiusWorld)) result.muzzleLightRadiusWorld = clean.muzzleLight.radiusWorld;
    if (!NearlyEqual(defaults.muzzleLight.lifetimeSeconds, clean.muzzleLight.lifetimeSeconds)) result.muzzleLightLifetimeSeconds = clean.muzzleLight.lifetimeSeconds;
    return result;
}

bool FpsWeaponFiringOverrideEmpty(const FpsWeaponFiringOverride& value)
{
    return !value.shotIntervalSeconds
            && !value.recoilTranslationImpulse
            && !value.recoilRotationImpulseDegrees
            && !value.recoilRollVariationDegrees
            && !value.recoilSpringFrequencyHz
            && !value.recoilDampingRatio
            && !value.cameraRecoilEnabled
            && !value.cameraRecoilPitchKickDegrees
            && !value.cameraRecoilPitchVariationDegrees
            && !value.cameraRecoilYawVariationDegrees
            && !value.cameraRecoilRollVariationDegrees
            && !value.cameraRecoilSpringFrequencyHz
            && !value.cameraRecoilSpringDampingRatio
            && !value.cameraRecoilMaxPitchDegrees
            && !value.cameraRecoilMaxYawDegrees
            && !value.cameraRecoilMaxRollDegrees
            && !value.muzzlePosition
            && !value.muzzleRotationDegrees
            && !value.flashLifetimeSeconds
            && !value.flashSizeWorld
            && !value.flashSizeVariation
            && !value.flashIrregularity
            && !value.flashForwardStretch
            && !value.flashMinimumLobeCount
            && !value.flashMaximumLobeCount
            && !value.flashRearSuppression
            && !value.flashEdgeSoftness
            && !value.flashRadianceStrength
            && !value.muzzleLightIntensity
            && !value.muzzleLightRadiusWorld
            && !value.muzzleLightLifetimeSeconds;
}

} // namespace game
