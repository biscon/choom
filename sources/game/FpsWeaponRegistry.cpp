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
                        && FpsViewmodelGripCorrectionOverrideEmpty(
                                value.gripCorrection)
                        && FpsViewmodelAttachmentLightingOverrideEmpty(
                                value.attachmentLighting);
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
                        && FpsViewmodelGripCorrectionOverrideEmpty(
                                value.gripCorrection)
                        && FpsViewmodelAttachmentLightingOverrideEmpty(
                                value.attachmentLighting);
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
                        && FpsViewmodelGripCorrectionOverrideEmpty(
                                value.gripCorrection)
                        && FpsViewmodelAttachmentLightingOverrideEmpty(
                                value.attachmentLighting);
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

} // namespace game
