#pragma once

#include "data/GameState.h"

ActorInstance* FindActorInstanceById(GameState& state, const std::string& actorId);
const ActorInstance* FindActorInstanceById(const GameState& state, const std::string& actorId);

ActorInstance* GetControlledActor(GameState& state);
const ActorInstance* GetControlledActor(const GameState& state);

int FindActorDefinitionIndexById(const GameState& state, const std::string& actorId);
const ActorDefinitionData* FindActorDefinitionById(const GameState& state, const std::string& actorId);

ActorDefinitionData* FindActorDefinitionByIndex(GameState& state, int actorDefIndex);
const ActorDefinitionData* FindActorDefinitionByIndex(const GameState& state, int actorDefIndex);
int FindActorInstanceIndexById(const GameState& state, const std::string& actorId);

int GetControlledActorIndex(const GameState& state);
bool HasControlledActor(const GameState& state);
