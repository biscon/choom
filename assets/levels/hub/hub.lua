function init()
    log("hub script initialized")
    startScript("npc_move_test1")
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

function npc_move_test1()
    while true do
        if not movePatrolNpc("zombie01", "wp1", "run") then return end
        if not movePatrolNpc("zombie01", "wp2", "walk") then return end
        if not movePatrolNpc("zombie01", "wp3", "walk") then return end
        if not movePatrolNpc("zombie01", "wp4", "walk") then return end
        if not movePatrolNpc("zombie01", "wp5", "walk") then return end
        if not movePatrolNpc("zombie01", "wp7", "walk") then return end
    end
end
