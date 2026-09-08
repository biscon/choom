#include "engine/render/ShaderSource.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace engine {
namespace {

constexpr std::size_t MaximumExpandedBytes = 4 * 1024 * 1024;
constexpr std::size_t MaximumIncludeDepth = 64;

std::string Trim(const std::string& value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

// Retain spacing so comments cannot concatenate directive tokens. Quoted
// include paths may contain characters that would otherwise begin comments.
std::string WithoutComments(const std::string& line, bool& blockComment)
{
    std::string result;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        if (blockComment) {
            if (line[i] == '*' && i + 1 < line.size() && line[i + 1] == '/') {
                blockComment = false;
                ++i;
            }
            result += ' ';
        } else if (!quoted && line[i] == '/' && i + 1 < line.size()
                && line[i + 1] == '*') {
            blockComment = true;
            ++i;
            result += ' ';
        } else if (!quoted && line[i] == '/' && i + 1 < line.size()
                && line[i + 1] == '/') {
            break;
        } else {
            if (line[i] == '"') quoted = !quoted;
            result += line[i];
        }
    }
    return result;
}

bool Identifier(const std::string& value)
{
    if (value.empty() || (!std::isalpha(static_cast<unsigned char>(value[0]))
            && value[0] != '_')) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '_';
    });
}

struct SourceExpansion {
    std::filesystem::path root;
    ShaderSource result;
    std::vector<std::filesystem::path> stack;
    std::vector<std::size_t> stackLines;
    std::string preamble;

    [[noreturn]] void Fail(const std::string& message) const
    {
        std::ostringstream diagnostic;
        diagnostic << message;
        for (std::size_t i = 0; i < stack.size(); ++i)
            diagnostic << "\n  " << (i == 0 ? "in " : "included from ")
                       << stack[i].generic_string() << ':' << stackLines[i];
        throw std::runtime_error(diagnostic.str());
    }

    void Append(const std::string& text)
    {
        if (result.text.size() + text.size() > MaximumExpandedBytes)
            Fail("Shader source exceeds 4 MiB expansion limit");
        result.text += text;
    }

    void Expand(const std::filesystem::path& relative, bool entry)
    {
        if (relative.is_absolute()) Fail("Absolute shader path is not allowed: " + relative.string());
        const auto path = std::filesystem::weakly_canonical(root / relative);
        const auto normalized = path.lexically_relative(root);
        if (normalized.empty() || *normalized.begin() == "..")
            Fail("Shader path escapes root: " + relative.string());
        if (std::find(stack.begin(), stack.end(), normalized) != stack.end())
            Fail("Shader include cycle: " + normalized.generic_string());
        if (stack.size() >= MaximumIncludeDepth) Fail("Shader include depth exceeds 64");
        stack.push_back(normalized);
        stackLines.push_back(1);
        std::ifstream input(path, std::ios::binary);
        if (!input) Fail("Cannot read shader file: " + normalized.generic_string());
        auto found = std::find(result.files.begin(), result.files.end(), normalized);
        const auto id = static_cast<std::size_t>(found - result.files.begin());
        if (found == result.files.end()) result.files.push_back(normalized);
        if (!entry) Append("#line 1 " + std::to_string(id) + "\n");
        std::string line;
        bool blockComment = false;
        bool versionSeen = false;
        std::size_t lineNumber = 0;
        while (std::getline(input, line)) {
            stackLines.back() = ++lineNumber;
            if (lineNumber == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
                line.erase(0, 3);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            const std::string uncommented = WithoutComments(line, blockComment);
            const std::string visible = Trim(uncommented);
            std::string directive, argument;
            if (!visible.empty() && visible[0] == '#') {
                const std::string body = Trim(visible.substr(1));
                const auto end = body.find_first_not_of(
                        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_");
                directive = body.substr(0, end);
                if (end != std::string::npos) argument = Trim(body.substr(end));
            }
            if (directive == "version") {
                if (!entry || versionSeen) Fail("#version is allowed only once, in the entry shader");
                if (argument.empty()) Fail("Missing #version value");
                versionSeen = true;
                // Emit comment-free directive so a trailing block comment
                // cannot swallow injected defines or source mapping.
                Append(visible + "\n" + preamble);
                Append("#line " + std::to_string(lineNumber + 1) + " " + std::to_string(id) + "\n");
            } else {
                if (entry && !versionSeen && !visible.empty())
                    Fail("Entry shader must start with #version (comments and whitespace are allowed)");
                if (directive == "include") {
                    if (argument.size() < 3 || argument.front() != '"'
                            || argument.back() != '"'
                            || argument.find('"', 1) != argument.size() - 1)
                        Fail("Expected #include \"relative/path.glsl\"");
                    Expand(normalized.parent_path() / argument.substr(1, argument.size() - 2), false);
                    Append("#line " + std::to_string(lineNumber + 1) + " " + std::to_string(id) + "\n");
                } else {
                    // Comments were already handled above. Removing them
                    // prevents cross-line comments spanning an include from
                    // affecting inserted GLSL, while preserving line numbers.
                    Append(uncommented + "\n");
                }
            }
        }
        if (input.bad()) Fail("Failed while reading shader file");
        if (blockComment) Fail("Unterminated shader block comment");
        if (entry && !versionSeen) Fail("Entry shader is missing #version");
        stack.pop_back();
        stackLines.pop_back();
    }
};

} // namespace

bool LoadShaderSource(const std::filesystem::path& root,
        const std::filesystem::path& entry, ShaderSource& result,
        std::string& error, const ShaderDefine* defines, std::size_t defineCount)
{
    result = {};
    error.clear();
    try {
        SourceExpansion expansion;
        expansion.root = std::filesystem::weakly_canonical(std::filesystem::absolute(root));
        if (defineCount != 0 && defines == nullptr)
            throw std::runtime_error("Missing shader define array");
        std::vector<std::string> names;
        for (std::size_t i = 0; i < defineCount; ++i) {
            const std::string name = defines[i].name ? defines[i].name : "";
            const std::string value = defines[i].value ? defines[i].value : "";
            if (!Identifier(name) || value.find_first_of("\r\n#\\") != std::string::npos
                    || value.find("/*") != std::string::npos || value.find("//") != std::string::npos
                    || std::find(names.begin(), names.end(), name) != names.end())
                throw std::runtime_error("Invalid or duplicate shader define: " + name);
            names.push_back(name);
            expansion.preamble += "#define " + name + " " + value + "\n";
        }
        expansion.Expand(entry, true);
        result = std::move(expansion.result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

} // namespace engine
