#include "game/PlayerFlashlight.h"

#include <cassert>
#include <cmath>

namespace {

bool Near(float left, float right, float epsilon = 0.0001f)
{
    return std::fabs(left - right) <= epsilon;
}

void TestDefaultsAndNormalization()
{
    const game::PlayerFlashlightApplicationSettings defaults;
    assert(game::PlayerFlashlightSettingsError(defaults).empty());
    assert(Near(defaults.intensity, 4.0f));
    assert(Near(defaults.reachWorld, 18.0f));
    assert(Near(defaults.coneRadiusWorld, 5.0f));
    assert(Near(defaults.shadowContactOffsetWorld, 0.005f));
    assert(Near(defaults.lateralOffsetWorld, 0.10f));
    assert(Near(defaults.aimConvergenceDistanceWorld, 10.0f));

    game::PlayerFlashlightApplicationSettings invalid = defaults;
    invalid.reachWorld = -1.0f;
    assert(!game::PlayerFlashlightSettingsError(invalid).empty());
    const auto normalized = game::NormalizePlayerFlashlightSettings(invalid);
    assert(Near(normalized.reachWorld, 1.0f));
}

void TestToggleAndRuntimeSource()
{
    game::PlayerFlashlightState state;
    game::SectorPreviewDynamicPointLightSource light;
    game::SectorLightAtmosphereSource atmosphere;
    const Camera3D camera{
            Vector3{1.0f, 1.6f, 2.0f},
            Vector3{1.0f, 1.6f, 1.0f},
            Vector3{0.0f, 1.0f, 0.0f},
            60.0f,
            CAMERA_PERSPECTIVE};
    assert(!game::UpdatePlayerFlashlight(
            state, {}, camera, 7, 1.0f / 60.0f, light, atmosphere));

    game::TogglePlayerFlashlight(state);
    assert(game::UpdatePlayerFlashlight(
            state, {}, camera, 7, 1.0f / 60.0f, light, atmosphere));
    assert(light.lightId == game::PlayerFlashlightRuntimeLightId);
    assert(light.ownerSectorId == 7);
    assert(light.light.kind == game::SectorPreviewDynamicLightKind::Spot);
    assert(light.light.castsShadow);
    assert(light.light.reserveSelection && light.light.reserveShadow);
    assert(light.light.profile == game::SectorDynamicLightProfile::Flashlight);
    assert(Near(light.light.position.x, 1.10f));
    assert(Near(light.light.position.y, 1.72f));
    assert(Near(light.light.position.z, 1.95f));
    assert(Near(light.light.radius, 18.0f));
    assert(Near(light.light.outerConeCos,
            std::cos(std::atan2(5.0f, 18.0f))));
    assert(Near(light.light.innerConeCos, light.light.outerConeCos));
    assert(Near(light.light.shadowBias, 0.005f));
    const Vector3 convergenceTarget{1.0f, 1.6f, -8.0f};
    const Vector3 fromOrigin{
            convergenceTarget.x - light.light.position.x,
            convergenceTarget.y - light.light.position.y,
            convergenceTarget.z - light.light.position.z};
    const float convergenceDistance = std::sqrt(
            fromOrigin.x * fromOrigin.x
                    + fromOrigin.y * fromOrigin.y
                    + fromOrigin.z * fromOrigin.z);
    assert(Near(light.light.direction.x,
            fromOrigin.x / convergenceDistance));
    assert(Near(light.light.direction.y,
            fromOrigin.y / convergenceDistance));
    assert(Near(light.light.direction.z,
            fromOrigin.z / convergenceDistance));
    assert(atmosphere.lightId == light.lightId);
    assert(atmosphere.shape == game::SectorLightAtmosphereShape::Cone);
    assert(atmosphere.atmosphere.proxy.shaft.enabled);

    const Vector3 oldDirection = state.smoothedDirection;
    Camera3D turned = camera;
    turned.target = Vector3{2.0f, 1.6f, 2.0f};
    assert(game::UpdatePlayerFlashlight(
            state, {}, turned, 7, 0.01f, light, atmosphere));
    assert(state.smoothedDirection.x > oldDirection.x);
    assert(state.smoothedDirection.x < 1.0f);
    assert(state.smoothedDirection.z < 0.0f);

    game::SetPlayerFlashlightEnabled(state, false);
    assert(!state.directionValid);
}

} // namespace

int main()
{
    TestDefaultsAndNormalization();
    TestToggleAndRuntimeSource();
    return 0;
}
