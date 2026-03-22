function Scene_onEnter()
    math.randomseed(os.time())
    if not flag("store_init") then
        setFlag("store_init", true)
        -- first time only stuff
        -- e.g. intro dialogue, item placement, whatever
    end
    -- start running scene scripts (animation, interactivity etc)
end

function Scene_look_ledger()
    walkToHotspot("ledger")
    face("left")
    say("Some kind of ledger, probably detailing the stores financial transactions.")
    sayActor("store_clerk", "Yes it's where I keep track of all my orders sir.")
    say("Interesting.")
    return true
end

function Scene_use_ledger()
    disableControls()
    walkToHotspot("ledger")
    face("left")
    playAnimation("reach_left")
    delay(600)
    if not flag("can_use_ledger") then
        sayActor("store_clerk", "That is not for sale good sir!")
    else
        say("This ledger is full of transactions involving the hotel.")
        say("Apparently they order a lot of food items, for a place that's supposed to be closed down.")
    end
    enableControls()
    return true
end

-- Flavor hotspots --------------------------------------------------------
function Scene_look_notice_board()
    walkToHotspot("notice_board")
    face("back")
    say("Notices, schedules, and scraps of paper.")
    say("Some are so old the ink has nearly vanished.")
    return true
end

function Scene_use_notice_board()
    return Scene_look_notice_board()
end

function Scene_look_shelves()
    walkToHotspot("shelves")
    face("back")
    say("Tinned goods, jars, and things I can't quite identify.")
    say("All of it looks like it's been here a long time.")
    return true
end

function Scene_use_shelves()
    return Scene_look_shelves()
end

function Scene_look_file_cabinet()
    walkToHotspot("file_cabinet")
    face("back")
    say("Drawers for records.")
    say("Organized, but not inviting.")
    return true
end

function Scene_use_file_cabinet()
    walkToHotspot("file_cabinet")
    face("back")
    sayActor("store_clerk", "Keep your hands of my files, they're private!")
    say("Sorry")
    return true
end

-- Store clerk ----------------------------------------------------

function Scene_look_actor_store_clerk()
    walkToHotspot("ledger")
    face("left")
    say("A young man, trying very hard not to be noticed.")
    return true
end

function Scene_use_actor_store_clerk()
    walkToHotspot("ledger")
    face("left")

    sayActor("store_clerk", "Yes, sir?")

    Adv.runConversationDynamic("store_clerk_intro", {

        buy_supplies = function()
            setFlag("asked_store_buy", true)
            say("Do you sell provisions?")
            sayActor("store_clerk", "A few things. Not much comes through these days.")
            sayActor("store_clerk", "What we have is on the shelves.")
        end,

        about_town = function()
            setFlag("asked_store_town", true)
            say("I passed a church on my way in.")
            say("St. Mary's, I think.")
            say("It looked... abandoned.")
            sayActor("store_clerk", "It is.")
            say("What happened to it?")
            sayActor("store_clerk", "Folks stopped attending.")
            say("Just like that?")
            sayActor("store_clerk", "Not all at once.")
            sayActor("store_clerk", "They found... other places to be.")
        end,

        ledger = function()
            setFlag("asked_store_ledger", true)
            setFlag("saw_store_ledger", true)

            say("What's that book?")
            sayActor("store_clerk", "Just accounts.")
            say("You keep careful records.")
            sayActor("store_clerk", "I have to.")
            say("I noticed the inn comes up often.")
            sayActor("store_clerk", "They take deliveries.")
            say("Even this late?")
            sayActor("store_clerk", "Sometimes it's better not to delay things.")
        end,

        inn = function()
            setFlag("asked_store_inn", true)
            say("The inn seems empty.")
            sayActor("store_clerk", "It isn't.")
            say("The clerk told me otherwise.")
            sayActor("store_clerk", "He would.")
            say("Why?")
            sayActor("store_clerk", "You should be careful where you stay.")
        end,

        friend = function()
            setFlag("asked_store_friend", true)
            say("I'm looking for someone.")
            sayActor("store_clerk", "Then I hope you find them quickly.")
            say("Why quickly?")
            sayActor("store_clerk", "Because after a while...")
            sayActor("store_clerk", "...people stop asking to leave.")
        end,

        goodbye = function()
            say("I'll leave you to it.")
            sayActor("store_clerk", "Yes, sir.")
            return "exit"
        end

    }, function()
        return Adv.hiddenOptions({
            buy_supplies = flag("asked_store_buy"),
            ledger = flag("asked_store_ledger"),
            about_town = flag("asked_store_town"),
            inn = flag("asked_store_inn"),
            friend = flag("asked_store_friend")
        })
    end)

    return true
end

-- Dog the bounty hunter ---------------------------------------------

function ComfortDog()
    local barks = {
        "Woof!",
        "Grr...",
        "Arf!"
    }

    while true do
        playPropAnimation("german_shepard", "bark")
        delay(500)
        setPropAnimation("german_shepard", "idle")
        startSayAt(3*100, 3*300, barks[math.random(#barks)], YELLOW)
        delay(2200)
    end
end

function Scene_use_dog()
    disableControls()
    walkToHotspot("dog")
    face("left")
    playAnimation("pickup_left")
    delay(300)
    playPropAnimation("german_shepard", "wake_up")
    delay(1000)
    for i = 1, 3 do
        if i == 2 then
            faceActor("store_clerk", "front")
            startWalkTo(3*240, 3*336)
        end
        playPropAnimation("german_shepard", "bark")
        delay(500)
    end
    setPropAnimation("german_shepard", "idle")
    delay(200)
    face("left")
    sayAt(3*100, 3*300, "Woof!", RED)
    sayActor("store_clerk", "Easy there little fella!.")
    walkActorToHotspot("store_clerk", "dog")
    faceActor("store_clerk", "left")
    playActorAnimation("store_clerk", "pickup_left")
    delay(300)
    sayActor("store_clerk", "Who's a good boy?.")
    sayActor("store_clerk", "You are!.")
    sayAt(3*100, 3*300, "Woof!", BLUE)
    enableControls()
    setFlag("can_use_ledger", true)
    startScript("ComfortDog")
    --sayProp("german_shepard", "Woof!", RED)

    return true
end
