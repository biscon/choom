#pragma once

#include "engine/render/HdrEffectPolicy.h"
#include "engine/render/RenderTarget.h"

#include <raylib.h>

#include <string>

namespace game {

enum class SectorBloomDebugView {
    Normal,
    SceneBefore,
    Prefilter,
    BlurredBloom,
    BloomOnly,
    SceneAfter
};

constexpr bool IsSectorBloomDiagnosticView(SectorBloomDebugView view)
{
    return view != SectorBloomDebugView::Normal;
}

struct SectorBloomDiagnostics {
    bool ready = false;
    bool disabled = false;
    bool finiteHalfProtection = true;
    int sceneWidth = 0;
    int sceneHeight = 0;
    int bloomWidth = 0;
    int bloomHeight = 0;
    engine::HdrBloomSettings settings;
    std::string status;
    std::string prefilterTarget;
    std::string blurATarget;
    std::string blurBTarget;
};

class SectorBloomRenderer {
public:
    void Shutdown();
    bool Apply(
            engine::RenderTarget& sceneTarget,
            engine::RenderTarget& sceneScratch,
            const engine::HdrBloomSettings& settings);

    void SetDebugView(SectorBloomDebugView value) { debugView = value; }
    SectorBloomDebugView DebugView() const { return debugView; }
    const engine::RenderTarget* DebugSource() const { return debugSource; }
    const SectorBloomDiagnostics& Diagnostics() const { return diagnostics; }
    bool IsLoaded() const;

private:
    bool EnsureResources(int sceneWidth, int sceneHeight);
    void DisableForCurrentKey(const std::string& reason, int width, int height);

    Shader prefilterShader = {};
    Shader blurShader = {};
    Shader compositeShader = {};
    int prefilterSourceTexelSizeLoc = -1;
    int prefilterThresholdLoc = -1;
    int prefilterSoftKneeLoc = -1;
    int blurTexelSizeLoc = -1;
    int blurDirectionLoc = -1;
    int blurRadiusLoc = -1;
    int compositeBloomTextureLoc = -1;
    int compositeIntensityLoc = -1;
    int compositeBloomOnlyLoc = -1;
    engine::RenderTarget prefilterTarget;
    engine::RenderTarget blurA;
    engine::RenderTarget blurB;
    int sceneWidth = 0;
    int sceneHeight = 0;
    int failedWidth = 0;
    int failedHeight = 0;
    bool failedForCurrentKey = false;
    SectorBloomDebugView debugView = SectorBloomDebugView::Normal;
    const engine::RenderTarget* debugSource = nullptr;
    SectorBloomDiagnostics diagnostics;
};

const char* SectorBloomDebugViewName(SectorBloomDebugView view);

} // namespace game
