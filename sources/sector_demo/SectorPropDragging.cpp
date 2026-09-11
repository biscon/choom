#include "sector_demo/SectorPropDragging.h"
#include "engine/EngineContext.h"
#include "sector_demo/SectorBoxSweep.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorUnits.h"
#include <cmath>

namespace game
{
namespace
{
constexpr float DragAccelerationSeconds = 0.3f;
constexpr float DragBrakingSeconds = 0.2f;
constexpr float DragCenterSeconds = 0.2f;
constexpr float DragYawLimit = 40.0f * DEG2RAD;
constexpr float DragPitchLimit = 25.0f * DEG2RAD;

float AngleDifference(float angle, float center)
{
    return std::remainder(angle - center, 2.0f * PI);
}

struct DragMotionStep
{
    float displacement = 0;
    float velocity = 0;
    double seconds = 0;
};

// Integrate one constant-acceleration phase exactly. Split at peak speed,
// braking onset, zero velocity, and endpoints so reversal cannot skip a sweep.
DragMotionStep NextMotionStep(float position, float length, float maxSpeed, float velocity,
                              int request, double seconds)
{
    const int direction = velocity != 0 ? (velocity > 0 ? 1 : -1) : request;
    if (!direction || seconds <= 0)
        return {0, 0, seconds};
    const double distance = direction > 0 ? length - position : position;
    if (distance <= 0)
        return {0, 0, seconds};
    const double speed = std::abs(velocity);
    const double acceleration = maxSpeed / DragAccelerationSeconds;
    const double braking = maxSpeed / DragBrakingSeconds;
    const double stoppingDistance = speed * speed / (2 * braking);
    double rate = 0;
    double phaseTime = seconds;
    bool stops = false;
    if (speed > 0 && (request != direction || distance <= stoppingDistance + 1e-7))
    {
        rate = -braking;
        phaseTime = speed / braking;
        if (distance < stoppingDistance)
            phaseTime = 2 * distance /
                        (speed + std::sqrt(std::max(0.0, speed * speed - 2 * braking * distance)));
        stops = true;
    }
    else if (speed < maxSpeed)
    {
        rate = acceleration;
        const double peak = std::sqrt(braking * (speed * speed + 2 * acceleration * distance) /
                                      (acceleration + braking));
        phaseTime = (std::min(double(maxSpeed), peak) - speed) / acceleration;
    }
    else
        phaseTime = (distance - stoppingDistance) / speed;

    const double elapsed = std::min(seconds, std::max(phaseTime, 1e-8));
    const double travel =
        std::clamp(speed * elapsed + 0.5 * rate * elapsed * elapsed, 0.0, distance);
    double nextSpeed = std::clamp(speed + rate * elapsed, 0.0, double(maxSpeed));
    if ((stops && elapsed >= phaseTime) || travel >= distance)
        nextSpeed = 0;
    return {float(direction * travel), float(direction * nextSpeed), elapsed};
}

bool Valid(engine::World &world, engine::Entity e)
{
    return world.IsAlive(e) && world.Has<SectorPropDrag>(e) &&
           world.Has<SectorObjectTransform>(e) && world.Has<SectorStaticModelCollider>(e) &&
           world.Get<SectorStaticModelCollider>(e).resolved &&
           !world.Get<SectorStaticModelCollider>(e).failed;
}
engine::SoundPlaybackHandle Play(engine::EngineContext &context, engine::SoundHandle sound,
                                 Vector3 position, bool loop = false)
{
    if (engine::IsNull(sound))
        return engine::NullSoundPlaybackHandle();
    engine::SoundPlaybackSettings settings;
    settings.looping = loop;
    return context.audio.PlaySoundAt(context.assets, sound, {position}, settings);
}
float SweepCollider(const SectorStaticModelCollider &moving, Vector2 delta, Vector2 center,
                    Vector2 x, Vector2 z, Vector2 half, float bottom, float top)
{
    if (moving.bottom >= top - 0.001f || moving.top <= bottom + 0.001f)
        return 1;
    Vector2 corners[4];
    SectorBoxCorners(center, x, z, half, corners);
    return SweepSectorBoxPolygon(moving.center, moving.axisX, moving.axisZ, moving.halfExtents,
                                 delta, corners, 4);
}
} // namespace
bool BeginSectorPropDrag(engine::EngineContext &context, const SectorTopologyMap &map,
                         SectorPropDragSession &session, engine::Entity entity,
                         const SectorFpsControllerState &player)
{
    if (!engine::IsNull(session.entity) || !Valid(context.world, entity) || !player.grounded ||
        player.crouchTargeted || player.crouchAmount > 0.01f)
        return false;
    const auto &drag = context.world.Get<SectorPropDrag>(entity);
    const auto *path = FindSectorPath(map.paths, drag.settings.pathEditorId);
    if (!path || path->length <= 0 || !std::isfinite(drag.settings.speedWorld) ||
        drag.settings.speedWorld <= 0)
        return false;
    const auto &transform = context.world.Get<SectorObjectTransform>(entity);
    const auto &collider = context.world.Get<SectorStaticModelCollider>(entity);
    if (player.feetPosition.y >= collider.top - 0.001f)
        return false;
    session = {};
    session.entity = entity;
    session.resetMouseLook = true;
    session.playerOffset = {player.feetPosition.x - transform.position.x,
                            player.feetPosition.z - transform.position.z};
    Vector2 tangent;
    EvaluateSectorPath(*path, drag.distanceWorld, &tangent);
    float sign = -Vector2DotProduct(session.playerOffset, tangent);
    if (std::abs(sign) < 0.0001f)
        sign =
            Vector2DotProduct(tangent, {std::cos(player.yawRadians), std::sin(player.yawRadians)});
    session.pushDirection = sign < 0 ? -1 : 1;
    Play(context, drag.startSound, transform.position);
    return true;
}
void EndSectorPropDrag(engine::EngineContext &context, SectorPropDragSession &session, bool playEnd)
{
    context.audio.StopSound(context.assets, session.loop);
    if (playEnd && Valid(context.world, session.entity))
        Play(context, context.world.Get<SectorPropDrag>(session.entity).endSound,
             context.world.Get<SectorObjectTransform>(session.entity).position);
    const bool resetLook = session.lookInitialized || session.resetMouseLook;
    session = {};
    session.resetMouseLook = resetLook;
}
void UpdateSectorPropDragMouseLook(engine::World &world, SectorPropDragSession &session,
                                   SectorFpsControllerState &player,
                                   const SectorFpsControllerConfig &config,
                                   const PlayerCameraApplicationSettings &settings,
                                   const SectorFpsControllerInput &input, float dt)
{
    if (session.resetMouseLook)
    {
        ResetSectorFpsMouseLook(player);
        session.resetMouseLook = false;
    }
    const Vector2 previous{player.yawRadians, player.pitchRadians};
    UpdateSectorFpsMouseLook(player, settings, input, dt);
    if (!Valid(world, session.entity))
        return;

    const auto &box = world.Get<SectorStaticModelCollider>(session.entity);
    const float eyeY =
        player.feetPosition.y + EffectiveSectorFpsControllerConfig(player, config).eyeHeight;
    const Vector3 toward{box.center.x - player.feetPosition.x, (box.bottom + box.top) * 0.5f - eyeY,
                         box.center.y - player.feetPosition.z};
    const float horizontal = std::hypot(toward.x, toward.z);
    const Vector2 center{horizontal > 0.0001f ? std::atan2(toward.z, toward.x) : previous.x,
                         ClampSectorFpsPitch(std::atan2(toward.y, horizontal))};
    if (!session.lookInitialized)
    {
        session.initialLookOffset = {
            std::clamp(AngleDifference(previous.x, center.x), -DragYawLimit, DragYawLimit),
            std::clamp(previous.y - center.y, -DragPitchLimit, DragPitchLimit)};
        session.lookInitialized = true;
    }
    session.lookOffset.x += player.yawRadians - previous.x;
    session.lookOffset.y += player.pitchRadians - previous.y;
    if (std::isfinite(dt) && dt > 0)
        session.lookCenterElapsed = std::min(DragCenterSeconds, session.lookCenterElapsed + dt);
    const float t = session.lookCenterElapsed / DragCenterSeconds;
    const float remainingBlend = 1 - t * t * (3 - 2 * t);
    const Vector2 initial = Vector2Scale(session.initialLookOffset, remainingBlend);
    const Vector2 requested = Vector2Add(initial, session.lookOffset);
    const float yawOffset = std::clamp(requested.x, -DragYawLimit, DragYawLimit);
    const float pitch =
        ClampSectorFpsPitch(center.y + std::clamp(requested.y, -DragPitchLimit, DragPitchLimit));
    const float pitchOffset = pitch - center.y;
    // Discard excess input, including filtered velocity, instead of accumulating
    // it behind the limits. Moving the mouse back must respond immediately.
    if (yawOffset != requested.x)
    {
        session.lookOffset.x = yawOffset - initial.x;
        player.mouseLook.angularVelocity.x = 0;
        player.mouseLook.deadZoneRemainder.x = 0;
    }
    if (pitchOffset != requested.y)
    {
        session.lookOffset.y = pitchOffset - initial.y;
        player.mouseLook.angularVelocity.y = 0;
        player.mouseLook.deadZoneRemainder.y = 0;
    }
    player.yawRadians = previous.x + AngleDifference(center.x + yawOffset, previous.x);
    player.pitchRadians = pitch;
    player.mouseLook.lastRotation = {player.yawRadians, player.pitchRadians};
}

void RefreshSectorDragColliders(engine::EngineContext &context, SectorRuntimeObjectState &objects)
{
    UpdateSectorStaticModelColliderSystem(context.world, context.assets);
    CollectSectorStaticModelColliders(context.world, objects.staticModelColliders);
    CollectSectorDynamicModelColliders(context.world, objects.dynamicModelColliders);
    objects.physicalModelColliders.clear();
    for (const auto &collider : objects.staticModelColliders)
        objects.physicalModelColliders.push_back(collider);
    for (const auto &collider : objects.windowColliders)
        objects.physicalModelColliders.push_back(collider);
}
bool UpdateSectorPropDrag(engine::EngineContext &context, const SectorTopologyMap &map,
                          SectorRuntimeObjectState &objects, const SectorCollisionWorld &collision,
                          const std::vector<NpcCollisionCylinder> &npcs,
                          SectorPropDragSession &session, SectorFpsControllerState &player,
                          const SectorFpsControllerConfig &config, SectorFpsControllerInput &input,
                          float dt)
{
    if (engine::IsNull(session.entity))
        return false;
    if (!Valid(context.world, session.entity) || !player.grounded)
    {
        EndSectorPropDrag(context, session);
        return false;
    }
    auto &drag = context.world.Get<SectorPropDrag>(session.entity);
    const auto *path = FindSectorPath(map.paths, drag.settings.pathEditorId);
    if (!path || path->length <= 0)
    {
        EndSectorPropDrag(context, session);
        return false;
    }
    const int requestedDirection =
        (int(input.moveForward) - int(input.moveBackward)) * session.pushDirection;
    input.moveForward = input.moveBackward = input.strafeLeft = input.strafeRight = false;
    input.run = input.jumpPressed = input.crouchTogglePressed = input.swimUp = input.swimDown =
        false;
    input.externalHorizontalMovementDelta = {};
    auto &transform = context.world.Get<SectorObjectTransform>(session.entity);
    auto moving = context.world.Get<SectorStaticModelCollider>(session.entity);
    double timeRemaining = std::isfinite(dt) ? std::max(0.0f, dt) : 0;
    float totalMovement = 0;
    bool blocked = false;
    const auto *playerSector = collision.FindSector(player.currentSectorId);
    const float playerSupport = playerSector ? playerSector->heights.floorZ : player.feetPosition.y;
    while (timeRemaining > 1e-8 && !blocked)
    {
        const auto step = NextMotionStep(drag.distanceWorld, path->length, drag.settings.speedWorld,
                                         session.velocityWorld, requestedDirection, timeRemaining);
        timeRemaining = std::max(0.0, timeRemaining - step.seconds);
        session.velocityWorld = step.velocity;
        const int direction = step.displacement > 0 ? 1 : -1;
        float remaining = std::abs(step.displacement);
        if (remaining == 0 && session.velocityWorld == 0)
            break;
        while (remaining > 0)
        {
            const size_t segment = SectorPathSegment(*path, drag.distanceWorld, direction);
            const float boundary = path->distances[segment + (direction > 0 ? 1 : 0)];
            const float travel = std::min(remaining, std::abs(boundary - drag.distanceWorld));
            if (travel <= 0)
            {
                session.velocityWorld = 0;
                break;
            }
            const float next = drag.distanceWorld + direction * travel;
            const Vector2 target = EvaluateSectorPath(*path, next);
            const Vector2 delta{target.x - transform.position.x, target.y - transform.position.z};
            float fraction = collision.SweepFlatBox(moving.center, moving.axisX, moving.axisZ,
                                                    moving.halfExtents, moving.bottom, moving.top,
                                                    drag.supportY, delta);
            SectorStaticModelCollider playerBox;
            playerBox.center = {player.feetPosition.x, player.feetPosition.z};
            playerBox.halfExtents = {config.playerRadius, config.playerRadius};
            playerBox.bottom = player.feetPosition.y;
            playerBox.top = player.feetPosition.y + config.playerHeight;
            fraction =
                std::min(fraction, collision.SweepFlatCircle(playerBox.center, config.playerRadius,
                                                             playerBox.bottom, playerBox.top,
                                                             playerSupport, delta));
            const auto obstacle = [&](Vector2 c, Vector2 x, Vector2 z, Vector2 h, float b, float t)
            {
                fraction = std::min(fraction, SweepCollider(moving, delta, c, x, z, h, b, t));
                SectorStaticModelCollider other;
                other.center = c;
                other.axisX = x;
                other.axisZ = z;
                other.halfExtents = h;
                other.bottom = b;
                other.top = t;
                fraction = std::min(fraction, SweepSectorPlayerAgainstModel(
                                                  playerBox.center, delta, config.playerRadius,
                                                  playerBox.bottom, playerBox.top, other));
            };
            for (const auto &other : objects.physicalModelColliders)
            {
                if (other.placedObjectId == moving.placedObjectId)
                    continue;
                obstacle(other.center, other.axisX, other.axisZ, other.halfExtents, other.bottom,
                         other.top);
            }
            for (const auto &door : objects.dynamicDoorColliders)
                obstacle(door.center, door.tangent, door.normal, door.halfExtents, door.bottom,
                         door.top);
            for (const auto &npc : npcs)
                obstacle({npc.feetPosition.x, npc.feetPosition.z}, {1, 0}, {0, 1},
                         {npc.radius, npc.radius}, npc.feetPosition.y,
                         npc.feetPosition.y + npc.height);
            const Vector2 accepted = Vector2Scale(delta, fraction);
            drag.distanceWorld =
                std::clamp(drag.distanceWorld + direction * travel * fraction, 0.0f, path->length);
            transform.position.x += accepted.x;
            transform.position.z += accepted.y;
            player.feetPosition.x += accepted.x;
            player.feetPosition.z += accepted.y;
            moving.center = Vector2Add(moving.center, accepted);
            remaining = std::max(0.0f, remaining - travel);
            totalMovement += travel * fraction;
            if (fraction < 1.0f)
            {
                session.velocityWorld = 0;
                blocked = true;
                break;
            }
        }
    }
    const bool moved = totalMovement > 0;
    if (moved)
    {
        player.currentSectorId = collision.FindSectorContainingPointPreferCurrent(
            {player.feetPosition.x, player.feetPosition.z}, player.currentSectorId);
        if (context.world.Has<SectorObject>(session.entity))
        {
            auto &object = context.world.Get<SectorObject>(session.entity);
            object.currentSectorId = collision.FindSectorContainingPointPreferCurrent(
                {transform.position.x, transform.position.z}, object.currentSectorId);
        }
        moving.resolvedPosition = transform.position;
        context.world.Get<SectorStaticModelCollider>(session.entity) = moving;
        RefreshSectorMovedPropLighting(context.world, objects, map, session.entity);
        RefreshSectorDragColliders(context, objects);
        if (!context.audio.IsSoundPlaying(session.loop))
            session.loop = Play(context, drag.movingSound, transform.position, true);
        context.audio.SetSoundPosition(session.loop, transform.position);
    }
    else
    {
        context.audio.StopSound(context.assets, session.loop);
        session.loop = engine::NullSoundPlaybackHandle();
    }
    return true;
}
} // namespace game
