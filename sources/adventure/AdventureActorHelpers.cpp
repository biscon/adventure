#include "AdventureActorHelpers.h"

ActorInstance* FindActorInstanceById(GameState& state, const std::string& actorId)
{
    for (ActorInstance& actor : state.adventure.actors) {
        if (actor.actorId == actorId) {
            return &actor;
        }
    }
    return nullptr;
}

const ActorInstance* FindActorInstanceById(const GameState& state, const std::string& actorId)
{
    for (const ActorInstance& actor : state.adventure.actors) {
        if (actor.actorId == actorId) {
            return &actor;
        }
    }
    return nullptr;
}

ActorInstance* GetControlledActor(GameState& state)
{
    const int index = state.adventure.controlledActorIndex;
    if (index < 0 || index >= static_cast<int>(state.adventure.actors.size())) {
        return nullptr;
    }
    return &state.adventure.actors[index];
}

const ActorInstance* GetControlledActor(const GameState& state)
{
    const int index = state.adventure.controlledActorIndex;
    if (index < 0 || index >= static_cast<int>(state.adventure.actors.size())) {
        return nullptr;
    }
    return &state.adventure.actors[index];
}

int FindActorDefinitionIndexById(const GameState& state, const std::string& actorId)
{
    for (int i = 0; i < static_cast<int>(state.adventure.actorDefinitions.size()); ++i) {
        if (state.adventure.actorDefinitions[i].actorId == actorId) {
            return i;
        }
    }
    return -1;
}

const ActorDefinitionData* FindActorDefinitionById(const GameState& state, const std::string& actorId)
{
    const int index = FindActorDefinitionIndexById(state, actorId);
    if (index < 0) {
        return nullptr;
    }
    return &state.adventure.actorDefinitions[index];
}

int FindActorInstanceIndexById(const GameState& state, const std::string& actorId)
{
    for (int i = 0; i < static_cast<int>(state.adventure.actors.size()); ++i) {
        if (state.adventure.actors[i].actorId == actorId) {
            return i;
        }
    }
    return -1;
}

ActorDefinitionData* FindActorDefinitionByIndex(GameState& state, int actorDefIndex)
{
    if (actorDefIndex < 0 ||
        actorDefIndex >= static_cast<int>(state.adventure.actorDefinitions.size())) {
        return nullptr;
    }
    return &state.adventure.actorDefinitions[actorDefIndex];
}

const ActorDefinitionData* FindActorDefinitionByIndex(const GameState& state, int actorDefIndex)
{
    if (actorDefIndex < 0 ||
        actorDefIndex >= static_cast<int>(state.adventure.actorDefinitions.size())) {
        return nullptr;
    }
    return &state.adventure.actorDefinitions[actorDefIndex];
}

int GetControlledActorIndex(const GameState& state)
{
    const int index = state.adventure.controlledActorIndex;
    if (index < 0 || index >= static_cast<int>(state.adventure.actors.size())) {
        return -1;
    }
    return index;
}

bool HasControlledActor(const GameState& state)
{
    return GetControlledActorIndex(state) >= 0;
}
