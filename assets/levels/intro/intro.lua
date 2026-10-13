function init()
    setPropAnimationProgress("lever_1", 1.0, "Po_Bo|Level_Down")
    setPropAnimationProgress("lever_2", 1.0, "Po_Bo|Level_Down")
    setPropAnimationProgress("lever_3", 1.0, "Po_Bo|Level_Down")
    setPropAnimationProgress("lever_4", 1.0, "Po_Bo|Level_Down")

    if not flag("intro_cutscene_1") then
        setFlag("intro_cutscene_1", true)
        startScript("introCutscene1")
    end
end

function shutdown()

end

function introCutscene1()
    teleportPlayer("default")
    playMapMusic("minimal_piano", false, 0.45)
    assert(startCutscene())
    delay(1000)
    text("Christmas Eve. Four electrical faults on a line that's supposed to be dead", BOTTOM)
    local move = startMovePlayer("intro_marker_1", "walk", 1.5)
    delay(1000)
    text("At least holiday callouts pay double.", BOTTOM)
    await(move)
    startLookAtProp("electrical1", 1500)
    text("That's the first panel, I should examine it further.", BOTTOM)
    assert(endCutscene())
    startText("Use [WASD] to move. Hold [SHIFT] to sprint.", CENTER, 2000)
end

function introTrigger1()
    text("Claire has the kids this Christmas. I told her I wanted the extra shift.", BOTTOM, 2500)
    delay(1000)
    startText("Easier than admitting I didn't want to sit in an empty house all night.", BOTTOM, 3000)
end

function flashlightHintTrigger()
    startText("Press [F] to toggle your fleshlight.", CENTER, 2000)
end


function canOpenDoor1()
    startText("I cannot leave before the job is done.", CENTER)
    return false
end

function canOpenExitDoor()
    local canOpen = flag("intro_examined_electrical1") and flag("intro_examined_electrical2") and flag("intro_examined_electrical3") and flag("intro_examined_electrical4")
    if canOpen then
        startScript("exitLevel")
        return false
    end
    startText("I cannot leave before I have checked all the electrical panels.", CENTER)
    return false
end

function exitLevel()
    assert(enableControls(false))
    fadeOut(1000)
    text("After walking through the dark tunnels for a while, I finally spot the last panel.", CENTER)
    delay(4000)
    -- assert(enableControls(true))
    changeMap("hub")
end


local lever1 = true
local lever2 = true
local lever3 = true
local lever4 = true

function useLever1()
    if not lever1 then
        playPropAnimation("lever_1",
                        "Po_Bo|Level_Down",
                        "once")
        lever1 = true
        playMapSound("lever_down", 0.8)
        setParticleEmitterEnabled("sparks_1", true)
    else
        playPropAnimation("lever_1",
                                "Po_Bo|Level_Down",
                                "once_reverse")
        playMapSound("lever_up", 0.8)
        setParticleEmitterEnabled("sparks_1", false)
        lever1 = false
    end
end

function useLever2()
    if not lever2 then
        playPropAnimation("lever_2",
                        "Po_Bo|Level_Down",
                        "once")
        lever2 = true
        playMapSound("lever_down", 0.8)
        setParticleEmitterEnabled("sparks_2", true)
    else
        playPropAnimation("lever_2",
                                "Po_Bo|Level_Down",
                                "once_reverse")
        playMapSound("lever_up", 0.8)
        setParticleEmitterEnabled("sparks_2", false)
        lever2 = false
    end
end

function useLever3()
    if not lever3 then
        playPropAnimation("lever_3",
                        "Po_Bo|Level_Down",
                        "once")
        lever3 = true
        playMapSound("lever_down", 0.8)
        setParticleEmitterEnabled("sparks_3", true)
    else
        playPropAnimation("lever_3",
                                "Po_Bo|Level_Down",
                                "once_reverse")
        playMapSound("lever_up", 0.8)
        setParticleEmitterEnabled("sparks_3", false)
        lever3 = false
    end
end

function useLever4()
    if not lever4 then
        playPropAnimation("lever_4",
                        "Po_Bo|Level_Down",
                        "once")
        lever4 = true
        playMapSound("lever_down", 0.8)
        setParticleEmitterEnabled("sparks_4", true)
    else
        playPropAnimation("lever_4",
                                "Po_Bo|Level_Down",
                                "once_reverse")
        playMapSound("lever_up", 0.8)
        setParticleEmitterEnabled("sparks_4", false)
        lever4 = false
    end
end

function useElectrical1()
    if flag("intro_examined_electrical1") then
        startText("I already checked this one. I should check the rest.", BOTTOM)
        return
    end
    if lever1 then
        startText("I should turn the power off first.", BOTTOM, 1000)
        else
        assert(enableControls(false))
        fadeOut(1000)
        playMapSound("electrical_current", 0.45)
        startText("Let's see what we have here...", CENTER)
        delay(4000)
        fadeIn(1000)
        text("Just a loose connection. Nothing worth dragging me underground for.", BOTTOM)
        startText("I should move on to the next.", BOTTOM)
        setFlag("intro_examined_electrical1", true)
        assert(enableControls(true))
    end
end

function useElectrical2()
    if flag("intro_examined_electrical2") then
        startText("I already checked this one.", BOTTOM)
        return
    end
    if lever2 then
        startText("I should turn the power off first.", BOTTOM, 1000)
        else
        assert(enableControls(false))
        fadeOut(1000)
        playMapSound("electrical_current", 0.45)
        delay(4000)
        fadeIn(1000)
        startText("Nothing out of the ordinary, let's move on.", BOTTOM)
        setFlag("intro_examined_electrical2", true)
        assert(enableControls(true))
    end
end

function useElectrical3()
    if flag("intro_examined_electrical3") then
        startText("I already checked this one.", BOTTOM)
        return
    end
    if lever3 then
        startText("I should turn the power off first.", BOTTOM, 1000)
        else
        assert(enableControls(false))
        fadeOut(1000)
        playMapSound("electrical_current", 0.45)
        delay(4000)
        fadeIn(1000)
        startText("This ones seems to be drawing an unusual amount of power. I should check the next.", BOTTOM)
        setFlag("intro_examined_electrical3", true)
        assert(enableControls(true))
    end
end

function useElectrical4()
    if flag("intro_examined_electrical4") then
        startText("I already checked this one.", BOTTOM)
        return
    end
    if lever4 then
        startText("I should turn the power off first.", BOTTOM, 1000)
        else
        assert(enableControls(false))
        fadeOut(1000)
        playMapSound("electrical_current", 0.45)
        delay(4000)
        fadeIn(1000)
        text("New conduit. No city markings, no inspection tag, and it isn't on the plans. Who the hell installed this?", BOTTOM)
        startText("The last panel should be through that door on the right. Last check, then I'm going home.", BOTTOM)
        lookAtProp("exit_sign", 2000, 0.5)
        setFlag("intro_examined_electrical4", true)
        assert(enableControls(true))
    end
end