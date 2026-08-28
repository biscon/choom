#include "game/save/GameSaveStorage.h"

#include "game/save/GameSaveSerialization.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace game {
namespace {

std::string SlotFileName(int slot)
{
    char value[32]{};
    std::snprintf(value, sizeof(value), "slot%02d.json", slot);
    return value;
}

bool ReadFileBounded(
        const std::filesystem::path& path,
        std::string& value,
        std::string& error)
{
    std::error_code filesystemError;
    const std::uintmax_t size = std::filesystem::file_size(path, filesystemError);
    if (filesystemError) {
        error = "Could not inspect save file '" + path.generic_string()
                + "': " + filesystemError.message();
        return false;
    }
    if (size > GameSaveMaximumJsonBytes) {
        error = "Save file exceeds the maximum size";
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "Could not open save file '" + path.generic_string() + "'";
        return false;
    }
    std::string candidate;
    candidate.resize(static_cast<std::size_t>(size));
    if (size > 0) {
        input.read(candidate.data(), static_cast<std::streamsize>(size));
    }
    if (!input && !input.eof()) {
        error = "Could not read save file '" + path.generic_string() + "'";
        return false;
    }
    value = std::move(candidate);
    error.clear();
    return true;
}

bool WriteFileAtomically(
        const std::filesystem::path& target,
        const std::string& value,
        std::string& error)
{
    const std::filesystem::path temporary = target.string() + ".tmp";
    const std::filesystem::path backup = target.string() + ".bak";
    std::error_code filesystemError;
    std::filesystem::remove(temporary, filesystemError);
    filesystemError.clear();
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            error = "Could not create temporary save file '"
                    + temporary.generic_string() + "'";
            return false;
        }
        output.write(value.data(), static_cast<std::streamsize>(value.size()));
        output.flush();
        if (!output) {
            output.close();
            std::filesystem::remove(temporary, filesystemError);
            error = "Could not finish writing temporary save file '"
                    + temporary.generic_string() + "'";
            return false;
        }
    }

    const bool hadTarget = std::filesystem::exists(target, filesystemError)
            && !filesystemError;
    std::filesystem::remove(backup, filesystemError);
    filesystemError.clear();
    if (hadTarget) {
        std::filesystem::rename(target, backup, filesystemError);
        if (filesystemError) {
            std::filesystem::remove(temporary, filesystemError);
            error = "Could not stage the previous save for replacement";
            return false;
        }
    }
    std::filesystem::rename(temporary, target, filesystemError);
    if (filesystemError) {
        std::error_code rollbackError;
        if (hadTarget) std::filesystem::rename(backup, target, rollbackError);
        std::filesystem::remove(temporary, rollbackError);
        error = "Could not publish save file '" + target.generic_string()
                + "': " + filesystemError.message();
        return false;
    }
    if (hadTarget) std::filesystem::remove(backup, filesystemError);
    error.clear();
    return true;
}

std::size_t Utf8CharacterCount(const std::string& value, bool& valid)
{
    valid = true;
    std::size_t count = 0;
    for (std::size_t i = 0; i < value.size();) {
        const unsigned char first = static_cast<unsigned char>(value[i]);
        std::size_t length = 0;
        if (first < 0x80) length = 1;
        else if ((first & 0xe0u) == 0xc0u) length = 2;
        else if ((first & 0xf0u) == 0xe0u) length = 3;
        else if ((first & 0xf8u) == 0xf0u) length = 4;
        else { valid = false; return 0; }
        if (i + length > value.size()) { valid = false; return 0; }
        for (std::size_t continuation = 1; continuation < length; ++continuation) {
            if ((static_cast<unsigned char>(value[i + continuation]) & 0xc0u) != 0x80u) {
                valid = false;
                return 0;
            }
        }
        i += length;
        ++count;
    }
    return count;
}

std::time_t UtcTmToTime(std::tm value)
{
#if defined(_WIN32)
    return _mkgmtime(&value);
#else
    return timegm(&value);
#endif
}

} // namespace

std::filesystem::path ResolveGameSaveRoot(const std::string& applicationId)
{
    const std::string safeId = applicationId.empty() ? "engine" : applicationId;
#if defined(_WIN32)
    if (const char* appData = std::getenv("APPDATA")) {
        return std::filesystem::path(appData) / safeId / "saves";
    }
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / "Library" / "Application Support"
                / safeId / "saves";
    }
#else
    if (const char* dataHome = std::getenv("XDG_DATA_HOME")) {
        if (dataHome[0] != '\0') {
            return std::filesystem::path(dataHome) / safeId / "saves";
        }
    }
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".local" / "share" / safeId / "saves";
    }
#endif
    return std::filesystem::current_path() / "saves";
}

std::filesystem::path GameSaveSlotPath(
        const std::filesystem::path& root,
        int slot)
{
    return root / SlotFileName(slot);
}

bool IsValidGameSaveName(const std::string& name)
{
    bool validUtf8 = false;
    const std::size_t characters = Utf8CharacterCount(name, validUtf8);
    if (!validUtf8 || characters == 0 || characters > GameSaveMaximumNameCharacters) {
        return false;
    }
    return name.find_first_not_of(" \t\r\n") != std::string::npos;
}

std::string CurrentGameSaveTimestampUtc()
{
    const std::time_t now = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

std::string FormatGameSaveTimestampLocal(const std::string& utcTimestamp)
{
    std::tm utc{};
    std::istringstream input(utcTimestamp);
    input >> std::get_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    if (!input || !input.eof()) return utcTimestamp;
    const std::time_t timestamp = UtcTmToTime(utc);
    if (timestamp == static_cast<std::time_t>(-1)) return utcTimestamp;
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &timestamp);
#else
    localtime_r(&timestamp, &local);
#endif
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%d %H:%M");
    return output.str();
}

std::vector<GameSaveSlotInfo> ScanGameSaveSlots(
        const std::filesystem::path& root)
{
    std::vector<GameSaveSlotInfo> result;
    result.reserve(GameSaveSlotCount);
    for (int slot = 1; slot <= GameSaveSlotCount; ++slot) {
        GameSaveSlotInfo info;
        info.slot = slot;
        const std::filesystem::path path = GameSaveSlotPath(root, slot);
        std::error_code filesystemError;
        if (!std::filesystem::exists(path, filesystemError)) {
            info.status = filesystemError
                    ? GameSaveSlotStatus::Corrupt : GameSaveSlotStatus::Empty;
            if (filesystemError) info.error = filesystemError.message();
            result.push_back(std::move(info));
            continue;
        }
        std::string input;
        if (!ReadFileBounded(path, input, info.error)) {
            info.status = GameSaveSlotStatus::Corrupt;
            result.push_back(std::move(info));
            continue;
        }
        GameSaveData save;
        if (!DeserializeGameSave(input, save, info.error)) {
            info.status = info.error.find("unsupported save format version")
                            != std::string::npos
                    ? GameSaveSlotStatus::Incompatible
                    : GameSaveSlotStatus::Corrupt;
            result.push_back(std::move(info));
            continue;
        }
        if (save.slot != slot) {
            info.status = GameSaveSlotStatus::Corrupt;
            info.error = "Save file slot does not match its filename";
            result.push_back(std::move(info));
            continue;
        }
        info.status = GameSaveSlotStatus::Ready;
        info.name = save.name;
        info.savedAtUtc = save.savedAtUtc;
        info.displayTimestamp = FormatGameSaveTimestampLocal(save.savedAtUtc);
        info.currentLevelId = save.currentLevelId;
        if (!save.thumbnailFile.empty()) {
            const std::filesystem::path thumbnail = root / save.thumbnailFile;
            if (std::filesystem::is_regular_file(thumbnail, filesystemError)
                    && !filesystemError) {
                info.thumbnailPath = thumbnail.generic_string();
            }
        }
        result.push_back(std::move(info));
    }
    return result;
}

bool LoadGameSaveSlot(
        const std::filesystem::path& root,
        int slot,
        GameSaveData& save,
        std::string& error)
{
    if (slot < 1 || slot > GameSaveSlotCount) {
        error = "Save slot is invalid";
        return false;
    }
    std::string input;
    if (!ReadFileBounded(GameSaveSlotPath(root, slot), input, error)) return false;
    GameSaveData candidate;
    if (!DeserializeGameSave(input, candidate, error)) return false;
    if (candidate.slot != slot) {
        error = "Save file slot does not match its filename";
        return false;
    }
    save = std::move(candidate);
    error.clear();
    return true;
}

bool WriteGameSaveSlot(
        const std::filesystem::path& root,
        const GameSaveData& save,
        std::string& error)
{
    std::string json;
    if (!SerializeGameSave(save, json, error)) return false;
    std::error_code filesystemError;
    std::filesystem::create_directories(root, filesystemError);
    if (filesystemError) {
        error = "Could not create save directory '" + root.generic_string()
                + "': " + filesystemError.message();
        return false;
    }
    return WriteFileAtomically(GameSaveSlotPath(root, save.slot), json, error);
}

} // namespace game
