#pragma once

#include "engine/EngineContext.h"
#include "engine/ui/UI.h"
#include "game/ApplicationFlow.h"
#include "game/FpsWeaponRegistry.h"
#include "game/SectorGameSession.h"
#include "sector_editor/SectorEditor.h"

#include <optional>
#include <string>

namespace game {

enum class ApplicationContentKind {
    Empty,
    Editor2D,
    Sector3D
};

class GameApplication {
public:
    bool Init(engine::EngineContext& context);
    void Shutdown(engine::EngineContext& context);

    void RenderInteractiveUI(
            engine::UIContext& contentUi,
            engine::UIContext& menuUi,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font,
            engine::FontHandle smallFont);
    void Update(engine::EngineContext& context, float dt);

    ApplicationContentKind BackgroundContentKind() const;
    bool ShouldRefreshBackground() const;
    bool IsMenuOpen() const;
    bool QuitRequested() const { return flow.quitRequested; }

    void Render2D(engine::AssetManager& assets);
    void Render3DShadowMaps(engine::EngineContext& context);
    void Render3DScene(engine::EngineContext& context);
    void Render3DViewmodel(engine::AssetManager& assets);
    void Render3DOverlays();
    void Apply3DPostProcessing(
            engine::AssetManager& assets,
            RenderTexture2D& sceneTarget);
    void Render3DHud(Rectangle playableViewport) const;

private:
    void HandleMenuAction(
            engine::EngineContext& context,
            MainMenuAction action);
    void StartNewGame(engine::EngineContext& context);
    void ResumeGame(engine::EngineContext& context);
    void OpenEditor(engine::EngineContext& context);
    ApplicationScreen BackgroundScreen() const;

    ApplicationFlowState flow;
    SectorEditor editor;
    SectorGameSession gameSession;
    SectorSceneRuntime gameScene;
    FpsWeaponRegistry weaponRegistry;
    FpsApplicationSettings applicationSettings;
    std::string menuStatus;
    std::optional<MainMenuAction> pendingMenuAction;
    bool editorAttachedToGame = false;
    bool initialized = false;
};

} // namespace game
