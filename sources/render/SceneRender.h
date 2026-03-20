#pragma once

#include "data/GameState.h"

void RenderAdventureScene(const GameState& state);

bool ApplySceneSampleEffectRegionPass(
        const GameState& state,
        int effectRegionIndex,
        const RenderTexture2D& sourceTarget,
        RenderTexture2D& destTarget);