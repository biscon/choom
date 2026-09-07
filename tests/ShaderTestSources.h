#pragma once

#include "engine/render/ShaderSource.h"
#include "game/ShaderPrograms.h"

#include <initializer_list>
#include <stdexcept>

namespace test {

inline std::string ReadShaderStage(game::GameShader program, bool vertex = false)
{
    const auto& definition = game::GameShaderPrograms.at(static_cast<std::size_t>(program));
    const char* path = vertex ? definition.vertexPath : definition.fragmentPath;
    if (!path) return {};
    engine::ShaderSource source;
    std::string error;
    if (!engine::LoadShaderSource(SHADER_ROOT, path, source, error,
            definition.defines, definition.defineCount))
        throw std::runtime_error(error);
    return source.text;
}

inline std::string ReadShaderPrograms(std::initializer_list<game::GameShader> programs)
{
    std::string source;
    for (const auto program : programs) {
        source += ReadShaderStage(program, true);
        source += ReadShaderStage(program);
    }
    return source;
}

} // namespace test
