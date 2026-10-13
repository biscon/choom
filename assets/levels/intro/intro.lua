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
            say("How are you holding up old man?")
            say("draymond","Old man.. Not too old to whip yo ass boy!.")
            say("You could just crush me with a gravitational wave, from a slight wiggle of that giant ass.")
            say("draymond", "That does it boy, im gonna go medieval on your ass!")
            say("Sorry Dray but im not really feeling it tonight...")
            say("draymond", "That wife of yours trippin' again?")
            say("Ex wife. Yeah but that's not really it. I guess I am just at a point where it all feels a little pointless.")
            say("I get up, go to work. Go home. Eat. Goto sleep. Only highlight is when I get to see my kids.")
            say("Claire has been fighting me on that recently.")
            say("draymond", "Sorry to hear man, but at least you have a home, I live in a goddamn subway equipment room.")
            say("Touche, but least you made it your own.")
            setFlag("draymond_asked_greeting", true)
        end,
        snitching = function()
            say("Have you seen anyone fuck around with the electrical panels?.")
            say("draymond","Not to my knowledge no. Why do you ask son?")
            say("Because I've been sent down here at this god forsaken time to check on unusually large power draw.")
            say("But so far I haven't seen jack shit.")
            say("draymond","Well I haven't seen shit son. I heard rumors about some activity at the old abandoned subway station up ahead.")
            say("draymond", "Lights are still on an and sometimes I hear a train running. I figured it was just the subway company running some tests or something.")
            say("If so they haven't notified us about it, that might explain the power draw though.")
            setFlag("draymond_asked_snitching", true)
        end,
        xmas = function()
            say("What are you up to this Christmas?.")
            say("draymond", "Imma' put cotton wool in my ears and go down to that salvation army soup kitchen.")
            say("draymond", "Get me some of that sweet juicy turkey.")
            say("Why the cotton wool?")
            say("draymond", "So I can't hear them singing all the good damn time.")
            say("I guess there really is no such thing as a free meal.")
            say("draymond", "Ain't that the truth.")
            setFlag("draymond_asked_xmas", true)
        end,
        goodbye = function()
            say("Goodbye.")
            say("draymond","Peace son.")
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
