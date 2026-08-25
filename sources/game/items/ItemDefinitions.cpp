#include "game/items/ItemDefinitions.h"

#include "util/json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace game {
namespace {

using Json = nlohmann::ordered_json;

bool DecodeUtf8(std::string_view text, size_t& outCodepoints)
{
    outCodepoints = 0;
    for (size_t cursor = 0; cursor < text.size();) {
        const unsigned char first = static_cast<unsigned char>(text[cursor]);
        std::uint32_t value = 0;
        size_t count = 0;
        std::uint32_t minimum = 0;
        if (first <= 0x7f) {
            value = first;
            count = 1;
        } else if ((first & 0xe0u) == 0xc0u) {
            value = first & 0x1fu;
            count = 2;
            minimum = 0x80;
        } else if ((first & 0xf0u) == 0xe0u) {
            value = first & 0x0fu;
            count = 3;
            minimum = 0x800;
        } else if ((first & 0xf8u) == 0xf0u) {
            value = first & 0x07u;
            count = 4;
            minimum = 0x10000;
        } else {
            return false;
        }
        if (cursor + count > text.size()) return false;
        for (size_t index = 1; index < count; ++index) {
            const unsigned char next = static_cast<unsigned char>(
                    text[cursor + index]);
            if ((next & 0xc0u) != 0x80u) return false;
            value = (value << 6u) | (next & 0x3fu);
        }
        if ((count > 1 && value < minimum)
                || value > 0x10ffffu
                || (value >= 0xd800u && value <= 0xdfffu)) {
            return false;
        }
        cursor += count;
        ++outCodepoints;
    }
    return true;
}

bool ValidateText(
        std::string_view text,
        size_t maximumCodepoints,
        bool multiline,
        const char* field,
        std::string& error)
{
    size_t count = 0;
    if (!DecodeUtf8(text, count)) {
        error = std::string(field) + " must contain valid UTF-8";
        return false;
    }
    if (count == 0 || count > maximumCodepoints) {
        error = std::string(field) + " must contain 1-"
                + std::to_string(maximumCodepoints) + " codepoints";
        return false;
    }
    for (const unsigned char character : text) {
        if (character >= 0x80u) continue;
        if (character == '\n' && multiline) continue;
        if (character == '\t' && multiline) continue;
        if (character < 0x20u || character == 0x7fu) {
            error = std::string(field) + " contains an unsupported control character";
            return false;
        }
    }
    return true;
}

bool ReadRequiredString(
        const Json& object,
        const char* field,
        const std::string& context,
        std::string& output,
        std::string& error)
{
    const auto it = object.find(field);
    if (it == object.end() || !it->is_string()) {
        error = context + "." + field + " must be a string";
        return false;
    }
    output = it->get<std::string>();
    return true;
}

bool ReadRequiredFloat(
        const Json& object,
        const char* field,
        const std::string& context,
        float& output,
        std::string& error)
{
    const auto it = object.find(field);
    if (it == object.end() || !it->is_number()) {
        error = context + "." + field + " must be a number";
        return false;
    }
    const double value = it->get<double>();
    if (!std::isfinite(value)
            || std::abs(value) > std::numeric_limits<float>::max()) {
        error = context + "." + field + " must be a finite float";
        return false;
    }
    output = static_cast<float>(value);
    return true;
}

} // namespace

const char* ItemTypeName(ItemType type)
{
    switch (type) {
        case ItemType::Object: return "object";
        case ItemType::Weapon: return "weapon";
        case ItemType::Ammo: return "ammo";
        case ItemType::Health: return "health";
    }
    return "object";
}

bool ParseItemType(std::string_view text, ItemType& outType)
{
    if (text == "object") outType = ItemType::Object;
    else if (text == "weapon") outType = ItemType::Weapon;
    else if (text == "ammo") outType = ItemType::Ammo;
    else if (text == "health") outType = ItemType::Health;
    else return false;
    return true;
}

bool IsValidItemDefinitionId(std::string_view id)
{
    if (id.empty() || id.size() > kMaximumItemIdBytes) return false;
    return std::all_of(id.begin(), id.end(), [](char character) {
        return (character >= 'A' && character <= 'Z')
                || (character >= 'a' && character <= 'z')
                || (character >= '0' && character <= '9')
                || character == '_'
                || character == '-';
    });
}

bool IsValidItemModelPath(std::string_view path)
{
    if (path.size() <= std::string_view{"assets/models/"}.size()
            || path.compare(0, std::string_view{"assets/models/"}.size(),
                    "assets/models/") != 0
            || path.find('\\') != std::string_view::npos) {
        return false;
    }
    const std::filesystem::path parsed{std::string{path}};
    if (parsed.is_absolute()
            || parsed.lexically_normal().generic_string() != path) {
        return false;
    }
    for (const std::filesystem::path& segment : parsed) {
        if (segment == "." || segment == "..") return false;
    }
    std::string extension = parsed.extension().generic_string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
    return extension == ".glb" || extension == ".gltf";
}

ItemDefinition MakeDefaultItemDefinition()
{
    ItemDefinition definition;
    definition.id = "new_item";
    definition.title = "New Item";
    definition.description = "Describe this item.";
    return definition;
}

bool ValidateItemDefinition(
        const ItemDefinition& definition,
        const FpsWeaponRegistry& weapons,
        std::string& error)
{
    if (!IsValidItemDefinitionId(definition.id)) {
        error = "Item ID must contain 1-63 letters, digits, underscores, or dashes";
        return false;
    }
    if (!ValidateText(definition.title, kMaximumItemTitleCodepoints,
                false, "Item title", error)
            || !ValidateText(definition.description,
                    kMaximumItemDescriptionCodepoints,
                    true, "Item description", error)) {
        return false;
    }
    if (!IsValidItemModelPath(definition.modelPath)) {
        error = "Item modelPath must be a normalized .glb or .gltf path under assets/models";
        return false;
    }
    if (!std::isfinite(definition.weightKg) || definition.weightKg < 0.0f) {
        error = "Item weightKg must be finite and non-negative";
        return false;
    }
    if (definition.type == ItemType::Weapon || definition.type == ItemType::Ammo) {
        if (definition.weaponId.empty()
                || FindFpsWeaponDefinition(weapons, definition.weaponId) == nullptr) {
            error = "Item '" + definition.id
                    + "' references missing weapon '" + definition.weaponId + "'";
            return false;
        }
    }
    if (definition.type == ItemType::Health) {
        if (definition.healAmount <= 0) {
            error = "Health item healAmount must be a positive integer";
            return false;
        }
        if (definition.healOverTime
                && (!std::isfinite(definition.healDurationSeconds)
                    || definition.healDurationSeconds <= 0.0f)) {
            error = "Timed health item healDurationSeconds must be finite and positive";
            return false;
        }
    }
    error.clear();
    return true;
}

bool ValidateItemRegistry(
        const ItemRegistry& registry,
        const FpsWeaponRegistry& weapons,
        std::string& error)
{
    if (registry.version != kItemRegistryFormatVersion) {
        error = "Item registry version must be 1";
        return false;
    }
    std::unordered_set<std::string> ids;
    for (const ItemDefinition& definition : registry.items) {
        if (!ValidateItemDefinition(definition, weapons, error)) return false;
        if (!ids.insert(definition.id).second) {
            error = "Duplicate item ID '" + definition.id + "'";
            return false;
        }
    }
    error.clear();
    return true;
}

const ItemDefinition* FindItemDefinition(
        const ItemRegistry& registry,
        std::string_view id)
{
    const auto found = std::find_if(
            registry.items.begin(), registry.items.end(),
            [id](const ItemDefinition& definition) {
                return definition.id == id;
            });
    return found == registry.items.end() ? nullptr : &*found;
}

std::vector<std::string> SortedItemDefinitionIds(const ItemRegistry& registry)
{
    std::vector<std::string> ids;
    ids.reserve(registry.items.size());
    for (const ItemDefinition& definition : registry.items) {
        ids.push_back(definition.id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool ParseItemRegistryJson(
        std::string_view jsonText,
        const FpsWeaponRegistry& weapons,
        ItemRegistry& outRegistry,
        std::string& error)
{
    try {
        const Json root = Json::parse(jsonText.begin(), jsonText.end());
        if (!root.is_object()) {
            error = "Item registry root must be an object";
            return false;
        }
        const auto version = root.find("version");
        const auto items = root.find("items");
        if (version == root.end() || !version->is_number_integer()
                || version->get<int>() != kItemRegistryFormatVersion) {
            error = "Item registry version must be 1";
            return false;
        }
        if (items == root.end() || !items->is_array()) {
            error = "Item registry.items must be an array";
            return false;
        }
        ItemRegistry parsed;
        parsed.items.reserve(items->size());
        for (size_t index = 0; index < items->size(); ++index) {
            const Json& object = (*items)[index];
            const std::string context = "Item registry.items["
                    + std::to_string(index) + "]";
            if (!object.is_object()) {
                error = context + " must be an object";
                return false;
            }
            ItemDefinition definition;
            std::string type;
            if (!ReadRequiredString(object, "id", context, definition.id, error)
                    || !ReadRequiredString(object, "title", context,
                            definition.title, error)
                    || !ReadRequiredString(object, "description", context,
                            definition.description, error)
                    || !ReadRequiredString(object, "modelPath", context,
                            definition.modelPath, error)
                    || !ReadRequiredString(object, "type", context, type, error)
                    || !ReadRequiredFloat(object, "weightKg", context,
                            definition.weightKg, error)) {
                return false;
            }
            if (!ParseItemType(type, definition.type)) {
                error = context + ".type must be object, weapon, ammo, or health";
                return false;
            }
            if (definition.type == ItemType::Weapon
                    || definition.type == ItemType::Ammo) {
                if (!ReadRequiredString(object, "weaponId", context,
                            definition.weaponId, error)) {
                    return false;
                }
            }
            if (definition.type == ItemType::Health) {
                const auto amount = object.find("healAmount");
                if (amount == object.end() || !amount->is_number_integer()) {
                    error = context + ".healAmount must be an integer";
                    return false;
                }
                definition.healAmount = amount->get<int>();
                const auto timed = object.find("healOverTime");
                if (timed != object.end()) {
                    if (!timed->is_boolean()) {
                        error = context + ".healOverTime must be a boolean";
                        return false;
                    }
                    definition.healOverTime = timed->get<bool>();
                }
                if (definition.healOverTime
                        && !ReadRequiredFloat(object, "healDurationSeconds",
                                context, definition.healDurationSeconds, error)) {
                    return false;
                }
            }
            parsed.items.push_back(std::move(definition));
        }
        if (!ValidateItemRegistry(parsed, weapons, error)) return false;
        std::sort(parsed.items.begin(), parsed.items.end(),
                [](const ItemDefinition& left, const ItemDefinition& right) {
                    return left.id < right.id;
                });
        outRegistry = std::move(parsed);
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = std::string{"Could not parse item registry: "} + exception.what();
        return false;
    }
}

bool SerializeItemRegistryJson(
        const ItemRegistry& registry,
        const FpsWeaponRegistry& weapons,
        std::string& outJson,
        std::string& error)
{
    if (!ValidateItemRegistry(registry, weapons, error)) return false;
    std::vector<const ItemDefinition*> sorted;
    sorted.reserve(registry.items.size());
    for (const ItemDefinition& definition : registry.items) {
        sorted.push_back(&definition);
    }
    std::sort(sorted.begin(), sorted.end(),
            [](const ItemDefinition* left, const ItemDefinition* right) {
                return left->id < right->id;
            });
    Json items = Json::array();
    for (const ItemDefinition* definition : sorted) {
        Json value = {
                {"id", definition->id},
                {"title", definition->title},
                {"description", definition->description},
                {"modelPath", definition->modelPath},
                {"type", ItemTypeName(definition->type)},
                {"weightKg", definition->weightKg}};
        if (definition->type == ItemType::Weapon
                || definition->type == ItemType::Ammo) {
            value["weaponId"] = definition->weaponId;
        }
        if (definition->type == ItemType::Health) {
            value["healAmount"] = definition->healAmount;
            if (definition->healOverTime) {
                value["healOverTime"] = true;
                value["healDurationSeconds"] = definition->healDurationSeconds;
            }
        }
        items.push_back(std::move(value));
    }
    outJson = Json{{"version", kItemRegistryFormatVersion},
                   {"items", std::move(items)}}.dump(2) + "\n";
    error.clear();
    return true;
}

bool LoadItemRegistry(
        const std::filesystem::path& path,
        const FpsWeaponRegistry& weapons,
        ItemRegistry& outRegistry,
        std::string& error)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "Could not open item registry '" + path.generic_string() + "'";
        return false;
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.good() && !input.eof()) {
        error = "Could not read item registry '" + path.generic_string() + "'";
        return false;
    }
    return ParseItemRegistryJson(contents.str(), weapons, outRegistry, error);
}

bool SaveItemRegistry(
        const std::filesystem::path& path,
        const ItemRegistry& registry,
        const FpsWeaponRegistry& weapons,
        std::string& error)
{
    std::string json;
    if (!SerializeItemRegistryJson(registry, weapons, json, error)) return false;
    std::error_code directoryError;
    std::filesystem::create_directories(path.parent_path(), directoryError);
    if (directoryError) {
        error = "Could not create item registry directory: "
                + directoryError.message();
        return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "Could not open item registry for writing '"
                + path.generic_string() + "'";
        return false;
    }
    output.write(json.data(), static_cast<std::streamsize>(json.size()));
    if (!output) {
        error = "Could not write item registry '" + path.generic_string() + "'";
        return false;
    }
    error.clear();
    return true;
}

} // namespace game
