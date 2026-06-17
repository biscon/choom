#pragma once

namespace engine {

struct InputConfig {
    float doubleClickThreshold = 0.3f;
    float clickMaxMoveDistance = 8.0f;

    float keyRepeatInitialDelay = 0.45f;
    float keyRepeatInterval = 0.04f;
};

} // namespace engine
