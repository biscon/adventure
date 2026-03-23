function Scene_onEnter()
    if not flag("town_square_init") then
        setFlag("town_square_init", true)
        -- first time only stuff
        -- e.g. intro dialogue, item placement, whatever
    end
    -- start running scene scripts (animation, interactivity etc)
    startScript("FogPulseLoop")
end

function Scene_look_hotel_sign()
    walkToHotspot("hotel_sign")
    face("back")
    say("\"The Adrift Inn.\"")
    say("The letters look newer than the wood they're nailed to.")
    return true
end

-- let both actions do the same where it doesnt make sense to have to different ones
function Scene_use_hotel_sign()
    return Scene_look_hotel_sign()
end

function Scene_look_store_sign()
    walkToHotspot("store_sign")
    face("back")
    say("\"O'Malley's Goods & Supplies.\"")
    say("The paint is fading, but someone keeps it from disappearing completely.")
    return true
end

function Scene_use_store_sign()
    return Scene_look_store_sign()
end

function Scene_look_alley()
    walkToHotspot("alley")
    face("back")
    say("A narrow alley disappearing into shadow.")
    say("Something about it makes me not want to linger.")
    return true
end

function Scene_use_alley()
    return Scene_look_alley()
end

function Scene_look_anchors()
    walkToHotspot("anchors")
    face("back")
    say("Rust has eaten most of the metal away.")
    say("Like they haven't seen a proper ship in years.")
    return true
end

function Scene_use_anchors()
    return Scene_look_anchors()
end

function Scene_look_to_store()
    walkToExit("to_store")
    face("back")
    say("A plain little storefront.")
    say("Still, it feels more honest than most things here.")
    return true
end

function Scene_look_to_hotel_lobby()
    walkToExit("to_hotel_lobby")
    say("A shabby hotel entrance.")
    say("The sort of place where you sleep in your clothes and keep one eye open.")
    return true
end

-- Effect scripts -------------------------

function FogPulseLoopOLD()
    local baseA = 0.25
    local baseB = 0.18

    local targetA = baseA
    local targetB = baseB

    while true do
        if math.random(1, 100) <= 18 then
            targetA = math.random(75, 95) / 100
            targetB = math.random(20, 55) / 100
        end

        -- drift slowly toward target values
        baseA = baseA + (targetA - baseA) * 0.08
        baseB = baseB + (targetB - baseB) * 0.08

        local a = math.max(0, math.min(1, baseA))
        local b = math.max(0, math.min(1, baseB))

        setEffectRegionOpacity("square_ground_fog", a)
        setEffectRegionOpacity("square_ground_fog_detail", b)

        delay(math.random(40, 120))
    end
end

function FogPulseLoopLame()
    local baseA = 0.45
    local baseB = 0.38

    local targetA = baseA
    local targetB = baseB

    while true do
        if math.random(1, 100) <= 10 then
            targetA = math.random(12, 18) / 100
            targetB = math.random(6, 11) / 100
        end

        baseA = baseA + (targetA - baseA) * 0.025
        baseB = baseB + (targetB - baseB) * 0.02

        local a = math.max(0, math.min(1, baseA))
        local b = math.max(0, math.min(1, baseB))

        setEffectRegionOpacity("square_ground_fog", a)
        setEffectRegionOpacity("square_ground_fog_detail", b)

        delay(math.random(120, 240))
    end
end

function FogPulseLoop()
    local baseA = 0.34
    local baseB = 0.28

    local cycleDuration = 18000
    local cycleStart = os.clock() * 1000.0

    while true do
        local now = os.clock() * 1000.0
        local t = (now - cycleStart) / cycleDuration

        if t >= 1.0 then
            cycleStart = now
            cycleDuration = math.random(16000, 26000)
            t = 0.0
        end

        -- smooth ebb/flow wave from 0..1..0
        local wave = 0.5 - 0.5 * math.cos(t * math.pi * 2.0)

        -- broad fog breathes more than detail layer
        local a = baseA + wave * 0.035
        local b = baseB + wave * 0.015

        -- occasional slightly denser passing patch
        if math.random(1, 100) <= 6 then
            a = a + math.random(0, 10) / 1000.0
            b = b + math.random(0, 6) / 1000.0
        end

        a = math.max(0.0, math.min(1.0, a))
        b = math.max(0.0, math.min(1.0, b))

        setEffectRegionOpacity("square_ground_fog", a)
        setEffectRegionOpacity("square_ground_fog_detail", b)
        setEffectRegionOpacity("alley_fog", a)
        setEffectRegionOpacity("alley_fog_detail", b)

        delay(math.random(120, 220))
    end
end