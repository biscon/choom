#pragma once

#include <raylib.h>

namespace engine {

constexpr float Rgba16fMaximumFinite = 65504.0f;

float SanitizeLinearHdrChannelForRgba16f(float value);
Vector3 SanitizeLinearHdrForRgba16f(Vector3 value);
Vector4 SanitizeLinearHdrForRgba16f(Vector4 value, float alphaFallback = 0.0f);
float SanitizeBoundedAlpha(float value, float fallback = 0.0f);

struct HdrBloomSettings {
    bool enabled = true;
    float threshold = 1.0f;
    float softKnee = 0.5f;
    float intensity = 0.25f;
    float radius = 1.0f;
};

HdrBloomSettings NormalizeHdrBloomSettings(HdrBloomSettings settings);
Vector3 EvaluateHdrBloomPrefilter(Vector3 linearSceneRgb, const HdrBloomSettings& settings);
Vector4 CompositeHdrPremultipliedAtmosphere(
        Vector4 linearScene,
        Vector4 premultipliedScattering);
Vector4 CompositeHdrBloom(Vector4 linearScene, Vector3 bloomRgb, float intensity);

enum class HdrPostProcessOverlayRoute {
    Skip,
    DrawSceneTarget,
    CommitScratchThenDrawSceneTarget
};

HdrPostProcessOverlayRoute ResolveHdrPostProcessOverlayRoute(
        bool overlayRequested,
        bool presentingFromScratch,
        bool diagnosticPresentationOverride);

// Canonical GLSL 330 storage policy for generated shaders and policy tests.
// Embedded renderer shaders mirror this named contract at RGBA16F writes.
extern const char* const Rgba16fStoragePolicyGlsl;

} // namespace engine
