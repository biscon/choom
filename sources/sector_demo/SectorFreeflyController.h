#pragma once

#include "engine/input/Input.h"
#include "sector_demo/SectorViewPose.h"

namespace game {

struct SectorFreeflyControllerState {
    SectorViewPose pose = {};
    bool mouseLookEnabled = true;
    int mouseLookWarmupFrames = 0;
};

void ResetSectorFreeflyController(
        SectorFreeflyControllerState& state,
        const SectorViewPose& pose);
void EnterSectorFreeflyController(SectorFreeflyControllerState& state);
void LeaveSectorFreeflyController();
void SetSectorFreeflyMouseLookEnabled(
        SectorFreeflyControllerState& state,
        bool enabled);
void UpdateSectorFreeflyController(
        SectorFreeflyControllerState& state,
        engine::Input& input,
        float dt);

} // namespace game
