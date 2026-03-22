function Scene_onEnter()
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
    say("A ledger filled with careful entries.")
    say("Most of it means nothing to me.")
    return true
end
function Scene_use_ledger()
    return Scene_look_ledger()
end

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
    return Scene_look_file_cabinet()
end

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
