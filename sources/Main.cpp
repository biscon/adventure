#include <raylib.h>
#include "data/GameState.h"
#include "menu/Menu.h"
#include "settings/Settings.h"
#include "input/Input.h"
#include "adventure/Adventure.h"
#include "render/SceneRender.h"
#include "render/UiRender.h"
#include "render/DebugRender.h"
#include "adventure/ItemDefinitionAsset.h"
#include "debug/DebugConsole.h"
#include "resources/Resources.h"
#include "scripting/ScriptSystem.h"
#include "adventure/DialogueChoiceAsset.h"

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
    InitWindow(1920, 1080, "Adventure");
    //SetTargetFPS(60);
    SetExitKey(0);

    RenderTexture2D sceneTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(sceneTarget.texture, TEXTURE_FILTER_BILINEAR);

    GameState state;

    InitSettings(state.settings, "settings.json");
    ApplySettings(state.settings);

    InitInput(state.input);
    LoadAllItemDefinitions(state);
    LoadAllDialogueChoiceSets(state);

    MenuInit(&state);
    DebugConsoleInit(state);

    while (!WindowShouldClose())
    {
        UpdateInput(state.input);
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

        ProcessGameModeInput(state);
        UpdateDebugConsole(state, dt);

        if(state.mode == GameMode::Menu) MenuHandleInput(state);

        if (state.mode == GameMode::Menu || state.adventure.hasPendingSceneLoad) {
            AdventureProcessPendingLoads(state);
        }

        if (state.mode == GameMode::Game) {
            AdventureUpdate(state, dt);
        }


        BeginTextureMode(sceneTarget);
        ClearBackground(BLACK);
        if (state.mode == GameMode::Game) {
            RenderAdventureScene(state);
            RenderAdventureUi(state);
            RenderAdventureDebug(state);
        }

        if(state.mode == GameMode::Menu) MenuRenderUi(state);

        RenderDebugConsole(state);

        EndTextureMode();

        // render sceneTarget to screen
        BeginDrawing();
        ClearBackground(BLACK);

        // blit 1080p to actual screen size. Settings menu make sure there are only resolutions with the same aspect ratio (eg 1080p 1440p and 4k)
        Rectangle src = GetFullscreenSrcRect(sceneTarget.texture);
        DrawTexturePro(sceneTarget.texture, src, dst, {0,0}, 0.0f, WHITE);

        DrawFPS(10, 10);
        EndDrawing();
    }

    ScriptSystemShutdown(state.script);
    DebugConsoleShutdown();
    UnloadAllResources(state.resources);
    UnloadRenderTexture(sceneTarget);
    CloseWindow();

    return 0;
}
