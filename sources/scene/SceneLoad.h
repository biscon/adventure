#pragma once

#include "data/GameState.h"

bool LoadSceneById(GameState& state, const char* sceneId);
void UnloadCurrentScene(GameState& state);
