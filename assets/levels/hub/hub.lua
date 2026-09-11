local function elinConversation()
    local returning = flag("elin_intro_conversation_finished")
    runConversationDynamic(returning and "elin_repeat_topics" or "elin_intro_topics", {
        tunnels = function()
            say("Do you know a way out of these tunnels?")
            startPlayNpcAnimation("elin", "Talking_2")
            say("elin", "Not yet. Every passage seems to lead somewhere darker.", "afraid")
            setFlag("elin_asked_tunnels", true)
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
        goodbye = function()
            if returning then
                say("Talk later.")
                say("elin", "Talk later.")
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
            tunnels = flag("elin_asked_tunnels"),
            people = flag("elin_asked_people"),
        })
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
    --startScript("patrolZombie01")
    --startScript("patrolZombie02")
    --startScript("patrolZombie03")
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
    say("elin", "I've been walking these dark tunnels forever. You're are the first person I met so far.")
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

    assert(startConversation("elin", { reposition = false }))
    elinConversation()
    assert(endConversation())
    assert(endCutscene())
    startPlayNpcAnimation("elin", "Twerking")
end

function trigger_2()
    log("trigger_2")
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

local function movePatrolNpc(instanceId, markerId, gait)
    local ok, reason = moveNpc(instanceId, markerId, gait)
    if ok then
        return true
    end
    log("stopping patrol for " .. instanceId .. ": " .. (reason or "move failed"))
    return false
end

local patrolMarkers = {
    "marker_1",
    "marker_2",
    "marker_3",
    "marker_4",
    "marker_5",
    "marker_6",
    "marker_7",
}

local patrolTargets = {}

local function choosePatrolMarker(previousMarkerIndex)
    local candidateCount = 0
    for markerIndex = 1, #patrolMarkers do
        if markerIndex ~= previousMarkerIndex
                and patrolTargets[markerIndex] == nil then
            candidateCount = candidateCount + 1
        end
    end

    if candidateCount == 0 then
        return nil
    end

    local selectedCandidate = math.random(candidateCount)
    for markerIndex = 1, #patrolMarkers do
        if markerIndex ~= previousMarkerIndex
                and patrolTargets[markerIndex] == nil then
            selectedCandidate = selectedCandidate - 1
            if selectedCandidate == 0 then
                return markerIndex
            end
        end
    end

    return nil
end

local function patrolNpc(instanceId)
    local previousMarkerIndex = nil

    while true do
        local markerIndex = choosePatrolMarker(previousMarkerIndex)
        if markerIndex == nil then
            delay(0)
        else
            patrolTargets[markerIndex] = instanceId
            local gait = math.random(2) == 1 and "run" or "walk"
            local arrived = movePatrolNpc(
                    instanceId,
                    patrolMarkers[markerIndex],
                    gait)
            patrolTargets[markerIndex] = nil

            if not arrived then
                return
            end

            previousMarkerIndex = markerIndex
            delay(math.random(2000, 5000))
        end
    end
end

function patrolZombie01()
    patrolNpc("zombie01")
end

function patrolZombie02()
    patrolNpc("zombie02")
end

function patrolZombie03()
    patrolNpc("zombie03")
end
