#pragma once

#include <external/cgltf.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

namespace engine {

inline std::string NormalizeAssetSourcePath(const std::string& path)
{
    return std::filesystem::absolute(path).lexically_normal().generic_string();
}

inline std::string ResolveModelImagePath(const std::string& model, const char* uri)
{
    std::string decoded = uri ? uri : "";
    if (!decoded.empty()) decoded.resize(cgltf_decode_uri(decoded.data()));
    return NormalizeAssetSourcePath((std::filesystem::path(model).parent_path() / decoded).string());
}

// Explicit load-phase scratch. Hash content, not timestamps: exporters can
// replace same-size files without changing the timestamp's coarse resolution.
struct AssetSourceRevisions {
    std::unordered_map<std::string, std::string> files;
    std::unordered_map<std::string, std::string> models;
    void Clear() { files.clear(); models.clear(); }
    std::string File(const std::string& source) {
        const std::string path = NormalizeAssetSourcePath(source);
        const auto found = files.find(path);
        if (found != files.end()) return found->second;
        std::ifstream stream(path, std::ios::binary);
        uint64_t hash = 14695981039346656037ull;
        std::array<char, 65536> bytes{};
        if (!stream) return files.emplace(path, "missing").first->second;
        while (stream) {
            stream.read(bytes.data(), bytes.size());
            for (std::streamsize i = 0; i < stream.gcount(); ++i) {
                hash ^= static_cast<unsigned char>(bytes[static_cast<size_t>(i)]);
                hash *= 1099511628211ull;
            }
        }
        return files.emplace(path, stream.bad() ? "read-error" : std::to_string(hash)).first->second;
    }
    std::string Model(const std::string& source) {
        const std::string path = NormalizeAssetSourcePath(source);
        const auto found = models.find(path);
        if (found != models.end()) return found->second;
        std::string revision = File(path);
        cgltf_options options{};
        cgltf_data* data = nullptr;
        if (cgltf_parse_file(&options, path.c_str(), &data) == cgltf_result_success && data) {
            const auto append = [&](const char* uri) {
                if (!uri || std::string(uri).rfind("data:", 0) == 0) return;
                const std::string dependency = ResolveModelImagePath(path, uri);
                revision += "|" + dependency + ":" + File(dependency);
            };
            for (size_t i = 0; i < data->buffers_count; ++i) append(data->buffers[i].uri);
            for (size_t i = 0; i < data->images_count; ++i) append(data->images[i].uri);
        }
        if (data) cgltf_free(data);
        return models.emplace(path, std::move(revision)).first->second;
    }
};

} // namespace engine
