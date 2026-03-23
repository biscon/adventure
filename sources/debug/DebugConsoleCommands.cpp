#include "debug/DebugConsoleInternal.h"
#include "debug/DebugConsole.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "adventure/AdventureUpdate.h"
#include "adventure/AdventureHelpers.h"
#include "audio/Audio.h"
#include "save/SaveGame.h"
#include "resources/TextureAsset.h"
#include "render/EffectShaderRegistry.h"

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

bool ExecuteConsoleSlashCommand(GameState& state, const std::string& line)
{
    const std::vector<std::string> args = SplitConsoleWords(line);
    if (args.empty()) {
        return true;
    }

    const std::string& cmd = args[0];

    if (cmd == "/help") {
        DebugConsoleAddLine(state, "Console commands:", SKYBLUE);
        DebugConsoleAddLine(state, "  /help", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /quit", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /clear", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /copylast [numLines]", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /goto <sceneId> [spawnId]", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /reload", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /save <slot>", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /load <slot>", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /saves", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /resources", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /flags", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /items", LIGHTGRAY);
        DebugConsoleAddLine(state, "  /layers", LIGHTGRAY);
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

    if (cmd == "/quit") {
        state.mode = GameMode::Quit;
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
        auto TextureFilterModeToText = [](TextureFilterMode mode) -> const char* {
            switch (mode) {
                case TextureFilterMode::Bilinear:
                    return "bilinear";
                case TextureFilterMode::Point:
                default:
                    return "point";
            }
        };

        auto TextureWrapModeToText = [](TextureWrapMode mode) -> const char* {
            switch (mode) {
                case TextureWrapMode::Repeat:
                    return "repeat";
                case TextureWrapMode::Clamp:
                default:
                    return "clamp";
            }
        };

        DebugConsoleAddLine(
                state,
                TextFormat("textures: %d", static_cast<int>(state.resources.textures.size())),
                SKYBLUE);

        for (const TextureResource& tex : state.resources.textures) {
            DebugConsoleAddLine(
                    state,
                    TextFormat("  [tex %d] %s", tex.handle, tex.path.c_str()),
                    LIGHTGRAY);

            DebugConsoleAddLine(
                    state,
                    TextFormat("      pma=%s filter=%s wrap=%s size=%dx%d scope=%s",
                               tex.premultiplyAlpha ? "true" : "false",
                               TextureFilterModeToText(tex.filterMode),
                               TextureWrapModeToText(tex.wrapMode),
                               tex.texture.width,
                               tex.texture.height,
                               tex.scope == ResourceScope::Global ? "global" : "scene"),
                    GRAY);
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

        auto DepthModeToText = [](ScenePropDepthMode mode) -> const char* {
            switch (mode) {
                case ScenePropDepthMode::Back:
                    return "back";
                case ScenePropDepthMode::Front:
                    return "front";
                case ScenePropDepthMode::DepthSorted:
                default:
                    return "depthSorted";
            }
        };

        auto BlendModeToText = [](SceneEffectBlendMode mode) -> const char* {
            switch (mode) {
                case SceneEffectBlendMode::Add:
                    return "add";
                case SceneEffectBlendMode::Multiply:
                    return "multiply";
                case SceneEffectBlendMode::Normal:
                default:
                    return "normal";
            }
        };

        DebugConsoleAddLine(state, "effect sprites:", SKYBLUE);

        const int spriteCount = std::min(
                static_cast<int>(state.adventure.currentScene.effectSprites.size()),
                static_cast<int>(state.adventure.effectSprites.size()));

        if (spriteCount <= 0) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
        } else {
            for (int i = 0; i < spriteCount; ++i) {
                const SceneEffectSpriteData& sceneEffect = state.adventure.currentScene.effectSprites[i];
                const EffectSpriteInstance& effect = state.adventure.effectSprites[i];

                const char* shaderName = SceneEffectShaderTypeToString(effect.shaderType);

                std::string line =
                        "  " + sceneEffect.id +
                        " shader=" + shaderName +
                        " depth=" + DepthModeToText(sceneEffect.depthMode) +
                        " blend=" + BlendModeToText(sceneEffect.blendMode) +
                        " overlay=" + std::string(sceneEffect.renderAsOverlay ? "true" : "false") +
                        " opacity=" + std::to_string(effect.opacity);

                if (!effect.visible) {
                    line += " hidden";
                }

                DebugConsoleAddLine(state, line, LIGHTGRAY);
            }
        }

        DebugConsoleAddLine(state, "effect regions:", SKYBLUE);

        const int regionCount = std::min(
                static_cast<int>(state.adventure.currentScene.effectRegions.size()),
                static_cast<int>(state.adventure.effectRegions.size()));

        if (regionCount <= 0) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
        } else {
            for (int i = 0; i < regionCount; ++i) {
                const SceneEffectRegionData& sceneEffect = state.adventure.currentScene.effectRegions[i];
                const EffectRegionInstance& effect = state.adventure.effectRegions[i];

                const char* shaderName = SceneEffectShaderTypeToString(effect.shaderType);

                const bool isPoly =
                        sceneEffect.polygon.vertices.size() >= 3;

                std::string shapeText;
                if (isPoly) {
                    shapeText = "poly(" + std::to_string(static_cast<int>(sceneEffect.polygon.vertices.size())) + ")";
                } else {
                    shapeText = "rect";
                }

                std::string line =
                        "  " + sceneEffect.id +
                        " shape=" + shapeText +
                        " shader=" + shaderName +
                        " depth=" + DepthModeToText(sceneEffect.depthMode) +
                        " blend=" + BlendModeToText(sceneEffect.blendMode) +
                        " overlay=" + std::string(sceneEffect.renderAsOverlay ? "true" : "false") +
                        " opacity=" + std::to_string(effect.opacity);

                if (!effect.visible) {
                    line += " hidden";
                }

                DebugConsoleAddLine(state, line, LIGHTGRAY);

                if (!isPoly) {
                    DebugConsoleAddLine(
                            state,
                            "      rect=(" +
                            std::to_string(static_cast<int>(sceneEffect.worldRect.x)) + "," +
                            std::to_string(static_cast<int>(sceneEffect.worldRect.y)) + "," +
                            std::to_string(static_cast<int>(sceneEffect.worldRect.width)) + "," +
                            std::to_string(static_cast<int>(sceneEffect.worldRect.height)) + ")",
                            GRAY);
                }
            }
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

    if (cmd == "/layers") {
        if (!state.adventure.currentScene.loaded) {
            DebugConsoleAddLine(state, "no scene loaded", RED);
            return true;
        }

        DebugConsoleAddLine(state, "background layers:", SKYBLUE);

        if (state.adventure.currentScene.backgroundLayers.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
        } else {
            for (const SceneImageLayer& layer : state.adventure.currentScene.backgroundLayers) {
                DebugConsoleAddLine(
                        state,
                        "  " + layer.name + "  [" + (layer.visible ? "visible" : "hidden") + "]",
                        LIGHTGRAY);
            }
        }

        DebugConsoleAddLine(state, "foreground layers:", SKYBLUE);

        if (state.adventure.currentScene.foregroundLayers.empty()) {
            DebugConsoleAddLine(state, "  <none>", LIGHTGRAY);
        } else {
            for (const SceneImageLayer& layer : state.adventure.currentScene.foregroundLayers) {
                DebugConsoleAddLine(
                        state,
                        "  " + layer.name + "  [" + (layer.visible ? "visible" : "hidden") + "]",
                        LIGHTGRAY);
            }
        }

        return true;
    }

    if (cmd == "/copylast") {
        int lineCount = -1; // -1 = all

        if (args.size() >= 2) {
            try {
                lineCount = std::stoi(args[1]);
            } catch (...) {
                DebugConsoleAddLine(state, "usage: /copylast [numLines]", RED);
                return true;
            }

            if (lineCount < 0) {
                DebugConsoleAddLine(state, "usage: /copylast [numLines]", RED);
                return true;
            }
        }

        const auto& lines = state.debug.console.lines;
        const int total = static_cast<int>(lines.size());

        int startIndex = 0;

        if (lineCount >= 0) {
            startIndex = std::max(0, total - lineCount);
        }

        std::string buffer;
        buffer.reserve(4096); // avoid too many reallocs

        for (int i = startIndex; i < total; ++i) {
            buffer += lines[i].text;
            buffer += '\n';
        }

        if (buffer.empty()) {
            DebugConsoleAddLine(state, "nothing to copy", LIGHTGRAY);
            return true;
        }

        SetClipboardText(buffer.c_str());

        if (lineCount < 0) {
            DebugConsoleAddLine(state, "copied entire console history to clipboard", SKYBLUE);
        } else {
            DebugConsoleAddLine(
                    state,
                    "copied last " + std::to_string(total - startIndex) + " lines to clipboard",
                    SKYBLUE);
        }

        return true;
    }

    DebugConsoleAddLine(state, "unknown command: " + cmd, RED);
    return true;
}
