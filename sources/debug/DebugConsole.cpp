#include "debug/DebugConsole.h"

#include <algorithm>
#include <string>
#include <sstream>
#include <vector>

#include "adventure/Adventure.h"
#include "resources/Resources.h"
#include "scripting/ScriptSystem.h"

#include "input/Input.h"
#include "raylib.h"
#include <filesystem>
#include <cstring>
#include "adventure/AdventureActorHelpers.h"
#include "save/SaveGame.h"
#include "audio/Audio.h"

static constexpr int CONSOLE_PADDING = 16;
static constexpr int CONSOLE_INPUT_HEIGHT = 40;
static constexpr int CONSOLE_LINE_HEIGHT = 24;
static constexpr int CONSOLE_TOP = 80;
static constexpr int CONSOLE_HEIGHT = 620;

static Font gConsoleFont{};
static bool gConsoleFontLoaded = false;
static constexpr int CONSOLE_FONT_SIZE = 28;

static bool IsPrintableConsoleCodepoint(unsigned int codepoint)
{
    return codepoint >= 32 && codepoint != 127;
}

static void ClampCaret(DebugConsoleData& console)
{
    if (console.caretIndex < 0) {
        console.caretIndex = 0;
    }

    const int maxIndex = static_cast<int>(console.input.size());
    if (console.caretIndex > maxIndex) {
        console.caretIndex = maxIndex;
    }
}

static void ResetCaretBlink(DebugConsoleData& console)
{
    console.caretBlinkMs = 0.0f;
    console.caretVisible = true;
}

static void InsertConsoleText(DebugConsoleData& console, const std::string& text)
{
    ClampCaret(console);
    console.input.insert(static_cast<size_t>(console.caretIndex), text);
    console.caretIndex += static_cast<int>(text.size());
}

static std::string SanitizeClipboardTextForConsole(const char* text)
{
    if (text == nullptr) {
        return {};
    }

    std::string out;
    out.reserve(std::strlen(text));

    bool previousWasSpace = false;

    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p != 0; ++p) {
        const unsigned char ch = *p;

        if (ch == '\r' || ch == '\n' || ch == '\t') {
            if (!previousWasSpace) {
                out.push_back(' ');
                previousWasSpace = true;
            }
            continue;
        }

        if (ch < 32 || ch == 127) {
            continue;
        }

        out.push_back(static_cast<char>(ch));
        previousWasSpace = (ch == ' ');
    }

    return out;
}

static void BackspaceConsoleText(DebugConsoleData& console)
{
    ClampCaret(console);

    if (console.caretIndex <= 0 || console.input.empty()) {
        return;
    }

    console.input.erase(static_cast<size_t>(console.caretIndex - 1), 1);
    console.caretIndex--;
}

static void DeleteConsoleText(DebugConsoleData& console)
{
    ClampCaret(console);

    if (console.caretIndex < 0 ||
        console.caretIndex >= static_cast<int>(console.input.size())) {
        return;
    }

    console.input.erase(static_cast<size_t>(console.caretIndex), 1);
}

static void MoveConsoleCaretHome(DebugConsoleData& console)
{
    console.caretIndex = 0;
    ResetCaretBlink(console);
}

static void MoveConsoleCaretEnd(DebugConsoleData& console)
{
    console.caretIndex = static_cast<int>(console.input.size());
    ResetCaretBlink(console);
}

static void PasteClipboardIntoConsole(DebugConsoleData& console)
{
    const char* clipboardText = GetClipboardText();
    const std::string sanitized = SanitizeClipboardTextForConsole(clipboardText);

    if (sanitized.empty()) {
        return;
    }

    InsertConsoleText(console, sanitized);
    ResetCaretBlink(console);
}

static void ClampConsoleState(DebugConsoleData& console)
{
    if (console.maxLines < 1) {
        console.maxLines = 1;
    }

    if (console.visibleLines < 1) {
        console.visibleLines = 1;
    }

    const int maxScroll = std::max(0, static_cast<int>(console.lines.size()) - console.visibleLines);
    if (console.scrollOffset < 0) {
        console.scrollOffset = 0;
    }
    if (console.scrollOffset > maxScroll) {
        console.scrollOffset = maxScroll;
    }

    if (console.history.empty()) {
        console.historyIndex = -1;
    } else if (console.historyIndex < -1) {
        console.historyIndex = -1;
    } else if (console.historyIndex >= static_cast<int>(console.history.size())) {
        console.historyIndex = static_cast<int>(console.history.size()) - 1;
    }
}

void DebugConsoleAddLine(GameState& state, const std::string& text, Color color)
{
    DebugConsoleData& console = state.debug.console;

    DebugConsoleLine line;
    line.text = text;
    line.color = color;
    console.lines.push_back(line);

    while (static_cast<int>(console.lines.size()) > console.maxLines) {
        console.lines.erase(console.lines.begin());
    }

    console.scrollOffset = 0;
    ClampConsoleState(console);
}

static std::vector<std::string> SplitConsoleWords(const std::string& text)
{
    std::vector<std::string> out;
    std::istringstream in(text);
    std::string part;

    while (in >> part) {
        out.push_back(part);
    }

    return out;
}

static bool TryParseConsoleSlotIndex(const std::string& text, int& outSlotIndex)
{
    outSlotIndex = -1;

    if (text.empty()) {
        return false;
    }

    char* endPtr = nullptr;
    const long value = std::strtol(text.c_str(), &endPtr, 10);
    if (endPtr == text.c_str() || *endPtr != '\0') {
        return false;
    }

    if (value < 1 || value > 999) {
        return false;
    }

    outSlotIndex = static_cast<int>(value);
    return true;
}

static bool ExecuteConsoleSlashCommand(GameState& state, const std::string& line)
{
    const std::vector<std::string> args = SplitConsoleWords(line);
    if (args.empty()) {
        return true;
    }

    const std::string& cmd = args[0];

    if (cmd == "/help") {
        DebugConsoleAddLine(state, "Console commands:", SKYBLUE);
        DebugConsoleAddLine(state, "  /help", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /clear", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /goto <sceneId> [spawnId]", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /reload", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /save <slot>", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /load <slot>", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /saves", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /resources", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /flags", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /items", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /actors", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /scenes", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /hotspots", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /props", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /effects", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /exits", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /spawns", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /audio", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /emitters", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /playemitter <emitterId>", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /stopemitter <emitterId>", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /play <audioId>", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /music <audioId> [fadeMs]", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /stopmusic [fadeMs]", LIGHTGRAY);
        return true;
    }

    if (cmd == "/clear") {
        state.debug.console.lines.clear();
        state.debug.console.scrollOffset = 0;
        return true;
    }

    if (cmd == "/goto") {
        if (args.size() < 2) {
            DebugConsoleAddLine(state, "usage: /goto <sceneId> [spawnId]", RED);
            return true;
        }

        const char* spawnId = nullptr;
        if (args.size() >= 3 && !args[2].empty()) {
            spawnId = args[2].c_str();
        }

        AdventureQueueLoadScene(state, args[1].c_str(), spawnId);
        DebugConsoleAddLine(state, "queued scene load: " + args[1], SKYBLUE);
        return true;
    }

    if (cmd == "/reload") {
        if (!state.adventure.currentScene.loaded || state.adventure.currentScene.sceneId.empty()) {
            DebugConsoleAddLine(state, "no scene loaded", RED);
            return true;
        }

        AdventureQueueLoadScene(state, state.adventure.currentScene.sceneId.c_str(), nullptr);
        DebugConsoleAddLine(state, "queued reload: " + state.adventure.currentScene.sceneId, SKYBLUE);
        return true;
    }

    if (cmd == "/save") {
        if (args.size() < 2) {
            DebugConsoleAddLine(state, "usage: /save <slot>", RED);
            return true;
        }

        int slotIndex = -1;
        if (!TryParseConsoleSlotIndex(args[1], slotIndex)) {
            DebugConsoleAddLine(state, "invalid slot index", RED);
            return true;
        }

        if (SaveGameToSlot(state, slotIndex)) {
            DebugConsoleAddLine(
                    state,
                    "saved slot " + std::to_string(slotIndex) + ": " + GetSaveSlotSummary(slotIndex),
                    SKYBLUE);
        } else {
            DebugConsoleAddLine(
                    state,
                    "failed saving slot " + std::to_string(slotIndex),
                    RED);
        }

        return true;
    }

    if (cmd == "/load") {
        if (args.size() < 2) {
            DebugConsoleAddLine(state, "usage: /load <slot>", RED);
            return true;
        }

        int slotIndex = -1;
        if (!TryParseConsoleSlotIndex(args[1], slotIndex)) {
            DebugConsoleAddLine(state, "invalid slot index", RED);
            return true;
        }

        if (!DoesSaveSlotExist(slotIndex)) {
            DebugConsoleAddLine(
                    state,
                    "save slot " + std::to_string(slotIndex) + " is empty",
                    RED);
            return true;
        }

        if (LoadGameFromSlot(state, slotIndex)) {
            DebugConsoleAddLine(
                    state,
                    "loaded slot " + std::to_string(slotIndex) + ": " + GetSaveSlotSummary(slotIndex),
                    SKYBLUE);
        } else {
            DebugConsoleAddLine(
                    state,
                    "failed loading slot " + std::to_string(slotIndex),
                    RED);
        }

        return true;
    }

    if (cmd == "/saves") {
        DebugConsoleAddLine(state, "save slots:", SKYBLUE);

        for (int slot = 1; slot <= 8; ++slot) {
            const std::string summary = GetSaveSlotSummary(slot);
            DebugConsoleAddLine(
                    state,
                    "  [" + std::to_string(slot) + "] " + summary,
                    LIGHTGRAY);
        }

        return true;
    }

    if (cmd == "/resources") {
        DebugConsoleAddLine(
                state,
                TextFormat("textures: %d", static_cast<int>(state.resources.textures.size())),
                SKYBLUE);

        for (const TextureResource& tex : state.resources.textures) {
            DebugConsoleAddLine(
                    state,
                    TextFormat("  [tex %d] %s", tex.handle, tex.path.c_str()),
                    LIGHTGRAY);
        }

        DebugConsoleAddLine(
                state,
                TextFormat("sprite assets: %d", static_cast<int>(state.resources.spriteAssets.size())),
                SKYBLUE);

        for (const SpriteAssetResource& asset : state.resources.spriteAssets) {
            DebugConsoleAddLine(
                    state,
                    TextFormat("  [sprite %d] %s", asset.handle, asset.sidecarPath.c_str()),
                    LIGHTGRAY);
        }

        DebugConsoleAddLine(
                state,
                TextFormat("sounds: %d", static_cast<int>(state.resources.sounds.size())),
                SKYBLUE);

        for (const SoundResource& sound : state.resources.sounds) {
            DebugConsoleAddLine(
                    state,
                    TextFormat("  [sound %d] %s", sound.handle, sound.path.c_str()),
                    LIGHTGRAY);
        }

        DebugConsoleAddLine(
                state,
                TextFormat("music streams: %d", static_cast<int>(state.resources.musics.size())),
                SKYBLUE);

        for (const MusicResource& music : state.resources.musics) {
            DebugConsoleAddLine(
                    state,
                    TextFormat("  [music %d] %s", music.handle, music.path.c_str()),
                    LIGHTGRAY);
        }

        return true;
    }

    if (cmd == "/flags") {
        DebugConsoleAddLine(state, "bool flags:", SKYBLUE);
        if (state.script.flags.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
        } else {
            for (const auto& kv : state.script.flags) {
                DebugConsoleAddLine(
                        state,
                        "  " + kv.first + " = " + (kv.second ? "true" : "false"),
                        LIGHTGRAY);
            }
        }

        DebugConsoleAddLine(state, "int flags:", SKYBLUE);
        if (state.script.ints.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
        } else {
            for (const auto& kv : state.script.ints) {
                DebugConsoleAddLine(
                        state,
                        "  " + kv.first + " = " + std::to_string(kv.second),
                        LIGHTGRAY);
            }
        }

        DebugConsoleAddLine(state, "string flags:", SKYBLUE);
        if (state.script.strings.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
        } else {
            for (const auto& kv : state.script.strings) {
                DebugConsoleAddLine(
                        state,
                        "  " + kv.first + " = \"" + kv.second + "\"",
                        LIGHTGRAY);
            }
        }

        return true;
    }

    if (cmd == "/items") {
        DebugConsoleAddLine(state, "item definitions:", SKYBLUE);

        if (state.adventure.itemDefinitions.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
        } else {
            for (const ItemDefinitionData& item : state.adventure.itemDefinitions) {
                DebugConsoleAddLine(
                        state,
                        "  " + item.itemId + "  (" + item.displayName + ")",
                        LIGHTGRAY);
            }
        }

        DebugConsoleAddLine(state, "inventories:", SKYBLUE);
        if (state.adventure.actorInventories.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
        } else {
            for (const ActorInventoryData& inv : state.adventure.actorInventories) {
                std::string lineText = "  " + inv.actorId + ":";
                if (inv.itemIds.empty()) {
                    lineText += " <empty>";
                } else {
                    for (const std::string& itemId : inv.itemIds) {
                        lineText += " " + itemId;
                    }
                }

                if (!inv.heldItemId.empty()) {
                    lineText += "   held=" + inv.heldItemId;
                }

                DebugConsoleAddLine(state, lineText, LIGHTGRAY);
            }
        }

        return true;
    }

    if (cmd == "/actors") {
        DebugConsoleAddLine(state, "actors:", SKYBLUE);

        if (state.adventure.actors.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
            return true;
        }

        const int controlledActorIndex = GetControlledActorIndex(state);

        for (int i = 0; i < static_cast<int>(state.adventure.actors.size()); ++i) {
            const ActorInstance& actor = state.adventure.actors[i];

            std::string lineText =
                    "  [" + std::to_string(i) + "] " + actor.actorId +
                    " pos=(" + std::to_string(static_cast<int>(actor.feetPos.x)) +
                    "," + std::to_string(static_cast<int>(actor.feetPos.y)) + ")";

            if (i == controlledActorIndex) {
                lineText += "  <controlled>";
            }

            if (!actor.visible) {
                lineText += "  hidden";
            }

            if (!actor.activeInScene) {
                lineText += "  inactive";
            }

            DebugConsoleAddLine(state, lineText, LIGHTGRAY);
        }

        return true;
    }

    if (cmd == "/scenes") {
        const std::filesystem::path scenesDir = std::filesystem::path(ASSETS_PATH "scenes");

        if (!std::filesystem::exists(scenesDir) || !std::filesystem::is_directory(scenesDir)) {
            DebugConsoleAddLine(state, "scenes directory missing", RED);
            return true;
        }

        DebugConsoleAddLine(state, "scenes:", SKYBLUE);

        int count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(scenesDir)) {
            if (!entry.is_directory()) {
                continue;
            }

            const std::string sceneId = entry.path().filename().string();
            DebugConsoleAddLine(state, "  " + sceneId, LIGHTGRAY);
            count++;
        }

        if (count == 0) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
        }

        return true;
    }

    if (cmd == "/hotspots") {
        if (!state.adventure.currentScene.loaded) {
            DebugConsoleAddLine(state, "no scene loaded", RED);
            return true;
        }

        DebugConsoleAddLine(state, "hotspots:", SKYBLUE);

        if (state.adventure.currentScene.hotspots.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
            return true;
        }

        for (const SceneHotspot& hotspot : state.adventure.currentScene.hotspots) {
            DebugConsoleAddLine(
                    state,
                    "  " + hotspot.id + "  (" + hotspot.displayName + ")",
                    LIGHTGRAY);
        }

        return true;
    }

    if (cmd == "/props") {
        if (!state.adventure.currentScene.loaded) {
            DebugConsoleAddLine(state, "no scene loaded", RED);
            return true;
        }

        DebugConsoleAddLine(state, "props:", SKYBLUE);

        const int count = std::min(
                static_cast<int>(state.adventure.currentScene.props.size()),
                static_cast<int>(state.adventure.props.size()));

        if (count <= 0) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
            return true;
        }

        for (int i = 0; i < count; ++i) {
            const ScenePropData& sceneProp = state.adventure.currentScene.props[i];
            const PropInstance& prop = state.adventure.props[i];

            std::string line =
                    "  " + sceneProp.id +
                    " pos=(" +
                    std::to_string(static_cast<int>(prop.feetPos.x)) +
                    "," +
                    std::to_string(static_cast<int>(prop.feetPos.y)) +
                    ")";

            if (!prop.visible) {
                line += " hidden";
            }

            DebugConsoleAddLine(state, line, LIGHTGRAY);
        }

        return true;
    }

    if (cmd == "/effects") {
        if (!state.adventure.currentScene.loaded) {
            DebugConsoleAddLine(state, "no scene loaded", RED);
            return true;
        }

        DebugConsoleAddLine(state, "effects:", SKYBLUE);

        const int count = std::min(
                static_cast<int>(state.adventure.currentScene.effectSprites.size()),
                static_cast<int>(state.adventure.effectSprites.size()));

        if (count <= 0) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
            return true;
        }

        for (int i = 0; i < count; ++i) {
            const SceneEffectSpriteData& sceneEffect = state.adventure.currentScene.effectSprites[i];
            const EffectSpriteInstance& effect = state.adventure.effectSprites[i];

            std::string depthModeText = "depthSorted";
            switch (sceneEffect.depthMode) {
                case ScenePropDepthMode::Back:
                    depthModeText = "back";
                    break;
                case ScenePropDepthMode::DepthSorted:
                    depthModeText = "depthSorted";
                    break;
                case ScenePropDepthMode::Front:
                    depthModeText = "front";
                    break;
            }

            std::string blendModeText = "normal";
            switch (sceneEffect.blendMode) {
                case SceneEffectBlendMode::Normal:
                    blendModeText = "normal";
                    break;
                case SceneEffectBlendMode::Add:
                    blendModeText = "add";
                    break;
                case SceneEffectBlendMode::Multiply:
                    blendModeText = "multiply";
                    break;
            }

            std::string line =
                    "  " + sceneEffect.id +
                    " depth=" + depthModeText +
                    " blend=" + blendModeText +
                    " opacity=" + std::to_string(effect.opacity);

            if (!effect.visible) {
                line += " hidden";
            }

            DebugConsoleAddLine(state, line, LIGHTGRAY);
        }

        return true;
    }

    if (cmd == "/exits") {
        if (!state.adventure.currentScene.loaded) {
            DebugConsoleAddLine(state, "no scene loaded", RED);
            return true;
        }

        DebugConsoleAddLine(state, "exits:", SKYBLUE);

        if (state.adventure.currentScene.exits.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
            return true;
        }

        for (const SceneExit& exitObj : state.adventure.currentScene.exits) {
            DebugConsoleAddLine(
                    state,
                    "  " + exitObj.id +
                    "  (" + exitObj.displayName + ")" +
                    " -> scene=" + exitObj.targetScene +
                    " spawn=" + exitObj.targetSpawn,
                    LIGHTGRAY);
        }

        return true;
    }

    if (cmd == "/spawns") {
        if (!state.adventure.currentScene.loaded) {
            DebugConsoleAddLine(state, "no scene loaded", RED);
            return true;
        }

        DebugConsoleAddLine(state, "spawns:", SKYBLUE);

        if (state.adventure.currentScene.spawns.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
            return true;
        }

        for (const SceneSpawnPoint& spawn : state.adventure.currentScene.spawns) {
            DebugConsoleAddLine(
                    state,
                    "  " + spawn.id +
                    " pos=(" +
                    std::to_string(static_cast<int>(spawn.position.x)) +
                    "," +
                    std::to_string(static_cast<int>(spawn.position.y)) +
                    ")",
                    LIGHTGRAY);
        }

        return true;
    }

    if (cmd == "/audio") {
        DebugConsoleAddLine(state, "audio definitions:", SKYBLUE);

        if (state.audio.definitions.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
            return true;
        }

        for (const AudioDefinitionData& def : state.audio.definitions) {
            const std::string typeText = (def.type == AudioType::Sound) ? "sound" : "music";
            const std::string scopeText = (def.scope == ResourceScope::Global) ? "global" : "scene";

            DebugConsoleAddLine(
                    state,
                    "  " + def.id + "  type=" + typeText + " scope=" + scopeText + " file=" + def.filePath,
                    LIGHTGRAY);
        }

        return true;
    }

    if (cmd == "/emitters") {
        if (!state.adventure.currentScene.loaded) {
            DebugConsoleAddLine(state, "no scene loaded", RED);
            return true;
        }

        DebugConsoleAddLine(state, "sound emitters:", SKYBLUE);

        const int count = std::min(
                static_cast<int>(state.adventure.currentScene.soundEmitters.size()),
                static_cast<int>(state.audio.sceneEmitters.size()));

        if (count <= 0) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
            return true;
        }

        for (int i = 0; i < count; ++i) {
            const SceneSoundEmitterData& sceneEmitter = state.adventure.currentScene.soundEmitters[i];
            const SoundEmitterInstance& emitter = state.audio.sceneEmitters[i];

            std::string line =
                    "  " + sceneEmitter.id +
                    " sound=" + sceneEmitter.soundId +
                    " radius=" + std::to_string(static_cast<int>(sceneEmitter.radius)) +
                    " loop=" + std::string(sceneEmitter.loop ? "true" : "false") +
                    " enabled=" + std::string(emitter.enabled ? "true" : "false") +
                    " active=" + std::string(emitter.active ? "true" : "false") +
                    " volume=" + std::to_string(emitter.volume);

            DebugConsoleAddLine(state, line, LIGHTGRAY);
        }

        return true;
    }

    if (cmd == "/play") {
        if (args.size() < 2) {
            DebugConsoleAddLine(state, "usage: /play <audioId>", RED);
            return true;
        }

        if (PlaySoundById(state, args[1])) {
            DebugConsoleAddLine(state, "played sound: " + args[1], SKYBLUE);
        } else {
            DebugConsoleAddLine(state, "failed playing sound: " + args[1], RED);
        }

        return true;
    }

    if (cmd == "/music") {
        if (args.size() < 2) {
            DebugConsoleAddLine(state, "usage: /music <audioId> [fadeMs]", RED);
            return true;
        }

        float fadeMs = 0.0f;
        if (args.size() >= 3) {
            try {
                fadeMs = std::stof(args[2]);
            } catch (...) {
                DebugConsoleAddLine(state, "usage: /music <audioId> [fadeMs]", RED);
                return true;
            }
        }

        if (PlayMusicById(state, args[1], fadeMs)) {
            if (fadeMs > 0.0f) {
                DebugConsoleAddLine(
                        state,
                        "playing music: " + args[1] +
                        " (fade " + std::to_string(static_cast<int>(fadeMs)) + " ms)",
                        SKYBLUE);
            } else {
                DebugConsoleAddLine(state, "playing music: " + args[1], SKYBLUE);
            }
        } else {
            DebugConsoleAddLine(state, "failed playing music: " + args[1], RED);
        }

        return true;
    }

    if (cmd == "/stopmusic") {
        float fadeMs = 0.0f;

        if (args.size() >= 2) {
            try {
                fadeMs = std::stof(args[1]);
            } catch (...) {
                DebugConsoleAddLine(state, "usage: /stopmusic [fadeMs]", RED);
                return true;
            }
        }

        StopMusic(state, fadeMs);

        if (fadeMs > 0.0f) {
            DebugConsoleAddLine(
                    state,
                    "stopping music with fade: " + std::to_string(static_cast<int>(fadeMs)) + " ms",
                    SKYBLUE);
        } else {
            DebugConsoleAddLine(state, "stopped music", SKYBLUE);
        }

        return true;
    }

    if (cmd == "/playemitter") {
        if (args.size() < 2) {
            DebugConsoleAddLine(state, "usage: /playemitter <emitterId>", RED);
            return true;
        }

        if (PlaySoundEmitterById(state, args[1])) {
            DebugConsoleAddLine(state, "played emitter: " + args[1], SKYBLUE);
        } else {
            DebugConsoleAddLine(state, "failed playing emitter: " + args[1], RED);
        }

        return true;
    }

    if (cmd == "/stopemitter") {
        if (args.size() < 2) {
            DebugConsoleAddLine(state, "usage: /stopemitter <emitterId>", RED);
            return true;
        }

        if (StopSoundEmitterById(state, args[1])) {
            DebugConsoleAddLine(state, "stopped emitter: " + args[1], SKYBLUE);
        } else {
            DebugConsoleAddLine(state, "failed stopping emitter: " + args[1], RED);
        }

        return true;
    }

    DebugConsoleAddLine(state, "unknown command: " + cmd, RED);
    return true;
}

static void SubmitConsoleLine(GameState& state)
{
    DebugConsoleData& console = state.debug.console;

    if (console.input.empty()) {
        return;
    }

    const std::string submitted = console.input;

    DebugConsoleAddLine(state, "> " + submitted, WHITE);

    if (console.history.empty() || console.history.back() != submitted) {
        console.history.push_back(submitted);
    }

    console.historyIndex = -1;
    console.input.clear();
    console.caretIndex = 0;

    if (!submitted.empty() && submitted[0] == '/') {
        ExecuteConsoleSlashCommand(state, submitted);
        return;
    }

    std::string outResult;
    std::string outError;
    const bool ok = ScriptSystemExecuteConsoleLine(
            state.script,
            submitted,
            outResult,
            outError);

    if (!ok) {
        DebugConsoleAddLine(state, outError, RED);
        return;
    }

    if (!outResult.empty()) {
        DebugConsoleAddLine(state, outResult, SKYBLUE);
    }
}

static void RecallHistoryUp(DebugConsoleData& console)
{
    if (console.history.empty()) {
        return;
    }

    if (console.historyIndex < 0) {
        console.historyIndex = static_cast<int>(console.history.size()) - 1;
    } else if (console.historyIndex > 0) {
        console.historyIndex--;
    }

    if (console.historyIndex >= 0 &&
        console.historyIndex < static_cast<int>(console.history.size())) {
        console.input = console.history[console.historyIndex];
        console.caretIndex = static_cast<int>(console.input.size());
    }
}

static void RecallHistoryDown(DebugConsoleData& console)
{
    if (console.history.empty()) {
        return;
    }

    if (console.historyIndex < 0) {
        return;
    }

    if (console.historyIndex < static_cast<int>(console.history.size()) - 1) {
        console.historyIndex++;
        console.input = console.history[console.historyIndex];
        console.caretIndex = static_cast<int>(console.input.size());
    } else {
        console.historyIndex = -1;
        console.input.clear();
        console.caretIndex = static_cast<int>(console.input.size());
    }
}

void UpdateDebugConsole(GameState& state, float dt)
{
    DebugConsoleData& console = state.debug.console;

    console.caretBlinkMs += dt * 1000.0f;
    if (console.caretBlinkMs >= 500.0f) {
        console.caretBlinkMs = 0.0f;
        console.caretVisible = !console.caretVisible;
    }

    bool suppressTextInputThisFrame = false;

    for (auto& ev : FilterEvents(state.input, true, InputEventType::KeyPressed)) {
        if (ev.key.key == KEY_GRAVE) {
            console.open = !console.open;
            console.caretBlinkMs = 0.0f;
            console.caretVisible = true;
            suppressTextInputThisFrame = true;
            ConsumeEvent(ev);
            continue;
        }

        if (!console.open) {
            continue;
        }

        switch (ev.key.key) {
            case KEY_ENTER:
                SubmitConsoleLine(state);
                console.caretBlinkMs = 0.0f;
                console.caretVisible = true;
                ConsumeEvent(ev);
                break;

            case KEY_BACKSPACE:
                BackspaceConsoleText(console);
                ResetCaretBlink(console);
                ConsumeEvent(ev);
                break;

            case KEY_DELETE:
                DeleteConsoleText(console);
                ResetCaretBlink(console);
                ConsumeEvent(ev);
                break;

            case KEY_HOME:
                MoveConsoleCaretHome(console);
                ConsumeEvent(ev);
                break;

            case KEY_END:
                MoveConsoleCaretEnd(console);
                ConsumeEvent(ev);
                break;

            case KEY_V:
            {
                const bool ctrlDown =
                        IsKeyDown(KEY_LEFT_CONTROL) ||
                        IsKeyDown(KEY_RIGHT_CONTROL);

                if (ctrlDown) {
                    PasteClipboardIntoConsole(console);
                    ConsumeEvent(ev);
                }
                break;
            }

            case KEY_L:
            {
                const bool ctrlDown =
                        IsKeyDown(KEY_LEFT_CONTROL) ||
                        IsKeyDown(KEY_RIGHT_CONTROL);

                if (ctrlDown) {
                    console.lines.clear();
                    console.scrollOffset = 0;
                    ResetCaretBlink(console);
                    ConsumeEvent(ev);
                }
                break;
            }

            case KEY_LEFT:
                if (console.caretIndex > 0) {
                    console.caretIndex--;
                    ResetCaretBlink(console);
                }
                ConsumeEvent(ev);
                break;

            case KEY_RIGHT:
                if (console.caretIndex < static_cast<int>(console.input.size())) {
                    console.caretIndex++;
                    ResetCaretBlink(console);
                }
                ConsumeEvent(ev);
                break;

            case KEY_UP:
                RecallHistoryUp(console);
                console.caretBlinkMs = 0.0f;
                console.caretVisible = true;
                ConsumeEvent(ev);
                break;

            case KEY_DOWN:
                RecallHistoryDown(console);
                console.caretBlinkMs = 0.0f;
                console.caretVisible = true;
                ConsumeEvent(ev);
                break;

            case KEY_PAGE_UP:
                console.scrollOffset += 20;
                ClampConsoleState(console);
                ConsumeEvent(ev);
                break;

            case KEY_PAGE_DOWN:
                console.scrollOffset -= 20;
                ClampConsoleState(console);
                ConsumeEvent(ev);
                break;

            case KEY_ESCAPE:
                console.open = false;
                ConsumeEvent(ev);
                break;

            default:
                break;
        }
    }

    if (!console.open) {
        return;
    }

    if (suppressTextInputThisFrame) {
        for (auto& ev : FilterEvents(state.input, true, InputEventType::TextInput)) {
            ConsumeEvent(ev);
        }
        return;
    }

    for (auto& ev : FilterEvents(state.input, true, InputEventType::TextInput)) {
        if (!IsPrintableConsoleCodepoint(ev.text.codepoint)) {
            continue;
        }

        InsertConsoleText(console, std::string(1, static_cast<char>(ev.text.codepoint)));
        ResetCaretBlink(console);
        ConsumeEvent(ev);
    }

    ClampConsoleState(console);
}

void RenderDebugConsole(const GameState& state)
{
    const DebugConsoleData& console = state.debug.console;
    if (!console.open) {
        return;
    }

    const Font font = gConsoleFontLoaded ? gConsoleFont : GetFontDefault();

    const Rectangle panelRect{
            static_cast<float>(CONSOLE_PADDING),
            static_cast<float>(CONSOLE_TOP),
            static_cast<float>(INTERNAL_WIDTH - CONSOLE_PADDING * 2),
            static_cast<float>(CONSOLE_HEIGHT)
    };

    const Rectangle inputRect{
            panelRect.x,
            panelRect.y + panelRect.height - static_cast<float>(CONSOLE_INPUT_HEIGHT),
            panelRect.width,
            static_cast<float>(CONSOLE_INPUT_HEIGHT)
    };

    const Rectangle linesRect{
            panelRect.x,
            panelRect.y,
            panelRect.width,
            panelRect.height - static_cast<float>(CONSOLE_INPUT_HEIGHT) - 6.0f
    };

    DrawRectangleRounded(panelRect, 0.02f, 4, Color{20, 20, 20, 220});
    DrawRectangleLinesEx(panelRect, 2.0f, Color{180, 180, 180, 255});

    DrawRectangleRec(inputRect, Color{32, 32, 32, 230});
    DrawRectangleLinesEx(inputRect, 1.0f, Color{140, 140, 140, 255});

    const float fontSize = 22.0f;
    const float spacing = 1.0f;

    const float topInset = 8.0f;
    const float bottomInset = 8.0f;
    const float availableLineHeight = linesRect.height - topInset - bottomInset;
    const int visibleLineCount = std::max(
            1,
            static_cast<int>(availableLineHeight / static_cast<float>(CONSOLE_LINE_HEIGHT)));

    const int totalLines = static_cast<int>(console.lines.size());
    const int maxScroll = std::max(0, totalLines - visibleLineCount);
    const int scrollOffset = std::clamp(console.scrollOffset, 0, maxScroll);

    const int startIndex = std::max(0, totalLines - visibleLineCount - scrollOffset);
    const int endIndex = std::min(totalLines, startIndex + visibleLineCount);

    float y = linesRect.y + topInset;
    for (int i = startIndex; i < endIndex; ++i) {
        DrawTextEx(
                font,
                console.lines[i].text.c_str(),
                Vector2{linesRect.x + 10.0f, y},
                fontSize,
                spacing,
                console.lines[i].color);
        y += static_cast<float>(CONSOLE_LINE_HEIGHT);
    }

    const Vector2 inputPos{ inputRect.x + 10.0f, inputRect.y + 8.0f };

    int caretIndex = console.caretIndex;
    if (caretIndex < 0) {
        caretIndex = 0;
    }
    if (caretIndex > static_cast<int>(console.input.size())) {
        caretIndex = static_cast<int>(console.input.size());
    }

    const std::string leftText =
            "> " + console.input.substr(0, static_cast<size_t>(caretIndex));
    const std::string rightText =
            console.input.substr(static_cast<size_t>(caretIndex));

    DrawTextEx(
            font,
            leftText.c_str(),
            inputPos,
            fontSize,
            spacing,
            WHITE);

    const Vector2 leftSize = MeasureTextEx(font, leftText.c_str(), fontSize, spacing);

    if (console.caretVisible) {
        const float caretX = inputPos.x + leftSize.x;
        const float caretTop = inputPos.y + 2.0f;
        const float caretBottom = inputPos.y + fontSize + 2.0f;

        DrawLineEx(
                Vector2{caretX, caretTop},
                Vector2{caretX, caretBottom},
                2.0f,
                WHITE);
    }

    if (!rightText.empty()) {
        DrawTextEx(
                font,
                rightText.c_str(),
                Vector2{inputPos.x + leftSize.x, inputPos.y},
                fontSize,
                spacing,
                WHITE);
    }

    //const std::string helpText =
    //        "` console  |  Enter submit  |  Up/Down history  |  Left/Right move  |  Del delete  |  PgUp/PgDn scroll";

    const std::string helpText =
            "` console  |  Enter submit  |  Up/Down history  |  Left/Right move  |  Home/End caret  |  Ctrl+V paste  |  Ctrl+L clear  |  Del delete  |  PgUp/PgDn scroll";

    DrawTextEx(
            font,
            helpText.c_str(),
            Vector2{panelRect.x + 10.0f, panelRect.y - 28.0f},
            18.0f,
            1.0f,
            LIGHTGRAY);
}

void DebugConsoleInit(GameState& state)
{
    state.debug.console = {};
    state.debug.console.caretVisible = true;
    state.debug.console.caretIndex = 0;

    const std::string fontPath = std::string(ASSETS_PATH) + "fonts/Inconsolata.otf";
    gConsoleFont = LoadFontEx(fontPath.c_str(), CONSOLE_FONT_SIZE, nullptr, 0);
    gConsoleFontLoaded = (gConsoleFont.texture.id != 0);

    if (gConsoleFontLoaded) {
        SetTextureFilter(gConsoleFont.texture, TEXTURE_FILTER_BILINEAR);
        DebugConsoleAddLine(state, std::string("Loaded console font: ") + fontPath, SKYBLUE);
    } else {
        DebugConsoleAddLine(state, std::string("Failed to load console font: ") + fontPath, RED);
    }
}

void DebugConsoleShutdown()
{
    if (gConsoleFontLoaded) {
        UnloadFont(gConsoleFont);
        gConsoleFont = {};
        gConsoleFontLoaded = false;
    }
}

