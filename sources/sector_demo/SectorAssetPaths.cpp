#include "sector_demo/SectorAssetPaths.h"

#include <filesystem>
#include <system_error>

namespace game {

namespace {

constexpr std::string_view kSectorAssetsPrefix = "assets/";

} // namespace

bool IsSectorAssetsPath(std::string_view path)
{
    return path.size() >= kSectorAssetsPrefix.size()
            && path.compare(0, kSectorAssetsPrefix.size(), kSectorAssetsPrefix) == 0;
}

std::string ResolveSectorAssetPath(std::string_view pathOrId)
{
    if (IsSectorAssetsPath(pathOrId)) {
        return std::string(ASSETS_PATH) + std::string(pathOrId.substr(kSectorAssetsPrefix.size()));
    }
    return std::string(pathOrId);
}

std::string ResolveSectorAudioAssetPath(std::string_view relativePath)
{
    return std::string(ASSETS_PATH "audio/") + std::string(relativePath);
}

std::string MakeSectorAssetRelativePath(std::string_view filesystemPath)
{
    const std::filesystem::path assetsRoot = std::filesystem::path(ASSETS_PATH).lexically_normal();
    const std::filesystem::path absolute = std::filesystem::path(filesystemPath).lexically_normal();
    std::error_code ec;
    const std::filesystem::path relative = std::filesystem::relative(absolute, assetsRoot, ec);
    if (!ec && !relative.empty() && relative.native().find("..") != 0) {
        return std::string{"assets/"} + relative.generic_string();
    }
    return std::filesystem::path(filesystemPath).generic_string();
}

} // namespace game
