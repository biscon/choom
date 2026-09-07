#include "engine/render/ShaderSource.h"
#include "game/ShaderPrograms.h"

#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--list") {
        for (const auto& program : game::GameShaderPrograms)
            std::cout << program.name << '\t'
                      << (program.vertexPath ? program.vertexPath : "-")
                      << '\t' << program.fragmentPath << '\n';
        return 0;
    }
    if (argc == 5 && std::string(argv[1]) == "--source") {
        for (const auto& program : game::GameShaderPrograms) {
            if (program.name != std::string(argv[3])) continue;
            const std::string stage = argv[4];
            const char* path = stage == "vert" ? program.vertexPath
                    : stage == "frag" ? program.fragmentPath : nullptr;
            if (!path) break;
            engine::ShaderSource source;
            std::string error;
            if (!engine::LoadShaderSource(argv[2], path, source, error,
                    program.defines, program.defineCount)) {
                std::cerr << program.name << ": " << error << '\n';
                return 1;
            }
            for (std::size_t i = 0; i < source.files.size(); ++i)
                std::cerr << i << ": " << source.files[i].generic_string() << '\n';
            std::cout << source.text;
            return 0;
        }
    }
    std::cerr << "Usage: shader_source_tool --list\n"
                 "       shader_source_tool --source SHADER_ROOT PROGRAM vert|frag\n";
    return 2;
}
