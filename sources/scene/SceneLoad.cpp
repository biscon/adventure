#include "SceneLoad.h"

#include <filesystem>
#include <fstream>
#include <unordered_set>

#include "utils/json.hpp"
#include "scene/TiledImport.h"
#include "resources/AsepriteAsset.h"
#include "nav/NavMeshBuild.h"
#include "raylib.h"
#include "scripting/ScriptSystem.h"
#include "adventure/AdventureActorHelpers.h"
#include "adventure/ActorDefinitionAsset.h"
#include "adventure/Inventory.h"
#include "resources/Resources.h"
#include "audio/Audio.h"
#include "resources/TextureAsset.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string NormalizePath(const fs::path& p)
{
    return p.lexically_normal().string();
}

static void ReleasePreparedSceneImages(PreparedSceneLoadData& prepared)
{
    for (PreparedSceneImageData& img : prepared.images) {
        if (img.image.data != nullptr) {
            UnloadImage(img.image);
            img.image = {};
        }
    }

    prepared.images.clear();
}

static bool LoadPreparedImage(const std::string& path, PreparedSceneImageData& outImage)
{
    outImage = {};
    outImage.path = path;
    outImage.image = LoadImage(path.c_str());

    if (outImage.image.data == nullptr) {
        TraceLog(LOG_ERROR, "Failed loading image on scene worker thread: %s", path.c_str());
        return false;
    }

    ImageAlphaPremultiply(&outImage.image);
    return true;
}

static bool PrepareSceneImages(const SceneData& scene, PreparedSceneLoadData& prepared)
{
    std::unordered_set<std::string> seenPaths;

    auto AddUniqueImage = [&](const std::string& path) -> bool
    {
        if (path.empty()) {
            return false;
        }

        if (seenPaths.find(path) != seenPaths.end()) {
            return true;
        }

        seenPaths.insert(path);

        PreparedSceneImageData img;
        if (!LoadPreparedImage(path, img)) {
            prepared.errorMessage = "Failed loading image: " + path;
            return false;
        }

        prepared.images.push_back(std::move(img));
        return true;
    };

    for (const SceneImageLayer& layer : scene.backgroundLayers) {
        if (!AddUniqueImage(layer.imagePath)) {
            return false;
        }
    }

    for (const SceneImageLayer& layer : scene.foregroundLayers) {
        if (!AddUniqueImage(layer.imagePath)) {
            return false;
        }
    }

    for (const SceneEffectSpriteData& effect : scene.effectSprites) {
        if (!AddUniqueImage(effect.imagePath)) {
            return false;
        }
    }

    for (const ScenePropData& prop : scene.props) {
        if (prop.visualType == ScenePropVisualType::Image) {
            if (!AddUniqueImage(prop.assetPath)) {
                return false;
            }
        }
    }

    return true;
}

static bool PrepareSceneLoadData(const char* sceneId,
                                 const char* spawnId,
                                 PreparedSceneLoadData& outPrepared)
{
    outPrepared = {};
    outPrepared.requestedSpawnId = (spawnId != nullptr) ? spawnId : "";

    const fs::path sceneFilePath = fs::path(ASSETS_PATH "scenes") / sceneId / "scene.json";
    const std::string sceneFileNorm = NormalizePath(sceneFilePath);

    json root;
    {
        std::ifstream in(sceneFileNorm);
        if (!in.is_open()) {
            outPrepared.errorMessage = "Failed to open scene file: " + sceneFileNorm;
            return false;
        }
        in >> root;
    }

    SceneData scene;
    scene.sceneId = root.value("sceneId", std::string(sceneId));
    scene.saveName = root.value("saveName", scene.sceneId);
    scene.sceneFilePath = sceneFileNorm;
    scene.baseAssetScale = root.value("baseAssetScale", 1);
    scene.worldWidth = root.value("worldWidth", 1920.0f);
    scene.worldHeight = root.value("worldHeight", 1080.0f);

    if (!root.contains("script")) {
        outPrepared.errorMessage = "Scene missing script: " + sceneFileNorm;
        return false;
    }

    scene.script = root.value("script", "");
    if (scene.script.empty()) {
        outPrepared.errorMessage = "Scene script attribute must not be empty: " + sceneFileNorm;
        return false;
    }

    if (root.contains("playerSpawn")) {
        scene.playerSpawn.x = root["playerSpawn"].value("x", 0.0f);
        scene.playerSpawn.y = root["playerSpawn"].value("y", 0.0f);
    }

    if (root.contains("scale")) {
        const auto& s = root["scale"];
        scene.scaleConfig.nearY = s.value("nearY", 0.0f);
        scene.scaleConfig.farY = s.value("farY", 1080.0f);
        scene.scaleConfig.nearScale = s.value("nearScale", 1.0f);
        scene.scaleConfig.farScale = s.value("farScale", 1.0f);
    }

    const fs::path sceneDir = fs::path(sceneFileNorm).parent_path();
    outPrepared.sceneDir = NormalizePath(sceneDir);

    const std::string tiledFileRel = root.value("tiledFile", "");
    if (tiledFileRel.empty()) {
        outPrepared.errorMessage = "Scene missing tiledFile: " + sceneFileNorm;
        return false;
    }

    const fs::path tiledPath = (sceneDir / tiledFileRel).lexically_normal();
    if (!ImportTiledSceneIntoSceneData(scene, tiledPath.string().c_str())) {
        outPrepared.errorMessage = "Failed to import Tiled scene: " + tiledPath.string();
        return false;
    }

    if (!BuildNavMesh(scene.navMesh)) {
        outPrepared.errorMessage = "Failed to build navmesh for scene: " + scene.sceneId;
        return false;
    }

    if (!PrepareSceneImages(scene, outPrepared)) {
        if (outPrepared.errorMessage.empty()) {
            outPrepared.errorMessage = "Failed preparing scene images";
        }
        ReleasePreparedSceneImages(outPrepared);
        return false;
    }

    outPrepared.scriptPath = NormalizePath((sceneDir / scene.script).lexically_normal());
    scene.loaded = true;
    outPrepared.scene = std::move(scene);
    outPrepared.success = true;
    return true;
}

static bool ResolvePreparedSceneResourceHandles(
        SceneData& scene,
        ResourceData& resources,
        PreparedSceneLoadData& prepared)
{
    std::unordered_map<std::string, TextureHandle> handleByPath;

    for (PreparedSceneImageData& img : prepared.images) {
        TextureHandle handle = LoadTextureAssetFromImage(
                resources,
                img.path.c_str(),
                img.image,
                ResourceScope::Scene);

        if (handle < 0) {
            TraceLog(LOG_ERROR, "Failed uploading prepared image to texture: %s", img.path.c_str());
            return false;
        }

        handleByPath[img.path] = handle;
    }

    for (SceneImageLayer& layer : scene.backgroundLayers) {
        auto it = handleByPath.find(layer.imagePath);
        if (it == handleByPath.end()) {
            TraceLog(LOG_ERROR, "Prepared background texture missing for path: %s", layer.imagePath.c_str());
            return false;
        }
        layer.textureHandle = it->second;
    }

    for (SceneImageLayer& layer : scene.foregroundLayers) {
        auto it = handleByPath.find(layer.imagePath);
        if (it == handleByPath.end()) {
            TraceLog(LOG_ERROR, "Prepared foreground texture missing for path: %s", layer.imagePath.c_str());
            return false;
        }
        layer.textureHandle = it->second;
    }

    for (SceneEffectSpriteData& effect : scene.effectSprites) {
        auto it = handleByPath.find(effect.imagePath);
        if (it == handleByPath.end()) {
            TraceLog(LOG_ERROR, "Prepared effect texture missing for path: %s", effect.imagePath.c_str());
            return false;
        }
        effect.textureHandle = it->second;
    }

    for (ScenePropData& prop : scene.props) {
        if (prop.visualType == ScenePropVisualType::Sprite) {
            prop.spriteAssetHandle = LoadSpriteAsset(resources, prop.assetPath.c_str(), ResourceScope::Scene);
            if (prop.spriteAssetHandle < 0) {
                TraceLog(LOG_ERROR, "Failed loading sprite prop asset for %s: %s",
                         prop.id.c_str(), prop.assetPath.c_str());
                return false;
            }
        } else if (prop.visualType == ScenePropVisualType::Image) {
            auto it = handleByPath.find(prop.assetPath);
            if (it == handleByPath.end()) {
                TraceLog(LOG_ERROR, "Prepared prop image missing for path: %s", prop.assetPath.c_str());
                return false;
            }
            prop.textureHandle = it->second;
        }
    }

    return true;
}

bool IsAsyncSceneLoadActive(const GameState& state)
{
    return state.adventure.sceneLoadJob.state == SceneLoadJobState::Running ||
           state.adventure.sceneLoadJob.state == SceneLoadJobState::Succeeded ||
           state.adventure.sceneLoadJob.state == SceneLoadJobState::Failed;
}

static void JoinSceneLoadWorker(SceneLoadJobData& job)
{
    if (job.worker.joinable()) {
        job.worker.join();
    }
}

static void ResetSceneLoadJob(SceneLoadJobData& job)
{
    job.state = SceneLoadJobState::Idle;
    job.sceneId.clear();
    job.spawnId.clear();
    job.prepared = {};
}

void CancelAsyncSceneLoad(GameState& state)
{
    SceneLoadJobData& job = state.adventure.sceneLoadJob;

    JoinSceneLoadWorker(job);
    ReleasePreparedSceneImages(job.prepared);
    ResetSceneLoadJob(job);
}

bool BeginAsyncSceneLoad(GameState& state, const char* sceneId, const char* spawnId)
{
    SceneLoadJobData& job = state.adventure.sceneLoadJob;
    if (job.state == SceneLoadJobState::Running) {
        return false;
    }

    CancelAsyncSceneLoad(state);

    job.sceneId = (sceneId != nullptr) ? sceneId : "";
    job.spawnId = (spawnId != nullptr) ? spawnId : "";
    job.state = SceneLoadJobState::Running;

    const std::string sceneIdCopy = job.sceneId;
    const std::string spawnIdCopy = job.spawnId;

    job.worker = std::thread([&job, sceneIdCopy, spawnIdCopy]() {
        PreparedSceneLoadData prepared;
        const bool ok = PrepareSceneLoadData(
                sceneIdCopy.c_str(),
                spawnIdCopy.empty() ? nullptr : spawnIdCopy.c_str(),
                prepared);

        std::lock_guard<std::mutex> lock(job.mutex);
        job.prepared = std::move(prepared);
        job.state = ok ? SceneLoadJobState::Succeeded : SceneLoadJobState::Failed;
    });

    return true;
}


static const SceneSpawnPoint* FindSpawnById(const SceneData& scene, const std::string& spawnId)
{
    for (const auto& spawn : scene.spawns) {
        if (spawn.id == spawnId) {
            return &spawn;
        }
    }
    return nullptr;
}

static bool ResolveSceneResourceHandles(SceneData& scene, ResourceData& resources)
{
    for (SceneImageLayer& layer : scene.backgroundLayers) {
        if (layer.imagePath.empty()) {
            TraceLog(LOG_ERROR, "Background layer missing image path: %s", layer.name.c_str());
            return false;
        }

        layer.textureHandle = LoadTextureAsset(resources, layer.imagePath.c_str(), ResourceScope::Scene);
        if (layer.textureHandle < 0) {
            TraceLog(LOG_ERROR, "Failed loading background layer texture: %s", layer.imagePath.c_str());
            return false;
        }
    }

    for (SceneImageLayer& layer : scene.foregroundLayers) {
        if (layer.imagePath.empty()) {
            TraceLog(LOG_ERROR, "Foreground layer missing image path: %s", layer.name.c_str());
            return false;
        }

        layer.textureHandle = LoadTextureAsset(resources, layer.imagePath.c_str(), ResourceScope::Scene);
        if (layer.textureHandle < 0) {
            TraceLog(LOG_ERROR, "Failed loading foreground layer texture: %s", layer.imagePath.c_str());
            return false;
        }
    }

    for (SceneEffectSpriteData& effect : scene.effectSprites) {
        if (effect.imagePath.empty()) {
            TraceLog(LOG_ERROR, "Effect missing image path: %s", effect.id.c_str());
            return false;
        }

        effect.textureHandle = LoadTextureAsset(resources, effect.imagePath.c_str(), ResourceScope::Scene);
        if (effect.textureHandle < 0) {
            TraceLog(LOG_ERROR, "Failed loading effect texture: %s", effect.imagePath.c_str());
            return false;
        }
    }

    for (ScenePropData& prop : scene.props) {
        if (prop.assetPath.empty()) {
            TraceLog(LOG_ERROR, "Prop missing asset path: %s", prop.id.c_str());
            return false;
        }

        if (prop.visualType == ScenePropVisualType::Sprite) {
            prop.spriteAssetHandle = LoadSpriteAsset(resources, prop.assetPath.c_str(), ResourceScope::Scene);
            if (prop.spriteAssetHandle < 0) {
                TraceLog(LOG_ERROR, "Failed loading sprite prop asset for %s: %s",
                         prop.id.c_str(), prop.assetPath.c_str());
                return false;
            }
        } else if (prop.visualType == ScenePropVisualType::Image) {
            prop.textureHandle = LoadTextureAsset(resources, prop.assetPath.c_str(), ResourceScope::Scene);
            if (prop.textureHandle < 0) {
                TraceLog(LOG_ERROR, "Failed loading image prop asset for %s: %s",
                         prop.id.c_str(), prop.assetPath.c_str());
                return false;
            }
        }
    }

    return true;
}

static void ApplyActorFacingFromSceneFacing(ActorInstance& actor, SceneFacing facing)
{
    switch (facing) {
        case SceneFacing::Left:
            actor.facing = ActorFacing::Left;
            actor.currentAnimation = "idle_right";
            actor.flipX = true;
            break;

        case SceneFacing::Right:
            actor.facing = ActorFacing::Right;
            actor.currentAnimation = "idle_right";
            actor.flipX = false;
            break;

        case SceneFacing::Back:
            actor.facing = ActorFacing::Back;
            actor.currentAnimation = "idle_back";
            actor.flipX = false;
            break;

        case SceneFacing::Front:
        default:
            actor.facing = ActorFacing::Front;
            actor.currentAnimation = "idle_front";
            actor.flipX = false;
            break;
    }

    actor.animationTimeMs = 0.0f;
    actor.inIdleState = true;
    actor.stoppedTimeMs = actor.idleDelayMs;
}

static void InitializeActorInstanceFromDefinition(
        ActorInstance& actor,
        const ActorDefinitionData& def)
{
    actor.actorId = def.actorId;
    actor.actorDefIndex = -1; // caller sets this correctly after creation if needed
    actor.activeInScene = true;
    actor.visible = true;

    actor.walkSpeed = def.walkSpeed;
    actor.fastMoveMultiplier = def.fastMoveMultiplier;
    actor.idleDelayMs = def.idleDelayMs;

    actor.currentAnimation = "idle_front";
    actor.flipX = false;
    actor.animationTimeMs = 0.0f;
    actor.stoppedTimeMs = 0.0f;
    actor.inIdleState = true;
    actor.path = {};
    actor.scriptAnimationActive = false;
    actor.scriptAnimationDurationMs = 0.0f;
}

static bool CommitPreparedSceneLoad(GameState& state, PreparedSceneLoadData& prepared)
{
    SceneData scene = std::move(prepared.scene);

    if (!ResolvePreparedSceneResourceHandles(scene, state.resources, prepared)) {
        return false;
    }

    const SceneSpawnPoint* chosenSpawn = nullptr;

    if (!prepared.requestedSpawnId.empty()) {
        chosenSpawn = FindSpawnById(scene, prepared.requestedSpawnId);
        if (chosenSpawn == nullptr) {
            TraceLog(LOG_WARNING,
                     "Requested spawn '%s' not found in scene '%s'",
                     prepared.requestedSpawnId.c_str(),
                     scene.sceneId.c_str());
        }
    }

    if (chosenSpawn == nullptr) {
        chosenSpawn = FindSpawnById(scene, "default");
    }

    state.adventure.controlsEnabled = true;
    state.adventure.currentScene = scene;

    if (!LoadSceneAudioDefinitions(state, prepared.sceneDir)) {
        TraceLog(LOG_ERROR, "Failed loading scene audio definitions: %s", prepared.sceneDir.c_str());
        return false;
    }

    state.adventure.props.clear();
    state.adventure.props.reserve(state.adventure.currentScene.props.size());

    for (int i = 0; i < static_cast<int>(state.adventure.currentScene.props.size()); ++i) {
        const ScenePropData& sceneProp = state.adventure.currentScene.props[i];

        PropInstance prop;
        prop.handle = state.adventure.nextPropHandle++;
        prop.scenePropIndex = i;
        prop.feetPos = sceneProp.feetPos;
        prop.visible = sceneProp.visible;
        prop.flipX = sceneProp.flipX;
        prop.currentAnimation = sceneProp.defaultAnimation;
        prop.animationTimeMs = 0.0f;
        prop.oneShotActive = false;
        prop.oneShotDurationMs = 0.0f;
        prop.moveActive = false;
        prop.moveStartPos = prop.feetPos;
        prop.moveTargetPos = prop.feetPos;
        prop.moveElapsedMs = 0.0f;
        prop.moveDurationMs = 0.0f;
        prop.moveInterpolation = PropMoveInterpolation::Linear;

        state.adventure.props.push_back(prop);
    }

    state.adventure.effectSprites.clear();
    state.adventure.effectSprites.reserve(state.adventure.currentScene.effectSprites.size());

    for (int i = 0; i < static_cast<int>(state.adventure.currentScene.effectSprites.size()); ++i) {
        const SceneEffectSpriteData& sceneEffect = state.adventure.currentScene.effectSprites[i];

        EffectSpriteInstance effect;
        effect.handle = state.adventure.nextEffectSpriteHandle++;
        effect.sceneEffectSpriteIndex = i;
        effect.visible = sceneEffect.visible;
        effect.opacity = sceneEffect.opacity;
        effect.tint = sceneEffect.tint;

        state.adventure.effectSprites.push_back(effect);
    }

    BuildSceneSoundEmitters(state);

    state.adventure.actors.clear();
    state.adventure.controlledActorIndex = -1;

    {
        int controlledDefIndex = -1;
        if (!EnsureActorDefinitionLoaded(state, "main_actor", &controlledDefIndex)) {
            TraceLog(LOG_ERROR, "Failed to load controlled actor definition: main_actor");
            return false;
        }

        const ActorDefinitionData* controlledDef =
                FindActorDefinitionByIndex(state, controlledDefIndex);
        if (controlledDef == nullptr) {
            TraceLog(LOG_ERROR, "Controlled actor definition missing after load: main_actor");
            return false;
        }

        ActorInstance actor{};
        actor.handle = state.adventure.nextActorHandle++;
        InitializeActorInstanceFromDefinition(actor, *controlledDef);
        actor.actorDefIndex = controlledDefIndex;

        if (chosenSpawn != nullptr) {
            actor.feetPos = chosenSpawn->position;
            ApplyActorFacingFromSceneFacing(actor, chosenSpawn->facing);
        } else {
            actor.feetPos = scene.playerSpawn;
            ApplyActorFacingFromSceneFacing(actor, SceneFacing::Front);
        }

        state.adventure.actors.push_back(actor);
        state.adventure.controlledActorIndex = 0;
        if (controlledDef->controllable &&
            FindActorInventoryByActorId(state, actor.actorId) == nullptr) {
            ActorInventoryData inv;
            inv.actorId = actor.actorId;
            state.adventure.actorInventories.push_back(inv);
        }
    }

    for (const SceneActorPlacement& placement : state.adventure.currentScene.actorPlacements) {
        if (placement.actorId == "main_actor") {
            TraceLog(LOG_WARNING,
                     "Ignoring actor placement for controlled actor '%s'; player uses scene spawns",
                     placement.actorId.c_str());
            continue;
        }

        int defIndex = -1;
        if (!EnsureActorDefinitionLoaded(state, placement.actorId, &defIndex)) {
            TraceLog(LOG_ERROR,
                     "Failed to load actor definition for scene actor: %s",
                     placement.actorId.c_str());
            continue;
        }

        const ActorDefinitionData* def = FindActorDefinitionByIndex(state, defIndex);
        if (def == nullptr) {
            TraceLog(LOG_ERROR,
                     "Missing actor definition after load for scene actor: %s",
                     placement.actorId.c_str());
            continue;
        }

        ActorInstance actor{};
        actor.handle = state.adventure.nextActorHandle++;
        InitializeActorInstanceFromDefinition(actor, *def);
        actor.actorDefIndex = defIndex;
        actor.feetPos = placement.position;
        actor.visible = placement.visible;
        actor.activeInScene = true;
        ApplyActorFacingFromSceneFacing(actor, placement.facing);

        state.adventure.actors.push_back(actor);
        if (def->controllable &&
            FindActorInventoryByActorId(state, actor.actorId) == nullptr) {
            ActorInventoryData inv;
            inv.actorId = actor.actorId;
            state.adventure.actorInventories.push_back(inv);
        }
    }

    state.adventure.camera = {};
    state.adventure.camera.viewportWidth = 1920.0f;
    state.adventure.camera.viewportHeight = 1080.0f;
    state.adventure.camera.position = { 0.0f, 0.0f };

    ScriptSystemInit(state);

    TraceLog(LOG_INFO, "Running scene script: %s", prepared.scriptPath.c_str());
    if (!ScriptSystemRunFile(state.script, prepared.scriptPath)) {
        TraceLog(LOG_ERROR, "Failed to run scene script: %s", prepared.scriptPath.c_str());
        return false;
    }

    ScriptSystemCallHook(state, "Scene_onEnter");

    TraceLog(LOG_INFO, "Loaded scene: %s", state.adventure.currentScene.sceneId.c_str());
    TraceLog(LOG_INFO,
             "Navmesh built: vertices=%d triangles=%d",
             static_cast<int>(state.adventure.currentScene.navMesh.vertices.size()),
             static_cast<int>(state.adventure.currentScene.navMesh.triangles.size()));

    return true;
}

void UnloadCurrentScene(GameState& state)
{
    state.adventure.currentScene = {};
    state.adventure.props.clear();
    state.adventure.effectSprites.clear();
    state.adventure.camera = {};
    state.adventure.pendingInteraction = {};
    state.adventure.debugTrianglePath.clear();
    state.adventure.hasLastClickWorldPos = false;
    state.adventure.hasLastResolvedTargetPos = false;
    state.adventure.actors.clear();
    state.adventure.controlledActorIndex = -1;
    state.adventure.actorDefinitions.clear();
    state.adventure.dialogueUi = {};

    ClearSceneAudio(state);
    UnloadSceneResources(state.resources);
}

bool LoadSceneById(GameState& state, const char* sceneId, SceneLoadMode loadMode)
{
    (void)loadMode;

    if (state.adventure.currentScene.loaded) {
        ScriptSystemCallHook(state, "Scene_onExit");
        ScriptSystemShutdown(state.script);
    }

    UnloadCurrentScene(state);

    PreparedSceneLoadData prepared;
    if (!PrepareSceneLoadData(sceneId, state.adventure.pendingSpawnId.c_str(), prepared)) {
        TraceLog(LOG_ERROR, "%s", prepared.errorMessage.c_str());
        ReleasePreparedSceneImages(prepared);
        return false;
    }

    const bool ok = CommitPreparedSceneLoad(state, prepared);
    ReleasePreparedSceneImages(prepared);
    return ok;
}

void PumpAsyncSceneLoad(GameState& state)
{
    SceneLoadJobData& job = state.adventure.sceneLoadJob;

    if (state.adventure.hasPendingSceneLoad && job.state == SceneLoadJobState::Idle) {
        const std::string sceneId = state.adventure.pendingSceneId;
        const std::string spawnId = state.adventure.pendingSpawnId;

        state.adventure.pendingSceneId.clear();
        state.adventure.pendingSpawnId.clear();
        state.adventure.hasPendingSceneLoad = false;

        if (!BeginAsyncSceneLoad(
                state,
                sceneId.c_str(),
                spawnId.empty() ? nullptr : spawnId.c_str())) {
            TraceLog(LOG_ERROR, "Failed starting async scene load: %s", sceneId.c_str());
            state.mode = GameMode::Menu;
        }

        return;
    }

    if (job.state == SceneLoadJobState::Running) {
        return;
    }

    if (job.state == SceneLoadJobState::Succeeded || job.state == SceneLoadJobState::Failed) {
        JoinSceneLoadWorker(job);

        PreparedSceneLoadData prepared;
        {
            std::lock_guard<std::mutex> lock(job.mutex);
            prepared = std::move(job.prepared);
        }

        const bool workerSucceeded = (job.state == SceneLoadJobState::Succeeded);
        ResetSceneLoadJob(job);

        if (!workerSucceeded) {
            TraceLog(LOG_ERROR, "%s", prepared.errorMessage.c_str());
            ReleasePreparedSceneImages(prepared);
            state.mode = GameMode::Menu;
            return;
        }

        if (state.adventure.currentScene.loaded) {
            ScriptSystemCallHook(state, "Scene_onExit");
            ScriptSystemShutdown(state.script);
        }

        UnloadCurrentScene(state);

        const bool ok = CommitPreparedSceneLoad(state, prepared);
        ReleasePreparedSceneImages(prepared);

        if (ok) {
            state.mode = GameMode::Game;
        } else {
            TraceLog(LOG_ERROR, "Scene load commit failed");
            state.mode = GameMode::Menu;
        }
    }
}
