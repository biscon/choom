#include "game/SectorLevelLoader.h"

#include "sector_demo/SectorTopologySerialization.h"

namespace game {

bool IsValidApplicationLevelName(const std::string& name)
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

std::string ApplicationLevelAssetPath(const std::string& name)
{
    if (!IsValidApplicationLevelName(name)) {
        return {};
    }
    return std::string{ASSETS_PATH "levels/"}
            + name + "/" + name + ".json";
}

bool LoadSectorRuntimeLevel(
        const std::string& path,
        SectorTopologyMap& outMap,
        std::string& error)
{
    SectorAuthoringDocument authoring;
    std::string authoringError;
    if (LoadSectorAuthoringDocument(path.c_str(), authoring, &authoringError)) {
        if (!authoring.derivation.success) {
            error = "Authoring graph derivation failed";
            return false;
        }
        outMap = std::move(authoring.derivation.topology);
        error.clear();
        return true;
    }

    std::string topologyError;
    if (LoadSectorTopologyMap(path.c_str(), outMap, &topologyError)) {
        error.clear();
        return true;
    }

    error = "Could not load level '" + path + "' as an authoring-graph or linedef map";
    if (!authoringError.empty()) {
        error += ": " + authoringError;
    } else if (!topologyError.empty()) {
        error += ": " + topologyError;
    }
    return false;
}

} // namespace game
