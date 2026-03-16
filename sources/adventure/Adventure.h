#pragma once

#include "adventure/AdventureUpdate.h"
#include "data/GameState.h"

// Script commands

bool AdventureScriptSay(GameState& state, const std::string& text, int durationMs = -1);
bool AdventureScriptSayProp(GameState& state,
                            const std::string& propId,
                            const std::string& text,
                            const Color* overrideColor = nullptr,
                            int durationMs = -1);
bool AdventureScriptSayAt(GameState& state,
                          Vector2 worldPos,
                          const std::string& text,
                          Color color = WHITE,
                          int durationMs = -1);

bool AdventureScriptWalkTo(GameState& state, Vector2 worldPos);
bool AdventureScriptWalkToHotspot(GameState& state, const std::string& hotspotId);
bool AdventureScriptWalkToExit(GameState& state, const std::string& exitId);
bool AdventureScriptFace(GameState& state, const std::string& facingName);
bool AdventureScriptChangeScene(GameState& state, const std::string& sceneId, const std::string& spawnId);
bool AdventureScriptPlayAnimation(GameState& state, const std::string& animationName);
bool AdventureScriptPlayPropAnimation(GameState& state, const std::string& propId, const std::string& animationName);
bool AdventureScriptSetPropAnimation(GameState& state, const std::string& propId, const std::string& animationName);
bool AdventureScriptSetPropPosition(GameState& state, const std::string& propId, Vector2 worldPos);
bool AdventureScriptMovePropTo(GameState& state,
                               const std::string& propId,
                               Vector2 targetPos,
                               float durationMs,
                               const std::string& interpolationName);
bool AdventureScriptSetPropVisible(GameState& state, const std::string& propId, bool visible);
bool AdventureScriptSetPropFlipX(GameState& state, const std::string& propId, bool flipX);

bool AdventureScriptSetPropPositionRelative(GameState& state, const std::string& propId, Vector2 delta);
bool AdventureScriptMovePropBy(GameState& state,
                               const std::string& propId,
                               Vector2 delta,
                               float durationMs,
                               const std::string& interpolationName);
bool AdventureScriptControlActor(GameState& state, const std::string& actorId);

bool AdventureScriptSayActor(GameState& state,
                             const std::string& actorId,
                             const std::string& text,
                             const Color* overrideColor = nullptr,
                             int durationMs = -1);

bool AdventureScriptWalkActorTo(GameState& state, const std::string& actorId, Vector2 worldPos, int* outActorIndex = nullptr);
bool AdventureScriptWalkActorToHotspot(GameState& state, const std::string& actorId, const std::string& hotspotId, int* outActorIndex = nullptr);
bool AdventureScriptWalkActorToExit(GameState& state, const std::string& actorId, const std::string& exitId, int* outActorIndex = nullptr);

bool AdventureScriptFaceActor(GameState& state, const std::string& actorId, const std::string& facingName);
bool AdventureScriptPlayActorAnimation(GameState& state, const std::string& actorId, const std::string& animationName);
bool AdventureScriptSetActorVisible(GameState& state, const std::string& actorId, bool visible);
bool AdventureScriptStartWalkTo(GameState& state, Vector2 worldPos);
bool AdventureScriptStartWalkToHotspot(GameState& state, const std::string& hotspotId);
bool AdventureScriptStartWalkToExit(GameState& state, const std::string& exitId);

bool AdventureScriptStartWalkActorTo(GameState& state, const std::string& actorId, Vector2 worldPos);
bool AdventureScriptStartWalkActorToHotspot(GameState& state, const std::string& actorId, const std::string& hotspotId);
bool AdventureScriptStartWalkActorToExit(GameState& state, const std::string& actorId, const std::string& exitId);
bool AdventureScriptSetControlsEnabled(GameState& state, bool enabled);

