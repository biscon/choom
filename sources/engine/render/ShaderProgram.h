#pragma once

#include "engine/render/ShaderSource.h"
#include <raylib.h>

namespace engine {

std::filesystem::path DefaultShaderRoot();

// Main-thread initialization/load only. Caller owns the returned program and
// must UnloadShader before closing the graphics context. Failure returns {}.
Shader LoadShaderProgram(const std::filesystem::path& root,
        const ShaderProgramDefinition& definition);

} // namespace engine
