#pragma once

#include "raylib.h"
#include "scene/SceneData.h"

struct EffectShaderEntry {
    SceneEffectShaderType type = SceneEffectShaderType::None;
    SceneEffectShaderCategory category = SceneEffectShaderCategory::None;

    Shader shader{};
    bool loaded = false;

    int timeLoc = -1;
    int scrollSpeedLoc = -1;
    int uvScaleLoc = -1;
    int distortionAmountLoc = -1;
    int noiseScrollSpeedLoc = -1;
    int intensityLoc = -1;
    int phaseOffsetLoc = -1;

    int sceneSizeLoc = -1;
    int regionPosLoc = -1;
    int regionSizeLoc = -1;

    int brightnessLoc = -1;
    int contrastLoc = -1;
    int saturationLoc = -1;
    int tintLoc = -1;
    int softnessLoc = -1;

    int usePolygonLoc = -1;
    int polygonVertexCountLoc = -1;
    int polygonPointsLoc = -1;
};

bool InitEffectShaderRegistry();
void ShutdownEffectShaderRegistry();

const EffectShaderEntry* FindEffectShaderEntry(SceneEffectShaderType type);

SceneEffectShaderCategory GetEffectShaderCategory(SceneEffectShaderType type);
const char* SceneEffectShaderTypeToString(SceneEffectShaderType type);
