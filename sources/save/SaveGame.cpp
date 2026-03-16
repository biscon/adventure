#include "save/SaveGame.h"

#include <filesystem>
#include <fstream>
#include <algorithm>

#include "utils/json.hpp"
#include "scene/SceneLoad.h"
#include "adventure/AdventureActorHelpers.h"
#include "adventure/Inventory.h"
#include "adventure/Dialogue.h"
#include "debug/DebugConsole.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace
{
    static constexpr int SAVE_VERSION = 1;

    struct SavedInventoryData {
        std::string actorId;
        std::vector<std::string> itemIds;
        std::string heldItemId;
        int pageStartIndex = 0;
    };

    struct SavedActorState {
        std::string actorId;
        Vector2 feetPos{};
        std::string facing;
        bool visible = true;
        bool activeInScene = true;
        std::string currentAnimation;
        bool flipX = false;
        float animationTimeMs = 0.0f;
    };

    struct SavedPropState {
        std::string id;
        Vector2 feetPos{};
        bool visible = true;
        bool flipX = false;
        std::string currentAnimation;
        float animationTimeMs = 0.0f;
    };

    struct SaveRestoreData {
        std::string sceneId;
        std::string controlledActorId;
        bool controlsEnabled = true;

        std::unordered_map<std::string, bool> flags;
        std::unordered_map<std::string, int> ints;
        std::unordered_map<std::string, std::string> strings;

        std::vector<SavedInventoryData> inventories;
        std::vector<SavedActorState> actors;
        std::vector<SavedPropState> props;
    };

    static std::string NormalizePath(const fs::path& p)
    {
        return p.lexically_normal().string();
    }

    static fs::path GetSaveDirPath()
    {
        return fs::path("saves");
    }

    static fs::path GetSaveSlotPath(int slotIndex)
    {
        return GetSaveDirPath() / ("slot" + std::to_string(slotIndex) + ".json");
    }

    static bool EnsureSaveDirExists()
    {
        std::error_code ec;
        fs::create_directories(GetSaveDirPath(), ec);
        return !ec;
    }

    static const char* FacingToString(ActorFacing facing)
    {
        switch (facing) {
            case ActorFacing::Left:  return "left";
            case ActorFacing::Right: return "right";
            case ActorFacing::Back:  return "back";
            case ActorFacing::Front:
            default:                 return "front";
        }
    }

    static ActorFacing StringToActorFacing(const std::string& s)
    {
        if (s == "left") {
            return ActorFacing::Left;
        }
        if (s == "right") {
            return ActorFacing::Right;
        }
        if (s == "back") {
            return ActorFacing::Back;
        }
        return ActorFacing::Front;
    }

    static json SerializeVector2(Vector2 v)
    {
        json j;
        j["x"] = v.x;
        j["y"] = v.y;
        return j;
    }

    static Vector2 DeserializeVector2(const json& j)
    {
        Vector2 v{};
        v.x = j.value("x", 0.0f);
        v.y = j.value("y", 0.0f);
        return v;
    }

    static void SerializeScriptState(const GameState& state, json& outRoot)
    {
        json scriptState;
        scriptState["flags"] = json::object();
        scriptState["ints"] = json::object();
        scriptState["strings"] = json::object();

        for (const auto& [key, value] : state.script.flags) {
            scriptState["flags"][key] = value;
        }

        for (const auto& [key, value] : state.script.ints) {
            scriptState["ints"][key] = value;
        }

        for (const auto& [key, value] : state.script.strings) {
            scriptState["strings"][key] = value;
        }

        outRoot["scriptState"] = scriptState;
    }

    static void SerializeInventories(const GameState& state, json& outRoot)
    {
        outRoot["inventories"] = json::array();

        for (const ActorInventoryData& inv : state.adventure.actorInventories) {
            json j;
            j["actorId"] = inv.actorId;
            j["itemIds"] = inv.itemIds;
            j["heldItemId"] = inv.heldItemId;
            j["pageStartIndex"] = inv.pageStartIndex;
            outRoot["inventories"].push_back(j);
        }
    }

    static void SerializeActors(const GameState& state, json& outRoot)
    {
        outRoot["actors"] = json::array();

        for (const ActorInstance& actor : state.adventure.actors) {
            json j;
            j["actorId"] = actor.actorId;
            j["feetPos"] = SerializeVector2(actor.feetPos);
            j["facing"] = FacingToString(actor.facing);
            j["visible"] = actor.visible;
            j["activeInScene"] = actor.activeInScene;
            j["currentAnimation"] = actor.currentAnimation;
            j["flipX"] = actor.flipX;
            j["animationTimeMs"] = actor.animationTimeMs;
            outRoot["actors"].push_back(j);
        }
    }

    static void SerializeProps(const GameState& state, json& outRoot)
    {
        outRoot["props"] = json::array();

        const int count = std::min(
                static_cast<int>(state.adventure.currentScene.props.size()),
                static_cast<int>(state.adventure.props.size()));

        for (int i = 0; i < count; ++i) {
            const ScenePropData& sceneProp = state.adventure.currentScene.props[i];
            const PropInstance& prop = state.adventure.props[i];

            json j;
            j["id"] = sceneProp.id;
            j["feetPos"] = SerializeVector2(prop.feetPos);
            j["visible"] = prop.visible;
            j["flipX"] = prop.flipX;
            j["currentAnimation"] = prop.currentAnimation;
            j["animationTimeMs"] = prop.animationTimeMs;
            outRoot["props"].push_back(j);
        }
    }

    static bool ParseSaveFile(const fs::path& savePath, SaveRestoreData& outData)
    {
        outData = {};

        json root;
        {
            std::ifstream in(savePath);
            if (!in.is_open()) {
                TraceLog(LOG_ERROR, "Failed to open save file: %s", savePath.string().c_str());
                return false;
            }
            in >> root;
        }

        const int version = root.value("version", 0);
        if (version != SAVE_VERSION) {
            TraceLog(LOG_ERROR,
                     "Unsupported save version %d in file: %s",
                     version,
                     savePath.string().c_str());
            return false;
        }

        outData.sceneId = root.value("sceneId", "");
        outData.controlledActorId = root.value("controlledActorId", "");
        outData.controlsEnabled = root.value("controlsEnabled", true);

        if (outData.sceneId.empty()) {
            TraceLog(LOG_ERROR, "Save file missing sceneId: %s", savePath.string().c_str());
            return false;
        }

        if (root.contains("scriptState") && root["scriptState"].is_object()) {
            const json& scriptState = root["scriptState"];

            if (scriptState.contains("flags") && scriptState["flags"].is_object()) {
                for (auto it = scriptState["flags"].begin(); it != scriptState["flags"].end(); ++it) {
                    outData.flags[it.key()] = it.value().get<bool>();
                }
            }

            if (scriptState.contains("ints") && scriptState["ints"].is_object()) {
                for (auto it = scriptState["ints"].begin(); it != scriptState["ints"].end(); ++it) {
                    outData.ints[it.key()] = it.value().get<int>();
                }
            }

            if (scriptState.contains("strings") && scriptState["strings"].is_object()) {
                for (auto it = scriptState["strings"].begin(); it != scriptState["strings"].end(); ++it) {
                    outData.strings[it.key()] = it.value().get<std::string>();
                }
            }
        }

        if (root.contains("inventories") && root["inventories"].is_array()) {
            for (const json& j : root["inventories"]) {
                SavedInventoryData inv;
                inv.actorId = j.value("actorId", "");
                inv.itemIds = j.value("itemIds", std::vector<std::string>{});
                inv.heldItemId = j.value("heldItemId", "");
                inv.pageStartIndex = j.value("pageStartIndex", 0);

                if (!inv.actorId.empty()) {
                    outData.inventories.push_back(inv);
                }
            }
        }

        if (root.contains("actors") && root["actors"].is_array()) {
            for (const json& j : root["actors"]) {
                SavedActorState actor;
                actor.actorId = j.value("actorId", "");
                actor.feetPos = j.contains("feetPos") ? DeserializeVector2(j["feetPos"]) : Vector2{};
                actor.facing = j.value("facing", "front");
                actor.visible = j.value("visible", true);
                actor.activeInScene = j.value("activeInScene", true);
                actor.currentAnimation = j.value("currentAnimation", "");
                actor.flipX = j.value("flipX", false);
                actor.animationTimeMs = j.value("animationTimeMs", 0.0f);

                if (!actor.actorId.empty()) {
                    outData.actors.push_back(actor);
                }
            }
        }

        if (root.contains("props") && root["props"].is_array()) {
            for (const json& j : root["props"]) {
                SavedPropState prop;
                prop.id = j.value("id", "");
                prop.feetPos = j.contains("feetPos") ? DeserializeVector2(j["feetPos"]) : Vector2{};
                prop.visible = j.value("visible", true);
                prop.flipX = j.value("flipX", false);
                prop.currentAnimation = j.value("currentAnimation", "");
                prop.animationTimeMs = j.value("animationTimeMs", 0.0f);

                if (!prop.id.empty()) {
                    outData.props.push_back(prop);
                }
            }
        }

        return true;
    }

    static void ClearTransientUiAndRuntimeState(GameState& state)
    {
        state.adventure.pendingInteraction = {};
        state.adventure.actionQueue = {};
        state.adventure.speechUi = {};
        state.adventure.hoverUi = {};
        state.adventure.dialogueUi = {};

        state.adventure.inventoryUi.open = false;
        state.adventure.inventoryUi.openAmount = 0.0f;
        state.adventure.inventoryUi.closeDelayRemainingMs = 0.0f;
        state.adventure.inventoryUi.hoveringInventory = false;
        state.adventure.inventoryUi.hoveredSlotIndex = -1;
        state.adventure.inventoryUi.hoveringPrevPage = false;
        state.adventure.inventoryUi.hoveringNextPage = false;
        state.adventure.inventoryUi.pickupPopup = {};

        state.adventure.hasLastClickWorldPos = false;
        state.adventure.hasLastResolvedTargetPos = false;
        state.adventure.debugTrianglePath.clear();
    }

    static void RestoreScriptState(GameState& state, const SaveRestoreData& data)
    {
        state.script.flags = data.flags;
        state.script.ints = data.ints;
        state.script.strings = data.strings;
    }

    static void RestoreInventories(GameState& state, const SaveRestoreData& data)
    {
        state.adventure.actorInventories.clear();

        for (const SavedInventoryData& saved : data.inventories) {
            ActorInventoryData inv;
            inv.actorId = saved.actorId;
            inv.itemIds = saved.itemIds;
            inv.heldItemId = saved.heldItemId;
            inv.pageStartIndex = saved.pageStartIndex;
            state.adventure.actorInventories.push_back(inv);
        }
    }

    static void RestoreControlledActor(GameState& state, const SaveRestoreData& data)
    {
        if (data.controlledActorId.empty()) {
            return;
        }

        const int actorIndex = FindActorInstanceIndexById(state, data.controlledActorId);
        if (actorIndex >= 0) {
            state.adventure.controlledActorIndex = actorIndex;
        }
    }

    static void RestoreActors(GameState& state, const SaveRestoreData& data)
    {
        for (const SavedActorState& saved : data.actors) {
            const int actorIndex = FindActorInstanceIndexById(state, saved.actorId);
            if (actorIndex < 0 ||
                actorIndex >= static_cast<int>(state.adventure.actors.size())) {
                continue;
            }

            ActorInstance& actor = state.adventure.actors[actorIndex];
            actor.feetPos = saved.feetPos;
            actor.facing = StringToActorFacing(saved.facing);
            actor.visible = saved.visible;
            actor.activeInScene = saved.activeInScene;
            actor.flipX = saved.flipX;
            actor.animationTimeMs = saved.animationTimeMs;

            if (!saved.currentAnimation.empty()) {
                actor.currentAnimation = saved.currentAnimation;
            }

            actor.path = {};
            actor.scriptAnimationActive = false;
            actor.scriptAnimationDurationMs = 0.0f;
        }
    }

    static int FindScenePropIndexById(const GameState& state, const std::string& propId)
    {
        for (int i = 0; i < static_cast<int>(state.adventure.currentScene.props.size()); ++i) {
            if (state.adventure.currentScene.props[i].id == propId) {
                return i;
            }
        }
        return -1;
    }

    static void RestoreProps(GameState& state, const SaveRestoreData& data)
    {
        for (const SavedPropState& saved : data.props) {
            const int propIndex = FindScenePropIndexById(state, saved.id);
            if (propIndex < 0 ||
                propIndex >= static_cast<int>(state.adventure.props.size())) {
                continue;
            }

            PropInstance& prop = state.adventure.props[propIndex];
            prop.feetPos = saved.feetPos;
            prop.visible = saved.visible;
            prop.flipX = saved.flipX;
            prop.animationTimeMs = saved.animationTimeMs;

            if (!saved.currentAnimation.empty()) {
                prop.currentAnimation = saved.currentAnimation;
            }

            prop.oneShotActive = false;
            prop.oneShotDurationMs = 0.0f;
            prop.moveActive = false;
            prop.moveStartPos = prop.feetPos;
            prop.moveTargetPos = prop.feetPos;
            prop.moveElapsedMs = 0.0f;
            prop.moveDurationMs = 0.0f;
        }
    }

    static bool ApplySaveRestoreData(GameState& state, const SaveRestoreData& data)
    {
        if (!LoadSceneById(state, data.sceneId.c_str())) {
            TraceLog(LOG_ERROR, "Failed to load save scene: %s", data.sceneId.c_str());
            return false;
        }

        RestoreScriptState(state, data);
        RestoreInventories(state, data);
        RestoreActors(state, data);
        RestoreProps(state, data);
        RestoreControlledActor(state, data);

        state.adventure.controlsEnabled = data.controlsEnabled;
        ClearTransientUiAndRuntimeState(state);
        state.mode = GameMode::Game;
        return true;
    }
}

bool SaveGameToSlot(GameState& state, int slotIndex)
{
    if (slotIndex < 1) {
        TraceLog(LOG_ERROR, "Invalid save slot index: %d", slotIndex);
        return false;
    }

    if (!state.adventure.currentScene.loaded) {
        TraceLog(LOG_ERROR, "Cannot save without a loaded scene");
        return false;
    }

    if (!EnsureSaveDirExists()) {
        TraceLog(LOG_ERROR, "Failed to create save directory");
        return false;
    }

    json root;
    root["version"] = SAVE_VERSION;
    root["sceneId"] = state.adventure.currentScene.sceneId;
    root["controlsEnabled"] = state.adventure.controlsEnabled;

    const ActorInstance* controlledActor = GetControlledActor(state);
    root["controlledActorId"] = controlledActor != nullptr ? controlledActor->actorId : "";

    SerializeScriptState(state, root);
    SerializeInventories(state, root);
    SerializeActors(state, root);
    SerializeProps(state, root);

    const fs::path savePath = GetSaveSlotPath(slotIndex);
    std::ofstream out(savePath);
    if (!out.is_open()) {
        TraceLog(LOG_ERROR, "Failed to open save slot for writing: %s", savePath.string().c_str());
        return false;
    }

    out << root.dump(4);

    TraceLog(LOG_INFO, "Saved game to slot %d: %s", slotIndex, savePath.string().c_str());
    return true;
}

bool LoadGameFromSlot(GameState& state, int slotIndex)
{
    if (slotIndex < 1) {
        TraceLog(LOG_ERROR, "Invalid load slot index: %d", slotIndex);
        return false;
    }

    const fs::path savePath = GetSaveSlotPath(slotIndex);
    SaveRestoreData data;
    if (!ParseSaveFile(savePath, data)) {
        return false;
    }

    return ApplySaveRestoreData(state, data);
}

bool DoesSaveSlotExist(int slotIndex)
{
    if (slotIndex < 1) {
        return false;
    }

    const fs::path savePath = GetSaveSlotPath(slotIndex);
    return fs::exists(savePath) && fs::is_regular_file(savePath);
}

std::string GetSaveSlotSummary(int slotIndex)
{
    if (slotIndex < 1) {
        return "Invalid";
    }

    const fs::path savePath = GetSaveSlotPath(slotIndex);
    if (!fs::exists(savePath) || !fs::is_regular_file(savePath)) {
        return "Empty";
    }

    json root;
    {
        std::ifstream in(savePath);
        if (!in.is_open()) {
            return "Unreadable";
        }
        in >> root;
    }

    const std::string sceneId = root.value("sceneId", "");
    if (sceneId.empty()) {
        return "Corrupt";
    }

    return sceneId;
}
