function init()
    setPropAnimationProgress("lever_1", 1.0, "Po_Bo|Level_Down")
    setPropAnimationProgress("lever_2", 1.0, "Po_Bo|Level_Down")
    startScript("introCutscene1")
    if not flag("intro_cutscene_1") then
        setFlag("intro_cutscene_1", true)

    end
end

function shutdown()

end

function introCutscene1()
    teleportPlayer("default")
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
end

function introTrigger1()
    text("Claire has the kids this Christmas. I told her I wanted the extra shift.", BOTTOM, 2500)
    delay(1000)
    startText("Easier than admitting I didn't want to sit in an empty house all night.", BOTTOM, 3000)
end

function canOpenDoor1()
    startText("I cannot leave before the job is done.", CENTER)
    return false
end

local lever1 = true
local lever2 = true

function useLever1()
    if not lever1 then
        playPropAnimation("lever_1",
                        "Po_Bo|Level_Down",
                        "once")
        lever1 = true
        playMapSound("lever_down", 0.8)
    else
        playPropAnimation("lever_1",
                                "Po_Bo|Level_Down",
                                "once_reverse")
        playMapSound("lever_up", 0.8)
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
    else
        playPropAnimation("lever_2",
                                "Po_Bo|Level_Down",
                                "once_reverse")
        playMapSound("lever_up", 0.8)
        lever2 = false
    end
end

function useElectrical1()
    if flag("intro_examined_electrical1") then
        startText("I already checked this one. I should move on to the next.", BOTTOM)
        return
    end
    if lever1 then
        startText("I should turn the power off first.", BOTTOM, 1000)
        else
        assert(enableControls(false))
        fadeOut(2000)
        text("Let's see what we have here...", CENTER)
        fadeIn(1000)
        text("Just a loose connection. Nothing worth dragging me underground for.", BOTTOM)
        startText("I should move on to the next.", BOTTOM)
        setFlag("intro_examined_electrical1", true)
        assert(enableControls(true))
    end
end

function useElectrical2()
    if flag("intro_examined_electrical2") then
        startText("I already checked this one. I should move on to the next.", BOTTOM)
        return
    end
    if lever2 then
        startText("I should turn the power off first.", BOTTOM, 1000)
        else
        assert(enableControls(false))
        fadeOut(2000)
        text("What do we got here...", CENTER)
        fadeIn(1000)
        startText("This ones seems to be drawing an unusual amount of power. I should check the next.", BOTTOM)
        setFlag("intro_examined_electrical2", true)
        assert(enableControls(true))
    end
end