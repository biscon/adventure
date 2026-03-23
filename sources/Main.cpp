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

static bool LoadFonts(GameState& state) {
    //const std::string fontPath = std::string(ASSETS_PATH) + "fonts/AtkinsonHyperlegible-Bold.ttf";
    //const std::string fontPath = std::string(ASSETS_PATH) + "fonts/AlegreyaSans-Bold.ttf";
    const std::string fontPath = std::string(ASSETS_PATH) + "fonts/IBMPlexSans-Bold.ttf";
    state.dialogueFont = LoadFontEx(fontPath.c_str(), 46, nullptr, 0);
    if(state.dialogueFont.texture.id == 0) {
        TraceLog(LOG_ERROR, "Could not load dialogue font from %s", fontPath.c_str());
        return false;
    }
    state.hoverLabelFont = LoadFontEx(fontPath.c_str(), 58, nullptr, 0);
    if(state.hoverLabelFont.texture.id == 0) {
        TraceLog(LOG_ERROR, "Could not load hover label font from %s", fontPath.c_str());
        return false;
    }
    state.speechFont = LoadFontEx(fontPath.c_str(), 50, nullptr, 0);
    if(state.speechFont.texture.id == 0) {
        TraceLog(LOG_ERROR, "Could not load speech font from %s", fontPath.c_str());
        return false;
    }
    state.ambientSpeechFont = LoadFontEx(fontPath.c_str(), 44, nullptr, 0);
    if(state.ambientSpeechFont.texture.id == 0) {
        TraceLog(LOG_ERROR, "Could not load ambient speech font from %s", fontPath.c_str());
        return false;
    }
    return true;
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

    {
        const int screenW = GetScreenWidth();
        const int screenH = GetScreenHeight();
        const int renderW = GetRenderWidth();
        const int renderH = GetRenderHeight();
        const Vector2 dpi = GetWindowScaleDPI();
        const int monitor = GetCurrentMonitor();

        TraceLog(LOG_INFO, "==== DISPLAY INFO ====");
        TraceLog(LOG_INFO, "Monitor: %d", monitor);
        TraceLog(LOG_INFO, "Screen (logical): %d x %d", screenW, screenH);
        TraceLog(LOG_INFO, "Render (framebuffer): %d x %d", renderW, renderH);
        TraceLog(LOG_INFO, "DPI scale: %.2f x %.2f", dpi.x, dpi.y);
        TraceLog(LOG_INFO, "Monitor size: %d x %d",
                 GetMonitorWidth(monitor),
                 GetMonitorHeight(monitor));
        TraceLog(LOG_INFO, "======================");
    }

    SetExitKey(0);
    RefreshResolutions(state.settings);
    ApplySettings(state.settings);

    if (!InitEffectShaderRegistry()) {
        TraceLog(LOG_WARNING, "One or more effect shaders failed to load");
    }

    RenderTexture2D worldTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(worldTarget.texture, TEXTURE_FILTER_BILINEAR);

    RenderTexture2D worldSampleTempTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(worldSampleTempTarget.texture, TEXTURE_FILTER_BILINEAR);

    RenderTexture2D uiTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(uiTarget.texture, TEXTURE_FILTER_BILINEAR);

    InitInput(state.input);
    InitAudio(state);
    InitCursor(state);

    if(!LoadFonts(state)) {
        TraceLog(LOG_ERROR, "Font loading failed.");
        state.mode = GameMode::Quit;
    }

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

        const Rectangle dst = BuildPresentationRect(
                static_cast<float>(INTERNAL_WIDTH),
                static_cast<float>(INTERNAL_HEIGHT),
                static_cast<float>(screenW),
                static_cast<float>(screenH)
        );

        SetMouseOffset(
                -static_cast<int>(dst.x),
                -static_cast<int>(dst.y));

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

        if (state.mode == GameMode::Menu) {
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
            RenderAdventureSceneFadeOverlay(state);
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
