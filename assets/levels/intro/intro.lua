function init()
    setPropAnimationProgress("lever_1", 1.0, "Po_Bo|Level_Down")
end

function shutdown()

end

function canOpenDoor1()
    text("I cannot leave before the job is done.", CENTER)
    return false
end

local lever1 = true

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

function useElectrical1()
    if flag("intro_examined_electrical1") then
        text("I already checked this one. I should move on to the next.", BOTTOM)
        return
    end
    if lever1 then
        text("I should turn the power off first.", BOTTOM)
        else
        assert(enableControls(false))
        fadeOut(2000)
        text("Let's see what we have here...", CENTER)
        fadeIn(1000)
        text("Nothing out of the ordinary. I should move on to the next.", BOTTOM)
        setFlag("intro_examined_electrical1", true)

        assert(enableControls(true))
    end
end