#pragma once

namespace game {
inline constexpr const char* DynamicLightSwayLabels[]{
    "Horizontal sway radius (m)", "Vertical sway amount (m)", "Sway speed"};
inline constexpr int DynamicLightSwayFieldCount = sizeof(DynamicLightSwayLabels)/sizeof(DynamicLightSwayLabels[0]);
inline float DynamicLightSwayInspectorContentHeight(float rowHeight, float gap)
{
    return rowHeight + gap + DynamicLightSwayFieldCount*(rowHeight*2.5f + gap*2);
}
} // namespace game
