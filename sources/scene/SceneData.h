#pragma once

#include <string>
#include <vector>
#include "raylib.h"
#include "resources/ResourceData.h"
#include "nav/NavMeshData.h"

enum class SceneFacing {
    Left,
    Right,
    Front,
    Back
};

struct ScenePolygon {
    std::vector<Vector2> vertices;
};

struct SceneSpawnPoint {
    std::string id;
    Vector2 position{};
    SceneFacing facing = SceneFacing::Front;
};

struct SceneHotspot {
    std::string id;
    std::string displayName;
    std::string lookText;

    ScenePolygon shape{};

    Vector2 walkTo{};
    SceneFacing facing = SceneFacing::Front;
};

struct SceneExit {
    std::string id;
    std::string displayName;
    std::string lookText;

    ScenePolygon shape{};

    Vector2 walkTo{};
    SceneFacing facing = SceneFacing::Front;

    std::string targetScene;
    std::string targetSpawn;
};

struct SceneImageLayer {
    std::string name;
    std::string imagePath;

    TextureHandle textureHandle = -1;

    Vector2 worldPos{};
    Vector2 sourceSize{};
    Vector2 worldSize{};

    float parallaxX = 1.0f;
    float parallaxY = 1.0f;

    bool visible = true;
    float opacity = 1.0f;
};

enum class ScenePropVisualType {
    None,
    Sprite,
    Image
};

enum class ScenePropDepthMode {
    DepthSorted,
    Back,
    Front
};

struct ScenePropData {
    std::string id;

    ScenePropVisualType visualType = ScenePropVisualType::None;

    SpriteAssetHandle spriteAssetHandle = -1;
    TextureHandle textureHandle = -1;

    Vector2 feetPos{};

    std::string defaultAnimation;
    bool flipX = false;
    bool visible = true;
    bool depthScaling = false;
    ScenePropDepthMode depthMode = ScenePropDepthMode::DepthSorted;
};

struct SceneActorPlacement {
    std::string actorId;
    Vector2 position{};
    SceneFacing facing = SceneFacing::Front;
    bool visible = true;
};

struct SceneScaleConfig {
    float nearY = 0.0f;
    float farY = 1080.0f;

    float nearScale = 1.0f;
    float farScale = 1.0f;
};

struct SceneData {
    std::string sceneId;
    std::string sceneFilePath;
    std::string tiledFilePath;
    std::string playerActorAssetPath;
    std::string script;

    int baseAssetScale = 1;

    float worldWidth = 1920.0f;
    float worldHeight = 1080.0f;

    Vector2 playerSpawn{};

    SceneScaleConfig scaleConfig{};

    std::vector<SceneImageLayer> backgroundLayers;
    std::vector<SceneImageLayer> foregroundLayers;

    NavMeshData navMesh;

    std::vector<SceneSpawnPoint> spawns;
    std::vector<SceneHotspot> hotspots;
    std::vector<SceneExit> exits;
    std::vector<ScenePropData> props;
    std::vector<SceneActorPlacement> actorPlacements;

    bool loaded = false;
};
