#pragma once

#include "game/ShaderPrograms.h"
#include "engine/render/ShaderProgram.h"

namespace game {

inline Shader LoadGameShader(GameShader program)
{
    const auto index = static_cast<std::size_t>(program);
    if (index >= GameShaderPrograms.size()) return {};
    return engine::LoadShaderProgram(engine::DefaultShaderRoot(), GameShaderPrograms[index]);
}

} // namespace game
