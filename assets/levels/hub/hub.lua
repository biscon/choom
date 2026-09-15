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
            if not flag("elin_shown_place_questions") then
                say("Do you know what this place is?")
                startPlayNpcAnimation("elin", "Talking")
                say("elin", "No. I found this office looking room and decided to stop, catch a breath and reevaluate my options.")
                startPlayNpcAnimation("elin", "Talking_2")
                say("elin", "Sure beats walking the dark tunnels, seems like somebody have been living here there is even a bed and all.")
            end
            elinPlaceQuestions()
            setFlag("elin_shown_place_questions", true)
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
            setFlag("elin_asked_storage_door", true)
            return "exit"
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
            locked_door = flag("elin_asked_storage_door") or not flag("tried_storage_door") or not flag("elin_asked_identity") or hasInventoryItemInstance("storage_door_key"),
            goodbye = not flag("elin_asked_identity")
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
    setNpcAnimation("elin", "Idle")
    elinConversation()
    assert(endConversation())
end

local function entranceIndicatorColor()
    local card = flag("entrance_card_accepted")
    local pin = flag("entrance_pin_accepted")
    if card and pin then return {0.015, 1.0, 0.05} end
    if card or pin then return {0.015, 0.12, 1.0} end
    return nil -- skin/model authored colour
end

local function refreshEntranceAccess()
    setFlag("entrance_door_unlocked",
        flag("entrance_card_accepted") and flag("entrance_pin_accepted"))
    local color = entranceIndicatorColor()
    local ok, reason
    if color then
        ok, reason = setPropEmissiveColor("entrance_keypad", "Light", color[1], color[2], color[3])
    else
        ok, reason = setPropEmissiveColor("entrance_keypad", "Light", nil)
    end
    -- Keep accepted colours saturated below the renderer's bright-core whitening.
    setPropEmissiveScale("entrance_keypad", color and 0.35 or 1.0)
    if not ok then log("Keypad indicator: " .. (reason or "unavailable")) end
end

function useEntranceKeypad()
    local code, reason = promptPinCode({
        skin = "simple_keypad",
        digits = 4,
        indicatorColor = entranceIndicatorColor(),
        validate = function(candidate) return candidate == "1984" end,
    })
    if code then
        setFlag("entrance_pin_accepted", true)
        refreshEntranceAccess()
    elseif reason ~= "cancelled" then
        log("Keypad: " .. (reason or "unavailable"))
    end
end

function setTunnelCollapsed(collapsed)
    setFogVolumeEnabled("tunnel_fog", collapsed)
    setPropEnabled("tunnel_debris_1", collapsed)
    setPropEnabled("tunnel_debris_2", collapsed)
    setPropEnabled("tunnel_debris_3", collapsed)
    setPropEnabled("tunnel_debris_4", collapsed)
    setPropEnabled("tunnel_debris_5", collapsed)
    setPropEnabled("tunnel_debris_6", collapsed)
    setPropEnabled("tunnel_debris_7", collapsed)
    setPropEnabled("tunnel_debris_8", collapsed)
end

function tunnelCollapseCutscene()
    setTunnelCollapsed(false)
    teleportPlayer("default")
    assert(startCutscene())
    delay(1000)
    movePlayer("collapse_marker_1", "walk", 1.25)
    playMapSound("tunnel_collapse", 1.0)
    local shake = startScreenShake(0.9, 5000, SHAKE_RUMBLE)
    delay(1000)
    text("Fuck!", BOTTOM, 750)
    --say("Fuck!")
    startMovePlayer("collapse_marker_2", "run", 4.5)
    delay(750)
    fadeOut(2000)
    delay(5500)
    setPlayerHealth(25)
    setTunnelCollapsed(true)
    teleportPlayer("collapse_marker_2")
    fadeIn(4000)
    delay(500)
    startText("My head...", BOTTOM, 1000)
    movePlayer("collapse_marker_3", "walk", 1.0, {
        lookAtProp = "tunnel_debris_1",
        turnDurationMs = 2500,
        targetHeight = 0.2,
    })
    startLookAtProp("tunnel_debris_1", 2000, 0.65)
    delay(500)
    text("Guess I won't be leaving that way..", BOTTOM, 1750)
    delay(250)
    lookAtProp("prop_498", 1500)
    assert(endCutscene())
end

function init()
    stopSoundEmitter("storage_radio_emitter")
    refreshEntranceAccess()
    setPropAnimationProgress("ceiling_switch_01", 0.0, "switch|switchAction")
    setPropAnimationProgress("ceiling_vent_01", 0.0, "Ventilator")
    playPropAnimation("ceiling_vent_01", "Ventilator", "loop")
    if not flag("tunnel_collapsed") then
        setFlag("tunnel_collapsed", true)
        startScript("tunnelCollapseCutscene")
    end
end

function shutdown()
    log("hub script shut down")
end

function intro_trigger_1()
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
    say("elin", "Oh no you're hurt, follow me!", "afraid")
    local elinArrival = assert(startMoveNpc("elin", "recovery_marker_1", "walk", 1.5, true))
    delay(1500)
    movePlayer("recovery_marker_2", "walk", 1.0, {
        lookAtNpc = "elin",
        turnDurationMs = 1750,
        targetHeight = 0.7,
    })
    assert(await(elinArrival))
    startNpcLookAtPlayer("elin", 500)
    startPlayNpcAnimation("elin", "Reaching Out", 4000)
    startSay("elin", "Lay down on the bed and let me help you.")
    --lookAtNpc("elin", 500, 0.7)
    fadeOut(3950)

    --startPlayNpcAnimation("elin", "Talking_2")
    --say("elin", "My name is Elin by the way, what is yours?. ")

    --assert(startConversation("elin", { reposition = false }))
    --elinConversation()
    --assert(endConversation())
    delay(2000)
    setPlayerHealth(35)
    teleportNpc("elin", "recovery_marker_4")
    setActiveCamera("bed_camera_1")
    fadeIn(2000)

    elinArrival = assert(startMoveNpc("elin", "recovery_marker_3", "walk", 1.5, true))

    delay(5000)
    assert(endCutscene())
    --startPlayNpcAnimation("elin", "Thankful")
end

function entrance_trigger()
    assert(startCutscene())
    delay(1000)
    say("I wonder what it will take to get through this one.")
    movePlayer("intro_marker_5", "walk", 1.5, {
        lookAtProp = "entrance_keypad",
    })
    say("Looks like it will require a key card and a code as well.")
    assert(teleportNpc("elin", "intro_marker_6"))

    local elinArrival = assert(startMoveNpc("elin", "intro_marker_7", "walk", nil, true))
    delay(1000)
    startSay("elin", "What have you found?")
    startLookAtNpc("elin", 2000, 0.7)
    delay(2000)
    startLookAtNpc("elin", 4000, 0.7)
    assert(await(elinArrival))
    lookAtNpc("elin", 350, 0.7)
    startPlayNpcAnimation("elin", "Excited_2")
    say("elin", "Wow! that is a big door! How do we open it?")
    say("It requires a key card and a pincode.")
    if hasInventoryItemInstance("blue_key_card") then
        say("I found a key card in the storage room. But I don't know the pincode.")
    else
        say("I have neither.")
    end
    startPlayNpcAnimation("elin", "Talking_2")
    say("elin", "You should be careful. I don't like the noises coming from in there.")
    startPlayNpcAnimation("elin", "No")
    say("elin", "I'll hang out in the office.")
    setFlag("elin_in_storage_room", false)
    startMoveNpc("elin", "intro_marker_3", "walk", nil, true)
    startLookAtNpc("elin", 2000, 0.55)
    delay(2000)
    assert(endCutscene())
end

function storage_trigger()
    assert(startCutscene())
    local movement = assert(startMovePlayer("intro_marker_8", "walk", 0.5, {
        lookAtProp = "tool_board",
    }))
    local elinArrival = assert(startMoveNpc("elin", "intro_marker_9", "run", nil, true))
    assert(await(elinArrival))
    local lookWait = startLookAtNpc("elin", 1000, 0.7)
    local elinSay = startSay("elin", "This looks cozy.")
    setFlag("elin_in_storage_room", true)
    assert(await(lookWait))
    assert(await(elinSay))
    elinArrival = assert(startMoveNpc("elin", "intro_marker_10", "walk", nil, true))
    lookAtNpc("elin", 3000, 0.7)
    assert(await(elinArrival))
    startPlayNpcAnimation("elin", "Reaching Out")
    lookAtNpc("elin", 250, 0.7)
    say("elin", "Hmm he seemed to like the buff ones..")
    startPlayNpcAnimation("elin", "Talking_2")
    say("elin", "This reminds me of my uncle, minus all the beer cans and the smell of old tobacco smoke.")
    startNpcLookAtPlayer("elin", 1000)
    say("Too bad, I could use a beer right about now, even a warm one.")
    startSay("elin", "Me too.")

    elinArrival = assert(startMoveNpc("elin", "intro_marker_11", "walk", nil, true))
    lookAtNpc("elin", 3000, 0.7)
    assert(await(elinArrival))
    startPlayNpcAnimation("elin", "Reaching Out")
    lookAtNpc("elin", 250, 0.7)
    say("elin", "Seems like we have us a equal oppertunity amorist.")
    startNpcLookAtPlayer("elin", 2000)
    say("It would seem so. Two sixpacks so far and none of them of the drinkable varierity.")
    startPlayNpcAnimation("elin", "Talking")
    say("elin", "Aww.")

    assert(endCutscene())
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

function open_storage_door()
    if not flag("storage_door_unlocked") then
        playMapSound("door_locked", 0.8)
        text("It's locked", CENTER)
        setFlag("tried_storage_door", true)
        return false
    end
    return true
end

function useStorageDoorKey(targetInstanceId)
    -- log("used key on: " .. targetInstanceId)
    if targetInstanceId == "storage_door" then
        playMapSound("door_unlock", 0.8)
        setFlag("storage_door_unlocked", true)
        setFlag("tried_storage_door", false)
        return true
    end
    return false
end

function canOpenEntranceDoor()
    if not flag("entrance_door_unlocked") then
        text("It won't budge.", CENTER)
        return false
    end
    return true
end

function useBlueKeyCard(targetInstanceId)
    if targetInstanceId == "entrance_keypad" then
        playMapSound("keycard_accept", 0.8)
        setFlag("entrance_card_accepted", true)
        refreshEntranceAccess()
        -- do not consume key card it can be used in other places
    end
    return false
end

function usePasswordNote()
    showNote("Login:", "user: user\npassword: im2good")
end

local storageRadioOn = false

function useStorageRadio()
    enableControls(false)
    playMapSound("light_switch_on_01", 0.8)
    if storageRadioOn then
        storageRadioOn = false
        stopSoundEmitter("storage_radio_emitter")
        if flag("elin_in_storage_room") then
            setNpcAnimation("elin", "Idle")
            startPlayNpcAnimation("elin", "Rejected")
            lookAtNpc("elin", 750, 0.7)
        end
    else
        storageRadioOn = true
        playSoundEmitter("storage_radio_emitter")
        if flag("elin_in_storage_room") then
            setNpcAnimation("elin", "Dancing", 1.175)
            delay(500)
            lookAtNpc("elin", 750, 0.7)
        end
    end
    enableControls(true)
end
