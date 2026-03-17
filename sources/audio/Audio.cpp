#include "audio/Audio.h"

#include "audio/AudioAsset.h"
#include "resources/Resources.h"
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

    state.audio.definitions = std::move(defs);
    state.audio.defIndexById.clear();

    for (int i = 0; i < static_cast<int>(state.audio.definitions.size()); ++i) {
        AudioDefinitionData& def = state.audio.definitions[i];
        state.audio.defIndexById[def.id] = i;

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
