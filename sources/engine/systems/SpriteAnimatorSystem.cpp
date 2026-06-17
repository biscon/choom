#include "engine/systems/SpriteAnimatorSystem.h"

#include "engine/components/SpriteAnimator.h"
#include "engine/components/SpriteRenderer.h"

#include <algorithm>

namespace engine {

static uint32_t PlaybackFrameCount(const SpriteClip& clip)
{
    if (clip.playback == SpritePlaybackMode::PingPong && clip.frameCount > 1) {
        return clip.frameCount * 2 - 2;
    }

    return clip.frameCount;
}

static uint32_t ResolveFrameOffset(const SpriteClip& clip, uint32_t frameInClip)
{
    if (clip.frameCount == 0) {
        return 0;
    }

    switch (clip.playback) {
        case SpritePlaybackMode::Reverse:
            return clip.frameCount - 1 - std::min(frameInClip, clip.frameCount - 1);
        case SpritePlaybackMode::PingPong: {
            if (clip.frameCount == 1) {
                return 0;
            }

            const uint32_t period = PlaybackFrameCount(clip);
            const uint32_t sequenceFrame = frameInClip % period;
            if (sequenceFrame < clip.frameCount) {
                return sequenceFrame;
            }
            return period - sequenceFrame;
        }
        case SpritePlaybackMode::Once:
        case SpritePlaybackMode::Loop:
            return std::min(frameInClip, clip.frameCount - 1);
    }

    return 0;
}

static void ApplyCurrentFrame(
        const SpriteAnimationAsset& asset,
        const SpriteClip& clip,
        const SpriteAnimator& animator,
        SpriteRenderer& renderer)
{
    if (clip.frameCount == 0 || asset.frames.empty()) {
        renderer.visible = false;
        return;
    }

    const uint32_t frameOffset = ResolveFrameOffset(clip, animator.frameInClip);
    const uint32_t frameIndex = clip.firstFrame + frameOffset;
    if (frameIndex >= asset.frames.size()) {
        renderer.visible = false;
        return;
    }

    const SpriteFrame& frame = asset.frames[frameIndex];
    Vector2 size = frame.sourceSize;
    if (size.x <= 0.0f || size.y <= 0.0f) {
        size = Vector2{frame.source.width, frame.source.height};
    }

    renderer.texture = asset.atlasTexture;
    renderer.source = frame.source;
    renderer.size = size;
    renderer.origin = Vector2{size.x * 0.5f, size.y * 0.5f};
    renderer.visible = true;
}

static bool AdvanceOneFrame(SpriteAnimator& animator, const SpriteClip& clip)
{
    const uint32_t sequenceLength = PlaybackFrameCount(clip);
    if (sequenceLength == 0) {
        return false;
    }

    const bool shouldLoop = animator.loop && clip.playback != SpritePlaybackMode::Once;
    if (animator.frameInClip + 1 < sequenceLength) {
        ++animator.frameInClip;
        return true;
    }

    if (shouldLoop) {
        animator.frameInClip = 0;
        return true;
    }

    animator.frameInClip = sequenceLength - 1;
    animator.finished = true;
    animator.playing = false;
    return false;
}

void SpriteAnimatorSystem(World& world, AssetManager& assets, float dt)
{
    world.ForEach<SpriteAnimator, SpriteRenderer>(
            [&assets, dt](Entity, SpriteAnimator& animator, SpriteRenderer& renderer) {
                const SpriteAnimationAsset* asset = assets.GetSpriteAnimation(animator.animation);
                if (asset == nullptr) {
                    return;
                }

                if (animator.clipIndex == InvalidSpriteClipIndex && !asset->clips.empty()) {
                    SetSpriteClip(animator, 0, true);
                }

                if (animator.clipIndex >= asset->clips.size()) {
                    renderer.visible = false;
                    return;
                }

                const SpriteClip& clip = asset->clips[animator.clipIndex];
                if (clip.frameCount == 0 || clip.firstFrame >= asset->frames.size()) {
                    renderer.visible = false;
                    return;
                }

                const uint32_t sequenceLength = PlaybackFrameCount(clip);
                if (sequenceLength == 0) {
                    renderer.visible = false;
                    return;
                }
                if (animator.frameInClip >= sequenceLength) {
                    animator.frameInClip = sequenceLength - 1;
                }

                if (animator.playing && !animator.finished && animator.speed > 0.0f) {
                    animator.elapsedInFrame += dt * animator.speed;

                    uint32_t guard = 0;
                    while (guard < 256) {
                        const uint32_t frameOffset = ResolveFrameOffset(clip, animator.frameInClip);
                        const uint32_t frameIndex = clip.firstFrame + frameOffset;
                        if (frameIndex >= asset->frames.size()) {
                            break;
                        }

                        const float duration = std::max(asset->frames[frameIndex].durationSeconds, 0.001f);
                        if (animator.elapsedInFrame < duration) {
                            break;
                        }

                        animator.elapsedInFrame -= duration;
                        if (!AdvanceOneFrame(animator, clip)) {
                            animator.elapsedInFrame = 0.0f;
                            break;
                        }
                        ++guard;
                    }
                }

                ApplyCurrentFrame(*asset, clip, animator, renderer);
            }
    );
}

} // namespace engine
