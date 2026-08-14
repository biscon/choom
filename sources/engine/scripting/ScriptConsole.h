#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace engine {

struct ScriptRuntime;

struct ScriptConsoleResult {
    bool success = false;
    bool evaluatedExpression = false;
    std::vector<std::string> values;
    std::string error;
};

ScriptConsoleResult ScriptSystemExecuteConsole(
        ScriptRuntime& scripts,
        std::string_view submittedText);

} // namespace engine
