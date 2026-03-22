function Scene_onEnter()
    if not flag("town_square_init") then
        setFlag("town_square_init", true)
        -- first time only stuff
        -- e.g. intro dialogue, item placement, whatever
    end
    -- start running scene scripts (animation, interactivity etc)
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