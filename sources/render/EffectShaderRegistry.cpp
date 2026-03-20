#include "render/EffectShaderRegistry.h"

#include <string>

namespace
{
    static EffectShaderEntry gEffectShaders[] = {
            { SceneEffectShaderType::UvScroll,    SceneEffectShaderCategory::SelfTexture, {} , false, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
            { SceneEffectShaderType::HeatShimmer, SceneEffectShaderCategory::SceneSample, {} , false, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
            { SceneEffectShaderType::RegionGrade, SceneEffectShaderCategory::SceneSample, {} , false, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
            { SceneEffectShaderType::WaterRipple, SceneEffectShaderCategory::SceneSample, {}, false, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }
    };

    static constexpr int gEffectShaderCount =
            static_cast<int>(sizeof(gEffectShaders) / sizeof(gEffectShaders[0]));

    static const char* GetShaderFragmentPath(SceneEffectShaderType type)
    {
        switch (type) {
            case SceneEffectShaderType::UvScroll:
                return ASSETS_PATH "shaders/selftexture/uv_scroll.fs";

            case SceneEffectShaderType::HeatShimmer:
                return ASSETS_PATH "shaders/scenesample/heat_shimmer.fs";

            case SceneEffectShaderType::RegionGrade:
                return ASSETS_PATH "shaders/scenesample/region_grade.fs";

            case SceneEffectShaderType::WaterRipple:
                return ASSETS_PATH "shaders/scenesample/water_ripple.fs";

            case SceneEffectShaderType::None:
            default:
                return nullptr;
        }
    }

    static void CacheUniformLocations(EffectShaderEntry& entry)
    {
        entry.timeLoc = GetShaderLocation(entry.shader, "uTime");
        entry.scrollSpeedLoc = GetShaderLocation(entry.shader, "uScrollSpeed");
        entry.uvScaleLoc = GetShaderLocation(entry.shader, "uUvScale");
        entry.distortionAmountLoc = GetShaderLocation(entry.shader, "uDistortionAmount");
        entry.noiseScrollSpeedLoc = GetShaderLocation(entry.shader, "uNoiseScrollSpeed");
        entry.intensityLoc = GetShaderLocation(entry.shader, "uIntensity");
        entry.phaseOffsetLoc = GetShaderLocation(entry.shader, "uPhaseOffset");
        entry.sceneSizeLoc = GetShaderLocation(entry.shader, "uSceneSize");
        entry.regionPosLoc = GetShaderLocation(entry.shader, "uRegionPos");
        entry.regionSizeLoc = GetShaderLocation(entry.shader, "uRegionSize");
        entry.brightnessLoc = GetShaderLocation(entry.shader, "uBrightness");
        entry.contrastLoc = GetShaderLocation(entry.shader, "uContrast");
        entry.saturationLoc = GetShaderLocation(entry.shader, "uSaturation");
        entry.tintLoc = GetShaderLocation(entry.shader, "uTint");
        entry.softnessLoc = GetShaderLocation(entry.shader, "uSoftness");
    }
}

SceneEffectShaderCategory GetEffectShaderCategory(SceneEffectShaderType type)
{
    switch (type) {
        case SceneEffectShaderType::UvScroll:
            return SceneEffectShaderCategory::SelfTexture;
        case SceneEffectShaderType::HeatShimmer:
        case SceneEffectShaderType::RegionGrade:
            return SceneEffectShaderCategory::SceneSample;
        case SceneEffectShaderType::WaterRipple:
            return SceneEffectShaderCategory::SceneSample;
        case SceneEffectShaderType::None:
        default:
            return SceneEffectShaderCategory::None;
    }
}

const char* SceneEffectShaderTypeToString(SceneEffectShaderType type)
{
    switch (type) {
        case SceneEffectShaderType::UvScroll:
            return "uv_scroll";

        case SceneEffectShaderType::HeatShimmer:
            return "heat_shimmer";

        case SceneEffectShaderType::RegionGrade:
            return "region_grade";

        case SceneEffectShaderType::WaterRipple:
            return "water_ripple";

        case SceneEffectShaderType::None:
        default:
            return "none";
    }
}

bool InitEffectShaderRegistry()
{
    bool allOk = true;

    for (int i = 0; i < gEffectShaderCount; ++i) {
        EffectShaderEntry& entry = gEffectShaders[i];

        const char* fragmentPath = GetShaderFragmentPath(entry.type);
        if (fragmentPath == nullptr) {
            continue;
        }

        entry.shader = LoadShader(nullptr, fragmentPath);

        if (entry.shader.id == 0) {
            TraceLog(LOG_ERROR,
                     "Failed to load effect shader '%s' from '%s'",
                     SceneEffectShaderTypeToString(entry.type),
                     fragmentPath);
            allOk = false;
            continue;
        }

        entry.loaded = true;
        CacheUniformLocations(entry);

        TraceLog(LOG_INFO,
                 "Loaded effect shader '%s' (%s)",
                 SceneEffectShaderTypeToString(entry.type),
                 fragmentPath);
    }

    return allOk;
}

void ShutdownEffectShaderRegistry()
{
    for (int i = 0; i < gEffectShaderCount; ++i) {
        EffectShaderEntry& entry = gEffectShaders[i];
        if (!entry.loaded) {
            continue;
        }

        UnloadShader(entry.shader);
        entry.shader = {};
        entry.loaded = false;

        entry.timeLoc = -1;
        entry.scrollSpeedLoc = -1;
        entry.uvScaleLoc = -1;
        entry.distortionAmountLoc = -1;
        entry.noiseScrollSpeedLoc = -1;
        entry.intensityLoc = -1;
        entry.phaseOffsetLoc = -1;
        entry.sceneSizeLoc = -1;
        entry.regionPosLoc = -1;
        entry.regionSizeLoc = -1;
        entry.brightnessLoc = -1;
        entry.contrastLoc = -1;
        entry.saturationLoc = -1;
        entry.tintLoc = -1;
        entry.softnessLoc = -1;
    }
}

const EffectShaderEntry* FindEffectShaderEntry(SceneEffectShaderType type)
{
    if (type == SceneEffectShaderType::None) {
        return nullptr;
    }

    for (int i = 0; i < gEffectShaderCount; ++i) {
        if (gEffectShaders[i].type == type) {
            return gEffectShaders[i].loaded ? &gEffectShaders[i] : nullptr;
        }
    }

    return nullptr;
}
