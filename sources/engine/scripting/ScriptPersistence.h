#pragma once

#include "engine/scripting/ScriptData.h"

#include <string>

namespace engine {

bool SavePersistentScriptStoreToJsonString(
        const PersistentScriptStore& store,
        std::string& output,
        std::string& error);

bool LoadPersistentScriptStoreFromJsonString(
        const std::string& input,
        PersistentScriptStore& store,
        std::string& error);

} // namespace engine
