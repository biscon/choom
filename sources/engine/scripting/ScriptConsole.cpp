#include "engine/scripting/ScriptConsole.h"

#include "engine/scripting/ScriptData.h"

#include "lua.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

namespace engine {
namespace {

constexpr size_t MaximumResultCount = 64;
constexpr size_t MaximumValueBytes = 4096;
constexpr size_t MaximumTotalBytes = 16 * 1024;

struct StackGuard {
    lua_State* state = nullptr;
    int top = 0;

    ~StackGuard()
    {
        if (state != nullptr) lua_settop(state, top);
    }
};

struct ExecutionGuard {
    ScriptRuntime& scripts;

    explicit ExecutionGuard(ScriptRuntime& value) : scripts(value)
    {
        scripts.consoleExecuting = true;
    }

    ~ExecutionGuard()
    {
        scripts.consoleExecuting = false;
    }
};

int TracebackHandler(lua_State* state)
{
    const char* message = lua_tostring(state, 1);
    if (message == nullptr) message = "<non-string Lua error>";
    luaL_traceback(state, state, message, 1);
    return 1;
}

std::string StackString(lua_State* state, int index)
{
    size_t length = 0;
    const char* text = lua_tolstring(state, index, &length);
    return text != nullptr ? std::string{text, length}
                           : std::string{"<Lua error unavailable>"};
}

void AppendEscaped(std::string& output, const char* text, size_t length)
{
    output.push_back('"');
    for (size_t i = 0; i < length && output.size() < MaximumValueBytes - 8; ++i) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        switch (ch) {
            case '\\': output += "\\\\"; break;
            case '"': output += "\\\""; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (ch < 0x20 || ch == 0x7f) {
                    static constexpr char Hex[] = "0123456789abcdef";
                    output += "\\x";
                    output.push_back(Hex[(ch >> 4) & 0xf]);
                    output.push_back(Hex[ch & 0xf]);
                } else {
                    output.push_back(static_cast<char>(ch));
                }
                break;
        }
    }
    if (output.size() >= MaximumValueBytes - 8) output += "...";
    output.push_back('"');
}

std::string PointerValue(
        lua_State* state,
        int index,
        const char* typeName,
        const char* detail = nullptr)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << '<' << typeName;
    if (detail != nullptr && detail[0] != '\0') output << ' ' << detail;
    output << ' ' << lua_topointer(state, index) << '>';
    return output.str();
}

std::string FormatValue(lua_State* state, int index)
{
    const int type = lua_type(state, index);
    switch (type) {
        case LUA_TNIL:
            return "nil";
        case LUA_TBOOLEAN:
            return lua_toboolean(state, index) ? "true" : "false";
        case LUA_TNUMBER: {
            if (lua_isinteger(state, index)) {
                return std::to_string(lua_tointeger(state, index));
            }
            std::ostringstream output;
            output.imbue(std::locale::classic());
            output << std::setprecision(std::numeric_limits<lua_Number>::max_digits10)
                   << lua_tonumber(state, index);
            return output.str();
        }
        case LUA_TSTRING: {
            size_t length = 0;
            const char* value = lua_tolstring(state, index, &length);
            std::string output;
            output.reserve(std::min(length + 2, MaximumValueBytes));
            AppendEscaped(output, value != nullptr ? value : "", length);
            return output;
        }
        case LUA_TTABLE:
            return PointerValue(state, index, "table");
        case LUA_TFUNCTION:
            return PointerValue(state, index, "function");
        case LUA_TTHREAD:
            return PointerValue(state, index, "thread");
        case LUA_TLIGHTUSERDATA:
            return PointerValue(state, index, "lightuserdata");
        case LUA_TUSERDATA: {
            const char* detail = nullptr;
            if (lua_getmetatable(state, index)) {
                lua_getfield(state, -1, "__name");
                detail = lua_tostring(state, -1);
                const std::string result = PointerValue(
                        state, index, "userdata", detail);
                lua_pop(state, 2);
                return result;
            }
            return PointerValue(state, index, "userdata");
        }
        default:
            return std::string{"<"} + lua_typename(state, type) + ">";
    }
}

bool LoadSubmitted(
        lua_State* state,
        std::string_view text,
        bool expression,
        std::string& error)
{
    std::string source;
    if (expression) {
        source.reserve(text.size() + 7);
        source = "return ";
        source.append(text.data(), text.size());
    } else {
        source.assign(text.data(), text.size());
    }
    const int status = luaL_loadbufferx(
            state, source.data(), source.size(), "=console", "t");
    if (status == LUA_OK) return true;
    error = StackString(state, -1);
    return false;
}

} // namespace

ScriptConsoleResult ScriptSystemExecuteConsole(
        ScriptRuntime& scripts,
        std::string_view submittedText)
{
    ScriptConsoleResult result;
    if (scripts.vm == nullptr) {
        result.error = "Lua unavailable: no map VM is active";
        return result;
    }
    if (scripts.phase == ScriptRuntimePhase::ShuttingDown) {
        result.error = "Lua unavailable: map VM is shutting down";
        return result;
    }
    if (scripts.phase != ScriptRuntimePhase::Active) {
        result.error = "Lua unavailable: map VM is not active";
        return result;
    }
    if (!scripts.initFinished) {
        result.error = "Lua unavailable: map VM is still loading";
        return result;
    }
    if (scripts.consoleExecuting) {
        result.error = "Lua console is already executing";
        return result;
    }

    lua_State* state = scripts.vm;
    const int baseTop = lua_gettop(state);
    StackGuard stack{state, baseTop};
    ExecutionGuard execution{scripts};

    std::string expressionError;
    if (LoadSubmitted(state, submittedText, true, expressionError)) {
        result.evaluatedExpression = true;
    } else {
        lua_settop(state, baseTop);
        std::string statementError;
        if (!LoadSubmitted(state, submittedText, false, statementError)) {
            result.error = "Lua compile error: " + statementError;
            return result;
        }
    }

    lua_pushcfunction(state, TracebackHandler);
    lua_insert(state, baseTop + 1);
    const int handlerIndex = baseTop + 1;
    const int status = lua_pcall(state, 0, LUA_MULTRET, handlerIndex);
    if (status != LUA_OK) {
        result.error = "Lua runtime error: " + StackString(state, -1);
        return result;
    }

    const int firstResult = handlerIndex + 1;
    const int available = std::max(0, lua_gettop(state) - handlerIndex);
    const size_t count = std::min(
            static_cast<size_t>(available), MaximumResultCount);
    size_t totalBytes = 0;
    for (size_t i = 0; i < count; ++i) {
        std::string value = FormatValue(
                state, firstResult + static_cast<int>(i));
        if (value.size() > MaximumValueBytes) {
            value.resize(MaximumValueBytes - 3);
            value += "...";
        }
        std::string line = "[" + std::to_string(i + 1) + "] " + value;
        if (totalBytes + line.size() > MaximumTotalBytes) {
            result.values.push_back("... Lua console results truncated ...");
            break;
        }
        totalBytes += line.size();
        result.values.push_back(std::move(line));
    }
    if (static_cast<size_t>(available) > count
            && totalBytes < MaximumTotalBytes) {
        result.values.push_back("... additional Lua results omitted ...");
    }
    result.success = true;
    return result;
}

} // namespace engine
