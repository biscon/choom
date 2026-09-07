#include "engine/render/ShaderProgram.h"

#include <rlgl.h>

namespace engine {

std::filesystem::path DefaultShaderRoot()
{
    return std::filesystem::path(ASSETS_PATH) / "shaders";
}

Shader LoadShaderProgram(const std::filesystem::path& root,
        const ShaderProgramDefinition& definition)
{
    ShaderSource vertex, fragment;
    std::string error;
    if (definition.fragmentPath == nullptr) {
        TraceLog(LOG_WARNING, "SHADER [%s]: missing fragment entry", definition.name);
        return {};
    }
    if ((definition.vertexPath && !LoadShaderSource(root, definition.vertexPath,
                    vertex, error, definition.defines, definition.defineCount))
            || !LoadShaderSource(root, definition.fragmentPath,
                    fragment, error, definition.defines, definition.defineCount)) {
        TraceLog(LOG_WARNING, "SHADER [%s]: %s", definition.name, error.c_str());
        return {};
    }
    Shader shader = LoadShaderFromMemory(
            definition.vertexPath ? vertex.text.c_str() : nullptr, fragment.text.c_str());
    if (shader.id == 0 || shader.id == rlGetShaderIdDefault()) {
        TraceLog(LOG_WARNING, "SHADER [%s]: compilation/link failed; source IDs:", definition.name);
        for (std::size_t i = 0; i < vertex.files.size(); ++i)
            TraceLog(LOG_WARNING, "  vertex %i: %s", static_cast<int>(i), vertex.files[i].generic_string().c_str());
        for (std::size_t i = 0; i < fragment.files.size(); ++i)
            TraceLog(LOG_WARNING, "  fragment %i: %s", static_cast<int>(i), fragment.files[i].generic_string().c_str());
        // Raylib allocates locations even for id=0. Its default program and
        // locations are borrowed and must never be freed by this failure path.
        if (shader.id == 0 && shader.locs) MemFree(shader.locs);
        return {};
    }
    return shader;
}

} // namespace engine
