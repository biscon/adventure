#include "audio/Audio.h"

#include <filesystem>
#include <algorithm>
#include <cmath>
#include "audio/AudioAsset.h"
#include "resources/Resources.h"
#include "adventure/AdventureActorHelpers.h"
#include "raylib.h"

static AudioDefinitionData* FindAudioDef(GameState& state, const std::string& id)
{
    auto it = state.audio.defIndexById.find(id);
    if (it == state.audio.defIndexById.end()) {
        return nullptr;
    }

    const int index = it->second;
    if (index < 0 || index >= static_cast<int>(state.audio.definitions.size())) {
        return nullptr;
    }

    return &state.audio.definitions[index];
}

static void WarnMissingAudioIdOnce(GameState& state, const std::string& id)
{
    for (const std::string& existing : state.audio.missingIdsLogged) {
        if (existing == id) {
            return;
        }
    }

    state.audio.missingIdsLogged.push_back(id);
    TraceLog(LOG_WARNING, "Audio id not found: %s", id.c_str());
}

static void RebuildAudioDefIndex(GameState& state)
{
    state.audio.defIndexById.clear();

    for (int i = 0; i < static_cast<int>(state.audio.definitions.size()); ++i) {
        state.audio.defIndexById[state.audio.definitions[i].id] = i;
    }
}

static void StopEmitterSound(SoundEmitterInstance& emitter)
{
    if (!emitter.active) {
        return;
    }

    StopSound(emitter.sound);
    UnloadSoundAlias(emitter.sound);
    emitter.sound = {};
    emitter.active = false;
}

static float Clamp01(float t)
{
    if (t < 0.0f) return 0.0f;
    if (t > 1.0f) return 1.0f;
    return t;
}

static void UpdateSceneSoundEmitters(GameState& state)
{
    if (!state.adventure.currentScene.loaded) {
        for (SoundEmitterInstance& emitter : state.audio.sceneEmitters) {
            StopEmitterSound(emitter);
        }
        return;
    }

    const ActorInstance* listener = GetControlledActor(state);
    if (listener == nullptr) {
        for (SoundEmitterInstance& emitter : state.audio.sceneEmitters) {
            StopEmitterSound(emitter);
        }
        return;
    }

    const float maxPanAmount = 0.35f;

    for (SoundEmitterInstance& emitter : state.audio.sceneEmitters) {
        if (emitter.sceneEmitterIndex < 0 ||
            emitter.sceneEmitterIndex >= static_cast<int>(state.adventure.currentScene.soundEmitters.size())) {
            StopEmitterSound(emitter);
            continue;
        }

        const SceneSoundEmitterData& sceneEmitter =
                state.adventure.currentScene.soundEmitters[emitter.sceneEmitterIndex];

        if (!emitter.enabled || sceneEmitter.radius <= 0.0f) {
            StopEmitterSound(emitter);
            continue;
        }

        AudioDefinitionData* def = FindAudioDef(state, sceneEmitter.soundId);
        if (def == nullptr) {
            WarnMissingAudioIdOnce(state, sceneEmitter.soundId);
            StopEmitterSound(emitter);
            continue;
        }

        if (def->type != AudioType::Sound) {
            TraceLog(LOG_WARNING,
                     "Sound emitter '%s' references non-sound audio id '%s'",
                     sceneEmitter.id.c_str(),
                     sceneEmitter.soundId.c_str());
            StopEmitterSound(emitter);
            continue;
        }

        Sound* base = GetSoundResource(state, def->soundHandle);
        if (base == nullptr) {
            TraceLog(LOG_WARNING,
                     "Sound emitter '%s' missing sound resource for audio id '%s'",
                     sceneEmitter.id.c_str(),
                     sceneEmitter.soundId.c_str());
            StopEmitterSound(emitter);
            continue;
        }

        const float dx = sceneEmitter.position.x - listener->feetPos.x;
        const float dy = sceneEmitter.position.y - listener->feetPos.y;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (dist >= sceneEmitter.radius) {
            StopEmitterSound(emitter);
            continue;
        }

        // linear attenuation
        const float atten = 1.0f - Clamp01(dist / sceneEmitter.radius);
        // more realistic exponential falloff
        //const float atten = std::pow(1.0f - Clamp01(dist / sceneEmitter.radius), 2.0f);

        const float finalVolume =
                def->volume *
                state.settings.soundVolume *
                emitter.volume *
                atten;

        float pan = 0.5f;
        if (sceneEmitter.pan) {
            //float normPan = dx / sceneEmitter.radius;
            float normPan = -dx / sceneEmitter.radius;
            if (normPan < -1.0f) normPan = -1.0f;
            if (normPan > 1.0f) normPan = 1.0f;

            pan = 0.5f + normPan * maxPanAmount;
            if (pan < 0.0f) pan = 0.0f;
            if (pan > 1.0f) pan = 1.0f;
        }

        if (!emitter.active) {
            emitter.sound = LoadSoundAlias(*base);
            if (emitter.sound.frameCount <= 0) {
                TraceLog(LOG_ERROR,
                         "Failed creating sound alias for emitter '%s'",
                         sceneEmitter.id.c_str());
                emitter.sound = {};
                emitter.active = false;
                continue;
            }

            emitter.active = true;
            SetSoundVolume(emitter.sound, finalVolume);
            SetSoundPan(emitter.sound, pan);
            PlaySound(emitter.sound);
        } else {
            if (!IsSoundPlaying(emitter.sound)) {
                PlaySound(emitter.sound);
            }

            SetSoundVolume(emitter.sound, finalVolume);
            SetSoundPan(emitter.sound, pan);
        }
    }
}

void InitAudio(GameState& state)
{
    if (state.audio.initialized) {
        return;
    }

    InitAudioDevice();

    state.audio = {};
    state.audio.initialized = true;

    std::vector<AudioDefinitionData> defs;
    if (!LoadAudioDefinitions(ASSETS_PATH "audio/audio.json", defs)) {
        TraceLog(LOG_WARNING, "Global audio definitions not loaded");
        return;
    }

    for (AudioDefinitionData& def : defs) {
        def.scope = ResourceScope::Global;
    }

    state.audio.definitions = std::move(defs);
    RebuildAudioDefIndex(state);

    for (int i = 0; i < static_cast<int>(state.audio.definitions.size()); ++i) {
        AudioDefinitionData& def = state.audio.definitions[i];

        if (def.type == AudioType::Sound) {
            def.soundHandle = LoadSoundResource(state, def.filePath, ResourceScope::Global);
            if (def.soundHandle < 0) {
                TraceLog(LOG_ERROR,
                         "Failed resolving sound resource for audio id '%s' (%s)",
                         def.id.c_str(),
                         def.filePath.c_str());
            }
        } else {
            def.musicHandle = LoadMusicResource(state, def.filePath, ResourceScope::Global);
            if (def.musicHandle < 0) {
                TraceLog(LOG_ERROR,
                         "Failed resolving music resource for audio id '%s' (%s)",
                         def.id.c_str(),
                         def.filePath.c_str());
            }
        }
    }

    TraceLog(LOG_INFO,
             "Audio initialized with %d global definitions",
             static_cast<int>(state.audio.definitions.size()));
}

void ShutdownAudio(GameState& state)
{
    if (!state.audio.initialized) {
        return;
    }

    for (ActiveSoundInstance& inst : state.audio.activeSounds) {
        if (inst.active) {
            StopSound(inst.sound);
            UnloadSoundAlias(inst.sound);
            inst.sound = {};
            inst.active = false;
        }
    }

    state.audio.activeSounds.clear();
    state.audio.music = {};
    state.audio.sceneEmitters.clear();

    ClearSceneAudio(state);
    CloseAudioDevice();

    state.audio = {};

    TraceLog(LOG_INFO, "Audio shutdown");
}

void UpdateAudio(GameState& state, float /*dt*/)
{
    if (!state.audio.initialized) {
        return;
    }

    if (state.audio.music.playing && state.audio.music.musicHandle >= 0) {
        Music* music = GetMusicResource(state, state.audio.music.musicHandle);
        if (music != nullptr) {
            UpdateMusicStream(*music);
        }
    }

    for (auto it = state.audio.activeSounds.begin(); it != state.audio.activeSounds.end(); ) {
        if (!it->active || !IsSoundPlaying(it->sound)) {
            if (it->active) {
                UnloadSoundAlias(it->sound);
            }
            it = state.audio.activeSounds.erase(it);
        } else {
            ++it;
        }
    }
    UpdateSceneSoundEmitters(state);
}

bool PlaySoundById(GameState& state, const std::string& id)
{
    AudioDefinitionData* def = FindAudioDef(state, id);
    if (def == nullptr) {
        WarnMissingAudioIdOnce(state, id);
        return false;
    }

    if (def->type != AudioType::Sound) {
        TraceLog(LOG_WARNING, "Audio id '%s' is not a sound", id.c_str());
        return false;
    }

    Sound* base = GetSoundResource(state, def->soundHandle);
    if (base == nullptr) {
        TraceLog(LOG_WARNING, "Sound resource missing for audio id '%s'", id.c_str());
        return false;
    }

    Sound alias = LoadSoundAlias(*base);
    if (alias.frameCount <= 0) {
        TraceLog(LOG_ERROR, "Failed creating sound alias for audio id '%s'", id.c_str());
        return false;
    }

    const float volume = def->volume * state.settings.soundVolume;
    SetSoundVolume(alias, volume);
    PlaySound(alias);

    ActiveSoundInstance inst;
    inst.sound = alias;
    inst.baseSoundHandle = def->soundHandle;
    inst.active = true;
    state.audio.activeSounds.push_back(inst);

    return true;
}

bool PlayMusicById(GameState& state, const std::string& id)
{
    AudioDefinitionData* def = FindAudioDef(state, id);
    if (def == nullptr) {
        WarnMissingAudioIdOnce(state, id);
        return false;
    }

    if (def->type != AudioType::Music) {
        TraceLog(LOG_WARNING, "Audio id '%s' is not music", id.c_str());
        return false;
    }

    Music* music = GetMusicResource(state, def->musicHandle);
    if (music == nullptr) {
        TraceLog(LOG_WARNING, "Music resource missing for audio id '%s'", id.c_str());
        return false;
    }

    if (state.audio.music.playing && state.audio.music.musicHandle >= 0) {
        Music* oldMusic = GetMusicResource(state, state.audio.music.musicHandle);
        if (oldMusic != nullptr) {
            StopMusicStream(*oldMusic);
        }
    }

    const float volume = def->volume * state.settings.musicVolume;
    SetMusicVolume(*music, volume);
    PlayMusicStream(*music);

    state.audio.music.musicHandle = def->musicHandle;
    state.audio.music.playing = true;
    state.audio.music.volume = volume;

    return true;
}

void StopMusic(GameState& state, float /*fadeMs*/)
{
    if (!state.audio.music.playing || state.audio.music.musicHandle < 0) {
        return;
    }

    Music* music = GetMusicResource(state, state.audio.music.musicHandle);
    if (music != nullptr) {
        StopMusicStream(*music);
    }

    state.audio.music = {};
}

bool LoadSceneAudioDefinitions(GameState& state, const std::string& sceneDir)
{
    namespace fs = std::filesystem;

    const fs::path audioJsonPath = fs::path(sceneDir) / "audio.json";
    if (!fs::exists(audioJsonPath)) {
        return true;
    }

    std::vector<AudioDefinitionData> defs;
    if (!LoadAudioDefinitions(audioJsonPath.lexically_normal().string(), defs)) {
        TraceLog(LOG_ERROR, "Failed loading scene audio definitions: %s", audioJsonPath.string().c_str());
        return false;
    }

    for (AudioDefinitionData& def : defs) {
        def.scope = ResourceScope::Scene;

        if (state.audio.defIndexById.find(def.id) != state.audio.defIndexById.end()) {
            TraceLog(LOG_ERROR, "Scene audio id collides with existing audio id: %s", def.id.c_str());
            return false;
        }

        if (def.type == AudioType::Sound) {
            def.soundHandle = LoadSoundResource(state, def.filePath, ResourceScope::Scene);
            if (def.soundHandle < 0) {
                TraceLog(LOG_ERROR,
                         "Failed resolving scene sound resource for audio id '%s' (%s)",
                         def.id.c_str(),
                         def.filePath.c_str());
                return false;
            }
        } else {
            def.musicHandle = LoadMusicResource(state, def.filePath, ResourceScope::Scene);
            if (def.musicHandle < 0) {
                TraceLog(LOG_ERROR,
                         "Failed resolving scene music resource for audio id '%s' (%s)",
                         def.id.c_str(),
                         def.filePath.c_str());
                return false;
            }
        }

        state.audio.definitions.push_back(def);
    }

    RebuildAudioDefIndex(state);

    TraceLog(LOG_INFO,
             "Loaded scene audio definitions: %d from %s",
             static_cast<int>(defs.size()),
             audioJsonPath.string().c_str());

    return true;
}

void BuildSceneSoundEmitters(GameState& state)
{
    state.audio.sceneEmitters.clear();
    state.audio.sceneEmitters.reserve(state.adventure.currentScene.soundEmitters.size());

    for (int i = 0; i < static_cast<int>(state.adventure.currentScene.soundEmitters.size()); ++i) {
        const SceneSoundEmitterData& sceneEmitter = state.adventure.currentScene.soundEmitters[i];

        SoundEmitterInstance inst;
        inst.sceneEmitterIndex = i;
        inst.enabled = sceneEmitter.enabled;
        inst.volume = sceneEmitter.volume;
        inst.active = false;
        inst.sound = {};

        state.audio.sceneEmitters.push_back(inst);
    }
}

void ClearSceneAudio(GameState& state)
{
    for (SoundEmitterInstance& emitter : state.audio.sceneEmitters) {
        if (emitter.active) {
            StopSound(emitter.sound);
            UnloadSoundAlias(emitter.sound);
            emitter.sound = {};
            emitter.active = false;
        }
    }

    state.audio.sceneEmitters.clear();

    bool stopCurrentMusic = false;
    if (state.audio.music.playing && state.audio.music.musicHandle >= 0) {
        AudioDefinitionData* playingDef = nullptr;

        for (AudioDefinitionData& def : state.audio.definitions) {
            if (def.type == AudioType::Music && def.musicHandle == state.audio.music.musicHandle) {
                playingDef = &def;
                break;
            }
        }

        if (playingDef != nullptr && playingDef->scope == ResourceScope::Scene) {
            stopCurrentMusic = true;
        }
    }

    if (stopCurrentMusic) {
        StopMusic(state);
    }

    state.audio.definitions.erase(
            std::remove_if(
                    state.audio.definitions.begin(),
                    state.audio.definitions.end(),
                    [](const AudioDefinitionData& def) {
                        return def.scope == ResourceScope::Scene;
                    }),
            state.audio.definitions.end());

    RebuildAudioDefIndex(state);
}
