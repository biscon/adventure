function Scene_onEnter()
    --startScript("Looper")
    log("Scene_onEnter fired")
    setFlag("entered_scene_once", true)
    giveItem("sausage")
end

function Looper()
    while true do
        log("tick")
        --sayActor("npc_actor", "Looping...", CYAN, 700)
        delay(1200)
    end
end

function Scene_onExit()
    print("On exit fired from LUA!!")
end

function Scene_use_furnace()
    if not walkToHotspot("furnace") then
        return false
    end

    log("about to face right")
    face("right")

    log("about to play reach_right")
    if not playAnimation("reach_right") then
        log("pickup anim missing, skipping")
    end

    delay(1000)

    setPropAnimation("cat", "walk_right")
    if flag("cat_moved") then
        log("cat moved")
        setPropFlipX("cat", true)
        movePropBy("cat", -250, -20, 1500, "accelerateDecelerate")
        setFlag("cat_moved", false)
    else
        log("cat not moved")
        setPropFlipX("cat", false)
        movePropBy("cat", 250, 20, 1500, "accelerateDecelerate")
        setFlag("cat_moved", true)
    end

    delay(1500)
    setPropAnimation("cat", "idle_right")

    sayProp("cat", "Meooow!", PINK)

    say("Easy there buddy.")

    sayProp("cat", "Meoooooow FOR HELVEDE!!!", RED)

    return true
end

function Scene_look_furnace()
    walkToHotspot("furnace")
    face("back")
    --delay(500)
    --say("A coal furnace. Mean-looking bastard.")
    giveItem("rusty_key")
    giveItem("sausage")
    say("I found a rusty key inside!.")
    return true
end

function Scene_look_to_first_floor()
    if not walkToExit("to_first_floor") then
        return false
    end

    face("back")
    say("Up we go.")
    return changeScene("first_floor", "stairs")
end

controlledActor = "main_actor"

function Scene_look_light_bulb()
    if controlledActor == "main_actor" then
        controlledActor = "npc_actor"
    else
        controlledActor = "main_actor"
    end
    controlActor(controlledActor)
    say("I am now in control.")
    -- walkActorTo("npc_actor", 1527, 663)
end

function Scene_use_window()
    disableControls()
    walkToHotspot("window")
    face("left")
    if not hasItem("rusty_key") then
        say("The window is locked. I should probably find the key first.")
    else
        say("The window is locked, maybe I should try to use the rusty key I found.")
    end
    enableControls()
    return true
end

function Scene_use_item_rusty_key_on_hotspot_window()
    disableControls()
    clearHeldItem()
    walkToHotspot("window")
    face("left")
    say("The key turns. The window unlocks.")

    setFlag("windowUnlocked", true)
    removeItem("rusty_key")
    enableControls()
    return true
end

function Scene_use_item_rusty_key_on_hotspot_furnace()
    say("I am not gonna burn it!!.")
    return false
end

function Scene_look_actor_npc_actor()
    say("He looks exhausted.")
    return true
end

function Scene_use_item_sausage_on_actor_main_actor()
    say("Om nom nom!")
    removeItem("sausage")
    return true
end

--[[
function Scene_use_actor_npc_actor()
    sayActor("npc_actor", "What do you want?")

    local choice = dialogue("npc_actor_intro")

    if choice == "who_are_you" then
        say("Who are you?")
        sayActor("npc_actor", "None of your business.")
    elseif choice == "what_happened" then
        say("What happened here?")
        sayActor("npc_actor", "Nothing you'd understand.")
    elseif choice == "goodbye" then
        say("Never mind.")
    else
        say("...")
    end

    return true
end
--]]

function BuildNpcActorIntroHiddenOptions()
    return Adv.hiddenOptions({
        who_are_you = flag("asked_npc_name"),
        what_happened = flag("asked_npc_what_happened"),
        apologize = not flag("insulted_npc_actor"),
        insult = flag("insulted_npc_actor")
    })
end

function Scene_use_actor_npc_actor()
    sayActor("npc_actor", "What do you want chico?")

    while true do
        local hidden = BuildNpcActorIntroHiddenOptions()
        local choice = dialogue("npc_actor_intro", hidden)

        if choice == nil then
            say("...")
            return true
        end

        if choice == "who_are_you" then
            setFlag("asked_npc_name", true)
            say("Who are you?")
            sayActor("npc_actor", "None of your business.")

        elseif choice == "what_happened" then
            setFlag("asked_npc_what_happened", true)
            say("What happened here?")
            sayActor("npc_actor", "Long day. Bad furnace. Worse company.")

        elseif choice == "rusty_key" then
            if hasItem("rusty_key") then
                say("I found a rusty key.")
                sayActor("npc_actor", "Good for you. Try not to lose it in your own skull.")
            else
                say("Seen any keys around?")
                sayActor("npc_actor", "No. And if I had, I wouldn't hand them to you.")
            end

        elseif choice == "sausage" then
            if hasItem("sausage") then
                say("I found a sausage.")
                sayActor("npc_actor", "Congratulations. A feast for kings.")
                say("You want it?")
                sayActor("npc_actor", "Not if you've been carrying it around in your pocket.")
            else
                say("You hungry?")
                sayActor("npc_actor", "Always. Doesn't mean I trust your cooking.")
            end

        elseif choice == "insult" then
            setFlag("insulted_npc_actor", true)
            say("You seem like a real joy to be around.")
            sayActor("npc_actor", "And you seem like a corpse that forgot to lie down.")

        elseif choice == "apologize" then
            say("Alright. Sorry.")
            sayActor("npc_actor", "Hmph. Better.")

        elseif choice == "give_sausage" then
            if hasItem("sausage") then
                say("Here. Take the sausage.")
                removeItem("sausage")
                sayActor("npc_actor", "Well... that's the least terrible thing you've done.")
                setFlag("npc_actor_fed", true)
            else
                say("I don't actually have it anymore.")
            end

        elseif choice == "goodbye" then
            say("Never mind.")
            return true

        else
            say("...")
            return true
        end
    end
end

function BuildMainActorConversationHiddenOptions()
    return Adv.hiddenOptions({
        who_are_you = flag("asked_main_actor_name"),
        what_happened = flag("asked_main_actor_what_happened"),
        rusty_key = not hasItem("rusty_key"),
        sausage = not hasItem("sausage"),
        give_sausage = not hasItem("sausage") or flag("main_actor_fed"),

        insult = flag("insulted_main_actor") or flag("apologized_to_main_actor"),
        apologize = (not flag("insulted_main_actor")) or flag("apologized_to_main_actor")
    })
end

function Scene_use_actor_main_actor()
    sayActor("main_actor", "Yes?")

    Adv.runConversationDynamic("npc_actor_intro", {
        who_are_you = function()
            setFlag("asked_main_actor_name", true)
            sayActor("npc_actor", "Who are you, exactly?")
            sayActor("main_actor", "The poor bastard doing all the work around here.")
        end,

        what_happened = function()
            setFlag("asked_main_actor_what_happened", true)
            sayActor("npc_actor", "What happened here?")
            sayActor("main_actor", "Bad luck, bad timing, and apparently bad company.")
        end,

        rusty_key = function()
            sayActor("npc_actor", "About that rusty key...")
            sayActor("main_actor", "Then keep hold of it. It looks important.")
        end,

        sausage = function()
            sayActor("npc_actor", "About this sausage...")
            sayActor("main_actor", "I was happier before I knew you had one.")
        end,

        give_sausage = function()
            sayActor("npc_actor", "Here. You can have the sausage.")
            removeItem("sausage")
            setFlag("main_actor_fed", true)
            sayActor("main_actor", "Finally, a productive conversation.")
        end,

        insult = function()
            setFlag("insulted_main_actor", true)
            sayActor("npc_actor", "You seem like a real pain in the ass.")
            sayProp("cat", "Meooow!", PINK)
            sayActor("npc_actor", "See even the cat thinks you're an asshole.")
            sayActor("main_actor", "And you seem very brave now that you're standing over there.")
        end,

        apologize = function()
            setFlag("insulted_main_actor", false)
            setFlag("apologized_to_main_actor", true)
            sayActor("npc_actor", "Alright. Sorry.")
            sayActor("main_actor", "Accepted. Don't make it a habit.")
        end,

        goodbye = function()
            sayActor("npc_actor", "Never mind.")
            return "exit"
        end
    }, BuildMainActorConversationHiddenOptions)

    return true
end
