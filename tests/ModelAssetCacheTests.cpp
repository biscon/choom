#include "engine/assets/ModelAssets.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>

namespace {
int failures = 0, decodes = 0, uploads = 0, unloads = 0;
unsigned int nextTexture = 100;
std::vector<std::vector<unsigned char>> uploadedBytes;
void Check(bool value, const char* message) {
    if (!value) { std::cerr << "FAILED: " << message << '\n'; ++failures; }
}
Texture2D Upload(Image image) {
    ++uploads;
    const int size = GetPixelDataSize(image.width, image.height, image.format);
    const auto* bytes = static_cast<const unsigned char*>(image.data);
    uploadedBytes.emplace_back(bytes, bytes + size);
    return {nextTexture++, image.width, image.height, 2, image.format};
}
void Unload(Texture2D) { ++unloads; }
Image Decode(void*) {
    ++decodes;
    auto* bytes = static_cast<unsigned char*>(MemAlloc(8));
    const unsigned char pixels[] = {10, 20, 30, 255, 40, 50, 60, 255};
    std::copy(std::begin(pixels), std::end(pixels), bytes);
    return {bytes, 2, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
}
Image FailedDecode(void*) { ++decodes; return {}; }
Image DecodePng(void* path) { return LoadImage(static_cast<const char*>(path)); }
void Write(const std::filesystem::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary); file << text;
}
}

namespace engine {
// Exercise ownership and import-cache operations without creating a window or GPU.
struct ModelAssetCacheTestAccess {
    static void Benchmark(const std::filesystem::path& directory) {
        const auto path = (directory / "benchmark.png").string();
        Image image = GenImageColor(512, 512, WHITE);
        auto* pixels = static_cast<Color*>(image.data);
        uint32_t random = 1234567;
        for (int i = 0; i < 512 * 512; ++i) {
            random ^= random << 13; random ^= random >> 17; random ^= random << 5;
            pixels[i] = {static_cast<unsigned char>(random), static_cast<unsigned char>(random >> 8),
                    static_cast<unsigned char>(random >> 16), 255};
        }
        Check(ExportImage(image, path.c_str()), "generated benchmark PNG saved");
        UnloadImage(image);
        ModelAssets assets;
        assets.uploadTexture = [](Image source) {
            return Texture2D{nextTexture++, source.width, source.height, 2, source.format};
        };
        assets.unloadTexture = [](Texture2D) {};
        const auto model = (directory / "model.gltf").string();
        const auto run = [&](bool reuse) {
            const auto start = LoadClock::now();
            for (int i = 0; i < 64; ++i) {
                assets.importing = std::make_shared<ModelAssets::CachedModel>();
                for (int role : {MATERIAL_MAP_METALNESS, MATERIAL_MAP_ROUGHNESS})
                    ModelAssets::ResolveGltfTexture(model.c_str(), "benchmark.png", 0, role,
                            &DecodePng, const_cast<char*>(path.c_str()), &assets);
                assets.importing.reset();
                if (!reuse) { assets.TrimCache(true); assets.UnloadReadyModels(); }
            }
            return std::chrono::duration<double, std::milli>(LoadClock::now() - start).count();
        };
        const double uncached = run(false), cached = run(true);
        std::cout << "Generated PNG, 64 repeated packed-image uses, CPU only: uncached "
                << uncached << " ms, cached " << cached << " ms ("
                << 100.0 * (1.0 - cached / uncached) << "% reduction)\n";
        assets.ShutdownMainThread();
    }
    static void Run(const std::filesystem::path& directory) {
        ModelAssets assets;
        LoadDiagnostics report;
        report.Begin("cache test");
        assets.BeginLoad(report);
        assets.uploadTexture = &Upload;
        assets.unloadTexture = &Unload;
        assets.importing = std::make_shared<ModelAssets::CachedModel>();
        const auto model = (directory / "model.gltf").string();
        const auto other = (directory / "other.gltf").string();
        const auto resolve = [&](const std::string& path, const char* uri, int index, int role) {
            return ModelAssets::ResolveGltfTexture(path.c_str(), uri, index, role, &Decode, nullptr, &assets);
        };
        const auto metal = resolve(model, "image.png", 0, MATERIAL_MAP_METALNESS);
        const auto rough = resolve(model, "image.png", 0, MATERIAL_MAP_ROUGHNESS);
        Check(decodes == 1 && uploads == 2, "packed source decoded once and both channels uploaded once");
        Check(uploadedBytes[0] == std::vector<unsigned char>({30, 60}), "metallic B channel retained");
        Check(uploadedBytes[1] == std::vector<unsigned char>({20, 50}), "roughness G channel retained");
        Check(metal.id != rough.id, "channel resources are distinct");
        Check(resolve(other, "./image.png", 0, MATERIAL_MAP_METALNESS).id == metal.id,
                "external image shared across normalized model paths");
        Check(decodes == 1 && uploads == 2, "external cache hit skips CPU and GPU work");
        const auto normal = resolve(model, "image.png", 0, MATERIAL_MAP_NORMAL);
        const auto base = resolve(model, "image.png", 0, MATERIAL_MAP_ALBEDO);
        Check(normal.id != base.id, "linear and explicitly decoded sRGB roles remain distinct");
        Check(resolve(model, "image.png", 0, MATERIAL_MAP_EMISSION).id == base.id,
                "compatible emissive/base-color byte textures can share");
        Check(resolve(model, nullptr, 0, MATERIAL_MAP_ALBEDO).id
                != resolve(other, nullptr, 0, MATERIAL_MAP_ALBEDO).id,
                "embedded images in separate models cannot alias");
        Check(resolve(model, nullptr, 0, MATERIAL_MAP_ALBEDO).id
                != resolve(model, nullptr, 1, MATERIAL_MAP_ALBEDO).id,
                "embedded image indices cannot alias");
        const auto bad = ModelAssets::ResolveGltfTexture(model.c_str(), "missing.png", 0,
                MATERIAL_MAP_NORMAL, &FailedDecode, nullptr, &assets);
        Check(bad.id == 0, "decode failure stays a missing texture");
        // A failed model can release its references without destroying textures
        // borrowed by an earlier successful import.
        const auto successful = assets.importing;
        assets.importing = std::make_shared<ModelAssets::CachedModel>();
        Check(resolve(model, "image.png", 0, MATERIAL_MAP_METALNESS).id == metal.id,
                "later import borrows the shared texture");
        assets.importing.reset();
        assets.cacheDirty = true;
        assets.TrimCache(true);
        Check(unloads == 0, "cache trimming cannot destroy actively borrowed textures");

        AssetScopeHandle first{0, 1}, second{1, 1};
        assets.OnScopeCreated(first); assets.OnScopeCreated(second);
        const ModelHandle a = assets.RequestModel(first, "model", model.c_str());
        auto& slot = assets.modelSlots[a.index];
        slot.state = ModelState::Ready;
        slot.resource->textures = successful->textures;
        slot.resource->bytes = ModelAssets::UnusedCacheBudget + 1;
        assets.cachedModels[slot.resourceKey] = slot.resource;
        const auto b = assets.RequestModel(second, "another-key", model.c_str());
        Check(assets.IsReady(b) && assets.modelSlots[a.index].resource == assets.modelSlots[b.index].resource,
                "two scopes share immutable backing data");
        const auto animated = assets.RequestModel(second, "model", model.c_str(), ModelLoad_Animations);
        Check(!assets.IsReady(animated), "different animation flags do not alias a retained static model");
        assets.UnloadScope(first);
        Check(!assets.IsReady(a) && assets.IsReady(b), "scope release invalidates only its handles");
        Write(directory / "image.png", "edited");
        assets.BeginLoad(report);
        const auto changed = assets.RequestModel(first, "model", model.c_str());
        Check(!assets.IsReady(changed) && assets.IsReady(b),
                "source edit creates a new version while the previous active scope stays valid");
        assets.UnloadScope(first);
        assets.cacheDirty = true; assets.TrimCache();
        Check(!assets.cachedModels.empty(), "active resources survive cache budget pressure");
        assets.UnloadScope(second);
        assets.TrimCache();
        Check(assets.cachedModels.empty(), "unused over-budget resource is evicted");

        // Release the synthetic successful importer, then verify exact unload count.
        successful->textures.clear();
        assets.ShutdownMainThread();
        Check(unloads == uploads, "every cached texture is destroyed exactly once at shutdown");
        Check(assets.cachedModels.empty() && assets.cachedTextures.empty(), "shutdown clears retained resources");
    }
};
}

int main(int argc, char** argv) {
    SetTraceLogLevel(LOG_ERROR);
    const auto directory = std::filesystem::temp_directory_path()
            / ("engine-model-cache-test-" + std::to_string(engine::LoadClock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    const std::string document = R"({"asset":{"version":"2.0"},"buffers":[{"uri":"data.bin","byteLength":4}],"images":[{"uri":"image.png"}]})";
    Write(directory / "model.gltf", document);
    Write(directory / "other.gltf", document + "\n");
    Write(directory / "data.bin", "1234"); Write(directory / "image.png", "abcd");
    engine::AssetSourceRevisions revisions;
    const auto original = revisions.Model((directory / "model.gltf").string());
    const auto timestamp = std::filesystem::last_write_time(directory / "data.bin");
    Write(directory / "data.bin", "4321");
    std::filesystem::last_write_time(directory / "data.bin", timestamp);
    revisions.Clear();
    const auto changedBuffer = revisions.Model((directory / "model.gltf").string());
    Check(changedBuffer != original, "same-size same-timestamp external buffer edit invalidates revision");
    Write(directory / "image.png", "dcba"); revisions.Clear();
    const auto changedImage = revisions.Model((directory / "model.gltf").string());
    Check(changedImage != changedBuffer, "external image edit invalidates model revision");
    Write(directory / "model.gltf", document + "\n"); revisions.Clear();
    Check(revisions.Model((directory / "model.gltf").string()) != changedImage, "model edit invalidates revision");
    Check(engine::NormalizeAssetSourcePath((directory / "./image.png").string())
            == engine::NormalizeAssetSourcePath((directory / "image.png").string()), "path normalization");
    engine::ModelAssetCacheTestAccess::Run(directory);
    engine::LoadPreparationBudget budget;
    budget.Begin(0); Check(budget.RemainingMilliseconds() == 0, "expired budget grants no additional work");
    budget.Begin(1000); Check(budget.RemainingMilliseconds() > 0, "fresh budget permits work");
    if (argc > 1 && std::string(argv[1]) == "--benchmark") engine::ModelAssetCacheTestAccess::Benchmark(directory);
    std::filesystem::remove_all(directory);
    return failures ? 1 : 0;
}
