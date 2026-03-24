function Scene_onEnter()
    if not flag("hotel_lobby_init") then
        setFlag("hotel_lobby_init", true)
        -- first time only stuff
        -- e.g. intro dialogue, item placement, whatever
    end
    -- start running scene scripts (animation, interactivity etc)
    startScript("LampGlowLoop")
    startScript("ElectricLampGlowLoop")
    startScript("HotelAmbienceLoop")
end

function Scene_onExit()
    stopScript("HotelAmbienceLoop")
    setSoundEmitterEnabled("hotel_room_tone", false)
end

function Scene_look_to_town_square()
    walkToExit("to_town_square")
    face("right")
    say("Back out to the square.")
    say("Fresh air… if you can call it that.")
    return true
end

function Scene_look_desk()
    walkToHotspot("desk")
    face("left")
    say("A well-worn reception desk.")
    say("Someone keeps it tidy, at least on the surface.")
    return true
end

function Scene_use_desk()
    return Scene_look_desk()
end

function Scene_look_key_rack()
    walkToHotspot("key_rack")
    face("back")
    say("Room keys, neatly arranged.")
    say("More than I expected for a place like this.")
    return true
end

function Scene_use_key_rack()
    return Scene_look_key_rack()
end

function Scene_look_chair()
    walkToHotspot("chair")
    face("left")
    say("An old chair with a deep seat.")
    say("Looks like it has seen more waiting than resting.")
    return true
end

function Scene_use_chair()
    walkToHotspot("chair")
    face("left")
    say("I'd rather not sit just yet.")
    return true
end

function Scene_look_stairs()
    walkToHotspot("stairs")
    face("left")
    say("The stairs lead up to the rooms.")
    say("The wood creaks even when I'm not on it.")
    return true
end

function Scene_use_stairs()
    walkToHotspot("stairs")
    face("left")

    if not flag("hotel_room_unlocked") then
        say("I should speak to whoever runs this place first.")
        return true
    end

    say("That must be my room upstairs.")
    -- later: changeScene("hotel_upstairs", "from_lobby")
    return true
end

function Scene_look_coat_rack()
    walkToHotspot("coat_rack")
    face("right")
    say("A few coats hang here.")
    say("None of them look recently worn.")
    return true
end

function Scene_use_coat_rack()
    return Scene_look_coat_rack()
end

function Scene_look_display_case()
    walkToHotspot("display_case")
    face("right")
    say("A model ship in a glass display case.")
    say("Someone has taken better care of it than anything else in the lobby.")
    return true
end

function Scene_use_display_case()
    return Scene_look_display_case()
end

function Scene_look_bell()
    walkToHotspot("bell")
    face("left")
    say("A small brass desk bell.")
    say("Polished more often than the rest of the lobby.")
    return true
end

function Scene_use_bell()
    disableControls()
    walkToHotspot("bell")
    face("back")

    local times = getInt("hotel_bell_rang_count")
    if times < 0 then
        times = 0
    end

    if times == 0 then
        --playSound("bell")
        setInt("hotel_bell_rang_count", 1)
        sayActor("hotel_clerk", "Do not do that, sir. I am standing right here.")
    elseif times == 1 then
        --playSound("bell")
        setInt("hotel_bell_rang_count", 2)
        sayActor("hotel_clerk", "I should prefer not to be summoned like a servant.")
    else
        say("I had better not be overly rude.")
    end
    enableControls()
    return true
end

function Scene_look_actor_hotel_clerk()
    walkToHotspot("desk")
    face("left")
    say("The clerk watches me with the kind of patience that usually runs out fast.")
    return true
end

function Scene_use_actor_hotel_clerk()
    disableControls()
    if flag("hotel_room_unlocked") then
        walkToHotspot("desk")
        face("left")
        say("I'd rather not speak to that unpleasant gentleman again.")
        say("I should go upstairs and see to my room.")
        enableControls()
        return true
    end

    walkToHotspot("desk")
    face("left")

    sayActor("hotel_clerk", "Yes?")

    enableControls()

    Adv.runConversationDynamic("hotel_clerk_intro", {
        need_room = function()
            if not flag("hotel_room_denied") then
                setFlag("asked_clerk_room", true)
                setFlag("hotel_room_denied", true)

                say("I need a room for the night.")
                sayActor("hotel_clerk", "No rooms.")

                if not flag("saw_store_ledger") then
                    say("Your key rack says otherwise.")
                    sayActor("hotel_clerk", "Then it ought to mind its own business.")
                    return
                end
            else
                say("I need a room for the night.")
                sayActor("hotel_clerk", "I told you, we're closed.")
                sayActor("hotel_clerk", "There are no rooms.")

                if not flag("saw_store_ledger") then
                    return
                end
            end

            -- Only reaches here if player HAS seen the ledger
            local followup = dialogue("hotel_clerk_room_followup")

            if followup == "confront_ledger" then
                setFlag("confronted_clerk_with_ledger", true)
                setFlag("hotel_room_unlocked", true)

                say("The store ledger tells a different story.")
                sayActor("hotel_clerk", "Does it.")
                say("Food, lamp oil, soap.")
                say("Enough supplies for a good many guests.")
                sayActor("hotel_clerk", "The storekeeper keeps his accounts. That is his affair.")
                say("And this is yours.")
                say("You told me you had no rooms.")
                sayActor("hotel_clerk", "I told you what seemed advisable.")
                say("Advisable for whom?")
                sayActor("hotel_clerk", "...")
                sayActor("hotel_clerk", "There may be one room available.")
                sayActor("hotel_clerk", "Upstairs. End of the hall.")
                sayActor("hotel_clerk", "You will keep to it, and trouble no one.")
                say("...")
                return "exit"

            elseif followup == "leave_it" or followup == nil then
                say("Very well. Good day to you.")
                return "exit"
            end
        end,

        who_are_you = function()
            setFlag("asked_clerk_name", true)
            say("Do you run this place?")
            sayActor("hotel_clerk", "I mind the desk.")
            say("That wasn't quite what I asked.")
            sayActor("hotel_clerk", "It was close enough.")
        end,

        about_town = function()
            setFlag("asked_clerk_town", true)
            say("Quiet town.")
            sayActor("hotel_clerk", "We prefer it that way.")
        end,

        friend = function()
            setFlag("asked_clerk_friend", true)
            say("I'm looking for a friend.")
            sayActor("hotel_clerk", "Then I hope your friend had sense enough to keep moving.")
        end,

        goodbye = function()
            say("Never mind.")
            return "exit"
        end
    }, function()
        return Adv.hiddenOptions({
            who_are_you = flag("asked_clerk_name"),
            about_town = flag("asked_clerk_town"),
            friend = flag("asked_clerk_friend")
        })
    end)

    return true
end

-- Effect scripts -------------------------

function LampGlowLoop()
    local baseA = 0.50
    local baseB = 0.35

    local targetA = baseA
    local targetB = baseB

    while true do
        if math.random(1, 100) <= 18 then
            targetA = math.random(75, 95) / 100
            targetB = math.random(20, 55) / 100
        end

        -- drift slowly toward target values
        baseA = baseA + (targetA - baseA) * 0.18
        baseB = baseB + (targetB - baseB) * 0.18

        -- fast flame flicker layered on top
        local flickerA = (math.random(-8, 8)) / 100
        local flickerB = (math.random(-12, 12)) / 800

        local a = math.max(0, math.min(1, baseA + flickerA))
        local b = math.max(0, math.min(1, baseB + flickerB))

        setEffectRegionOpacity("wall_lamp_glow1", a)
        setEffectRegionOpacity("wall_lamp_glow2", b)

        delay(math.random(40, 120))
    end
end

function ElectricLampGlowLoop()
    local baseA = 0.65
    local baseB = 0.45

    local targetA = baseA
    local targetB = baseB

    while true do
        -- VERY rare small target drift (power fluctuation)
        if math.random(1, 100) <= 5 then
            targetA = math.random(62, 68) / 100
            targetB = math.random(42, 48) / 100
        end

        -- very slow drift toward target (stable feel)
        baseA = baseA + (targetA - baseA) * 0.03
        baseB = baseB + (targetB - baseB) * 0.03

        -- tiny shimmer (filament noise, barely visible)
        local shimmerA = (math.random(-2, 2)) / 200   -- ±0.01
        local shimmerB = (math.random(-2, 2)) / 300   -- even subtler

        local a = math.max(0, math.min(1, baseA + shimmerA))
        local b = math.max(0, math.min(1, baseB + shimmerB))

        setEffectRegionOpacity("ceiling_lamp_glow1", a)
        setEffectRegionOpacity("ceiling_lamp_glow2", b)

        delay(math.random(80, 160)) -- slower updates = calmer light
    end
end

-- Audio ----------------------------------------------

function HotelAmbienceLoop()
    -- base bed
    setSoundEmitterEnabled("hotel_room_tone", true)

    while true do
        delay(math.random(8000, 20000)) -- 8–20 sec gaps

        local roll = math.random(1, 100)

        if roll <= 40 then
            -- structure creaks
            local creaks = {
                "floor_creak_left",
                "floor_creak_upstairs",
                "wall_creak_right"
            }
            playEmitter(creaks[math.random(#creaks)])

        elseif roll <= 70 then
            -- subtle object sounds
            local objects = {
                "three_knocks",
                "metal_pipe"
            }
            playEmitter(objects[math.random(#objects)])

        else
            -- the important one: upstairs presence
            playEmitter("upstairs_movement")

            -- give it space to breathe
            delay(math.random(5000, 10000))
        end
    end
end