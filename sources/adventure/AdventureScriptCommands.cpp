#include "adventure/Adventure.h"
#include "adventure/AdventureScriptInternal.h"
#include "adventure/AdventureActorHelpers.h"
#include "raylib.h"
#include "raymath.h"

bool AdventureScriptSay(GameState& state, const std::string& text, int durationMs)
{
    const ActorInstance* controlledActor = GetControlledActor(state);
    if (!state.adventure.currentScene.loaded || controlledActor == nullptr) {
        return false;
    }

    const ActorDefinitionData* actorDef =
            FindActorDefinitionByIndex(state, controlledActor->actorDefIndex);
    const Color talkColor = (actorDef != nullptr) ? actorDef->talkColor : WHITE;

    AdventureStartSpeech(
            state,
            SpeechAnchorType::Player,
            -1,
            -1,
            {},
            text,
            talkColor,
            durationMs);
    return true;
}

bool AdventureScriptSayProp(GameState& state,
                            const std::string& propId,
                            const std::string& text,
                            const Color* overrideColor,
                            int durationMs)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.currentScene.props.size()) ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    if (!state.adventure.props[scenePropIndex].visible) {
        return false;
    }

    Color color = WHITE;
    if (overrideColor != nullptr) {
        color = *overrideColor;
    }

    AdventureStartSpeech(
            state,
            SpeechAnchorType::Prop,
            -1,
            scenePropIndex,
            {},
            text,
            color,
            durationMs);
    return true;
}

bool AdventureScriptSayAt(GameState& state,
                          Vector2 worldPos,
                          const std::string& text,
                          Color color,
                          int durationMs)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    AdventureStartSpeech(
            state,
            SpeechAnchorType::Position,
            -1,
            -1,
            worldPos,
            text,
            color,
            durationMs);
    return true;
}

bool AdventureScriptWalkTo(GameState& state, Vector2 worldPos)
{
    ActorInstance* controlledActor = GetControlledActor(state);
    if (!state.adventure.currentScene.loaded || controlledActor == nullptr) {
        return false;
    }

    state.adventure.actionQueue.push({
                                             AdventureActionType::WalkToPoint,
                                             WalkToPointAction{worldPos, worldPos, false}
                                     });
    return true;
}

bool AdventureScriptWalkToHotspot(GameState& state, const std::string& hotspotId)
{
    ActorInstance* controlledActor = GetControlledActor(state);
    if (!state.adventure.currentScene.loaded || controlledActor == nullptr) {
        return false;
    }

    const auto& hotspots = state.adventure.currentScene.hotspots;
    for (int i = 0; i < static_cast<int>(hotspots.size()); ++i) {
        if (hotspots[i].id == hotspotId) {
            state.adventure.actionQueue.push({
                                                     AdventureActionType::WalkToPoint,
                                                     WalkToPointAction{hotspots[i].walkTo, hotspots[i].walkTo, false}
                                             });
            return true;
        }
    }

    return false;
}

bool AdventureScriptWalkToExit(GameState& state, const std::string& exitId)
{
    ActorInstance* controlledActor = GetControlledActor(state);
    if (!state.adventure.currentScene.loaded || controlledActor == nullptr) {
        return false;
    }

    const auto& exits = state.adventure.currentScene.exits;
    for (int i = 0; i < static_cast<int>(exits.size()); ++i) {
        if (exits[i].id == exitId) {
            state.adventure.actionQueue.push({
                                                     AdventureActionType::WalkToPoint,
                                                     WalkToPointAction{exits[i].walkTo, exits[i].walkTo, false}
                                             });
            return true;
        }
    }

    return false;
}

bool AdventureScriptFace(GameState& state, const std::string& facingName)
{
    ActorInstance* controlledActor = GetControlledActor(state);
    if (!state.adventure.currentScene.loaded || controlledActor == nullptr) {
        return false;
    }

    SceneFacing facing = SceneFacing::Front;

    if (facingName == "left") {
        facing = SceneFacing::Left;
    } else if (facingName == "right") {
        facing = SceneFacing::Right;
    } else if (facingName == "back") {
        facing = SceneFacing::Back;
    } else if (facingName == "front") {
        facing = SceneFacing::Front;
    } else {
        return false;
    }

    controlledActor->path = {};
    state.adventure.pendingInteraction = {};
    AdventureForceFacing(*controlledActor, facing);
    return true;
}

bool AdventureScriptChangeScene(GameState& state, const std::string& sceneId, const std::string& spawnId)
{
    if (sceneId.empty()) {
        return false;
    }

    AdventureQueueLoadScene(
            state,
            sceneId.c_str(),
            spawnId.empty() ? nullptr : spawnId.c_str());

    return true;
}

bool AdventureScriptPlayAnimation(GameState& state, const std::string& animationName)
{
    ActorInstance* controlledActor = GetControlledActor(state);
    if (!state.adventure.currentScene.loaded || controlledActor == nullptr) {
        return false;
    }
    ActorInstance& player = *controlledActor;

    float durationMs = 0.0f;
    const ActorDefinitionData* actorDef =
            FindActorDefinitionByIndex(state, player.actorDefIndex);
    if (actorDef == nullptr) {
        return false;
    }

    if (!AdventureTryGetSpriteAnimationDurationMs(state, actorDef->spriteAssetHandle, animationName, durationMs)) {
        TraceLog(LOG_WARNING, "Animation not found or has no duration: %s", animationName.c_str());
        return false;
    }

    player.path = {};
    state.adventure.pendingInteraction = {};

    player.currentAnimation = animationName;
    player.animationTimeMs = 0.0f;
    player.scriptAnimationActive = true;
    player.scriptAnimationDurationMs = durationMs;
    player.inIdleState = false;
    player.stoppedTimeMs = 0.0f;

    return true;
}

bool AdventureScriptPlayPropAnimation(GameState& state, const std::string& propId, const std::string& animationName)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.currentScene.props.size()) ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    const ScenePropData& sceneProp = state.adventure.currentScene.props[scenePropIndex];
    PropInstance& prop = state.adventure.props[scenePropIndex];

    if (sceneProp.visualType != ScenePropVisualType::Sprite) {
        return false;
    }

    float durationMs = 0.0f;
    if (!AdventureTryGetSpriteAnimationDurationMs(state, sceneProp.spriteAssetHandle, animationName, durationMs)) {
        TraceLog(LOG_WARNING,
                 "Prop animation not found or has no duration: prop=%s anim=%s",
                 propId.c_str(),
                 animationName.c_str());
        return false;
    }

    prop.currentAnimation = animationName;
    prop.animationTimeMs = 0.0f;
    prop.oneShotActive = true;
    prop.oneShotDurationMs = durationMs;
    prop.visible = true;

    return true;
}

bool AdventureScriptSetPropAnimation(GameState& state, const std::string& propId, const std::string& animationName)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.currentScene.props.size()) ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    const ScenePropData& sceneProp = state.adventure.currentScene.props[scenePropIndex];
    PropInstance& prop = state.adventure.props[scenePropIndex];

    if (sceneProp.visualType != ScenePropVisualType::Sprite) {
        return false;
    }

    float durationMs = 0.0f;
    if (!AdventureTryGetSpriteAnimationDurationMs(state, sceneProp.spriteAssetHandle, animationName, durationMs)) {
        TraceLog(LOG_WARNING,
                 "Prop animation not found or has no duration: prop=%s anim=%s",
                 propId.c_str(),
                 animationName.c_str());
        return false;
    }

    prop.currentAnimation = animationName;
    prop.animationTimeMs = 0.0f;
    prop.oneShotActive = false;
    prop.oneShotDurationMs = 0.0f;
    prop.visible = true;

    return true;
}

bool AdventureScriptSetPropPosition(GameState& state, const std::string& propId, Vector2 worldPos)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.currentScene.props.size()) ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    PropInstance& prop = state.adventure.props[scenePropIndex];
    prop.feetPos = worldPos;
    prop.moveActive = false;
    prop.moveStartPos = worldPos;
    prop.moveTargetPos = worldPos;
    prop.moveElapsedMs = 0.0f;
    prop.moveDurationMs = 0.0f;
    return true;
}

bool AdventureScriptMovePropTo(GameState& state,
                               const std::string& propId,
                               Vector2 targetPos,
                               float durationMs,
                               const std::string& interpolationName)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.currentScene.props.size()) ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    PropMoveInterpolation interpolation = PropMoveInterpolation::Linear;
    if (!AdventureParsePropMoveInterpolation(interpolationName, interpolation)) {
        return false;
    }

    PropInstance& prop = state.adventure.props[scenePropIndex];

    if (durationMs <= 0.0f) {
        prop.feetPos = targetPos;
        prop.moveActive = false;
        prop.moveStartPos = targetPos;
        prop.moveTargetPos = targetPos;
        prop.moveElapsedMs = 0.0f;
        prop.moveDurationMs = 0.0f;
        prop.moveInterpolation = interpolation;
        return true;
    }

    prop.moveActive = true;
    prop.moveStartPos = prop.feetPos;
    prop.moveTargetPos = targetPos;
    prop.moveElapsedMs = 0.0f;
    prop.moveDurationMs = durationMs;
    prop.moveInterpolation = interpolation;
    return true;
}

bool AdventureScriptSetPropVisible(GameState& state, const std::string& propId, bool visible)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    state.adventure.props[scenePropIndex].visible = visible;
    return true;
}

bool AdventureScriptSetPropFlipX(GameState& state, const std::string& propId, bool flipX)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    state.adventure.props[scenePropIndex].flipX = flipX;
    return true;
}

bool AdventureScriptSetPropPositionRelative(GameState& state, const std::string& propId, Vector2 delta)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    PropInstance& prop = state.adventure.props[scenePropIndex];
    const Vector2 newPos{
            prop.feetPos.x + delta.x,
            prop.feetPos.y + delta.y
    };

    prop.feetPos = newPos;
    prop.moveActive = false;
    prop.moveStartPos = newPos;
    prop.moveTargetPos = newPos;
    prop.moveElapsedMs = 0.0f;
    prop.moveDurationMs = 0.0f;
    return true;
}

bool AdventureScriptMovePropBy(GameState& state,
                               const std::string& propId,
                               Vector2 delta,
                               float durationMs,
                               const std::string& interpolationName)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    PropInstance& prop = state.adventure.props[scenePropIndex];
    const Vector2 targetPos{
            prop.feetPos.x + delta.x,
            prop.feetPos.y + delta.y
    };

    return AdventureScriptMovePropTo(
            state,
            propId,
            targetPos,
            durationMs,
            interpolationName);
}

bool AdventureScriptControlActor(GameState& state, const std::string& actorId)
{
    const int actorIndex = FindActorInstanceIndexById(state, actorId);
    if (actorIndex < 0 || actorIndex >= static_cast<int>(state.adventure.actors.size())) {
        return false;
    }

    ActorInstance& actor = state.adventure.actors[actorIndex];
    if (!actor.activeInScene || !actor.visible) {
        return false;
    }

    const ActorDefinitionData* actorDef = FindActorDefinitionByIndex(state, actor.actorDefIndex);
    if (actorDef == nullptr || !actorDef->controllable) {
        return false;
    }

    state.adventure.controlledActorIndex = actorIndex;
    return true;
}

bool AdventureScriptSayActor(GameState& state,
                             const std::string& actorId,
                             const std::string& text,
                             const Color* overrideColor,
                             int durationMs)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    ActorInstance* actor = AdventureFindSceneActorById(state, actorId);
    if (actor == nullptr) {
        return false;
    }

    const ActorDefinitionData* actorDef = AdventureGetActorDefinitionForInstance(state, *actor);

    Color color = WHITE;
    if (overrideColor != nullptr) {
        color = *overrideColor;
    } else if (actorDef != nullptr) {
        color = actorDef->talkColor;
    }

    const int actorIndex = FindActorInstanceIndexById(state, actorId);
    if (actorIndex < 0) {
        return false;
    }

    AdventureStartSpeech(
            state,
            SpeechAnchorType::Actor,
            actorIndex,
            -1,
            {},
            text,
            color,
            durationMs);

    return true;
}

bool AdventureScriptWalkActorTo(GameState& state, const std::string& actorId, Vector2 worldPos, int* outActorIndex)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    int actorIndex = -1;
    ActorInstance* actor = AdventureFindSceneActorById(state, actorId, &actorIndex);
    if (actor == nullptr) {
        return false;
    }

    if (outActorIndex != nullptr) {
        *outActorIndex = actorIndex;
    }

    return AdventureQueueActorPathToPoint(state, *actor, worldPos, false);
}

bool AdventureScriptWalkActorToHotspot(GameState& state, const std::string& actorId, const std::string& hotspotId, int* outActorIndex)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const auto& hotspots = state.adventure.currentScene.hotspots;
    for (const SceneHotspot& hotspot : hotspots) {
        if (hotspot.id == hotspotId) {
            return AdventureScriptWalkActorTo(state, actorId, hotspot.walkTo, outActorIndex);
        }
    }

    return false;
}

bool AdventureScriptWalkActorToExit(GameState& state, const std::string& actorId, const std::string& exitId, int* outActorIndex)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const auto& exits = state.adventure.currentScene.exits;
    for (const SceneExit& exitObj : exits) {
        if (exitObj.id == exitId) {
            return AdventureScriptWalkActorTo(state, actorId, exitObj.walkTo, outActorIndex);
        }
    }

    return false;
}

bool AdventureScriptFaceActor(GameState& state, const std::string& actorId, const std::string& facingName)
{
    ActorInstance* actor = AdventureFindSceneActorById(state, actorId);
    if (actor == nullptr) {
        return false;
    }

    SceneFacing facing = SceneFacing::Front;

    if (facingName == "left") {
        facing = SceneFacing::Left;
    } else if (facingName == "right") {
        facing = SceneFacing::Right;
    } else if (facingName == "back") {
        facing = SceneFacing::Back;
    } else if (facingName == "front") {
        facing = SceneFacing::Front;
    } else {
        return false;
    }

    actor->path = {};
    AdventureForceFacing(*actor, facing);
    return true;
}

bool AdventureScriptPlayActorAnimation(GameState& state, const std::string& actorId, const std::string& animationName)
{
    ActorInstance* actor = AdventureFindSceneActorById(state, actorId);
    if (actor == nullptr) {
        return false;
    }

    const ActorDefinitionData* actorDef = AdventureGetActorDefinitionForInstance(state, *actor);
    if (actorDef == nullptr) {
        return false;
    }

    float durationMs = 0.0f;
    if (!AdventureTryGetSpriteAnimationDurationMs(state, actorDef->spriteAssetHandle, animationName, durationMs)) {
        TraceLog(LOG_WARNING, "Actor animation not found or has no duration: actor=%s anim=%s",
                 actorId.c_str(), animationName.c_str());
        return false;
    }

    actor->path = {};
    actor->currentAnimation = animationName;
    actor->animationTimeMs = 0.0f;
    actor->scriptAnimationActive = true;
    actor->scriptAnimationDurationMs = durationMs;
    actor->inIdleState = false;
    actor->stoppedTimeMs = 0.0f;

    return true;
}

bool AdventureScriptSetActorVisible(GameState& state, const std::string& actorId, bool visible)
{
    const int actorIndex = FindActorInstanceIndexById(state, actorId);
    if (actorIndex < 0 || actorIndex >= static_cast<int>(state.adventure.actors.size())) {
        return false;
    }

    ActorInstance& actor = state.adventure.actors[actorIndex];
    actor.visible = visible;

    if (!visible) {
        actor.path = {};
        actor.scriptAnimationActive = false;
    }

    return true;
}

bool AdventureScriptStartWalkTo(GameState& state, Vector2 worldPos)
{
    return AdventureScriptWalkTo(state, worldPos);
}

bool AdventureScriptStartWalkToHotspot(GameState& state, const std::string& hotspotId)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    ActorInstance* controlledActor = GetControlledActor(state);
    if (controlledActor == nullptr) {
        return false;
    }

    const auto& hotspots = state.adventure.currentScene.hotspots;
    for (const SceneHotspot& hotspot : hotspots) {
        if (hotspot.id == hotspotId) {
            return AdventureQueueActorPathToPoint(state, *controlledActor, hotspot.walkTo, false);
        }
    }

    return false;
}

bool AdventureScriptStartWalkToExit(GameState& state, const std::string& exitId)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    ActorInstance* controlledActor = GetControlledActor(state);
    if (controlledActor == nullptr) {
        return false;
    }

    const auto& exits = state.adventure.currentScene.exits;
    for (const SceneExit& exitObj : exits) {
        if (exitObj.id == exitId) {
            return AdventureQueueActorPathToPoint(state, *controlledActor, exitObj.walkTo, false);
        }
    }

    return false;
}

bool AdventureScriptStartWalkActorTo(GameState& state, const std::string& actorId, Vector2 worldPos)
{
    return AdventureScriptWalkActorTo(state, actorId, worldPos, nullptr);
}

bool AdventureScriptStartWalkActorToHotspot(GameState& state, const std::string& actorId, const std::string& hotspotId)
{
    return AdventureScriptWalkActorToHotspot(state, actorId, hotspotId, nullptr);
}

bool AdventureScriptStartWalkActorToExit(GameState& state, const std::string& actorId, const std::string& exitId)
{
    return AdventureScriptWalkActorToExit(state, actorId, exitId, nullptr);
}

bool AdventureScriptSetControlsEnabled(GameState& state, bool enabled)
{
    state.adventure.controlsEnabled = enabled;
    return true;
}

static int FindEffectSpriteIndexById(const GameState& state, const std::string& effectId)
{
    const int count = std::min(
            static_cast<int>(state.adventure.currentScene.effectSprites.size()),
            static_cast<int>(state.adventure.effectSprites.size()));

    for (int i = 0; i < count; ++i) {
        if (state.adventure.currentScene.effectSprites[i].id == effectId) {
            return i;
        }
    }

    return -1;
}

bool AdventureScriptSetEffectVisible(GameState& state, const std::string& effectId, bool visible)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int effectIndex = FindEffectSpriteIndexById(state, effectId);
    if (effectIndex < 0 || effectIndex >= static_cast<int>(state.adventure.effectSprites.size())) {
        return false;
    }

    state.adventure.effectSprites[effectIndex].visible = visible;
    return true;
}

bool AdventureScriptIsEffectVisible(const GameState& state, const std::string& effectId, bool& outVisible)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int effectIndex = FindEffectSpriteIndexById(state, effectId);
    if (effectIndex < 0 || effectIndex >= static_cast<int>(state.adventure.effectSprites.size())) {
        return false;
    }

    outVisible = state.adventure.effectSprites[effectIndex].visible;
    return true;
}

bool AdventureScriptSetEffectOpacity(GameState& state, const std::string& effectId, float opacity)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int effectIndex = FindEffectSpriteIndexById(state, effectId);
    if (effectIndex < 0 || effectIndex >= static_cast<int>(state.adventure.effectSprites.size())) {
        return false;
    }

    state.adventure.effectSprites[effectIndex].opacity = Clamp(opacity, 0.0f, 1.0f);
    return true;
}
