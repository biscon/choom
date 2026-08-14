function init()
    log("hub script initialized")
    --startScript("npc_move_test1")
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

function npc_move_test1()
    log("npc move test1 ran!!!!!!!!!!!!!!!!")
    while true do
        moveNpc("zombie01", "wp1", "run")
        moveNpc("zombie01", "wp2", "run")
        moveNpc("zombie01", "wp3", "walk")
        moveNpc("zombie01", "wp4", "run")
        moveNpc("zombie01", "wp5", "walk")
        moveNpc("zombie01", "wp6", "walk")
        moveNpc("zombie01", "wp7", "run")
    end
end
