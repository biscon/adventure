#include "adventure/AdventureUpdate.h"

#include <cmath>
#include <algorithm>

#include "adventure/AdventureActionSystem.h"
#include "adventure/AdventureActorHelpers.h"
#include "adventure/AdventureScriptInternal.h"
#include "input/Input.h"
#include "raylib.h"
#include "scene/SceneHelpers.h"
#include "scene/SceneLoad.h"
#include "scripting/ScriptSystem.h"
#include "render/RenderHelpers.h"
#include "adventure/InventoryUi.h"
#include "adventure/Inventory.h"
#include "Adventure.h"
#include "adventure/Dialogue.h"
#include "adventure/AdventureCamera.h"
#include "raymath.h"


static void HandleDebugInput(GameState& state)
{
    for (auto& ev : FilterEvents(state.input, true, InputEventType::KeyPressed)) {
        switch (ev.key.key) {
            case KEY_F1:
                state.debug.showWalkPolygons = !state.debug.showWalkPolygons;
                ConsumeEvent(ev);
                break;
            case KEY_F2:
                state.debug.showNavTriangles = !state.debug.showNavTriangles;
                ConsumeEvent(ev);
                break;
            case KEY_F3:
                state.debug.showNavAdjacency = !state.debug.showNavAdjacency;
                ConsumeEvent(ev);
                break;
            case KEY_F4:
                state.debug.showPath = !state.debug.showPath;
                ConsumeEvent(ev);
                break;
            case KEY_F5:
                state.debug.showFeetPoints = !state.debug.showFeetPoints;
                ConsumeEvent(ev);
                break;
            case KEY_F6:
                state.debug.showScaleInfo = !state.debug.showScaleInfo;
                ConsumeEvent(ev);
                break;
            case KEY_F7:
                state.debug.showTrianglePath = !state.debug.showTrianglePath;
                ConsumeEvent(ev);
                break;
            case KEY_F8:
                state.debug.showSceneObjects = !state.debug.showSceneObjects;
                ConsumeEvent(ev);
                break;
            case KEY_F9:
                state.debug.showScripts = !state.debug.showScripts;
                ConsumeEvent(ev);
                break;
            case KEY_F10:
                state.debug.showEffects = !state.debug.showEffects;
                ConsumeEvent(ev);
                break;
            default:
                break;
        }
    }
}

void AdventureQueueLoadScene(GameState& state, const char* sceneId, const char* spawnId)
{
    state.adventure.pendingSceneId = sceneId;
    state.adventure.pendingSpawnId = (spawnId != nullptr) ? spawnId : "";
    state.adventure.hasPendingSceneLoad = true;
}

void AdventureProcessPendingLoads(GameState& state)
{
    if (!state.adventure.hasPendingSceneLoad) {
        return;
    }

    const std::string sceneId = state.adventure.pendingSceneId;
    state.adventure.pendingSceneId.clear();
    state.adventure.hasPendingSceneLoad = false;

    if (LoadSceneById(state, sceneId.c_str())) {
        state.mode = GameMode::Game;
    } else {
        TraceLog(LOG_ERROR, "Scene load failed: %s", sceneId.c_str());
        state.mode = GameMode::Menu;
    }

    state.adventure.pendingSpawnId.clear();
}

static void UpdateActorFacingAndAnimation(ActorInstance& actor, Vector2 movementDir)
{
    if (AdventureLength(movementDir) <= 0.0001f) {
        return;
    }

    actor.stoppedTimeMs = 0.0f;
    actor.inIdleState = false;

    const float absX = std::fabs(movementDir.x);
    const float absY = std::fabs(movementDir.y);

    const bool wasHorizontal =
            actor.facing == ActorFacing::Left ||
            actor.facing == ActorFacing::Right;

    bool useHorizontal = false;

    if (wasHorizontal) {
        useHorizontal = !(absY > absX * 1.8f);
    } else {
        useHorizontal = (absX > absY * 1.35f);
    }

    if (useHorizontal && absX > 0.0001f) {
        actor.currentAnimation = "walk_right";
        actor.flipX = movementDir.x < 0.0f;
        actor.facing = movementDir.x < 0.0f ? ActorFacing::Left : ActorFacing::Right;
        return;
    }

    if (movementDir.y < 0.0f) {
        actor.currentAnimation = "walk_back";
        actor.flipX = false;
        actor.facing = ActorFacing::Back;
    } else {
        actor.currentAnimation = "walk_front";
        actor.flipX = false;
        actor.facing = ActorFacing::Front;
    }
}

static bool UpdateActorMovement(ActorInstance& actor, float dt)
{
    if (!actor.path.active || actor.path.currentPoint >= static_cast<int>(actor.path.points.size())) {
        actor.path.active = false;

        if (!actor.scriptAnimationActive) {
            actor.stoppedTimeMs += dt * 1000.0f;

            if (!actor.inIdleState && actor.stoppedTimeMs >= actor.idleDelayMs) {
                AdventureSwitchToDirectionalIdle(actor);
                actor.inIdleState = true;
            }
        }

        return false;
    }

    const Vector2 target = actor.path.points[actor.path.currentPoint];
    Vector2 delta{
            target.x - actor.feetPos.x,
            target.y - actor.feetPos.y
    };

    const float dist = AdventureLength(delta);
    const float speedMultiplier = actor.path.fastMove ? actor.fastMoveMultiplier : 1.0f;
    const float moveSpeed = actor.walkSpeed * speedMultiplier;

    if (dist <= 1.0f) {
        actor.feetPos = target;
        actor.path.currentPoint++;

        if (actor.path.currentPoint >= static_cast<int>(actor.path.points.size())) {
            actor.path.active = false;
            actor.stoppedTimeMs = 0.0f;
            actor.animationTimeMs = 0.0f;
            return true;
        }

        return false;
    }

    const Vector2 dir = AdventureNormalizeOrZero(delta);
    const float step = moveSpeed * dt;

    UpdateActorFacingAndAnimation(actor, dir);

    if (dist <= step) {
        actor.feetPos = target;
        actor.path.currentPoint++;

        if (actor.path.currentPoint >= static_cast<int>(actor.path.points.size())) {
            actor.path.active = false;
            actor.stoppedTimeMs = 0.0f;
            actor.animationTimeMs = 0.0f;
            return true;
        }
    } else {
        actor.feetPos.x += dir.x * step;
        actor.feetPos.y += dir.y * step;
    }

    return false;
}

static bool UpdateControlledActorMovement(GameState& state, float dt)
{
    ActorInstance* controlledActor = GetControlledActor(state);
    if (controlledActor == nullptr) {
        return false;
    }

    return UpdateActorMovement(*controlledActor, dt);
}

static void ExecutePendingInteraction(GameState& state)
{
    if (!state.adventure.pendingInteraction.active) {
        return;
    }

    ActorInstance* player = GetControlledActor(state);
    if (player == nullptr) {
        return;
    }

    const PendingInteraction pending = state.adventure.pendingInteraction;
    state.adventure.pendingInteraction = {};

    if (pending.type == PendingInteractionType::UseHotspot) {
        if (pending.targetIndex >= 0 &&
            pending.targetIndex < static_cast<int>(state.adventure.currentScene.hotspots.size())) {
            const SceneHotspot& hotspot = state.adventure.currentScene.hotspots[pending.targetIndex];
            AdventureForceFacing(*player, hotspot.facing);
            TraceLog(LOG_INFO, "Use hotspot: %s", hotspot.displayName.c_str());
        }
        return;
    }

    if (pending.type == PendingInteractionType::LookHotspot) {
        if (pending.targetIndex >= 0 &&
            pending.targetIndex < static_cast<int>(state.adventure.currentScene.hotspots.size())) {
            const SceneHotspot& hotspot = state.adventure.currentScene.hotspots[pending.targetIndex];
            AdventureForceFacing(*player, hotspot.facing);

            const ActorDefinitionData* actorDef =
                    FindActorDefinitionByIndex(state, player->actorDefIndex);
            const Color talkColor = (actorDef != nullptr) ? actorDef->talkColor : WHITE;


            AdventureStartSpeech(
                    state,
                    SpeechAnchorType::Player,
                    -1,
                    -1,
                    {},
                    hotspot.lookText,
                    talkColor,
                    -1);
        }
        return;
    }

    if (pending.type == PendingInteractionType::UseExit) {
        if (pending.targetIndex >= 0 &&
            pending.targetIndex < static_cast<int>(state.adventure.currentScene.exits.size())) {
            const SceneExit& exitObj = state.adventure.currentScene.exits[pending.targetIndex];
            AdventureForceFacing(*player, exitObj.facing);
            AdventureQueueLoadScene(state, exitObj.targetScene.c_str(), exitObj.targetSpawn.c_str());
        }
        return;
    }

    if (pending.type == PendingInteractionType::LookExit) {
        if (pending.targetIndex >= 0 &&
            pending.targetIndex < static_cast<int>(state.adventure.currentScene.exits.size())) {
            const SceneExit& exitObj = state.adventure.currentScene.exits[pending.targetIndex];
            AdventureForceFacing(*player, exitObj.facing);
            const ActorDefinitionData* actorDef =
                    FindActorDefinitionByIndex(state, player->actorDefIndex);
            const Color talkColor = (actorDef != nullptr) ? actorDef->talkColor : WHITE;
            AdventureStartSpeech(
                    state,
                    SpeechAnchorType::Player,
                    -1,
                    -1,
                    {},
                    exitObj.lookText,
                    talkColor,
                    -1);

        }
        return;
    }

    if (pending.type == PendingInteractionType::UseActor) {
        if (pending.targetIndex >= 0 &&
            pending.targetIndex < static_cast<int>(state.adventure.actors.size())) {
            ActorInstance& targetActor = state.adventure.actors[pending.targetIndex];
            if (!targetActor.activeInScene || !targetActor.visible) {
                return;
            }

            ActorInstance* player = GetControlledActor(state);
            if (player == nullptr) {
                return;
            }

            Vector2 delta{
                    targetActor.feetPos.x - player->feetPos.x,
                    targetActor.feetPos.y - player->feetPos.y
            };

            if (std::fabs(delta.x) > std::fabs(delta.y)) {
                AdventureForceFacing(*player, delta.x < 0.0f ? SceneFacing::Left : SceneFacing::Right);
            } else {
                AdventureForceFacing(*player, delta.y < 0.0f ? SceneFacing::Back : SceneFacing::Front);
            }

            const std::string functionName = "Scene_use_actor_" + targetActor.actorId;
            const ScriptCallResult result = ScriptSystemCallTrigger(state, functionName);

            if (result == ScriptCallResult::Missing ||
                result == ScriptCallResult::ImmediateFalse) {
                AdventureScriptSay(state, "That won't work.", -1);
            }
        }
        return;
    }

    if (pending.type == PendingInteractionType::LookActor) {
        if (pending.targetIndex >= 0 &&
            pending.targetIndex < static_cast<int>(state.adventure.actors.size())) {
            ActorInstance& targetActor = state.adventure.actors[pending.targetIndex];
            if (!targetActor.activeInScene || !targetActor.visible) {
                return;
            }

            ActorInstance* player = GetControlledActor(state);
            if (player == nullptr) {
                return;
            }

            Vector2 delta{
                    targetActor.feetPos.x - player->feetPos.x,
                    targetActor.feetPos.y - player->feetPos.y
            };

            if (std::fabs(delta.x) > std::fabs(delta.y)) {
                AdventureForceFacing(*player, delta.x < 0.0f ? SceneFacing::Left : SceneFacing::Right);
            } else {
                AdventureForceFacing(*player, delta.y < 0.0f ? SceneFacing::Back : SceneFacing::Front);
            }

            const std::string functionName = "Scene_look_actor_" + targetActor.actorId;
            const ScriptCallResult result = ScriptSystemCallTrigger(state, functionName);

            if (result == ScriptCallResult::Missing ||
                result == ScriptCallResult::ImmediateFalse) {
                const ActorDefinitionData* actorDef =
                        FindActorDefinitionByIndex(state, player->actorDefIndex);
                const Color talkColor = (actorDef != nullptr) ? actorDef->talkColor : WHITE;

                const ActorDefinitionData* targetDef =
                        FindActorDefinitionByIndex(state, targetActor.actorDefIndex);

                std::string lookText = "It's ";
                if (targetDef != nullptr && !targetDef->displayName.empty()) {
                    lookText += targetDef->displayName;
                } else {
                    lookText += targetActor.actorId;
                }
                lookText += ".";

                AdventureStartSpeech(
                        state,
                        SpeechAnchorType::Player,
                        -1,
                        -1,
                        {},
                        lookText,
                        talkColor,
                        -1);
            }
        }
        return;
    }
}

static void UpdateHoverUi(GameState& state)
{
    state.adventure.hoverUi.active = false;
    state.adventure.hoverUi.displayName.clear();

    if (IsDialogueUiActive(state)) {
        return;
    }

    if (!state.adventure.currentScene.loaded) {
        return;
    }

    const ActorInventoryData* inv = GetControlledActorInventory(state);
    const bool holdingItem = (inv != nullptr && !inv->heldItemId.empty());

    const Vector2 mouseScreen = GetMousePosition();
    const Vector2 mouseWorld{
            mouseScreen.x + state.adventure.camera.position.x,
            mouseScreen.y + state.adventure.camera.position.y
    };

    const SceneData& scene = state.adventure.currentScene;

    const int controlledActorIndex = GetControlledActorIndex(state);

    int bestActorIndex = -1;
    float bestSortY = -1000000.0f;

    for (int i = 0; i < static_cast<int>(state.adventure.actors.size()); ++i) {
        if (!holdingItem && i == controlledActorIndex) {
            continue;
        }

        const ActorInstance& actor = state.adventure.actors[i];
        if (!actor.activeInScene || !actor.visible) {
            continue;
        }

        const Rectangle actorRect = GetActorInteractionRect(state, actor);
        if (!CheckCollisionPointRec(mouseScreen, actorRect)) {
            continue;
        }

        if (bestActorIndex < 0 || actor.feetPos.y > bestSortY) {
            bestActorIndex = i;
            bestSortY = actor.feetPos.y;
        }
    }

    if (bestActorIndex >= 0) {
        const ActorInstance& actor = state.adventure.actors[bestActorIndex];
        const ActorDefinitionData* actorDef =
                FindActorDefinitionByIndex(state, actor.actorDefIndex);

        state.adventure.hoverUi.active = true;

        if (actorDef != nullptr && !actorDef->displayName.empty()) {
            state.adventure.hoverUi.displayName = actorDef->displayName;
        } else {
            state.adventure.hoverUi.displayName = actor.actorId;
        }
        return;
    }

    for (int i = static_cast<int>(scene.exits.size()) - 1; i >= 0; --i) {
        if (PointInPolygon(mouseWorld, scene.exits[i].shape)) {
            state.adventure.hoverUi.active = true;
            state.adventure.hoverUi.displayName = scene.exits[i].displayName;
            return;
        }
    }

    for (int i = static_cast<int>(scene.hotspots.size()) - 1; i >= 0; --i) {
        if (PointInPolygon(mouseWorld, scene.hotspots[i].shape)) {
            state.adventure.hoverUi.active = true;
            state.adventure.hoverUi.displayName = scene.hotspots[i].displayName;
            return;
        }
    }
}

static void UpdateProps(GameState& state, float dt)
{
    const auto& sceneProps = state.adventure.currentScene.props;
    auto& props = state.adventure.props;

    const int count = std::min(
            static_cast<int>(sceneProps.size()),
            static_cast<int>(props.size()));

    for (int i = 0; i < count; ++i) {
        const ScenePropData& sceneProp = sceneProps[i];
        PropInstance& prop = props[i];

        if (prop.moveActive) {
            prop.moveElapsedMs += dt * 1000.0f;

            float t = 1.0f;
            if (prop.moveDurationMs > 0.0f) {
                t = prop.moveElapsedMs / prop.moveDurationMs;
            }

            if (t >= 1.0f) {
                prop.feetPos = prop.moveTargetPos;
                prop.moveActive = false;
                prop.moveElapsedMs = 0.0f;
                prop.moveDurationMs = 0.0f;
            } else {
                const float easedT = ApplyInterpolation(prop.moveInterpolation, t);
                prop.feetPos.x = prop.moveStartPos.x + (prop.moveTargetPos.x - prop.moveStartPos.x) * easedT;
                prop.feetPos.y = prop.moveStartPos.y + (prop.moveTargetPos.y - prop.moveStartPos.y) * easedT;
            }
        }

        if (!prop.visible) {
            continue;
        }

        if (sceneProp.visualType != ScenePropVisualType::Sprite) {
            continue;
        }

        prop.animationTimeMs += dt * 1000.0f;

        if (prop.oneShotActive && prop.animationTimeMs >= prop.oneShotDurationMs) {
            prop.oneShotActive = false;
            prop.currentAnimation = sceneProp.defaultAnimation;
            prop.animationTimeMs = 0.0f;
        }
    }
}

static int FindHoveredActorTarget(
        const GameState& state,
        Vector2 mouseScreen,
        bool includeControlledActor)
{
    const int controlledActorIndex = GetControlledActorIndex(state);

    int bestActorIndex = -1;
    float bestSortY = -1000000.0f;

    for (int i = 0; i < static_cast<int>(state.adventure.actors.size()); ++i) {
        if (!includeControlledActor && i == controlledActorIndex) {
            continue;
        }

        const ActorInstance& actor = state.adventure.actors[i];
        if (!actor.activeInScene || !actor.visible) {
            continue;
        }

        const Rectangle actorRect = GetActorInteractionRect(state, actor);
        if (!CheckCollisionPointRec(mouseScreen, actorRect)) {
            continue;
        }

        if (bestActorIndex < 0 || actor.feetPos.y > bestSortY) {
            bestActorIndex = i;
            bestSortY = actor.feetPos.y;
        }
    }

    return bestActorIndex;
}

static int FindHoveredHotspotTarget(const GameState& state, Vector2 mouseWorld)
{
    const auto& hotspots = state.adventure.currentScene.hotspots;

    for (int i = static_cast<int>(hotspots.size()) - 1; i >= 0; --i) {
        if (PointInPolygon(mouseWorld, hotspots[i].shape)) {
            return i;
        }
    }

    return -1;
}

static int FindHoveredExitTarget(const GameState& state, Vector2 mouseWorld)
{
    const auto& exits = state.adventure.currentScene.exits;

    for (int i = static_cast<int>(exits.size()) - 1; i >= 0; --i) {
        if (PointInPolygon(mouseWorld, exits[i].shape)) {
            return i;
        }
    }

    return -1;
}

static void HandleHeldItemWorldUse(GameState& state)
{
    ActorInventoryData* inv = GetControlledActorInventory(state);
    if (inv == nullptr || inv->heldItemId.empty()) {
        return;
    }

    for (auto& ev : FilterEvents(state.input, true, InputEventType::MouseClick)) {
        if (ev.mouse.button != MOUSE_BUTTON_LEFT) {
            continue;
        }

        const Vector2 mouseScreen = GetMousePosition();
        const Vector2 mouseWorld{
                mouseScreen.x + state.adventure.camera.position.x,
                mouseScreen.y + state.adventure.camera.position.y
        };

        bool success = false;
        ScriptCallResult result = ScriptCallResult::Missing;
        bool hadTarget = false;

        const int actorIndex = FindHoveredActorTarget(state, mouseScreen, true);
        if (actorIndex >= 0) {
            hadTarget = true;

            const ActorInstance& actor = state.adventure.actors[actorIndex];
            const std::string functionName =
                    "Scene_use_item_" + inv->heldItemId + "_on_actor_" + actor.actorId;
            result = ScriptSystemCallBoolHook(state, functionName, success);
        } else {
            const int hotspotIndex = FindHoveredHotspotTarget(state, mouseWorld);
            if (hotspotIndex >= 0) {
                hadTarget = true;

                const SceneHotspot& hotspot = state.adventure.currentScene.hotspots[hotspotIndex];
                const std::string functionName =
                        "Scene_use_item_" + inv->heldItemId + "_on_hotspot_" + hotspot.id;
                result = ScriptSystemCallBoolHook(state, functionName, success);
            } else {
                const int exitIndex = FindHoveredExitTarget(state, mouseWorld);
                if (exitIndex >= 0) {
                    hadTarget = true;

                    const SceneExit& exitObj = state.adventure.currentScene.exits[exitIndex];
                    const std::string functionName =
                            "Scene_use_item_" + inv->heldItemId + "_on_exit_" + exitObj.id;
                    result = ScriptSystemCallBoolHook(state, functionName, success);
                }
            }
        }

        if (!hadTarget) {
            ConsumeEvent(ev);
            return;
        }

        if (result == ScriptCallResult::ImmediateTrue) {
            inv->heldItemId.clear();
        } else if (result == ScriptCallResult::ImmediateFalse) {
            // handled failure, keep held item
        } else if (result == ScriptCallResult::StartedAsync) {
            // item-use scripts are treated as non-consuming unless they explicitly
            // remove the item or otherwise change inventory state themselves.
        } else if (result == ScriptCallResult::Missing) {
            AdventureScriptSay(state, "That won't work.", -1);
        } else if (result == ScriptCallResult::Busy) {
            // keep held item
        } else if (result == ScriptCallResult::Error) {
            // keep held item
        }

        ConsumeEvent(ev);
        return;
    }
}

static void UpdateSpeechSkipInput(GameState& state)
{
    if (!state.adventure.speechUi.active) {
        return;
    }

    if (!state.adventure.speechUi.skippable) {
        return;
    }

    for (auto& ev : FilterEvents(state.input, true, InputEventType::MouseClick)) {
        if (ev.mouse.button != MOUSE_BUTTON_LEFT &&
            ev.mouse.button != MOUSE_BUTTON_RIGHT) {
            continue;
        }

        state.adventure.speechUi = {};
        ConsumeEvent(ev);
        return;
    }
}

void AdventureUpdate(GameState& state, float dt)
{
    AdventureProcessPendingLoads(state);

    if (!state.adventure.currentScene.loaded) {
        return;
    }

    HandleDebugInput(state);
    UpdateInventoryUi(state, dt);
    UpdateDialogueUi(state);
    UpdateSpeechSkipInput(state);

    if (!IsDialogueUiActive(state) && !state.adventure.speechUi.active) {
        HandleHeldItemWorldUse(state);
        QueueAdventureActionsFromInput(state);
        ProcessAdventureActions(state);
    }

    const bool arrivedThisFrame = UpdateControlledActorMovement(state, dt);
    if (arrivedThisFrame) {
        ExecutePendingInteraction(state);
    }

    const int controlledActorIndex = GetControlledActorIndex(state);

    for (int i = 0; i < static_cast<int>(state.adventure.actors.size()); ++i) {
        if (i == controlledActorIndex) {
            continue;
        }

        ActorInstance& actor = state.adventure.actors[i];
        if (!actor.activeInScene || !actor.visible) {
            continue;
        }

        UpdateActorMovement(actor, dt);
    }

    UpdateCamera(state, dt);
    UpdateHoverUi(state);
    UpdateInventoryHoverUi(state);

    for (ActorInstance& actor : state.adventure.actors) {
        if (!actor.activeInScene || !actor.visible) {
            continue;
        }

        if (actor.scriptAnimationActive) {
            actor.animationTimeMs += dt * 1000.0f;

            if (actor.animationTimeMs >= actor.scriptAnimationDurationMs) {
                actor.scriptAnimationActive = false;
                AdventureSwitchToDirectionalIdle(actor);
                actor.inIdleState = true;
                actor.stoppedTimeMs = actor.idleDelayMs;
            }
        } else if (actor.path.active) {
            const float speedMultiplier = actor.path.fastMove ? actor.fastMoveMultiplier : 1.0f;
            actor.animationTimeMs += dt * 1000.0f * speedMultiplier;
        } else if (actor.inIdleState) {
            actor.animationTimeMs += dt * 1000.0f;
        }
    }

    if (state.adventure.speechUi.active) {
        state.adventure.speechUi.timerMs += dt * 1000.0f;
        if (state.adventure.speechUi.timerMs >= state.adventure.speechUi.durationMs) {
            state.adventure.speechUi = {};
        }
    }
    UpdateInventoryPickupPopup(state, dt);
    UpdateProps(state, dt);
    ScriptSystemUpdate(state, dt);
}