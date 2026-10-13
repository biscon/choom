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
    text("Christmas Eve. Electrical faults on a line that's supposed to be dead.", BOTTOM)
    local move = startMovePlayer("intro_marker_1", "walk", 1.5)
    delay(1000)
    text("At least holiday callouts pay double.", BOTTOM)
    await(move)
    startLookAtProp("electrical1", 1500)
    text("First panel. Let's have a look.", BOTTOM)
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
    startText("Can't leave the job half-finished.", BOTTOM)
    return false
end

function canOpenExitDoor()
    local canOpen = flag("intro_examined_electrical1") and flag("intro_examined_electrical2") and flag("intro_examined_electrical3") and flag("intro_examined_electrical4")
    if canOpen then
        startScript("exitLevel")
        return false
    end
    startText("Still got panels to check back here.", BOTTOM)
    return false
end

function exitLevel()
    assert(enableControls(false))
    fadeOut(1000)
    text("A little farther down the service tunnel...", CENTER)
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
        startText("I already checked this one.", BOTTOM)
        return
    end
    if lever1 then
        startText("I should turn the power off first.", BOTTOM, 1000)
        else
        assert(enableControls(false))
        fadeOut(1000)
        playMapSound("electrical_current", 0.45)
        startText("Let's see what we have here...", BOTTOM)
        delay(4000)
        fadeIn(1000)
        text("Just a loose connection. Nothing worth dragging me underground for.", BOTTOM)
        startText("That's that sorted.", BOTTOM)
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
        startText("Nothing out of the ordinary here.", BOTTOM)
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
        startText("Something on this circuit has been drawing a lot of power.", BOTTOM)
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
        startText("According to the plans, there's another panel through that door on the right.", BOTTOM)
        lookAtProp("exit_sign", 2000, 0.5)
        setFlag("intro_examined_electrical4", true)
        assert(enableControls(true))
    end
end

-- Dialogue -----------------------------------------------------------------------------------------------------------

function useDraymond(instanceId)
    local ok, reason = startConversation(instanceId)
    if not ok then log(reason); return end

    runConversationDynamic("draymond_intro_topics", {
        greeting = function()
            say("How you holding up, old man?")
            startPlayNpcAnimation("draymond", "Talking")
            say("draymond", "Old man? Still young enough to whip your ass, boy.")
            say("Just wiggle that giant ass of yours. The gravitational wave alone would kill me.")
            startPlayNpcAnimation("draymond", "Taunt", 5000)
            say("draymond", "That's it, boy. I'm gonna go medieval on your ass!")
            say("Sorry, Dray. Just not feeling it tonight.")
            startPlayNpcAnimation("draymond", "Talking")
            say("draymond", "That pretty lil' wife of yours trippin' again?")
            say("Ex-wife. And yeah, but it's not just that. Lately it all feels a bit pointless.")
            say("Get up. Go to work. Go home. Eat. Sleep. Seeing the kids is about the only thing I look forward to.")
            say("And Claire's been giving me shit about that lately.")
            startPlayNpcAnimation("draymond", "Yelling")
            say("draymond", "Shit. Sorry to hear that, man. Still, you got a home to go back to. I'm living in a goddamn subway equipment room.")
            say("Fair point. You've got a better heater, too.")
            setFlag("draymond_asked_greeting", true)
        end,
        snitching = function()
            say("You seen anyone fucking around with the electrical panels?")
            startPlayNpcAnimation("draymond", "Talking")
            say("draymond", "Nobody I've seen. Why you asking, son?")
            say("They sent me down here on Christmas fucking Eve because something's drawing too much power.")
            say("Haven't found what's causing it yet.")
            startPlayNpcAnimation("draymond", "Telling A Secret")
            say("draymond", "Well, I ain't seen anybody messing with your panels. But I've heard there's something going on at that abandoned station up ahead.")
            startPlayNpcAnimation("draymond", "Yelling")
            say("draymond", "The lights are on down there. Sometimes I hear a train. Figured the transit company was testing something.")
            say("Nobody told us. Would explain the power draw, though.")
            setFlag("draymond_asked_snitching", true)
        end,
        xmas = function()
            say("What are you doing for Christmas?")
            startPlayNpcAnimation("draymond", "Talking")
            say("draymond", "Gonna stuff some cotton in my ears and head down to that Salvation Army soup kitchen.")
            say("draymond", "Get me some nice, juicy turkey.")
            say("Why the cotton?")
            startPlayNpcAnimation("draymond", "Yelling")
            say("draymond", "So I don't have to listen to all that goddamn singing.")
            say("Guess there really is no such thing as a free meal.")
            startPlayNpcAnimation("draymond", "Talking")
            say("draymond", "Ain't that the truth.")
            setFlag("draymond_asked_xmas", true)
        end,
        goodbye = function()
            say("Catch you later, Dray.")
            startPlayNpcAnimation("draymond", "Waving")
            say("draymond", "Peace, son.")
            return "exit"
        end,
    }, function()
        return hiddenOptions({
            greeting = flag("draymond_asked_greeting"),
            snitching = flag("draymond_asked_snitching"),
            xmas = flag("draymond_asked_xmas"),
            --goodbye = not flag("elin_asked_identity")
        })
    end)
    assert(endConversation())
end
