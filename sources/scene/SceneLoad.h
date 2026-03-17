#pragma once

#include "data/GameState.h"

enum class SceneLoadMode {
    Normal,
    FromSave
};

bool LoadSceneById(GameState& state, const char* sceneId, SceneLoadMode loadMode = SceneLoadMode::Normal);
void UnloadCurrentScene(GameState& state);

bool IsAsyncSceneLoadActive(const GameState& state);
bool BeginAsyncSceneLoad(GameState& state, const char* sceneId, const char* spawnId);
void CancelAsyncSceneLoad(GameState& state);
void PumpAsyncSceneLoad(GameState& state);
