#include "game/FpsWeaponRegistry.h"

#include "util/json.hpp"

#include <algorithm>
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

Color MixColor(Color a, Color b, float amount)
{
    const auto channel = [amount](unsigned char first, unsigned char second) {
        return static_cast<unsigned char>(std::lround(
                static_cast<float>(first)
                + (static_cast<float>(second) - static_cast<float>(first))
                        * amount));
    };
    return Color{
            channel(a.r, b.r), channel(a.g, b.g),
            channel(a.b, b.b), channel(a.a, b.a)};
}

Color DarkenedColor(Color value)
{
    return Color{
            static_cast<unsigned char>(std::lround(value.r * 0.45f)),
            static_cast<unsigned char>(std::lround(value.g * 0.45f)),
            static_cast<unsigned char>(std::lround(value.b * 0.45f)),
            static_cast<unsigned char>(std::lround(value.a * 0.75f))};
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

void ValidateFiring(
        const FpsWeaponFiringDefinition& value,
        const std::string& context)
{
    const float values[] = {
            value.shotIntervalSeconds, value.maximumRangeWorld,
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
            value.muzzleSocket.position.x, value.muzzleSocket.position.y,
            value.muzzleSocket.position.z,
            value.muzzleSocket.rotationDegrees.x,
            value.muzzleSocket.rotationDegrees.y,
            value.muzzleSocket.rotationDegrees.z,
            value.muzzleFlash.lifetimeSeconds, value.muzzleFlash.sizeWorld,
            value.muzzleFlash.sizeVariation, value.muzzleFlash.edgeSoftness,
            value.muzzleLight.intensity,
            value.muzzleLight.radiusWorld, value.muzzleLight.lifetimeSeconds,
            value.muzzleLight.decayExponent};
    for (float component : values) {
        if (!std::isfinite(component)) Fail(context + " contains a non-finite value");
    }
    if (value.shotIntervalSeconds <= 0.0f || value.maximumRangeWorld <= 0.0f) {
        Fail(context + " shot interval and maximum range must be greater than zero");
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
    if (value.muzzleFlash.lifetimeSeconds <= 0.0f
            || value.muzzleFlash.sizeWorld <= 0.0f
            || value.muzzleFlash.sizeVariation < 0.0f
            || value.muzzleFlash.sizeVariation > 1.0f
            || value.muzzleFlash.edgeSoftness < 0.01f
            || value.muzzleFlash.edgeSoftness > 1.0f) {
        Fail(context + ".muzzleFlash contains an invalid lifetime, size, variation, or edge softness");
    }
    if (value.muzzleLight.intensity < 0.0f
            || value.muzzleLight.radiusWorld <= 0.0f
            || value.muzzleLight.lifetimeSeconds <= 0.0f
            || value.muzzleLight.decayExponent <= 0.0f) {
        Fail(context + ".muzzleLight contains an invalid intensity, radius, lifetime, or decay");
    }
}

FpsWeaponFiringDefinition ReadFiring(const Json& object, const std::string& context)
{
    if (!object.is_object()) Fail(context + " must be an object");
    FpsWeaponFiringDefinition result;
    result.shotIntervalSeconds = Number(object, "shotIntervalSeconds", context);
    result.maximumRangeWorld = Number(object, "maximumRangeWorld", context);

    const Json& recoil = Require(object, "recoil", context);
    const std::string recoilContext = context + ".recoil";
    result.recoil.translationImpulse = Vector(recoil, "translationImpulse", recoilContext);
    result.recoil.rotationImpulseDegrees = Vector(recoil, "rotationImpulseDegrees", recoilContext);
    result.recoil.rollVariationDegrees = Number(recoil, "rollVariationDegrees", recoilContext);
    result.recoil.springFrequencyHz = Number(recoil, "springFrequencyHz", recoilContext);
    result.recoil.dampingRatio = Number(recoil, "dampingRatio", recoilContext);
    result.recoil.maximumTranslation = Vector(recoil, "maximumTranslation", recoilContext);
    result.recoil.maximumRotationDegrees = Vector(recoil, "maximumRotationDegrees", recoilContext);

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
    const bool hasGradientColors = flash.find("coreColor") != flash.end()
            || flash.find("hotColor") != flash.end()
            || flash.find("warmColor") != flash.end()
            || flash.find("edgeColor") != flash.end();
    if (hasGradientColors) {
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
    } else {
        const Color inner = ReadColor(
                Require(flash, "innerColor", flashContext),
                flashContext + ".innerColor");
        const Color outer = ReadColor(
                Require(flash, "outerColor", flashContext),
                flashContext + ".outerColor");
        result.muzzleFlash.coreColor = inner;
        result.muzzleFlash.hotColor = MixColor(inner, outer, 0.30f);
        result.muzzleFlash.warmColor = outer;
        result.muzzleFlash.edgeColor = DarkenedColor(outer);
    }

    const Json& light = Require(object, "muzzleLight", context);
    const std::string lightContext = context + ".muzzleLight";
    result.muzzleLight.enabled = Boolean(light, "enabled", lightContext);
    result.muzzleLight.color = ReadColor(Require(light, "color", lightContext), lightContext + ".color");
    result.muzzleLight.intensity = Number(light, "intensity", lightContext);
    result.muzzleLight.radiusWorld = Number(light, "radiusWorld", lightContext);
    result.muzzleLight.lifetimeSeconds = Number(light, "lifetimeSeconds", lightContext);
    result.muzzleLight.decayExponent = Number(light, "decayExponent", lightContext);
    ValidateFiring(result, context);
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

void SetError(std::string* output, const std::string& message) { if (output) *output = message; }

bool NearlyEqual(float a, float b) { return std::abs(a - b) <= 0.0001f; }
bool Same(Vector3 a, Vector3 b) { return NearlyEqual(a.x,b.x) && NearlyEqual(a.y,b.y) && NearlyEqual(a.z,b.z); }

Json Vec(Vector3 value) { return Json::array({value.x, value.y, value.z}); }

} // namespace

bool ParseFpsWeaponRegistry(std::string_view text, FpsWeaponRegistry& output, std::string* error)
{
    try {
        const Json root = Json::parse(text.begin(), text.end());
        if (!root.is_object()) Fail("weapon registry root must be an object");
        FpsWeaponRegistry parsed;
        parsed.version = Integer(root, "version", "weapon registry");
        if (parsed.version != 1) Fail("weapon registry version must be 1");
        parsed.initialWeaponId = String(root, "initialWeaponId", "weapon registry");
        const Json& weapons = Require(root, "weapons", "weapon registry");
        if (!weapons.is_array() || weapons.empty()) Fail("weapon registry.weapons must be a non-empty array");
        std::unordered_set<std::string> ids;
        for (size_t i = 0; i < weapons.size(); ++i) {
            const Json& object = weapons[i];
            const std::string context = "weapon registry.weapons[" + std::to_string(i) + "]";
            if (!object.is_object()) Fail(context + " must be an object");
            FpsWeaponDefinition definition;
            definition.id = String(object, "id", context);
            if (!ids.insert(definition.id).second) Fail("duplicate weapon id '" + definition.id + "'");
            const auto crosshair = object.find("crosshair");
            if (crosshair != object.end()) {
                definition.crosshair = ReadCrosshair(
                        *crosshair,
                        context + ".crosshair");
            }
            definition.firing = ReadFiring(
                    Require(object, "firing", context),
                    context + ".firing");
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
            parsed.weapons.push_back(std::move(definition));
        }
        if (!FindFpsWeaponDefinition(parsed, parsed.initialWeaponId)) {
            Fail("initial weapon id '" + parsed.initialWeaponId + "' has no definition");
        }
        output = std::move(parsed);
        SetError(error, {});
        return true;
    } catch (const std::exception& exception) {
        SetError(error, exception.what());
        return false;
    }
}

bool LoadFpsWeaponRegistry(const std::string& path, FpsWeaponRegistry& output, std::string* error)
{
    std::ifstream input(path);
    if (!input) { SetError(error, "could not open weapon registry: " + path); return false; }
    std::ostringstream text; text << input.rdbuf();
    return ParseFpsWeaponRegistry(text.str(), output, error);
}

const FpsWeaponDefinition* FindFpsWeaponDefinition(const FpsWeaponRegistry& registry, std::string_view id)
{
    const auto it = std::find_if(registry.weapons.begin(), registry.weapons.end(),
            [id](const FpsWeaponDefinition& value) { return value.id == id; });
    return it == registry.weapons.end() ? nullptr : &*it;
}

bool ParseFpsApplicationSettings(std::string_view text, FpsApplicationSettings& output, std::string* error)
{
    try {
        const Json root = Json::parse(text.begin(), text.end());
        if (!root.is_object()) Fail("application settings root must be an object");
        FpsApplicationSettings parsed;
        parsed.version = Integer(root, "version", "application settings");
        if (parsed.version != 1) Fail("application settings version must be 1");
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
                    entry.firing.muzzlePosition = OptionalVector(*firing, "muzzlePosition", firingContext);
                    entry.firing.muzzleRotationDegrees = OptionalVector(*firing, "muzzleRotationDegrees", firingContext);
                    entry.firing.flashLifetimeSeconds = OptionalNumber(*firing, "flashLifetimeSeconds", firingContext);
                    entry.firing.flashSizeWorld = OptionalNumber(*firing, "flashSizeWorld", firingContext);
                    entry.firing.flashEdgeSoftness = OptionalNumber(*firing, "flashEdgeSoftness", firingContext);
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
    Json root = {{"version", 1}};
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
        if (entry.firing.muzzlePosition) firing["muzzlePosition"] = Vec(*entry.firing.muzzlePosition);
        if (entry.firing.muzzleRotationDegrees) firing["muzzleRotationDegrees"] = Vec(*entry.firing.muzzleRotationDegrees);
        if (entry.firing.flashLifetimeSeconds) firing["flashLifetimeSeconds"] = *entry.firing.flashLifetimeSeconds;
        if (entry.firing.flashSizeWorld) firing["flashSizeWorld"] = *entry.firing.flashSizeWorld;
        if (entry.firing.flashEdgeSoftness) firing["flashEdgeSoftness"] = *entry.firing.flashEdgeSoftness;
        if (entry.firing.muzzleLightIntensity) firing["muzzleLightIntensity"] = *entry.firing.muzzleLightIntensity;
        if (entry.firing.muzzleLightRadiusWorld) firing["muzzleLightRadiusWorld"] = *entry.firing.muzzleLightRadiusWorld;
        if (entry.firing.muzzleLightLifetimeSeconds) firing["muzzleLightLifetimeSeconds"] = *entry.firing.muzzleLightLifetimeSeconds;
        if (!firing.empty()) value["firing"] = std::move(firing);
        if (!value.empty()) overrides[entry.weaponId] = std::move(value);
    }
    root["viewmodelOverrides"] = std::move(overrides);
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

FpsWeaponFiringDefinition ClampFpsWeaponFiringDefinition(
        FpsWeaponFiringDefinition value)
{
    value.shotIntervalSeconds = std::clamp(value.shotIntervalSeconds, 0.03f, 5.0f);
    value.maximumRangeWorld = std::clamp(value.maximumRangeWorld, 1.0f, 10000.0f);
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
    clampVector(value.muzzleSocket.position, -2.0f, 2.0f);
    clampVector(value.muzzleSocket.rotationDegrees, -360.0f, 360.0f);
    value.muzzleFlash.lifetimeSeconds = std::clamp(value.muzzleFlash.lifetimeSeconds, 0.005f, 10.0f);
    value.muzzleFlash.sizeWorld = std::clamp(value.muzzleFlash.sizeWorld, 0.005f, 2.0f);
    value.muzzleFlash.sizeVariation = std::clamp(value.muzzleFlash.sizeVariation, 0.0f, 1.0f);
    value.muzzleFlash.edgeSoftness = std::clamp(value.muzzleFlash.edgeSoftness, 0.01f, 1.0f);
    value.muzzleLight.intensity = std::clamp(value.muzzleLight.intensity, 0.0f, 100.0f);
    value.muzzleLight.radiusWorld = std::clamp(value.muzzleLight.radiusWorld, 0.05f, 100.0f);
    value.muzzleLight.lifetimeSeconds = std::clamp(value.muzzleLight.lifetimeSeconds, 0.005f, 2.0f);
    value.muzzleLight.decayExponent = std::clamp(value.muzzleLight.decayExponent, 0.1f, 10.0f);
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
        if (value->muzzlePosition) result.muzzleSocket.position = *value->muzzlePosition;
        if (value->muzzleRotationDegrees) result.muzzleSocket.rotationDegrees = *value->muzzleRotationDegrees;
        if (value->flashLifetimeSeconds) result.muzzleFlash.lifetimeSeconds = *value->flashLifetimeSeconds;
        if (value->flashSizeWorld) result.muzzleFlash.sizeWorld = *value->flashSizeWorld;
        if (value->flashEdgeSoftness) result.muzzleFlash.edgeSoftness = *value->flashEdgeSoftness;
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
    if (!Same(defaults.muzzleSocket.position, clean.muzzleSocket.position)) result.muzzlePosition = clean.muzzleSocket.position;
    if (!Same(defaults.muzzleSocket.rotationDegrees, clean.muzzleSocket.rotationDegrees)) result.muzzleRotationDegrees = clean.muzzleSocket.rotationDegrees;
    if (!NearlyEqual(defaults.muzzleFlash.lifetimeSeconds, clean.muzzleFlash.lifetimeSeconds)) result.flashLifetimeSeconds = clean.muzzleFlash.lifetimeSeconds;
    if (!NearlyEqual(defaults.muzzleFlash.sizeWorld, clean.muzzleFlash.sizeWorld)) result.flashSizeWorld = clean.muzzleFlash.sizeWorld;
    if (!NearlyEqual(defaults.muzzleFlash.edgeSoftness, clean.muzzleFlash.edgeSoftness)) result.flashEdgeSoftness = clean.muzzleFlash.edgeSoftness;
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
            && !value.muzzlePosition
            && !value.muzzleRotationDegrees
            && !value.flashLifetimeSeconds
            && !value.flashSizeWorld
            && !value.flashEdgeSoftness
            && !value.muzzleLightIntensity
            && !value.muzzleLightRadiusWorld
            && !value.muzzleLightLifetimeSeconds;
}

} // namespace game
