#pragma once

#include "data/GameState.h"

void InitAudio(GameState& state);
void ShutdownAudio(GameState& state);
void UpdateAudio(GameState& state, float dt);

bool PlaySoundById(GameState& state, const std::string& id);
bool PlayMusicById(GameState& state, const std::string& id);
void StopMusic(GameState& state, float fadeMs = 0.0f);
