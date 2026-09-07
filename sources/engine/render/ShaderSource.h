#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace engine {

struct ShaderDefine {
    const char* name;
    const char* value;
};

struct ShaderProgramDefinition {
    const char* name;
    const char* vertexPath; // nullptr requests raylib's default vertex stage.
    const char* fragmentPath;
    const ShaderDefine* defines = nullptr;
    std::size_t defineCount = 0;
};

struct ShaderSource {
    std::string text;
    // Numeric GLSL source IDs index this list; entry source is always zero.
    std::vector<std::filesystem::path> files;
};

// CPU only. Call during initialization/loading, never from a steady draw path.
// Includes are textual, relative to their parent, and confined to root.
// GLSL handles ordinary macro/include guards after expansion.
bool LoadShaderSource(
        const std::filesystem::path& root,
        const std::filesystem::path& entry,
        ShaderSource& result,
        std::string& error,
        const ShaderDefine* defines = nullptr,
        std::size_t defineCount = 0);

} // namespace engine
