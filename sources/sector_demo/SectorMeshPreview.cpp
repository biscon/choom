#include "sector_demo/SectorMeshPreview.h"

#include "engine/assets/TextureLoadFlags.h"
#include "sector_demo/SectorMeshBuilder.h"
#include "sector_demo/SectorMap.h"

#include <raylib.h>
#include <raymath.h>

#include <cmath>
#include <cstdio>

namespace game {

namespace {

constexpr float MouseSensitivity = 0.0030f;
constexpr float MoveSpeed = 5.0f;
constexpr float PitchLimit = 1.45f;
constexpr int MouseLookWarmupFrames = 2;

bool StartsWith(const std::string& value, const char* prefix)
{
    const std::string prefixString(prefix);
    return value.compare(0, prefixString.size(), prefixString) == 0;
}

} // namespace

bool SectorMeshPreview::Rebuild(
        engine::AssetManager& assets,
        const SectorMap& map,
        const char* scopeName,
        std::string& error)
{
    Shutdown(assets);
    error.clear();

    if (map.sectors.empty()) {
        error = "Preview failed: no sectors";
        return false;
    }

    if (map.texturesById.empty()) {
        error = "Preview failed: missing texture table";
        return false;
    }

    assetScope = assets.CreateScope(scopeName == nullptr ? "sector_mesh_preview" : scopeName);
    if (engine::IsNull(assetScope)) {
        error = "Preview failed: could not create asset scope";
        return false;
    }

    for (const std::string& textureId : SortedSectorTextureIds(map)) {
        const SectorTextureDefinition* texture = FindSectorTexture(map, textureId);
        if (texture == nullptr) {
            continue;
        }

        const std::string resolvedPath = ResolveAssetPath(texture->path);
        engine::TextureHandle handle = assets.RequestTexture(
                assetScope,
                texture->id.c_str(),
                resolvedPath.c_str(),
                SectorTextureLoadFlags(texture->filter)
        );
        textureHandlesById.emplace(texture->id, handle);
    }

    meshes = BuildSectorMeshes(map);
    if (meshes.batches.empty()) {
        Shutdown(assets);
        error = "Preview failed: mesh builder produced no batches";
        return false;
    }

    material = LoadMaterialDefault();
    defaultMaterialTexture = material.maps[MATERIAL_MAP_DIFFUSE].texture;
    materialLoaded = true;

    sectorCount = map.sectors.size();
    position = map.playerStartPosition;
    yawRadians = map.playerStartYawRadians;
    pitchRadians = 0.0f;
    mouseLookEnabled = true;
    camera.fovy = 75.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    UpdateCamera();

    initialized = true;
    Enter();
    return true;
}

void SectorMeshPreview::Shutdown(engine::AssetManager& assets)
{
    if (!initialized
            && engine::IsNull(assetScope)
            && meshes.batches.empty()
            && !materialLoaded) {
        return;
    }

    Leave();
    UnloadSectorMeshes(meshes);
    textureHandlesById.clear();
    sectorCount = 0;

    if (materialLoaded) {
        material.maps[MATERIAL_MAP_DIFFUSE].texture = defaultMaterialTexture;
        UnloadMaterial(material);
        material = Material{};
        defaultMaterialTexture = Texture2D{};
        materialLoaded = false;
    }

    if (!engine::IsNull(assetScope)) {
        assets.UnloadScope(assetScope);
        assetScope = engine::NullAssetScopeHandle();
    }

    initialized = false;
}

void SectorMeshPreview::Enter()
{
    if (!initialized) {
        return;
    }

    mouseLookEnabled = true;
    mouseLookWarmupFrames = MouseLookWarmupFrames;
    DisableCursor();
}

void SectorMeshPreview::Leave()
{
    EnableCursor();
}

void SectorMeshPreview::Update(engine::Input& input, float dt)
{
    if (!initialized) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this](engine::InputEvent& event) {
                if (event.key.key != KEY_F11) {
                    return;
                }

                mouseLookEnabled = !mouseLookEnabled;
                if (mouseLookEnabled) {
                    mouseLookWarmupFrames = MouseLookWarmupFrames;
                    DisableCursor();
                } else {
                    EnableCursor();
                }
                engine::ConsumeEvent(event);
            }
    );

    if (mouseLookEnabled) {
        if (mouseLookWarmupFrames > 0) {
            --mouseLookWarmupFrames;
        } else {
            const Vector2 mouseDelta = input.MouseDelta();
            yawRadians += mouseDelta.x * MouseSensitivity;
            pitchRadians -= mouseDelta.y * MouseSensitivity;
            pitchRadians = Clamp(pitchRadians, -PitchLimit, PitchLimit);
        }
    }

    if (mouseLookEnabled) {
        Vector3 forward{std::cos(yawRadians), 0.0f, std::sin(yawRadians)};
        Vector3 right{-forward.z, 0.0f, forward.x};
        Vector3 movement{};

        if (input.IsKeyDown(KEY_W)) {
            movement = Vector3Add(movement, forward);
        }
        if (input.IsKeyDown(KEY_S)) {
            movement = Vector3Subtract(movement, forward);
        }
        if (input.IsKeyDown(KEY_D)) {
            movement = Vector3Add(movement, right);
        }
        if (input.IsKeyDown(KEY_A)) {
            movement = Vector3Subtract(movement, right);
        }
        if (input.IsKeyDown(KEY_SPACE)) {
            movement.y += 1.0f;
        }
        if (input.IsKeyDown(KEY_LEFT_CONTROL) || input.IsKeyDown(KEY_RIGHT_CONTROL)) {
            movement.y -= 1.0f;
        }

        if (Vector3LengthSqr(movement) > 0.0001f) {
            movement = Vector3Normalize(movement);
            position = Vector3Add(position, Vector3Scale(movement, MoveSpeed * dt));
        }
    }

    UpdateCamera();
}

void SectorMeshPreview::Render(engine::AssetManager& assets)
{
    if (!initialized) {
        return;
    }

    BeginMode3D(camera);
    for (const SectorMeshBatch& batch : meshes.batches) {
        const engine::TextureHandle textureHandle = TextureForId(batch.textureId);
        const Texture2D* texture = assets.GetTexture(textureHandle);
        material.maps[MATERIAL_MAP_DIFFUSE].texture = (texture != nullptr)
                ? *texture
                : defaultMaterialTexture;

        DrawMesh(batch.mesh, material, MatrixIdentity());
    }
    EndMode3D();
}

SectorMeshPreviewPose SectorMeshPreview::Pose() const
{
    return SectorMeshPreviewPose{position, yawRadians, pitchRadians};
}

void SectorMeshPreview::ApplyPose(const SectorMeshPreviewPose& pose)
{
    position = pose.position;
    yawRadians = pose.yawRadians;
    pitchRadians = pose.pitchRadians;
    UpdateCamera();
}

void SectorMeshPreview::SetMouseLookEnabled(bool enabled)
{
    if (!initialized) {
        return;
    }

    mouseLookEnabled = enabled;
    if (mouseLookEnabled) {
        mouseLookWarmupFrames = MouseLookWarmupFrames;
        DisableCursor();
    } else {
        EnableCursor();
    }
}

float SectorMeshPreview::AssetProgress(engine::AssetManager& assets) const
{
    return engine::IsNull(assetScope) ? 1.0f : assets.GetScopeProgress(assetScope);
}

std::string SectorMeshPreview::ResolveAssetPath(const std::string& path)
{
    if (StartsWith(path, "assets/")) {
        return std::string(ASSETS_PATH) + path.substr(7);
    }

    return path;
}

engine::TextureHandle SectorMeshPreview::TextureForId(const std::string& textureId) const
{
    const auto it = textureHandlesById.find(textureId);
    if (it == textureHandlesById.end()) {
        return engine::NullTextureHandle();
    }

    return it->second;
}

void SectorMeshPreview::UpdateCamera()
{
    const float cosPitch = std::cos(pitchRadians);
    const Vector3 look{
            std::cos(yawRadians) * cosPitch,
            std::sin(pitchRadians),
            std::sin(yawRadians) * cosPitch
    };

    camera.position = position;
    camera.target = Vector3Add(position, look);
    camera.up = Vector3{0.0f, 1.0f, 0.0f};
}

} // namespace game
