local function elinPlaceQuestions()
    runConversationDynamic("elin_place_questions", {
        elin_arrival = function()
          say("How did you get here?")
          startPlayNpcAnimation("elin", "Talking_2")
          say("elin", "I was at a subway station when the chaos started. A security person ushered me into a side tunnel.")
          startPlayNpcAnimation("elin", "Rejected")
          say("elin", "He then went back to look for more people and left me to wander alone in these dark tunnels..", "afraid")
          setFlag("elin_asked_arrival", true)
        end,
        back = function()
          say("Let me ask you something else.")
          say("elin", "Yes?")
          return "exit"
        end,
    }, function()
        return hiddenOptions({
          elin_arrival = flag("elin_asked_arrival"),
        })
    end)
end


local function elinConversation()
    local returning = flag("elin_intro_conversation_finished")
    runConversationDynamic("elin_intro_topics", {
        identity = function()
            say("My name is James. Not Jim, definitely not Jimbo, just James.")
            startPlayNpcAnimation("elin", "Salute")
            say("elin", "James it is, pleasure to meet you.", "happy")
            setFlag("elin_asked_identity", true)
        end,
        place = function()
            say("Do you know what this place is?")
            startPlayNpcAnimation("elin", "Talking")
            say("elin", "No. I found this office looking room and decided to stop, catch a breath and reevaluate my options.")
            startPlayNpcAnimation("elin", "Talking_2")
            say("elin", "Sure beats walking the dark tunnels, seems like somebody have been living here there is even a bed and all.")

            elinPlaceQuestions()
            setFlag("elin_asked_place", flag("elin_asked_arrival"))
        end,
        people = function()
            say("Have you seen anyone else down here?", { mood = "afraid" })
            startPlayNpcAnimation("elin", "No")
            say("elin", "No one I could talk to. I was starting to think I was alone.")
            startPlayNpcAnimation("elin", "Talking")
            say("elin", "But there are two of us now. That's a start.", "relieved")
            startPlayNpcAnimation("elin", "Excited")
            setFlag("elin_asked_people", true)
        end,
        locked_door = function()
            say("Have you seen a key or anything lying around that could be useful for unlocking the door in the hallway?")
            local anim = startPlayNpcAnimation("elin", "No")
            say("elin", "No I haven't found anything, have you looked around the office?.")
            assert(await(anim))
            setFlag("elin_asked_pre_entrance_door", true)
        end,
        goodbye = function()
            if returning then
                say("Talk later.")
                say("elin", "Later.")
            else
                say("I'll look around. Stay close.")
                startPlayNpcAnimation("elin", "Talking")
                say("elin", "Be careful. And don't disappear on me.")
            end
            setFlag("elin_intro_conversation_finished", true)
            return "exit"
        end,
    }, function()
        return hiddenOptions({
            identity = flag("elin_asked_identity"),
            place = flag("elin_asked_place") or not flag("elin_asked_identity"),
            people = flag("elin_asked_people")  or not flag("elin_asked_identity"),
            locked_door = flag("elin_asked_pre_entrance_door") or not flag("tried_pre_entrance_door") or not flag("elin_asked_identity")
        })
    end, function()
        if returning then
            return { goodbye = "Talk later." }
        end
    end)
end

function useElin(instanceId)
    local ok, reason = startConversation(instanceId)
    if not ok then
        log("Could not start Elin conversation: " .. (reason or "unknown reason"))
        return
    end
    elinConversation()
    assert(endConversation())
end

function init()
    log("hub script initialized")
    setPropAnimationProgress("ceiling_switch_01", 0.0, "switch|switchAction")
    setPropAnimationProgress("ceiling_vent_01", 0.0, "Ventilator")
    playPropAnimation("ceiling_vent_01", "Ventilator", "loop")
end

function shutdown()
    log("hub script shut down")
end

function intro_trigger_1()
    log("trigger_1")
    assert(startCutscene())
    openDoor("office_door")
    say("elin", "Who is there?!!...", "afraid")
    assert(startPlayNpcAnimation("elin", "Waving"))
    movePlayer("intro_marker_1", "walk", 2.0, {
        lookAtNpc = "elin",
        turnDurationMs = 750,
        targetHeight = 0.7,
    })
    delay(2000)
    startPlayNpcAnimation("elin", "Excited_2")
    say("elin", "Yes finally another person!!. Come closer.", "relieved")
    movePlayer("intro_marker_2", "walk", 2.0, {
        lookAtNpc = "elin",
        turnDurationMs = 1000,
        targetHeight = 0.45,
    })
    lookAtNpc("elin", 1500, 0.7)
    startPlayNpcAnimation("elin", "Talking_2")
    say("elin", "I've been walking these dark tunnels forever. You're are the first person I met so far. Follow me.")
    local elinArrival = assert(startMoveNpc("elin", "intro_marker_3", "walk", 1.5, true))
    delay(1500)
    movePlayer("intro_marker_4", "walk", 1.25, {
        lookAtNpc = "elin",
        turnDurationMs = 750,
        targetHeight = 0.7,
    })
    assert(await(elinArrival))
    npcLookAtPlayer("elin", 500)
    lookAtNpc("elin", 500, 0.7)

    startPlayNpcAnimation("elin", "Talking_2")
    say("elin", "My name is Elin by the way, what is yours?. ")

    assert(startConversation("elin", { reposition = false }))
    elinConversation()
    assert(endConversation())
    assert(endCutscene())
    startPlayNpcAnimation("elin", "Thankful")
end

local ceilingVentOn = true
local ceilingVentStarted = true
local officeLightsOn = true

function setOfficeLight(enabled)
    setDynamicLightEnabled("office_spot_1", enabled)
    setDynamicLightEnabled("office_spot_2", enabled)
    setDynamicLightEnabled("light_point_8", enabled)
    setDynamicLightEnabled("light_point_9", enabled)
    setDynamicLightEnabled("light_point_12", enabled)
    setDynamicLightEnabled("light_point_11", enabled)
    setPropEmissiveScale("office_lamp_1", enabled and 1.0 or 0.0)
    setPropEmissiveScale("office_lamp_2", enabled and 1.0 or 0.0)
    setPropEmissiveScale("office_lamp_3", enabled and 1.0 or 0.0)
    setPropEmissiveScale("office_lamp_4", enabled and 1.0 or 0.0)
end

function toggleOfficeLights()
    playMapSound("light_switch_on_01", 0.8)
    if officeLightsOn then
        playPropAnimation(
                "light_switch_01",
                "switch|switchAction",
                "once_reverse")
        officeLightsOn = false
        setOfficeLight(officeLightsOn)
        return
    end

    officeLightsOn = true
    playPropAnimation(
            "light_switch_01",
            "switch|switchAction",
            "once")
    setOfficeLight(officeLightsOn)
end

local radioOn = true

function toggleRadio()
    log("radio toggled")
    playMapSound("light_switch_on_01", 0.8)
    if radioOn then
        radioOn = false
        stopSoundEmitter("radio_emitter")
    else
        radioOn = true
        playSoundEmitter("radio_emitter")
    end
end

function toggleCeilingVent()
    playMapSound("light_switch_on_01", 0.8)
    if ceilingVentOn then
        playPropAnimation(
                "ceiling_switch_01",
                "switch|switchAction",
                "once_reverse")
        pausePropAnimation("ceiling_vent_01")
        ceilingVentOn = false
        return
    end

    playPropAnimation(
            "ceiling_switch_01",
            "switch|switchAction",
            "once")
    if ceilingVentStarted then
        resumePropAnimation("ceiling_vent_01")
    else
        playPropAnimation("ceiling_vent_01", "Ventilator", "loop")
        ceilingVentStarted = true
    end
    ceilingVentOn = true
end

function open_pre_entrance_door()
    if not flag("pre_entrance_door_unlocked") then
        playMapSound("door_locked", 0.8)
        text("It's locked", CENTER)
        setFlag("tried_pre_entrance_door", true)
        return false
    end
    return true
end

function usePreEntranceDoorKey(targetInstanceId)
    -- log("used key on: " .. targetInstanceId)
    if targetInstanceId == "pre_entrance_door" then
        setFlag("pre_entrance_door_unlocked", true)
        say("Voila!. the door is now unlocked.")
        return true
    end
    say("That won't work.")
    return false
end
