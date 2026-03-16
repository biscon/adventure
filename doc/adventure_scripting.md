# Adventure Engine Lua Scripting Guide

This document describes the current Lua scripting API exposed by the engine.

It covers:

- scene hooks
- dialogue and waits
- persistent script state
- inventory functions
- actor control
- prop control
- movement
- control locking
- background scripts
- interaction naming conventions

---

# General Notes

Scene scripts are normal Lua files loaded per scene.

Typical scene-level hooks look like this:

```lua
function Scene_onEnter()
    log("Entered scene")
end

function Scene_onExit()
    log("Leaving scene")
end
```

Some functions are **blocking** from the script's point of view. These yield the current script coroutine and resume automatically later.

Examples of yielding commands:

- `say(...)`
- `sayActor(...)`
- `sayProp(...)`
- `sayAt(...)`
- `walkTo(...)`
- `walkToHotspot(...)`
- `walkToExit(...)`
- `walkActorTo(...)`
- `walkActorToHotspot(...)`
- `walkActorToExit(...)`
- `delay(...)`

Non-blocking variants are prefixed with `start...`.

---

# Scene Hook Conventions

These are not hardcoded as a complete list, but the engine commonly looks for hooks like these:

```lua
Scene_onEnter()
Scene_onExit()
Scene_use_<targetId>()
Scene_look_<targetId>()
```

Inventory / item interaction hooks also follow naming conventions:

```lua
Scene_look_item_<itemId>()
Scene_use_item_<heldItemId>_on_item_<targetItemId>()
Scene_use_item_<heldItemId>_on_hotspot_<hotspotId>()
Scene_use_item_<heldItemId>_on_actor_<actorId>()
Scene_use_item_<heldItemId>_on_exit_<exitId>()
```

Return values matter for many interaction hooks:

- `return true` → handled success
- `return false` → handled failure
- missing function → engine fallback may run

For item use specifically, returning `false` keeps the held item selected.

---

# Talk Color Globals

Named talk colors are exposed as Lua globals.

Examples:

```lua
WHITE
YELLOW
PINK
CYAN
RED
```

These can be passed to speech functions.

---

# Persistent Script State

These values live in the script state and persist across scene changes until reset by game/session logic.

## setFlag(name, value)

Sets a boolean flag.

```lua
setFlag("windowUnlocked", true)
```

## flag(name)

Reads a boolean flag. Missing flags default to `false`.

```lua
if flag("windowUnlocked") then
    say("It is already unlocked.")
end
```

## setInt(name, value)

Sets an integer value.

```lua
setInt("coinCount", 3)
```

## getInt(name)

Reads an integer value. Missing values default to `-1`.

```lua
local coins = getInt("coinCount")
```

## setString(name, value)

Sets a string value.

```lua
setString("lastRoom", "basement")
```

## getString(name)

Reads a string value. Missing values default to `""`.

```lua
local room = getString("lastRoom")
```

---

# Inventory Functions

These work on the currently controlled actor unless otherwise noted.

## hasItem(itemId)

Returns whether the controlled actor has the item.

```lua
if hasItem("rusty_key") then
    say("I still have the key.")
end
```

## giveItem(itemId)

Gives an item to the controlled actor.

```lua
giveItem("rusty_key")
```

## removeItem(itemId)

Removes an item from the controlled actor.

```lua
removeItem("rusty_key")
```

## actorHasItem(actorId, itemId)

Checks whether a specific actor has an item.

```lua
if actorHasItem("sam", "badge") then
    say("Sam has the badge.")
end
```

## giveItemTo(actorId, itemId)

Gives an item to a specific actor.

```lua
giveItemTo("sam", "badge")
```

## removeItemFrom(actorId, itemId)

Removes an item from a specific actor.

```lua
removeItemFrom("sam", "badge")
```

---

# Speech

Speech commands yield until the speech finishes.

## say(text [, colorName] [, durationMs])
## say(text [, durationMs])

Speaks as the controlled actor.

Examples:

```lua
say("This place reeks.")
say("This place reeks.", 2500)
say("This place reeks.", YELLOW)
say("This place reeks.", YELLOW, 2500)
```

## sayActor(actorId, text [, colorName] [, durationMs])
## sayActor(actorId, text [, durationMs])

Speaks anchored to an actor.

```lua
sayActor("janitor", "Keep out of there.")
sayActor("janitor", "Keep out of there.", CYAN)
```

## sayProp(propId, text [, colorName] [, durationMs])
## sayProp(propId, text [, durationMs])

Speaks anchored to a prop.

```lua
sayProp("radio", "Bzzzzt...")
```

## sayAt(x, y, text [, colorName] [, durationMs])
## sayAt(x, y, text [, durationMs])

Speaks at a world position.

```lua
sayAt(850, 420, "A cold draft blows in here.")
```

---

# Movement and Timing

These functions yield until complete.

## walkTo(x, y)

Walk the controlled actor to a world position.

```lua
walkTo(1100, 640)
```

## walkToHotspot(hotspotId)

Walk the controlled actor to a hotspot's walk target.

```lua
walkToHotspot("furnace")
```

## walkToExit(exitId)

Walk the controlled actor to an exit's walk target.

```lua
walkToExit("stairs_up")
```

## walkActorTo(actorId, x, y)

Walk a specific actor to a world position.

```lua
walkActorTo("janitor", 900, 610)
```

## walkActorToHotspot(actorId, hotspotId)

```lua
walkActorToHotspot("janitor", "door")
```

## walkActorToExit(actorId, exitId)

```lua
walkActorToExit("janitor", "hallway")
```

## delay(ms)

Yield for a fixed time in milliseconds.

```lua
delay(1000)
say("One second later...")
```

---

# Non-Blocking Movement

These start movement and return immediately.

## startWalkTo(x, y)

```lua
startWalkTo(1100, 640)
```

## startWalkToHotspot(hotspotId)

```lua
startWalkToHotspot("furnace")
```

## startWalkToExit(exitId)

```lua
startWalkToExit("stairs_up")
```

## startWalkActorTo(actorId, x, y)

```lua
startWalkActorTo("janitor", 900, 610)
```

## startWalkActorToHotspot(actorId, hotspotId)

```lua
startWalkActorToHotspot("janitor", "door")
```

## startWalkActorToExit(actorId, exitId)

```lua
startWalkActorToExit("janitor", "hallway")
```

---

# Facing and Scene Changes

## face(facingName)

Faces the controlled actor immediately.

Valid values:

- `"left"`
- `"right"`
- `"front"`
- `"back"`

```lua
face("left")
```

## faceActor(actorId, facingName)

Faces a specific actor immediately.

```lua
faceActor("janitor", "right")
```

## changeScene(sceneId [, spawnId])

Queues a scene change.

```lua
changeScene("street")
changeScene("street", "alley_entry")
```

---

# Animation Control

## playAnimation(animationName)

Play a one-shot animation on the controlled actor.

```lua
playAnimation("use")
```

## playActorAnimation(actorId, animationName)

Play a one-shot animation on a specific actor.

```lua
playActorAnimation("janitor", "wave")
```

## playPropAnimation(propId, animationName)

Play a one-shot animation on a prop.

```lua
playPropAnimation("furnace_fire", "ignite")
```

## setPropAnimation(propId, animationName)

Set a looping / persistent prop animation.

```lua
setPropAnimation("furnace_fire", "idle")
```

---

# Prop Control

## setPropPosition(propId, x, y)

Set a prop to an absolute world position.

```lua
setPropPosition("crate", 840, 620)
```

## setPropPositionRelative(propId, dx, dy)

Move a prop relative to its current position.

```lua
setPropPositionRelative("crate", 20, 0)
```

## movePropTo(propId, x, y, durationMs, interpolation)

Tween a prop to an absolute position.

Supported interpolation names:

- `"linear"`
- `"accelerate"`
- `"decelerate"`
- `"accelerateDecelerate"`
- `"overshoot"`

```lua
movePropTo("crate", 900, 620, 500, "overshoot")
```

## movePropBy(propId, dx, dy, durationMs, interpolation)

Tween a prop relative to its current position.

```lua
movePropBy("crate", 60, 0, 400, "accelerateDecelerate")
```

## setPropVisible(propId, visible)

Show or hide a prop.

```lua
setPropVisible("rusty_key_prop", false)
```

## setPropFlipX(propId, flipX)

Flip a prop horizontally.

```lua
setPropFlipX("crate", true)
```

---

# Actor Control

## controlActor(actorId)

Switches the controlled actor.

The target actor must exist in the scene, be visible, and be marked controllable.

```lua
controlActor("sam")
```

## setActorVisible(actorId, visible)

Show or hide an actor.

```lua
setActorVisible("janitor", false)
```

---

# Controls Locking

## disableControls()

Disables player controls.

This also prevents the inventory from opening, though held items are not dropped automatically.

```lua
disableControls()
```

## enableControls()

Re-enables player controls.

```lua
enableControls()
```

---

# Background Scripts

The engine supports multiple coroutines. Scene hooks usually run as foreground scripts. You can also start background scripts manually.

## startScript(functionName)

Starts a background function by name.

```lua
startScript("StreetTraffic")
```

## stopScript(functionName)

Requests that a running script stop.

```lua
stopScript("StreetTraffic")
```

## stopAllScripts()

Requests that all running scripts stop.

```lua
stopAllScripts()
```

---

# Logging Helpers

## log(text)

Logs a plain message.

```lua
log("Entered puzzle branch")
```

## logf(format, ...)

Formatted log helper using Lua's `string.format`.

```lua
logf("Player has %d coins", getInt("coinCount"))
```

---

# Item Interaction Examples

## Examine item

```lua
function Scene_look_item_rusty_key()
    say("An old iron key covered in rust.")
end
```

If this function is missing, the engine falls back to the item's `lookText`.

## Item on hotspot

```lua
function Scene_use_item_rusty_key_on_hotspot_window()
    say("The key turns in the lock.")
    setFlag("windowUnlocked", true)
    removeItem("rusty_key")
    return true
end
```

## Item on hotspot, handled failure

```lua
function Scene_use_item_rusty_key_on_hotspot_furnace()
    say("I am not gonna burn it!!.")
    return false
end
```

Returning `false` keeps the held item selected.

## Item on item

```lua
function Scene_use_item_glue_on_item_broken_handle()
    say("That should hold.")
    removeItem("glue")
    return true
end
```

## Item on actor

```lua
function Scene_use_item_badge_on_actor_guard()
    sayActor("guard", "All right, go ahead.")
    return true
end
```

## Item on exit

```lua
function Scene_use_item_key_on_exit_door()
    say("The key turns.")
    removeItem("key")
    return true
end
```

---

# Recommended Return Semantics

For interaction handlers:

- `return true` when the action succeeds
- `return false` when the action is explicitly handled but fails

For inventory item use handlers, missing script hooks fall back to:

```lua
That won't work.
```

For successful interactions that consume an item, remove it explicitly in script:

```lua
removeItem("rusty_key")
return true
```

---

# Notes on Yielding

Functions like `say(...)`, `walkTo(...)`, and `delay(...)` yield the script coroutine.

This is fine for normal scene scripting.

For item-use success/failure behavior, remember that yielding handlers are treated specially by the engine. If a script should consume an item, the safest pattern is to explicitly remove the item in script.

---

# Complete API Summary

## State
- `setFlag(name, value)`
- `flag(name)`
- `setInt(name, value)`
- `getInt(name)`
- `setString(name, value)`
- `getString(name)`

## Inventory
- `hasItem(itemId)`
- `giveItem(itemId)`
- `removeItem(itemId)`
- `actorHasItem(actorId, itemId)`
- `giveItemTo(actorId, itemId)`
- `removeItemFrom(actorId, itemId)`

## Speech
- `say(text [, colorName] [, durationMs])`
- `say(text [, durationMs])`
- `sayActor(actorId, text [, colorName] [, durationMs])`
- `sayActor(actorId, text [, durationMs])`
- `sayProp(propId, text [, colorName] [, durationMs])`
- `sayProp(propId, text [, durationMs])`
- `sayAt(x, y, text [, colorName] [, durationMs])`
- `sayAt(x, y, text [, durationMs])`

## Movement / waits
- `walkTo(x, y)`
- `walkToHotspot(hotspotId)`
- `walkToExit(exitId)`
- `walkActorTo(actorId, x, y)`
- `walkActorToHotspot(actorId, hotspotId)`
- `walkActorToExit(actorId, exitId)`
- `delay(ms)`

## Non-blocking movement
- `startWalkTo(x, y)`
- `startWalkToHotspot(hotspotId)`
- `startWalkToExit(exitId)`
- `startWalkActorTo(actorId, x, y)`
- `startWalkActorToHotspot(actorId, hotspotId)`
- `startWalkActorToExit(actorId, exitId)`

## Facing / scene
- `face(facingName)`
- `faceActor(actorId, facingName)`
- `changeScene(sceneId [, spawnId])`

## Animation
- `playAnimation(animationName)`
- `playActorAnimation(actorId, animationName)`
- `playPropAnimation(propId, animationName)`
- `setPropAnimation(propId, animationName)`

## Props
- `setPropPosition(propId, x, y)`
- `setPropPositionRelative(propId, dx, dy)`
- `movePropTo(propId, x, y, durationMs, interpolation)`
- `movePropBy(propId, dx, dy, durationMs, interpolation)`
- `setPropVisible(propId, visible)`
- `setPropFlipX(propId, flipX)`

## Actor control
- `controlActor(actorId)`
- `setActorVisible(actorId, visible)`

## Input / control lock
- `disableControls()`
- `enableControls()`

## Coroutine control
- `startScript(functionName)`
- `stopScript(functionName)`
- `stopAllScripts()`

## Logging
- `log(text)`
- `logf(format, ...)`
