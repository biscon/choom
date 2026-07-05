#pragma once

#include <string>
#include <string_view>

namespace game {

bool IsSectorAssetsPath(std::string_view path);
std::string ResolveSectorAssetPath(std::string_view pathOrId);
std::string MakeSectorAssetRelativePath(std::string_view filesystemPath);

} // namespace game
