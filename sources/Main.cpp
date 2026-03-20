#include <raylib.h>
#include "data/GameState.h"
#include "menu/Menu.h"
#include "settings/Settings.h"
#include "input/Input.h"
#include "adventure/AdventureUpdate.h"
#include "render/SceneRender.h"
#include "render/UiRender.h"
#include "render/DebugRender.h"
#include "adventure/ItemDefinitionAsset.h"
#include "debug/DebugConsole.h"
#include "debug/DebugConsoleTraceLog.h"
#include "resources/Resources.h"
#include "scripting/ScriptSystem.h"
#include "adventure/DialogueChoiceAsset.h"
#include "audio/Audio.h"
#include "ui/Cursor.h"
#include "render/EffectShaderRegistry.h"

static bool IsMouseInInternalView()
{
    const Vector2 m = GetMousePosition();
    return m.x >= 0.0f && m.y >= 0.0f &&
           m.x < static_cast<float>(INTERNAL_WIDTH) &&
           m.y < static_cast<float>(INTERNAL_HEIGHT);
}

static Rectangle GetFullscreenSrcRect(const Texture2D& tex)
{
    return Rectangle{
            0.5f,
            0.5f,
            (float)tex.width  - 1.0f,
            -(float)tex.height + 1.0f
    };
}

static void BlitRenderTarget(
        const RenderTexture2D& source,
        RenderTexture2D& dest)
{
    BeginTextureMode(dest);
    ClearBackground(BLACK);
    DrawTexturePro(
            source.texture,
            GetFullscreenSrcRect(source.texture),
            Rectangle{0.0f, 0.0f, static_cast<float>(dest.texture.width), static_cast<float>(dest.texture.height)},
            Vector2{0.0f, 0.0f},
            0.0f,
            WHITE);
    EndTextureMode();
}

static void ProcessGameModeInput(GameState& state) {
    for (auto& ev : FilterEvents(state.input, true, InputEventType::KeyPressed)) {
        if(ev.key.key == KEY_ESCAPE) {
            if(state.mode == GameMode::Game) {
                state.mode = GameMode::Menu;
                TraceLog(LOG_DEBUG, "Opening menu");
                ConsumeEvent(ev);
            }
        }
    }
}

int main()
{
    SetConfigFlags(FLAG_VSYNC_HINT);
    InstallDebugConsoleTraceLogHook();
    InitWindow(1920, 1080, "Adventure");
    SetExitKey(0);

    if (!InitEffectShaderRegistry()) {
        TraceLog(LOG_WARNING, "One or more effect shaders failed to load");
    }

    RenderTexture2D worldTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(worldTarget.texture, TEXTURE_FILTER_BILINEAR);

    RenderTexture2D sceneTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(sceneTarget.texture, TEXTURE_FILTER_BILINEAR);

    RenderTexture2D sceneSampleTempTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(sceneSampleTempTarget.texture, TEXTURE_FILTER_BILINEAR);

    GameState state;

    InitSettings(state.settings, "settings.json");
    ApplySettings(state.settings);

    InitInput(state.input);
    InitAudio(state);
    InitCursor(state);

    LoadAllItemDefinitions(state);
    LoadAllDialogueChoiceSets(state);

    MenuInit(&state);
    DebugConsoleInit(state);
    FlushPendingDebugConsoleTraceLog(state);

    while (!WindowShouldClose())
    {
        if(state.mode == GameMode::Quit) break;

        const float dt = GetFrameTime();
        const int screenW = GetScreenWidth();
        const int screenH = GetScreenHeight();
        const Rectangle dst = { 0, 0 , (float) screenW, (float) screenH};

        SetMouseOffset(-static_cast<int>(dst.x), -static_cast<int>(dst.y));
        SetMouseScale(
                static_cast<float>(INTERNAL_WIDTH) / dst.width,
                static_cast<float>(INTERNAL_HEIGHT) / dst.height
        );

        UpdateInput(state.input);
        UpdateCursor(state);

        FlushPendingDebugConsoleTraceLog(state);

        ProcessGameModeInput(state);
        UpdateDebugConsole(state, dt);
        MenuUpdate(dt);

        if(state.mode == GameMode::Menu) MenuHandleInput(state);

        if (state.mode == GameMode::Menu || state.adventure.hasPendingSceneLoad) {
            AdventureProcessPendingLoads(state);
        }

        if (state.mode == GameMode::Game) {
            AdventureUpdate(state, dt);
        }

        UpdateAudio(state, dt);


        if (state.mode == GameMode::Game) {
            BeginTextureMode(worldTarget);
            ClearBackground(BLACK);
            RenderAdventureScene(state);
            EndTextureMode();

            BlitRenderTarget(worldTarget, sceneTarget);

            RenderTexture2D* currentSource = &sceneTarget;
            RenderTexture2D* currentDest = &sceneSampleTempTarget;

            const int effectRegionCount = std::min(
                    static_cast<int>(state.adventure.currentScene.effectRegions.size()),
                    static_cast<int>(state.adventure.effectRegions.size()));

            for (int i = 0; i < effectRegionCount; ++i) {
                const EffectRegionInstance& effect = state.adventure.effectRegions[i];
                if (!effect.visible) {
                    continue;
                }

                if (GetEffectShaderCategory(effect.shaderType) != SceneEffectShaderCategory::SceneSample) {
                    continue;
                }

                if (ApplySceneSampleEffectRegionPass(state, i, *currentSource, *currentDest)) {
                    std::swap(currentSource, currentDest);
                }
            }

            if (currentSource != &sceneTarget) {
                BlitRenderTarget(*currentSource, sceneTarget);
            }

            BeginTextureMode(sceneTarget);
            RenderAdventureUi(state);
            RenderAdventureDebug(state);
            EndTextureMode();
        } else {
            BeginTextureMode(sceneTarget);
            ClearBackground(BLACK);
            EndTextureMode();
        }

        BeginTextureMode(sceneTarget);

        if (state.mode == GameMode::Menu) {
            MenuRenderUi(state);
        }

        MenuRenderOverlay();
        RenderDebugConsole(state);

        EndTextureMode();



        // render sceneTarget to screen
        BeginDrawing();
        ClearBackground(BLACK);

        // blit 1080p to actual screen size. Settings menu make sure there are only resolutions with the same aspect ratio (eg 1080p 1440p and 4k)
        Rectangle src = GetFullscreenSrcRect(sceneTarget.texture);
        DrawTexturePro(sceneTarget.texture, src, dst, {0,0}, 0.0f, WHITE);

        float scale = dst.width / INTERNAL_WIDTH;
        RenderCursor(state, scale);

        if(state.settings.showFPS) DrawFPS(10, 10);
        EndDrawing();
    }

    FlushPendingDebugConsoleTraceLog(state);

    ScriptSystemShutdown(state.script);
    DebugConsoleShutdown();
    ShutdownAudio(state);
    UnloadAllResources(state.resources);
    UnloadRenderTexture(worldTarget);
    UnloadRenderTexture(sceneTarget);
    UnloadRenderTexture(sceneSampleTempTarget);
    ShutdownEffectShaderRegistry();
    ShutdownCursor(state);
    CloseWindow();

    return 0;
}
