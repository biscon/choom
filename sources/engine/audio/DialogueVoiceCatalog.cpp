#include "engine/audio/DialogueSpeech.h"
#include "util/json.hpp"
#include <fstream>
#include <stdexcept>

namespace engine {
namespace {
using Json = nlohmann::ordered_json;
Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Cannot open " + path.string());
    return Json::parse(file);
}
std::filesystem::path ChildPath(const std::filesystem::path& root, const std::string& relative)
{
    const std::filesystem::path path(relative);
    if (path.empty() || path.is_absolute()) throw std::runtime_error("Expected relative dialogue asset path");
    for (const auto& part : path) if (part == "..") throw std::runtime_error("Dialogue asset path escapes its directory");
    return root / path;
}
} // namespace


std::vector<DialogueVoiceCatalogEntry> ReadDialogueVoiceCatalog(const std::filesystem::path& assetsRoot)
{
    std::vector<DialogueVoiceCatalogEntry> entries;
    try {
        const auto config = ReadJson(assetsRoot / "config/dialogue_speech.json");
        const auto& voices = config.at("voices");
        if (!voices.is_object()) throw std::runtime_error("Expected voice catalog object");
        entries.reserve(voices.size());
        for (auto it = voices.begin(); it != voices.end(); ++it) {
            try {
                const auto path = ChildPath(assetsRoot, it.value().get<std::string>());
                const auto manifest = ReadJson(path);
                if (manifest.at("schema_version") != 2 || manifest.at("voice_identity_id") != it.key())
                    throw std::runtime_error("Voice identity/version mismatch");
                entries.push_back({it.key(), manifest.value("display_name", it.key()), path});
            } catch (const std::exception& error) {
                TraceLog(LOG_WARNING, "Dialogue profile '%s': %s", it.key().c_str(), error.what());
            }
        }
    } catch (const std::exception& error) {
        TraceLog(LOG_WARNING, "Dialogue catalog unavailable: %s", error.what());
    }
    return entries;
}

} // namespace engine
