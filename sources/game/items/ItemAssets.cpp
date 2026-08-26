#include "game/items/ItemAssets.h"

#include "engine/assets/AssetManager.h"
#include "sector_demo/SectorAssetPaths.h"

#include <raymath.h>

#include <algorithm>
#include <array>
#include <cstdio>

namespace {

constexpr const char* IconVertexShader = R"GLSL(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec2 fragTexCoord;
out vec3 fragNormal;
void main() {
    fragTexCoord = vertexTexCoord;
    fragNormal = normalize((matNormal*vec4(vertexNormal, 0.0)).xyz);
    gl_Position = mvp*vec4(vertexPosition, 1.0);
}
)GLSL";

constexpr const char* IconFragmentShader = R"GLSL(
#version 330
in vec2 fragTexCoord;
in vec3 fragNormal;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
out vec4 finalColor;
void main() {
    vec4 base = texture(texture0, fragTexCoord)*colDiffuse;
    if (base.a <= 0.001) discard;
    vec3 n = normalize(fragNormal);
    float key = max(dot(n, normalize(vec3(-0.45, 0.78, 0.42))), 0.0);
    float fill = max(dot(n, normalize(vec3(0.70, 0.30, -0.64))), 0.0);
    float rim = pow(1.0-max(dot(n, normalize(vec3(-0.53, -0.40, -0.75))), 0.0), 3.0);
    vec3 light = vec3(0.20) + vec3(0.70)*key + vec3(0.24,0.30,0.42)*fill
            + vec3(0.20,0.26,0.34)*rim;
    finalColor = vec4(base.rgb*light, base.a);
}
)GLSL";

void DrawPlaceholder(Image& atlas, Rectangle destination)
{
    const Rectangle inset{
            destination.x + 8.0f, destination.y + 8.0f,
            destination.width - 16.0f, destination.height - 16.0f};
    ImageDrawRectangleRec(&atlas, inset, Color{45, 23, 49, 255});
    constexpr int Tile = 16;
    for (int y = 0; y < 7; ++y) {
        for (int x = 0; x < 7; ++x) {
            if (((x + y) & 1) == 0) {
                ImageDrawRectangle(
                        &atlas,
                        static_cast<int>(inset.x) + x * Tile,
                        static_cast<int>(inset.y) + y * Tile,
                        Tile, Tile,
                        Color{210, 62, 198, 255});
            }
        }
    }
}

bool DrawModelIconCell(
        const engine::ModelAsset& asset,
        const game::ItemIconCameraFit& fit,
        Shader shader,
        Image& outImage)
{
    if (asset.model.meshCount <= 0 || asset.model.meshes == nullptr
            || asset.model.materialCount <= 0
            || asset.model.materials == nullptr) {
        return false;
    }
    RenderTexture2D target = LoadRenderTexture(
            game::kItemIconCellPixels, game::kItemIconCellPixels);
    if (target.id == 0) return false;
    Camera3D camera{};
    camera.position = fit.position;
    camera.target = fit.target;
    camera.up = fit.up;
    camera.fovy = fit.orthographicSize;
    camera.projection = CAMERA_ORTHOGRAPHIC;
    BeginTextureMode(target);
    ClearBackground(BLANK);
    BeginMode3D(camera);
    for (int meshIndex = 0; meshIndex < asset.model.meshCount; ++meshIndex) {
        const int materialIndex = asset.model.meshMaterial != nullptr
                ? asset.model.meshMaterial[meshIndex] : 0;
        Material material = asset.model.materials[std::clamp(
                materialIndex, 0, std::max(0, asset.model.materialCount - 1))];
        material.shader = shader;
        DrawMesh(asset.model.meshes[meshIndex], material, asset.model.transform);
    }
    EndMode3D();
    EndTextureMode();
    outImage = LoadImageFromTexture(target.texture);
    UnloadRenderTexture(target);
    if (outImage.data == nullptr) return false;
    ImageFlipVertical(&outImage);
    return true;
}

} // namespace

namespace game {

void RebuildItemModelAssets(
        engine::AssetManager& assets,
        const ItemRegistry& registry,
        ItemModelAssetState& state)
{
    if (!engine::IsNull(state.scope)) assets.UnloadScope(state.scope);
    state = ItemModelAssetState{};
    state.scope = assets.CreateScope("global_item_models");
    state.sourceRegistryRevision = registry.revision;
    state.entries.reserve(registry.items.size());
    std::vector<const ItemDefinition*> sorted;
    sorted.reserve(registry.items.size());
    for (const ItemDefinition& definition : registry.items) sorted.push_back(&definition);
    std::sort(sorted.begin(), sorted.end(),
            [](const ItemDefinition* left, const ItemDefinition* right) {
                return left->id < right->id;
            });
    for (const ItemDefinition* definition : sorted) {
        ItemModelAssetEntry entry;
        entry.definitionId = definition->id;
        entry.modelPath = definition->modelPath;
        const std::string resolved = ResolveSectorAssetPath(definition->modelPath);
        const std::string key = "global_item_model_" + definition->id;
        entry.model = assets.RequestModel(
                state.scope, key.c_str(), resolved.c_str(),
                engine::ModelLoad_None);
        state.entries.push_back(std::move(entry));
    }
}

void ShutdownItemModelAssets(
        engine::AssetManager& assets,
        ItemModelAssetState& state)
{
    if (!engine::IsNull(state.scope)) assets.UnloadScope(state.scope);
    state = ItemModelAssetState{};
}

const ItemModelAssetEntry* FindItemModelAsset(
        const ItemModelAssetState& state,
        std::string_view definitionId)
{
    const auto found = std::find_if(
            state.entries.begin(), state.entries.end(),
            [definitionId](const ItemModelAssetEntry& entry) {
                return entry.definitionId == definitionId;
            });
    return found == state.entries.end() ? nullptr : &*found;
}

ItemModelAssetStatus GetItemModelAssetStatus(
        const engine::AssetManager& assets,
        const ItemModelAssetEntry& entry)
{
    if (assets.IsReady(entry.model)) return ItemModelAssetStatus::Ready;
    if (assets.HasFailed(entry.model)) return ItemModelAssetStatus::Failed;
    return ItemModelAssetStatus::Pending;
}

bool UpdateItemIconPreparation(
        engine::AssetManager& assets,
        const ItemRegistry& registry,
        ItemModelAssetState& state)
{
    if (IsItemIconPreparationTerminal(state)
            && state.preparedIconRevision == registry.revision) {
        return true;
    }
    if (state.sourceRegistryRevision != registry.revision) {
        state.iconPreparation = ItemIconPreparationState::Failed;
        state.iconDiagnostic = "Item icon source revision does not match the registry";
        return true;
    }
    for (const ItemModelAssetEntry& entry : state.entries) {
        if (GetItemModelAssetStatus(assets, entry)
                == ItemModelAssetStatus::Pending) {
            state.iconPreparation = ItemIconPreparationState::WaitingForModels;
            return false;
        }
    }
    std::string layoutError;
    if (!BuildItemIconAtlasLayout(registry, state.iconLayout, layoutError)) {
        state.iconPreparation = ItemIconPreparationState::Failed;
        state.iconDiagnostic = layoutError;
        return true;
    }
    if (state.iconLayout.regions.empty()) {
        state.preparedIconRevision = registry.revision;
        state.iconPreparation = ItemIconPreparationState::Ready;
        state.iconDiagnostic.clear();
        return true;
    }
    Image atlas = GenImageColor(
            state.iconLayout.widthPixels,
            state.iconLayout.heightPixels,
            BLANK);
    if (atlas.data == nullptr) {
        state.iconPreparation = ItemIconPreparationState::Failed;
        state.iconDiagnostic = "Could not allocate the item icon CPU atlas";
        return true;
    }
    Shader shader = LoadShaderFromMemory(IconVertexShader, IconFragmentShader);
    if (shader.id == 0) {
        UnloadImage(atlas);
        state.iconPreparation = ItemIconPreparationState::Failed;
        state.iconDiagnostic = "Could not create the item icon lighting shader";
        return true;
    }
    for (ItemIconRegion& region : state.iconLayout.regions) {
        const ItemModelAssetEntry* entry = FindItemModelAsset(
                state, region.definitionId);
        const engine::ModelAsset* model = entry != nullptr
                ? assets.GetModelAsset(entry->model) : nullptr;
        if (model == nullptr || !model->hasLocalBounds) {
            region.placeholder = true;
            DrawPlaceholder(atlas, region.source);
            continue;
        }
        const ItemIconCameraFit fit = BuildItemIconCameraFit(model->localBounds);
        Image cell{};
        if (!fit.valid || !DrawModelIconCell(*model, fit, shader, cell)) {
            region.placeholder = true;
            DrawPlaceholder(atlas, region.source);
            continue;
        }
        ImageDraw(
                &atlas,
                cell,
                Rectangle{0.0f, 0.0f,
                        static_cast<float>(cell.width),
                        static_cast<float>(cell.height)},
                region.source,
                WHITE);
        UnloadImage(cell);
    }
    UnloadShader(shader);
    state.iconAtlas = assets.CreateTextureFromImage(
            state.scope,
            "global_item_icon_atlas",
            atlas,
            engine::TextureColorUsage::DisplaySrgb,
            engine::TextureLoad_BilinearFilter);
    UnloadImage(atlas);
    if (engine::IsNull(state.iconAtlas)) {
        state.iconPreparation = ItemIconPreparationState::Failed;
        state.iconDiagnostic = "Could not upload the item icon atlas";
        return true;
    }
    state.preparedIconRevision = registry.revision;
    state.iconPreparation = ItemIconPreparationState::Ready;
    state.iconDiagnostic.clear();
    return true;
}

} // namespace game
