#include "engine/render/ShaderSource.h"
#include "game/ShaderPrograms.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {

void Check(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

void Write(const std::filesystem::path& root, const std::string& path, const std::string& text)
{
    const auto file = root / path;
    std::filesystem::create_directories(file.parent_path());
    std::ofstream output(file, std::ios::binary);
    output << text;
    Check(static_cast<bool>(output), "Could not write shader fixture");
}

void TestExpansion(const std::filesystem::path& root)
{
    Write(root, "nested/values.glsl", "#ifndef VALUES_GLSL\n#define VALUES_GLSL\nconst float value = 2.0;\n#endif\n");
    Write(root, "nested/shared.glsl", "#include \"values.glsl\"\nfloat sampleValue() { return value; }\n");
    Write(root, "main.frag.glsl", "\xEF\xBB\xBF/* #include \"ignored.glsl\" */\r\n"
            "// #version 999\r\n#version 330\r\n"
            "# include /* relative */ \"nested/shared.glsl\" // comment\r\n"
            "#include \"nested/../nested/values.glsl\"\r\nvoid main() {}\r\n");
    const engine::ShaderDefine defines[]{{"VARIANT", "2"}, {"EXTRA", "1"}};
    engine::ShaderSource source;
    std::string error;
    Check(engine::LoadShaderSource(root, "main.frag.glsl", source, error, defines, 2), error);
    Check(source.files.size() == 3 && source.files[0] == "main.frag.glsl"
            && source.files[1] == "nested/shared.glsl" && source.files[2] == "nested/values.glsl",
            "Source IDs must be stable and deduplicate normalized paths");
    Check(source.text.find("#version 330\n#define VARIANT 2\n#define EXTRA 1\n#line 4 0\n")
            != std::string::npos, "Defines must follow the real version directive");
    Check(source.text.find("#line 1 2\n#ifndef VALUES_GLSL") != std::string::npos
            && source.text.find("#line 2 1\nfloat sampleValue") != std::string::npos
            && source.text.find("#line 5 0\n") != std::string::npos
            && source.text.find("#line 6 0\nvoid main") != std::string::npos,
            "Nested includes must restore parent source IDs and line numbers");
    Check(source.text.find("#include") == std::string::npos
            && source.text.find("\r") == std::string::npos, "Normalize CRLF and remove commented directives");
    const auto first = source.text.find("#ifndef VALUES_GLSL");
    Check(source.text.find("#ifndef VALUES_GLSL", first + 1) != std::string::npos,
            "Repeated includes preserve GLSL include guards for the compiler");
    Write(root, "minimal.vert.glsl", "#version 330");
    Check(engine::LoadShaderSource(root, "minimal.vert.glsl", source, error), error);
    Check(source.text.find("#version 330\n") == 0, "Allow missing final newline");
    Write(root, "comment.frag.glsl", "/* leading\ncomment */ #version 330 /* trailing\n"
            "#include \"ignored.glsl\" */\nvoid main() {}\n");
    Check(engine::LoadShaderSource(root, "comment.frag.glsl", source, error), error);
    Check(source.text.find("#include") == std::string::npos
            && source.text.find("void main() {}") != std::string::npos,
            "Multiline comments must not consume injected directives");
}

void TestFailures(const std::filesystem::path& root)
{
    engine::ShaderSource source;
    std::string error;
    const auto fails = [&](const std::string& text, const std::string& expected) {
        Write(root, "bad.frag.glsl", text);
        source.text = "stale source";
        Check(!engine::LoadShaderSource(root, "bad.frag.glsl", source, error),
                "Invalid shader unexpectedly loaded: " + expected);
        Check(source.text.empty() && source.files.empty(), "Failure must clear partial/stale source");
        Check(error.find(expected) != std::string::npos, error);
        Check(error.find("bad.frag.glsl:") != std::string::npos, "Missing entry diagnostic: " + error);
    };
    fails("void main() {}", "must start with #version");
    fails("// nothing here\n", "missing #version");
    fails("#version\n", "Missing #version value");
    fails("#version 330\n#version 330\n", "only once");
    fails("#version 330\n#include <other.glsl>\n", "Expected #include");
    fails("#version 330\n#include \"other.glsl\" junk\n", "Expected #include");
    fails("#version 330\n#include \"missing.glsl\"\n", "Cannot read shader file");
    fails("#version 330\n#include \"../outside.glsl\"\n", "escapes root");
    fails("#version 330\n#include \"/absolute.glsl\"\n", "Absolute shader path");
    fails("#version 330\n/* open", "Unterminated shader block comment");
    Write(root, "version.glsl", "#version 330\n");
    fails("#version 330\n#include \"version.glsl\"\n", "only once");
    Write(root, "a.glsl", "#include \"b.glsl\"\n");
    Write(root, "b.glsl", "#include \"a.glsl\"\n");
    fails("#version 330\n#include \"a.glsl\"\n", "include cycle");
    Check(error.find("a.glsl:1") != std::string::npos && error.find("b.glsl:1") != std::string::npos,
            "Cycle error must retain the include chain");
    Write(root, "valid.vert.glsl", "#version 330\nvoid main() {}\n");
    const engine::ShaderDefine duplicate[]{{"VALUE", "1"}, {"VALUE", "2"}};
    Check(!engine::LoadShaderSource(root, "valid.vert.glsl", source, error, duplicate, 2),
            "Duplicate defines must fail");
    const engine::ShaderDefine injected[]{{"VALUE", "1\n#error invalid"}};
    Check(!engine::LoadShaderSource(root, "valid.vert.glsl", source, error, injected, 1),
            "Define directive injection must fail");
    for (int i = 0; i < 66; ++i)
        Write(root, "deep" + std::to_string(i) + ".glsl",
                "#include \"deep" + std::to_string(i + 1) + ".glsl\"\n");
    fails("#version 330\n#include \"deep0.glsl\"\n", "depth exceeds");
    Write(root, "huge.glsl", std::string(4 * 1024 * 1024, ' '));
    fails("#version 330\n#include \"huge.glsl\"\n", "expansion limit");
    // Shader editing must take effect at the next explicit load; there is no
    // hidden process-wide source cache that could return stale includes.
    Write(root, "change.glsl", "const float changed = 1.0;\n");
    Write(root, "reload.frag.glsl", "#version 330\n#include \"change.glsl\"\n");
    Check(engine::LoadShaderSource(root, "reload.frag.glsl", source, error), error);
    Write(root, "change.glsl", "const float changed = 2.0;\n");
    Check(engine::LoadShaderSource(root, "reload.frag.glsl", source, error), error);
    Check(source.text.find("changed = 2.0") != std::string::npos, "Explicit loads must read current files");
}

void TestProgramCatalog()
{
    std::set<std::string> names, covered;
    for (const auto& program : game::GameShaderPrograms) {
        Check(program.name && program.fragmentPath, "Missing shader program descriptor");
        Check(names.insert(program.name).second, "Duplicate shader program name");
        for (const char* path : {program.vertexPath, program.fragmentPath}) {
            if (!path) continue;
            engine::ShaderSource source;
            std::string error;
            Check(engine::LoadShaderSource(SHADER_ROOT, path, source, error,
                    program.defines, program.defineCount), error);
            covered.insert(path);
        }
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(SHADER_ROOT)) {
        if (!entry.is_regular_file()) continue;
        const auto relative = entry.path().lexically_relative(SHADER_ROOT).generic_string();
        if (relative.find(".vert.glsl") != std::string::npos || relative.find(".frag.glsl") != std::string::npos)
            Check(covered.count(relative) != 0, "Uncatalogued shader stage: " + relative);
    }
}

} // namespace

int main()
{
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() / ("engine-shader-tests-" + std::to_string(unique));
    try {
        std::filesystem::create_directories(root);
        TestExpansion(root);
        TestFailures(root);
        TestProgramCatalog();
        std::filesystem::remove_all(root);
        std::cout << "Shader source tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::filesystem::remove_all(root);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
