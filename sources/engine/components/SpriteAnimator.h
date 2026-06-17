#pragma once

#include "engine/assets/AssetHandles.h"

#include <cstdint>

namespace engine {

static constexpr uint32_t InvalidSpriteClipIndex = UINT32_MAX;

struct SpriteAnimator {
    SpriteAnimationHandle animation;
    uint32_t clipIndex = InvalidSpriteClipIndex;
    float elapsedInFrame = 0.0f;
    uint32_t frameInClip = 0;
    float speed = 1.0f;
    bool playing = true;
    bool loop = true;
    bool finished = false;
};

inline void SetSpriteClip(SpriteAnimator& animator, uint32_t clipIndex, bool restart = true)
{
    if (!restart && animator.clipIndex == clipIndex) {
        return;
    }

    if (animator.clipIndex != clipIndex || restart) {
        animator.clipIndex = clipIndex;
        animator.elapsedInFrame = 0.0f;
        animator.frameInClip = 0;
        animator.finished = false;
        animator.playing = true;
    }
}

} // namespace engine
