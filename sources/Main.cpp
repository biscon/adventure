#include <raylib.h>
#include <cmath>
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

static Rectangle GetFullscreenSrcRect(const Texture2D& tex)
{
    return Rectangle{
            0.5f,
            0.5f,
            (float)tex.width  - 1.0f,
            -(float)tex.height + 1.0f
    };
}

static Rectangle BuildPresentationRect(float backbufferWidth, float backbufferHeight,
                                       float drawableWidth, float drawableHeight)
{
    const float backbufferAspect = backbufferWidth / backbufferHeight;
    const float drawableAspect = drawableWidth / drawableHeight;

    Rectangle dst{};

    if (drawableAspect > backbufferAspect) {
        dst.height = drawableHeight;
        dst.width = std::round(dst.height * backbufferAspect);
        dst.x = std::floor((drawableWidth - dst.width) * 0.5f);
        dst.y = 0.0f;
    } else {
        dst.width = drawableWidth;
        dst.height = std::round(dst.width / backbufferAspect);
        dst.x = 0.0f;
        dst.y = std::floor((drawableHeight - dst.height) * 0.5f);
    }

    return dst;
}

static Vector2 GetDrawableSize()
{
    const int screenW = GetScreenWidth();
    const int screenH = GetScreenHeight();
    const Vector2 dpiScale = GetWindowScaleDPI();

    Vector2 drawableSize{};
    drawableSize.x = std::round(static_cast<float>(screenW) * dpiScale.x);
    drawableSize.y = std::round(static_cast<float>(screenH) * dpiScale.y);

    if (drawableSize.x <= 0.0f) {
        drawableSize.x = static_cast<float>(screenW);
    }

    if (drawableSize.y <= 0.0f) {
        drawableSize.y = static_cast<float>(screenH);
    }

    return drawableSize;
}

static Rectangle BuildShakenWorldDestRect(const GameState& state, const Rectangle& baseDst)
{
    Rectangle dst = baseDst;

    const ScreenShakeState& shake = state.adventure.screenShake;
    if (!shake.active) {
        return dst;
    }

    const float scaleX = baseDst.width / static_cast<float>(INTERNAL_WIDTH);
    const float scaleY = baseDst.height / static_cast<float>(INTERNAL_HEIGHT);

    const float remaining01 = 1.0f - (shake.elapsedMs / std::max(shake.durationMs, 0.001f));
    const float fadedStrengthX = shake.strengthX * remaining01;
    const float fadedStrengthY = shake.strengthY * remaining01;

    const float offsetX = shake.currentOffset.x * scaleX;
    const float offsetY = shake.currentOffset.y * scaleY;

    const float padX = std::ceil(std::abs(fadedStrengthX) * scaleX);
    const float padY = std::ceil(std::abs(fadedStrengthY) * scaleY);

    dst.x -= padX;
    dst.y -= padY;
    dst.width += padX * 2.0f;
    dst.height += padY * 2.0f;

    dst.x += offsetX;
    dst.y += offsetY;

    return dst;
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
    GameState state;
    InitSettings(state.settings, "settings.json");

    unsigned int flags = 0;
    if (state.settings.vsync) {
        flags |= FLAG_VSYNC_HINT;
    }
    #if defined(__APPLE__)
        flags |= FLAG_WINDOW_HIGHDPI;
    #endif
    SetConfigFlags(flags);

    InstallDebugConsoleTraceLogHook();
    InitWindow(1920, 1080, "Adventure");
    SetExitKey(0);

    if (!InitEffectShaderRegistry()) {
        TraceLog(LOG_WARNING, "One or more effect shaders failed to load");
    }

    RenderTexture2D worldTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(worldTarget.texture, TEXTURE_FILTER_BILINEAR);

    RenderTexture2D worldSampleTempTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(worldSampleTempTarget.texture, TEXTURE_FILTER_BILINEAR);

    RenderTexture2D uiTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(uiTarget.texture, TEXTURE_FILTER_BILINEAR);

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

        const Vector2 drawableSize = GetDrawableSize();
        const Rectangle dst = BuildPresentationRect(
                static_cast<float>(INTERNAL_WIDTH),
                static_cast<float>(INTERNAL_HEIGHT),
                drawableSize.x,
                drawableSize.y);

        const Vector2 dpiScale = GetWindowScaleDPI();
        const float safeDpiX = (dpiScale.x > 0.0f) ? dpiScale.x : 1.0f;
        const float safeDpiY = (dpiScale.y > 0.0f) ? dpiScale.y : 1.0f;

        SetMouseOffset(
                -static_cast<int>(std::round(dst.x / safeDpiX)),
                -static_cast<int>(std::round(dst.y / safeDpiY)));

        SetMouseScale(
                static_cast<float>(INTERNAL_WIDTH) / (dst.width / safeDpiX),
                static_cast<float>(INTERNAL_HEIGHT) / (dst.height / safeDpiY)
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
            RenderAdventureSceneComposited(state, worldTarget, worldSampleTempTarget);

            BeginTextureMode(worldTarget);
            RenderAdventureDebug(state);
            EndTextureMode();

            BeginTextureMode(uiTarget);
            ClearBackground(BLANK);
            RenderAdventureUi(state);
            EndTextureMode();
        } else {
            BeginTextureMode(worldTarget);
            ClearBackground(BLACK);
            EndTextureMode();

            BeginTextureMode(uiTarget);
            ClearBackground(BLANK);
            EndTextureMode();
        }

        BeginTextureMode(uiTarget);

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
        Rectangle worldSrc = GetFullscreenSrcRect(worldTarget.texture);
        Rectangle shakenWorldDst = BuildShakenWorldDestRect(state, dst);
        DrawTexturePro(worldTarget.texture, worldSrc, shakenWorldDst, {0,0}, 0.0f, WHITE);

        Rectangle uiSrc = GetFullscreenSrcRect(uiTarget.texture);
        DrawTexturePro(uiTarget.texture, uiSrc, dst, {0,0}, 0.0f, WHITE);

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
    UnloadRenderTexture(worldSampleTempTarget);
    UnloadRenderTexture(uiTarget);
    ShutdownEffectShaderRegistry();
    ShutdownCursor(state);
    CloseWindow();

    return 0;
}
