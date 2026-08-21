function init()
    log("hub script initialized")
    startScript("patrolZombie01")
    startScript("patrolZombie02")
    startScript("patrolZombie03")
end

function shutdown()
    log("hub script shut down")
end

function trigger_1()
    log("trigger_1")
end

function trigger_2()
    log("trigger_2")
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
