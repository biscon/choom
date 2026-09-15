#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace game {

// direction == 0 chooses the nearest point (halfway ties away from zero).
// Otherwise choose the next point in that direction, tolerating float storage
// and authoring/world or radians/degrees round trips at an existing grid point.
inline float SnapSectorEditorPreviewGridCoordinate(
        float value, double spacing, int direction)
{
    if (!std::isfinite(value) || !std::isfinite(spacing) || spacing <= 0.0) {
        return value;
    }
    double index = static_cast<double>(value) / spacing;
    const double tolerance = std::min(spacing * 0.001,
            4.0 * std::numeric_limits<float>::epsilon()
                    * std::max(1.0, std::fabs(static_cast<double>(value)))) / spacing;
    if (direction == 0) {
        // Decimal halfway inputs may lie just either side after float storage.
        const double halfIndex = std::round(index * 2.0) * 0.5;
        if (std::fabs(index - halfIndex) <= tolerance) index = halfIndex;
        index = std::round(index);
    } else {
        const double nearest = std::round(index);
        if (std::fabs(index - nearest) <= tolerance) index = nearest;
        index = direction > 0 ? std::floor(index) + 1.0 : std::ceil(index) - 1.0;
    }
    return static_cast<float>(index * spacing);
}

} // namespace game
