#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace game
{
// Shader literals may start with blank lines. Insert after the version directive,
// never after the first newline; defines before #version are invalid GLSL.
inline std::string InsertSectorShaderPreamble(std::string source, std::string_view preamble)
{
    const auto version = source.find("#version");
    if (version == std::string::npos)
        throw std::invalid_argument("Sector shader is missing #version");
    auto lineEnd = source.find('\n', version);
    if (lineEnd == std::string::npos) {
        source.push_back('\n');
        lineEnd = source.size() - 1;
    }
    source.insert(lineEnd + 1, preamble);
    return source;
}
} // namespace game
