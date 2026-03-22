function Scene_onEnter()
    if not flag("hotel_lobby_init") then
        setFlag("hotel_lobby_init", true)
        -- first time only stuff
        -- e.g. intro dialogue, item placement, whatever
    end
    -- start running scene scripts (animation, interactivity etc)
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
    say("I should speak to whoever runs this place first.")
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

function Scene_look_actor_hotel_clerk()
    walkToHotspot("desk")
    face("left")
    say("The clerk watches me with the kind of patience that usually runs out fast.")
    return true
end

function Scene_use_actor_hotel_clerk()
    walkToHotspot("desk")
    face("left")

    sayActor("hotel_clerk", "Yes?")

    Adv.runConversationDynamic("hotel_clerk_intro", {
        need_room = function()
            setFlag("asked_clerk_room", true)
            setFlag("hotel_room_denied", true)
            say("I need a room for the night.")
            sayActor("hotel_clerk", "No rooms.")
            say("Your key rack says otherwise.")
            sayActor("hotel_clerk", "Then it ought to mind its own business.")
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
            sayActor("hotel_clerk", "Then I hope your friend wanted to be found.")
        end,

        ledger = function()
            setFlag("confronted_clerk_with_ledger", true)
            setFlag("hotel_room_unlocked", true)

            say("I saw the store ledger.")
            sayActor("hotel_clerk", "Did you.")
            say("You told me you had no rooms.")
            sayActor("hotel_clerk", "I told you what seemed best at the time.")
            say("For whom?")
            sayActor("hotel_clerk", "...")
            sayActor("hotel_clerk", "There may be one room available.")
            sayActor("hotel_clerk", "Upstairs. End of the hall.")
        end,

        goodbye = function()
            say("Never mind.")
            return "exit"
        end
    }, function()
        return Adv.hiddenOptions({
            who_are_you = flag("asked_clerk_name"),
            about_town = flag("asked_clerk_town"),
            friend = flag("asked_clerk_friend"),
            need_room = flag("hotel_room_unlocked"),
            ledger = not flag("saw_store_ledger") or flag("confronted_clerk_with_ledger")
        })
    end)

    return true
end
