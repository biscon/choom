#include "engine/scripting/ScriptPersistence.h"

#include "util/json.hpp"

#include <exception>
#include <map>
#include <stdexcept>

namespace engine {
namespace {

using Json = nlohmann::ordered_json;

template <typename T>
Json OrderedValues(const std::unordered_map<std::string, T>& values)
{
    Json result = Json::object();
    std::map<std::string, T> ordered(values.begin(), values.end());
    for (const auto& entry : ordered) {
        result[entry.first] = entry.second;
    }
    return result;
}

const Json& RequireObject(const Json& root, const char* name)
{
    const auto found = root.find(name);
    if (found == root.end() || !found->is_object()) {
        throw std::runtime_error(std::string{"persistent script store."}
                + name + " must be an object");
    }
    return *found;
}

} // namespace

bool SavePersistentScriptStoreToJsonString(
        const PersistentScriptStore& store,
        std::string& output,
        std::string& error)
{
    try {
        Json root = Json::object();
        root["bools"] = OrderedValues(store.bools);
        root["ints"] = OrderedValues(store.ints);
        root["strings"] = OrderedValues(store.strings);
        std::string candidate = root.dump(2);
        output = std::move(candidate);
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = std::string{"Could not serialize persistent script store: "}
                + exception.what();
        return false;
    }
}

bool LoadPersistentScriptStoreFromJsonString(
        const std::string& input,
        PersistentScriptStore& store,
        std::string& error)
{
    try {
        const Json root = Json::parse(input);
        if (!root.is_object()) {
            throw std::runtime_error("persistent script store must be an object");
        }

        PersistentScriptStore candidate;
        for (const auto& entry : RequireObject(root, "bools").items()) {
            if (entry.key().empty() || !entry.value().is_boolean()) {
                throw std::runtime_error("persistent bool entries require non-empty keys and boolean values");
            }
            candidate.bools.emplace(entry.key(), entry.value().get<bool>());
        }
        for (const auto& entry : RequireObject(root, "ints").items()) {
            if (entry.key().empty() || !entry.value().is_number_integer()) {
                throw std::runtime_error("persistent int entries require non-empty keys and integer values");
            }
            candidate.ints.emplace(entry.key(), entry.value().get<int64_t>());
        }
        for (const auto& entry : RequireObject(root, "strings").items()) {
            if (entry.key().empty() || !entry.value().is_string()) {
                throw std::runtime_error("persistent string entries require non-empty keys and string values");
            }
            candidate.strings.emplace(entry.key(), entry.value().get<std::string>());
        }

        store = std::move(candidate);
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = std::string{"Could not parse persistent script store: "}
                + exception.what();
        return false;
    }
}

} // namespace engine
