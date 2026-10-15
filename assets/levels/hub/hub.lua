local function elinPlaceQuestions()
    runConversationDynamic("elin_place_questions", {
        elin_arrival = function()
          say("How did you get here?")
          startPlayNpcAnimation("elin", "Talking_2")
          say("elin", "I was at the station when the lights went out. Everyone started pushing for the exits.")
          startPlayNpcAnimation("elin", "Rejected")
          say("elin", "A security guard opened a side door and waved me through. Then he went back for the others.", "afraid")
          say("elin", "I kept going. Never saw him again.")
          setFlag("elin_asked_arrival", true)
        end,
        back = function()
          say("Let me ask you something else.")
          say("elin", "Sure.")
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
            say("Thanks for patching me up. I'm James.")
            startPlayNpcAnimation("elin", "Waving")
            say("elin", "Elin. Nice to meet you, James. Could've picked a better place.", "happy")
            say("What the hell happened back there? The tunnel started shaking, then there was this huge bang.")
            say("I ran. Next thing I knew, I was on the floor. Managed to get up and find you.")
            startPlayNpcAnimation("elin", "Talking_2")
            say("elin", "I heard it too. Sounds like you only just got clear before the whole thing came down.")
            startPlayNpcAnimation("elin", "Talking")
            say("elin", "You had a nasty cut on your head. Didn't look like you were going to get much farther.")
            say("Well, I owe you one.")
            setFlag("elin_asked_identity", true)
        end,
        place = function()
            say("Any idea what this place is?")
            if not flag("elin_shown_place_questions") then
                startPlayNpcAnimation("elin", "Talking")
                say("elin", "Not really. I found this office and stopped to catch my breath.")
                startPlayNpcAnimation("elin", "Talking_2")
                say("elin", "There's a bed, the lights work... Beats stumbling around in the dark.")
                say("elin", "I thought whoever works here would be back by now.")
            else
                say("elin", "Still no idea. What did you want to know?")
            end
            elinPlaceQuestions()
            setFlag("elin_shown_place_questions", true)
            setFlag("elin_asked_place", flag("elin_asked_arrival"))
        end,
        people = function()
            say("Have you seen anyone else down here?", { mood = "afraid" })
            startPlayNpcAnimation("elin", "No")
            say("elin", "Not down here. I was starting to think I was on my own.")
            startPlayNpcAnimation("elin", "Talking")
            say("elin", "At least there's two of us now.", "relieved")
            startPlayNpcAnimation("elin", "Excited")
            setFlag("elin_asked_people", true)
        end,
        locked_door = function()
            say("Found any keys? That door in the hallway's locked.")
            local anim = startPlayNpcAnimation("elin", "No")
            say("elin", "No luck. Have a proper look around the office. I haven't checked everywhere.")
            assert(await(anim))
            setFlag("elin_asked_storage_door", true)
            return "exit"
        end,
        goodbye = function()
            if returning then
                say("Talk later.")
                say("elin", "Later.")
            else
                say("I'll have a look around. Stay close.")
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
    say("elin", "Who's there?!", "afraid")
    assert(startPlayNpcAnimation("elin", "Waving"))
    movePlayer("intro_marker_1", "walk", 2.0, {
        lookAtNpc = "elin",
        turnDurationMs = 750,
        targetHeight = 0.7,
    })
    delay(2000)
    startPlayNpcAnimation("elin", "Excited_2")
    say("elin", "Oh, thank God. Come here.", "relieved")
    movePlayer("intro_marker_2", "walk", 2.0, {
        lookAtNpc = "elin",
        turnDurationMs = 1000,
        targetHeight = 0.45,
    })
    lookAtNpc("elin", 1500, 0.7)
    startPlayNpcAnimation("elin", "Talking_2")
    say("elin", "Shit, you're bleeding. Come on, this way.", "afraid")
    local elinArrival = assert(startMoveNpc("elin", "recovery_marker_1", "walk", 1.5, true))
    delay(1500)
    movePlayer("recovery_marker_2", "walk", 1.0, {
        lookAtNpc = "elin",
        turnDurationMs = 2750,
        targetHeight = 0.7,
    })
    assert(await(elinArrival))
    startNpcLookAtPlayer("elin", 500)
    startPlayNpcAnimation("elin", "Reaching Out", 4000)
    startSay("elin", "Lie down. Let me have a look.")
    --lookAtNpc("elin", 500, 0.7)
    fadeOut(3950)

    delay(2000)
    setPlayerHealth(35)
    assert(teleportNpc("elin", "recovery_marker_4"))
    setActiveCamera("bed_camera_1")
    fadeIn(2000)
    playMapMusic("crystal_clear_water", false, 0.85)
    assert(trackCameraNpc("elin", 2000, 0.75))
    delay(2000)
    startPlayNpcAnimation("elin", "Talking_2")
    say("elin", "Keep still. I think I saw a first-aid kit over here.")
    moveNpc("elin", "recovery_marker_3", "walk", 1.5, true)
    startSay("elin", "Found it!", "happy")
    startPlayNpcAnimation("elin", "Reaching Out", 1000)
    delay(1500)
    moveNpc("elin", "recovery_marker_4", "walk", 1.5, true)
    startPlayNpcAnimation("elin", "Reaching Out", 3000)
    fadeOut(1950)
    delay(2000)
    setPlayerHealth(50)
    say("elin", "There. That's better. Try to get some rest.", "relieved")
    delay(2000)
    text("A while later...", CENTER, 2000)

    assert(stopCameraTracking())
    setCameraRotation(-89.897, 21.142, 0)
    assert(trackCameraNpc("elin", 0, 0.75))

    fadeIn(4000)
    startPlayNpcAnimation("elin", "Thankful")
    say("elin", "Hey. You look a bit more alive.", "happy")
    moveNpc("elin", "intro_marker_3", "walk", 1.5, true)

    moveCamera("intro_marker_4", 3000, 1.65)
    teleportPlayer("intro_marker_4")
    stopCameraTracking()
    setActiveCamera("player")
    assert(endCutscene())
end

function entrance_trigger()
    assert(startCutscene())
    delay(1000)
    text("How the hell am I getting through this?", BOTTOM)
    movePlayer("intro_marker_5", "walk", 1.5, {
        lookAtProp = "entrance_keypad",
    })
    text("Card reader and a keypad. Of course.", BOTTOM)
    assert(teleportNpc("elin", "intro_marker_6"))

    local elinArrival = assert(startMoveNpc("elin", "intro_marker_7", "walk", nil, true))
    delay(1000)
    startSay("elin", "Find something?")
    startLookAtNpc("elin", 2000, 0.7)
    delay(2000)
    startLookAtNpc("elin", 4000, 0.7)
    assert(await(elinArrival))
    lookAtNpc("elin", 350, 0.7)
    startPlayNpcAnimation("elin", "Excited_2")
    say("elin", "Jesus. What are they keeping behind that?")
    say("Whatever it is, they want a keycard and a code.")
    local cardAccepted = flag("entrance_card_accepted")
    local pinAccepted = flag("entrance_pin_accepted")
    local hasCard = hasInventoryItemInstance("blue_key_card")
    if cardAccepted and pinAccepted then
        say("Both accepted. Should open now.")
    elseif cardAccepted then
        say("The reader accepted the card. Still need the code.")
    elseif pinAccepted and hasCard then
        say("The code worked. Just need to try this card from the storage room.")
    elseif pinAccepted then
        say("The code worked. Still need a keycard.")
    elseif hasCard then
        say("I've got a card from the storage room. Still need the code.")
    else
        say("Haven't got either.")
    end
    startPlayNpcAnimation("elin", "Talking_2")
    say("elin", "Listen. You hear that?")
    say("elin", "I don't like the sound of whatever's in there. Be careful.")
    startPlayNpcAnimation("elin", "No")
    say("elin", "I'll be back in the office.")
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
    local elinSay = startSay("elin", "Found the janitor's room, then. Any sign of the janitor?")
    setFlag("elin_in_storage_room", true)
    assert(await(lookWait))
    assert(await(elinSay))
    elinArrival = assert(startMoveNpc("elin", "intro_marker_10", "walk", nil, true))
    lookAtNpc("elin", 3000, 0.7)
    assert(await(elinArrival))
    startPlayNpcAnimation("elin", "Reaching Out")
    lookAtNpc("elin", 250, 0.7)
    say("elin", "Well. Someone likes muscles.")
    startPlayNpcAnimation("elin", "Talking_2")
    say("elin", "My uncle's a janitor. He'd never leave his tools like that.")
    say("elin", "Mind you, there'd be beer cans everywhere and the whole place would stink of tobacco.")
    startNpcLookAtPlayer("elin", 1000)
    say("Shame about the beer. I'd take a warm one at this point.")
    startSay("elin", "You and me both.")

    elinArrival = assert(startMoveNpc("elin", "intro_marker_11", "walk", nil, true))
    lookAtNpc("elin", 3000, 0.7)
    assert(await(elinArrival))
    startPlayNpcAnimation("elin", "Reaching Out")
    lookAtNpc("elin", 250, 0.7)
    say("elin", "Oh, he's not fussy. Fair enough.")
    startNpcLookAtPlayer("elin", 2000)
    say("Two six-packs, and neither one's any fucking use to me.")
    startPlayNpcAnimation("elin", "Talking")
    say("elin", "Speak for yourself.")

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

function useStaffingNotice()
    showNote("CHRISTMAS COVER", [[
Most annex staff will be off from December 24 through December 27.

Meals for the holiday crew are in the kitchen freezer. Please take only what you need. Last year's situation will not be repeated.

Receiving is closed to outside deliveries until December 28. Internal transfers continue on the usual schedule.

If a transfer fault occurs, call the duty supervisor. Do not enter the chamber to retrieve a stalled load.

And whoever borrowed the lounge heater: bring it back.

Merry Christmas.]])
end

function useServiceReminderNotice()
    showNote("TRANSFER EQUIPMENT - SERVICE REMINDER", [[
Before servicing a transfer unit, close and secure the inner shutter.

Do not include newspapers, receipts, calendars, or original packaging with a load. Check beneath trays and inside empty containers.

If you hear speech from the receiving side, do not answer. Report the unit number to the duty supervisor.

A voice is not grounds for opening the shutter.]])
end

function usePasswordComplaint()
    showNote([[
Stop resetting the outer door code.

It's the title of the book beside my bed. Four numbers. Even you should manage that.

And put my spare key back when you're done. I'm tired of getting locked out of my own workshop.]])
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
