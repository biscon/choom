#include "game/FpsWeaponPellets.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {

Vector3 FpsWeaponPelletDirection(
        Vector3 rawAimDirection,
        const FpsWeaponPelletDefinition& rawDefinition,
        int pelletIndex,
        uint64_t shotSequence)
{
    Vector3 aimDirection = Vector3Normalize(rawAimDirection);
    if (Vector3LengthSqr(aimDirection) <= 0.000001f) {
        aimDirection = Vector3{0.0f, 0.0f, 1.0f};
    }
    const FpsWeaponPelletDefinition definition{
            rawDefinition.enabled,
            std::clamp(rawDefinition.count, 1, MaxFpsWeaponPellets),
            std::isfinite(rawDefinition.spreadHalfAngleDegrees)
                    ? std::clamp(
                            rawDefinition.spreadHalfAngleDegrees,
                            0.0f,
                            45.0f)
                    : FpsWeaponPelletDefinition{}
                            .spreadHalfAngleDegrees};
    if (!definition.enabled
            || definition.count <= 1
            || pelletIndex <= 0
            || definition.spreadHalfAngleDegrees <= 0.0f) {
        return aimDirection;
    }

    uint32_t random = static_cast<uint32_t>(shotSequence)
            ^ static_cast<uint32_t>(shotSequence >> 32u)
            ^ (static_cast<uint32_t>(pelletIndex) * 0x9e3779b9u)
            ^ 0x85ebca6bu;
    const auto randomUnit = [&random]() {
        random ^= random << 13u;
        random ^= random >> 17u;
        random ^= random << 5u;
        return static_cast<float>(random & 0x00ffffffu)
                / static_cast<float>(0x01000000u);
    };
    constexpr float Pi = 3.14159265358979323846f;
    const float radius = std::sqrt(randomUnit());
    const float azimuth = randomUnit() * 2.0f * Pi;
    const float tangentRadius = std::tan(
            definition.spreadHalfAngleDegrees * Pi / 180.0f)
            * radius;
    const Vector3 reference = std::fabs(aimDirection.y) < 0.999f
            ? Vector3{0.0f, 1.0f, 0.0f}
            : Vector3{1.0f, 0.0f, 0.0f};
    const Vector3 right = Vector3Normalize(
            Vector3CrossProduct(reference, aimDirection));
    const Vector3 up = Vector3CrossProduct(aimDirection, right);
    const Vector3 offset = Vector3Add(
            Vector3Scale(right, std::cos(azimuth) * tangentRadius),
            Vector3Scale(up, std::sin(azimuth) * tangentRadius));
    return Vector3Normalize(Vector3Add(aimDirection, offset));
}

} // namespace game
