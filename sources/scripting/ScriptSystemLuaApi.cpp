#include <sstream>
#include "scripting/ScriptSystemInternal.h"
#include "adventure/Adventure.h"
#include "scripting/ScriptSystem.h"
#include "ui/TalkColors.h"
#include "adventure/Inventory.h"
#include "debug/DebugConsole.h"
#include "adventure/Dialogue.h"
#include "raymath.h"
#include "audio/Audio.h"

static bool ParseOptionalTalkColorAndDuration(
        lua_State* L,
        int firstOptionalArgIndex,
        Color& outColor,
        bool& outHasColor,
        int& outDurationMs)
{
    outColor = WHITE;
    outHasColor = false;
    outDurationMs = -1;

    const int top = lua_gettop(L);
    if (top < firstOptionalArgIndex) {
        return true;
    }

    const int type = lua_type(L, firstOptionalArgIndex);
    if (type == LUA_TSTRING) {
        const char* colorName = lua_tostring(L, firstOptionalArgIndex);
        if (colorName == nullptr || !TryGetTalkColorByName(std::string(colorName), outColor)) {
            return false;
        }

        outHasColor = true;

        if (top >= firstOptionalArgIndex + 1) {
            outDurationMs = static_cast<int>(luaL_checkinteger(L, firstOptionalArgIndex + 1));
        }

        return true;
    }

    if (type == LUA_TNUMBER) {
        outDurationMs = static_cast<int>(luaL_checkinteger(L, firstOptionalArgIndex));
        return true;
    }

    if (type == LUA_TNIL) {
        return true;
    }

    return false;
}

static bool ParseOptionalStringList(
        lua_State* L,
        int argIndex,
        std::vector<std::string>& outValues)
{
    outValues.clear();

    const int top = lua_gettop(L);
    if (top < argIndex || lua_isnoneornil(L, argIndex)) {
        return true;
    }

    if (!lua_istable(L, argIndex)) {
        return false;
    }

    const lua_Integer len = luaL_len(L, argIndex);
    for (lua_Integer i = 1; i <= len; ++i) {
        lua_geti(L, argIndex, i);

        if (!lua_isstring(L, -1)) {
            lua_pop(L, 1);
            return false;
        }

        const char* value = lua_tostring(L, -1);
        outValues.push_back(value != nullptr ? std::string(value) : std::string());
        lua_pop(L, 1);
    }

    return true;
}


// async helper
static int Lua_WaitContinuation(lua_State* L, int status, lua_KContext ctx)
{
    (void)status;
    (void)ctx;

    lua_pushboolean(L, 1);
    return 1;
}

static int Lua_DialogueContinuation(lua_State* L, int status, lua_KContext ctx)
{
    (void)status;
    (void)ctx;

    if (lua_gettop(L) >= 1) {
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

int Lua_consolePrint(lua_State* L)
{
    const int argCount = lua_gettop(L);

    std::ostringstream out;
    for (int i = 1; i <= argCount; ++i) {
        if (i > 1) {
            out << "    ";
        }

        const int type = lua_type(L, i);
        switch (type) {
            case LUA_TNIL:
                out << "nil";
                break;

            case LUA_TBOOLEAN:
                out << (lua_toboolean(L, i) ? "true" : "false");
                break;

            case LUA_TNUMBER:
                if (lua_isinteger(L, i)) {
                    out << static_cast<long long>(lua_tointeger(L, i));
                } else {
                    out << lua_tonumber(L, i);
                }
                break;

            case LUA_TSTRING:
            {
                const char* s = lua_tostring(L, i);
                out << (s != nullptr ? s : "");
                break;
            }

            default:
                out << "<" << lua_typename(L, type) << ">";
                break;
        }
    }

    const std::string text = out.str();
    TraceLog(LOG_INFO, "[LUA] %s", text.c_str());
    ScriptSystemConsolePrint(text);
    return 0;
}

static int Lua_setFlag(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    const bool value = lua_toboolean(L, 2) != 0;

    gameState->script.flags[std::string(name)] = value;
    lua_pushboolean(L, 1);
    return 1;
}

static int Lua_flag(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);

    bool value = false;
    auto it = gameState->script.flags.find(std::string(name));
    if (it != gameState->script.flags.end()) {
        value = it->second;
    }

    lua_pushboolean(L, value ? 1 : 0);
    return 1;
}

static int Lua_setInt(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    const int value = static_cast<int>(luaL_checkinteger(L, 2));

    gameState->script.ints[std::string(name)] = value;
    lua_pushboolean(L, 1);
    return 1;
}

static int Lua_getInt(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);

    int value = -1;
    auto it = gameState->script.ints.find(std::string(name));
    if (it != gameState->script.ints.end()) {
        value = it->second;
    }

    lua_pushinteger(L, value);
    return 1;
}

static int Lua_setString(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    const char* value = luaL_checkstring(L, 2);

    gameState->script.strings[std::string(name)] = std::string(value);
    lua_pushboolean(L, 1);
    return 1;
}

static int Lua_getString(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);

    std::string value;
    auto it = gameState->script.strings.find(std::string(name));
    if (it != gameState->script.strings.end()) {
        value = it->second;
    }

    lua_pushstring(L, value.c_str());
    return 1;
}

static int Lua_hasItem(lua_State* L)
{
    const char* itemId = luaL_checkstring(L, 1);

    if (gameState == nullptr || itemId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool hasItem = ControlledActorHasItem(*gameState, std::string(itemId));
    lua_pushboolean(L, hasItem ? 1 : 0);
    return 1;
}

static int Lua_giveItem(lua_State* L)
{
    const char* itemId = luaL_checkstring(L, 1);

    if (gameState == nullptr || itemId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = GiveItem(*gameState, std::string(itemId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_removeItem(lua_State* L)
{
    const char* itemId = luaL_checkstring(L, 1);

    if (gameState == nullptr || itemId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = RemoveItem(*gameState, std::string(itemId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_actorHasItem(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const char* itemId = luaL_checkstring(L, 2);

    if (gameState == nullptr || actorId == nullptr || itemId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool hasItem = ActorHasItem(*gameState, std::string(actorId), std::string(itemId));
    lua_pushboolean(L, hasItem ? 1 : 0);
    return 1;
}

static int Lua_giveItemTo(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const char* itemId = luaL_checkstring(L, 2);

    if (gameState == nullptr || actorId == nullptr || itemId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = GiveItemToActor(*gameState, std::string(actorId), std::string(itemId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_removeItemFrom(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const char* itemId = luaL_checkstring(L, 2);

    if (gameState == nullptr || actorId == nullptr || itemId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = RemoveItemFromActor(*gameState, std::string(actorId), std::string(itemId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_say(lua_State* L)
{
    const char* text = luaL_checkstring(L, 1);

    Color color{};
    bool hasColor = false;
    int durationMs = -1;
    if (!ParseOptionalTalkColorAndDuration(L, 2, color, hasColor, durationMs)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (gameState == nullptr || text == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSay(*gameState, std::string(text), durationMs);
    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingWait(L, ScriptWaitType::SpeechComplete, 0.0f)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}


static int Lua_sayProp(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const char* text = luaL_checkstring(L, 2);

    Color color{};
    bool hasColor = false;
    int durationMs = -1;
    if (!ParseOptionalTalkColorAndDuration(L, 3, color, hasColor, durationMs)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (gameState == nullptr || propId == nullptr || text == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSayProp(
            *gameState,
            std::string(propId),
            std::string(text),
            hasColor ? &color : nullptr,
            durationMs);

    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingWait(L, ScriptWaitType::SpeechComplete, 0.0f)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_sayAt(lua_State* L)
{
    const float x = static_cast<float>(luaL_checknumber(L, 1));
    const float y = static_cast<float>(luaL_checknumber(L, 2));
    const char* text = luaL_checkstring(L, 3);

    Color color = WHITE;
    bool hasColor = false;
    int durationMs = -1;
    if (!ParseOptionalTalkColorAndDuration(L, 4, color, hasColor, durationMs)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (gameState == nullptr || text == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSayAt(
            *gameState,
            Vector2{x, y},
            std::string(text),
            hasColor ? color : WHITE,
            durationMs);

    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingWait(L, ScriptWaitType::SpeechComplete, 0.0f)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_walkTo(lua_State* L)
{
    const float x = static_cast<float>(luaL_checknumber(L, 1));
    const float y = static_cast<float>(luaL_checknumber(L, 2));

    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptWalkTo(*gameState, Vector2{x, y});
    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingWait(L, ScriptWaitType::WalkComplete, 0.0f)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_walkToHotspot(lua_State* L)
{
    const char* id = luaL_checkstring(L, 1);

    if (gameState == nullptr || id == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptWalkToHotspot(*gameState, std::string(id));
    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingWait(L, ScriptWaitType::WalkComplete, 0.0f)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_walkToExit(lua_State* L)
{
    const char* id = luaL_checkstring(L, 1);

    if (gameState == nullptr || id == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptWalkToExit(*gameState, std::string(id));
    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingWait(L, ScriptWaitType::WalkComplete, 0.0f)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_delay(lua_State* L)
{
    const float ms = static_cast<float>(luaL_checknumber(L, 1));

    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingWait(L, ScriptWaitType::DelayMs, ms)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_face(lua_State* L)
{
    const char* facing = luaL_checkstring(L, 1);

    if (gameState == nullptr || facing == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptFace(*gameState, std::string(facing));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_changeScene(lua_State* L)
{
    const char* sceneId = luaL_checkstring(L, 1);
    const char* spawnId = luaL_optstring(L, 2, "");

    if (gameState == nullptr || sceneId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptChangeScene(
            *gameState,
            std::string(sceneId),
            std::string(spawnId != nullptr ? spawnId : ""));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_log(lua_State* L)
{
    const char* text = luaL_checkstring(L, 1);
    const std::string msg = (text != nullptr) ? std::string(text) : std::string();

    TraceLog(LOG_INFO, "[LUA] %s", msg.c_str());
    ScriptSystemConsolePrint(msg);
    return 0;
}

static int Lua_logf(lua_State* L)
{
    const char* fmt = luaL_checkstring(L, 1);
    const int argCount = lua_gettop(L);

    lua_getglobal(L, "string");
    lua_getfield(L, -1, "format");
    lua_remove(L, -2);

    lua_pushstring(L, fmt);
    for (int i = 2; i <= argCount; ++i) {
        lua_pushvalue(L, i);
    }

    if (lua_pcall(L, argCount, 1, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        const std::string msg = std::string("[LUA] logf format error: ") +
                                (err != nullptr ? err : "<unknown>");
        TraceLog(LOG_ERROR, "%s", msg.c_str());
        ScriptSystemConsolePrint(msg);
        lua_pop(L, 1);
        return 0;
    }

    const char* text = lua_tostring(L, -1);
    const std::string msg = (text != nullptr) ? std::string(text) : std::string();

    TraceLog(LOG_INFO, "[LUA] %s", msg.c_str());
    ScriptSystemConsolePrint(msg);
    lua_pop(L, 1);
    return 0;
}

static int Lua_playAnimation(lua_State* L)
{
    const char* animationName = luaL_checkstring(L, 1);

    if (gameState == nullptr || animationName == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptPlayAnimation(*gameState, std::string(animationName));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_playPropAnimation(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const char* animationName = luaL_checkstring(L, 2);

    if (gameState == nullptr || propId == nullptr || animationName == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptPlayPropAnimation(
            *gameState,
            std::string(propId),
            std::string(animationName));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_setPropAnimation(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const char* animationName = luaL_checkstring(L, 2);

    if (gameState == nullptr || propId == nullptr || animationName == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetPropAnimation(
            *gameState,
            std::string(propId),
            std::string(animationName));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_setPropPosition(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const float x = static_cast<float>(luaL_checknumber(L, 2));
    const float y = static_cast<float>(luaL_checknumber(L, 3));

    if (gameState == nullptr || propId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetPropPosition(
            *gameState,
            std::string(propId),
            Vector2{x, y});

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_movePropTo(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const float x = static_cast<float>(luaL_checknumber(L, 2));
    const float y = static_cast<float>(luaL_checknumber(L, 3));
    const float durationMs = static_cast<float>(luaL_checknumber(L, 4));
    const char* interpolation = luaL_checkstring(L, 5);

    if (gameState == nullptr || propId == nullptr || interpolation == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptMovePropTo(
            *gameState,
            std::string(propId),
            Vector2{x, y},
            durationMs,
            std::string(interpolation));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_setPropVisible(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const bool visible = lua_toboolean(L, 2) != 0;

    if (gameState == nullptr || propId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetPropVisible(
            *gameState,
            std::string(propId),
            visible);

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_setPropFlipX(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const bool flipX = lua_toboolean(L, 2) != 0;

    if (gameState == nullptr || propId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetPropFlipX(
            *gameState,
            std::string(propId),
            flipX);

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_setPropPositionRelative(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const float dx = static_cast<float>(luaL_checknumber(L, 2));
    const float dy = static_cast<float>(luaL_checknumber(L, 3));

    if (gameState == nullptr || propId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetPropPositionRelative(
            *gameState,
            std::string(propId),
            Vector2{dx, dy});

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_movePropBy(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const float dx = static_cast<float>(luaL_checknumber(L, 2));
    const float dy = static_cast<float>(luaL_checknumber(L, 3));
    const float durationMs = static_cast<float>(luaL_checknumber(L, 4));
    const char* interpolation = luaL_checkstring(L, 5);

    if (gameState == nullptr || propId == nullptr || interpolation == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptMovePropBy(
            *gameState,
            std::string(propId),
            Vector2{dx, dy},
            durationMs,
            std::string(interpolation));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_controlActor(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);

    if (gameState == nullptr || actorId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptControlActor(*gameState, std::string(actorId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_sayActor(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const char* text = luaL_checkstring(L, 2);

    Color color{};
    bool hasColor = false;
    int durationMs = -1;
    if (!ParseOptionalTalkColorAndDuration(L, 3, color, hasColor, durationMs)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (gameState == nullptr || actorId == nullptr || text == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSayActor(
            *gameState,
            std::string(actorId),
            std::string(text),
            hasColor ? &color : nullptr,
            durationMs);

    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingWait(L, ScriptWaitType::SpeechComplete, 0.0f)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_walkActorTo(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const float x = static_cast<float>(luaL_checknumber(L, 2));
    const float y = static_cast<float>(luaL_checknumber(L, 3));

    if (gameState == nullptr || actorId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    int actorIndex = -1;
    const bool ok = AdventureScriptWalkActorTo(
            *gameState,
            std::string(actorId),
            Vector2{x, y},
            &actorIndex);

    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingActorWalkWait(L, actorIndex)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_walkActorToHotspot(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const char* hotspotId = luaL_checkstring(L, 2);

    if (gameState == nullptr || actorId == nullptr || hotspotId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    int actorIndex = -1;
    const bool ok = AdventureScriptWalkActorToHotspot(
            *gameState,
            std::string(actorId),
            std::string(hotspotId),
            &actorIndex);

    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingActorWalkWait(L, actorIndex)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_walkActorToExit(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const char* exitId = luaL_checkstring(L, 2);

    if (gameState == nullptr || actorId == nullptr || exitId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    int actorIndex = -1;
    const bool ok = AdventureScriptWalkActorToExit(
            *gameState,
            std::string(actorId),
            std::string(exitId),
            &actorIndex);

    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingActorWalkWait(L, actorIndex)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_faceActor(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const char* facing = luaL_checkstring(L, 2);

    if (gameState == nullptr || actorId == nullptr || facing == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptFaceActor(
            *gameState,
            std::string(actorId),
            std::string(facing));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_playActorAnimation(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const char* animationName = luaL_checkstring(L, 2);

    if (gameState == nullptr || actorId == nullptr || animationName == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptPlayActorAnimation(
            *gameState,
            std::string(actorId),
            std::string(animationName));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_setActorVisible(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const bool visible = lua_toboolean(L, 2) != 0;

    if (gameState == nullptr || actorId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetActorVisible(
            *gameState,
            std::string(actorId),
            visible);

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startWalkTo(lua_State* L)
{
    const float x = static_cast<float>(luaL_checknumber(L, 1));
    const float y = static_cast<float>(luaL_checknumber(L, 2));

    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptStartWalkTo(*gameState, Vector2{x, y});
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startWalkToHotspot(lua_State* L)
{
    const char* hotspotId = luaL_checkstring(L, 1);

    if (gameState == nullptr || hotspotId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptStartWalkToHotspot(*gameState, std::string(hotspotId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startWalkToExit(lua_State* L)
{
    const char* exitId = luaL_checkstring(L, 1);

    if (gameState == nullptr || exitId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptStartWalkToExit(*gameState, std::string(exitId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startWalkActorTo(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const float x = static_cast<float>(luaL_checknumber(L, 2));
    const float y = static_cast<float>(luaL_checknumber(L, 3));

    if (gameState == nullptr || actorId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptStartWalkActorTo(
            *gameState,
            std::string(actorId),
            Vector2{x, y});

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startWalkActorToHotspot(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const char* hotspotId = luaL_checkstring(L, 2);

    if (gameState == nullptr || actorId == nullptr || hotspotId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptStartWalkActorToHotspot(
            *gameState,
            std::string(actorId),
            std::string(hotspotId));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startWalkActorToExit(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const char* exitId = luaL_checkstring(L, 2);

    if (gameState == nullptr || actorId == nullptr || exitId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptStartWalkActorToExit(
            *gameState,
            std::string(actorId),
            std::string(exitId));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_disableControls(lua_State* L)
{
    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetControlsEnabled(*gameState, false);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_enableControls(lua_State* L)
{
    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetControlsEnabled(*gameState, true);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startScript(lua_State* L)
{
    const char* functionName = luaL_checkstring(L, 1);

    if (gameState == nullptr || functionName == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = ScriptSystemStartFunction(gameState->script, std::string(functionName));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_stopScript(lua_State* L)
{
    const char* functionName = luaL_checkstring(L, 1);

    if (gameState == nullptr || functionName == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    ScriptSystemStopFunction(gameState->script, std::string(functionName));
    lua_pushboolean(L, 1);
    return 1;
}

static int Lua_stopAllScripts(lua_State* L)
{
    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    ScriptSystemStopAll(gameState->script);
    lua_pushboolean(L, 1);
    return 1;
}

static int Lua_clearHeldItem(lua_State* L)
{
    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    ClearControlledActorHeldItem(*gameState);
    lua_pushboolean(L, 1);
    return 1;
}

static int Lua_setHeldItem(lua_State* L)
{
    const char* itemId = luaL_checkstring(L, 1);

    if (gameState == nullptr || itemId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = SetControlledActorHeldItem(*gameState, std::string(itemId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_dialogue(lua_State* L)
{
    const char* choiceSetId = luaL_checkstring(L, 1);

    std::vector<std::string> hiddenOptionIds;
    if (!ParseOptionalStringList(L, 2, hiddenOptionIds)) {
        lua_pushnil(L);
        return 1;
    }

    if (gameState == nullptr || choiceSetId == nullptr) {
        lua_pushnil(L);
        return 1;
    }

    const std::vector<std::string>* hiddenPtr =
            hiddenOptionIds.empty() ? nullptr : &hiddenOptionIds;

    const bool ok = StartDialogueChoice(*gameState, std::string(choiceSetId), hiddenPtr);
    if (!ok) {
        lua_pushnil(L);
        return 1;
    }

    if (!StartPendingDialogueWait(L)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_DialogueContinuation);
}

static int Lua_setEffectVisible(lua_State* L)
{
    const char* effectId = luaL_checkstring(L, 1);
    const bool visible = lua_toboolean(L, 2) != 0;

    if (gameState == nullptr || effectId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetEffectVisible(
            *gameState,
            std::string(effectId),
            visible);

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_effectVisible(lua_State* L)
{
    const char* effectId = luaL_checkstring(L, 1);

    if (gameState == nullptr || effectId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    bool visible = false;
    const bool ok = AdventureScriptIsEffectVisible(
            *gameState,
            std::string(effectId),
            visible);

    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, visible ? 1 : 0);
    return 1;
}

static int Lua_setEffectOpacity(lua_State* L)
{
    const char* effectId = luaL_checkstring(L, 1);
    const float opacity = static_cast<float>(luaL_checknumber(L, 2));

    if (gameState == nullptr || effectId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetEffectOpacity(
            *gameState,
            std::string(effectId),
            opacity);

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_setEffectTint(lua_State* L)
{
    const char* effectId = luaL_checkstring(L, 1);
    const int r = static_cast<int>(luaL_checkinteger(L, 2));
    const int g = static_cast<int>(luaL_checkinteger(L, 3));
    const int b = static_cast<int>(luaL_checkinteger(L, 4));
    const int a = static_cast<int>(luaL_optinteger(L, 5, 255));

    if (gameState == nullptr || effectId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    Color tint{};
    tint.r = static_cast<unsigned char>(Clamp(r, 0, 255));
    tint.g = static_cast<unsigned char>(Clamp(g, 0, 255));
    tint.b = static_cast<unsigned char>(Clamp(b, 0, 255));
    tint.a = static_cast<unsigned char>(Clamp(a, 0, 255));

    const bool ok = AdventureScriptSetEffectTint(
            *gameState,
            std::string(effectId),
            tint);

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_playSound(lua_State* L)
{
    const char* audioId = luaL_checkstring(L, 1);

    if (gameState == nullptr || audioId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptPlaySound(*gameState, std::string(audioId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_playMusic(lua_State* L)
{
    const char* audioId = luaL_checkstring(L, 1);

    if (gameState == nullptr || audioId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptPlayMusic(*gameState, std::string(audioId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_stopMusic(lua_State* L)
{
    const float fadeMs = static_cast<float>(luaL_optnumber(L, 1, 0.0));

    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptStopMusic(*gameState, fadeMs);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_setSoundEmitterEnabled(lua_State* L)
{
    const char* emitterId = luaL_checkstring(L, 1);
    const bool enabled = lua_toboolean(L, 2) != 0;

    if (gameState == nullptr || emitterId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetSoundEmitterEnabled(
            *gameState,
            std::string(emitterId),
            enabled);

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_soundEmitterEnabled(lua_State* L)
{
    const char* emitterId = luaL_checkstring(L, 1);

    if (gameState == nullptr || emitterId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    bool enabled = false;
    const bool ok = AdventureScriptGetSoundEmitterEnabled(
            *gameState,
            std::string(emitterId),
            enabled);

    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, enabled ? 1 : 0);
    return 1;
}

static int Lua_setSoundEmitterVolume(lua_State* L)
{
    const char* emitterId = luaL_checkstring(L, 1);
    const float volume = static_cast<float>(luaL_checknumber(L, 2));

    if (gameState == nullptr || emitterId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetSoundEmitterVolume(
            *gameState,
            std::string(emitterId),
            volume);

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

void RegisterLuaAPI(lua_State* L)
{
    lua_register(L, "setFlag", Lua_setFlag);
    lua_register(L, "flag", Lua_flag);
    lua_register(L, "setInt", Lua_setInt);
    lua_register(L, "getInt", Lua_getInt);
    lua_register(L, "setString", Lua_setString);
    lua_register(L, "getString", Lua_getString);

    lua_register(L, "hasItem", Lua_hasItem);
    lua_register(L, "giveItem", Lua_giveItem);
    lua_register(L, "removeItem", Lua_removeItem);

    lua_register(L, "actorHasItem", Lua_actorHasItem);
    lua_register(L, "giveItemTo", Lua_giveItemTo);
    lua_register(L, "removeItemFrom", Lua_removeItemFrom);
    lua_register(L, "clearHeldItem", Lua_clearHeldItem);
    lua_register(L, "setHeldItem", Lua_setHeldItem);

    lua_register(L, "say", Lua_say);
    lua_register(L, "sayProp", Lua_sayProp);
    lua_register(L, "sayAt", Lua_sayAt);
    lua_register(L, "dialogue", Lua_dialogue);
    lua_register(L, "walkTo", Lua_walkTo);
    lua_register(L, "walkToHotspot", Lua_walkToHotspot);
    lua_register(L, "walkToExit", Lua_walkToExit);
    lua_register(L, "delay", Lua_delay);
    lua_register(L, "face", Lua_face);
    lua_register(L, "changeScene", Lua_changeScene);
    lua_register(L, "playAnimation", Lua_playAnimation);
    lua_register(L, "playPropAnimation", Lua_playPropAnimation);
    lua_register(L, "setPropAnimation", Lua_setPropAnimation);
    lua_register(L, "setPropPosition", Lua_setPropPosition);
    lua_register(L, "movePropTo", Lua_movePropTo);

    lua_register(L, "setPropVisible", Lua_setPropVisible);
    lua_register(L, "setPropFlipX", Lua_setPropFlipX);
    lua_register(L, "setPropPositionRelative", Lua_setPropPositionRelative);
    lua_register(L, "movePropBy", Lua_movePropBy);

    lua_register(L, "controlActor", Lua_controlActor);
    lua_register(L, "sayActor", Lua_sayActor);
    lua_register(L, "walkActorTo", Lua_walkActorTo);
    lua_register(L, "walkActorToHotspot", Lua_walkActorToHotspot);
    lua_register(L, "walkActorToExit", Lua_walkActorToExit);
    lua_register(L, "faceActor", Lua_faceActor);
    lua_register(L, "playActorAnimation", Lua_playActorAnimation);
    lua_register(L, "setActorVisible", Lua_setActorVisible);

    lua_register(L, "startWalkTo", Lua_startWalkTo);
    lua_register(L, "startWalkToHotspot", Lua_startWalkToHotspot);
    lua_register(L, "startWalkToExit", Lua_startWalkToExit);

    lua_register(L, "startWalkActorTo", Lua_startWalkActorTo);
    lua_register(L, "startWalkActorToHotspot", Lua_startWalkActorToHotspot);
    lua_register(L, "startWalkActorToExit", Lua_startWalkActorToExit);

    lua_register(L, "disableControls", Lua_disableControls);
    lua_register(L, "enableControls", Lua_enableControls);

    lua_register(L, "startScript", Lua_startScript);
    lua_register(L, "stopScript", Lua_stopScript);
    lua_register(L, "stopAllScripts", Lua_stopAllScripts);

    lua_register(L, "setEffectVisible", Lua_setEffectVisible);
    lua_register(L, "effectVisible", Lua_effectVisible);
    lua_register(L, "setEffectOpacity", Lua_setEffectOpacity);
    lua_register(L, "setEffectTint", Lua_setEffectTint);

    lua_register(L, "playSound", Lua_playSound);
    lua_register(L, "playMusic", Lua_playMusic);
    lua_register(L, "stopMusic", Lua_stopMusic);

    lua_register(L, "setSoundEmitterEnabled", Lua_setSoundEmitterEnabled);
    lua_register(L, "soundEmitterEnabled", Lua_soundEmitterEnabled);
    lua_register(L, "setSoundEmitterVolume", Lua_setSoundEmitterVolume);

    lua_register(L, "print", Lua_consolePrint);
    lua_register(L, "log", Lua_log);
    lua_register(L, "logf", Lua_logf);
}

void RegisterLuaTalkColorGlobals(lua_State* L)
{
    for (int i = 0; i < GetTalkColorEntryCount(); ++i) {
        const TalkColorEntry& entry = GetTalkColorEntry(i);
        lua_pushstring(L, entry.name);
        lua_setglobal(L, entry.name);
    }
}
