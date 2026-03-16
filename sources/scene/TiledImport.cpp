#include "TiledImport.h"

#include <filesystem>
#include <fstream>
#include <optional>

#include "utils/json.hpp"
#include "resources/TextureAsset.h"
#include "resources/AsepriteAsset.h"
#include "raylib.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

static float GetFloatOrDefault(const json& j, const char* key, float defaultValue)
{
    auto it = j.find(key);
    if (it == j.end()) {
        return defaultValue;
    }

    if (it->is_number_float() || it->is_number_integer()) {
        return it->get<float>();
    }

    return defaultValue;
}

static std::string NormalizePath(const fs::path& p)
{
    return p.lexically_normal().string();
}

static const json* FindProperty(const json& obj, const char* name)
{
    auto it = obj.find("properties");
    if (it == obj.end() || !it->is_array()) {
        return nullptr;
    }

    for (const auto& prop : *it) {
        if (prop.value("name", "") == name) {
            return &prop;
        }
    }

    return nullptr;
}

static std::string GetStringPropertyOrDefault(const json& obj, const char* name, const char* defaultValue)
{
    const json* prop = FindProperty(obj, name);
    if (prop == nullptr) {
        return defaultValue;
    }
    return prop->value("value", std::string(defaultValue));
}

static bool GetFloatProperty(const json& obj, const char* name, float& outValue)
{
    const json* prop = FindProperty(obj, name);
    if (prop == nullptr) {
        return false;
    }

    if (prop->contains("value")) {
        const auto& v = (*prop)["value"];
        if (v.is_number_float() || v.is_number_integer()) {
            outValue = v.get<float>();
            return true;
        }
    }

    return false;
}

static bool GetBoolProperty(const json& obj, const char* name, bool& outValue)
{
    const json* prop = FindProperty(obj, name);
    if (prop == nullptr) {
        return false;
    }

    if (prop->contains("value")) {
        const auto& v = (*prop)["value"];
        if (v.is_boolean()) {
            outValue = v.get<bool>();
            return true;
        }
    }

    return false;
}

static bool ParseScenePropVisualType(const std::string& s, ScenePropVisualType& outType)
{
    if (s == "sprite") {
        outType = ScenePropVisualType::Sprite;
        return true;
    }
    if (s == "image") {
        outType = ScenePropVisualType::Image;
        return true;
    }
    return false;
}

static bool ParseScenePropDepthMode(const std::string& s, ScenePropDepthMode& outMode)
{
    if (s == "depthSorted") {
        outMode = ScenePropDepthMode::DepthSorted;
        return true;
    }
    if (s == "back") {
        outMode = ScenePropDepthMode::Back;
        return true;
    }
    if (s == "front") {
        outMode = ScenePropDepthMode::Front;
        return true;
    }
    return false;
}

static bool ParseFacing(const std::string& s, SceneFacing& outFacing)
{
    if (s == "left") {
        outFacing = SceneFacing::Left;
        return true;
    }
    if (s == "right") {
        outFacing = SceneFacing::Right;
        return true;
    }
    if (s == "front") {
        outFacing = SceneFacing::Front;
        return true;
    }
    if (s == "back") {
        outFacing = SceneFacing::Back;
        return true;
    }
    return false;
}

static bool LoadFacingProperty(const json& obj, SceneFacing& outFacing)
{
    const std::string facingStr = GetStringPropertyOrDefault(obj, "facing", "");
    if (facingStr.empty()) {
        return false;
    }
    return ParseFacing(facingStr, outFacing);
}

static ScenePolygon BuildWorldPolygon(
        const json& obj,
        float totalOffsetX,
        float totalOffsetY,
        int baseAssetScale)
{
    ScenePolygon poly;

    const float objX = GetFloatOrDefault(obj, "x", 0.0f);
    const float objY = GetFloatOrDefault(obj, "y", 0.0f);

    if (!obj.contains("polygon") || !obj["polygon"].is_array()) {
        return poly;
    }

    for (const auto& pt : obj["polygon"]) {
        Vector2 v{};
        v.x = (totalOffsetX + objX + pt.value("x", 0.0f)) * static_cast<float>(baseAssetScale);
        v.y = (totalOffsetY + objY + pt.value("y", 0.0f)) * static_cast<float>(baseAssetScale);
        poly.vertices.push_back(v);
    }

    return poly;
}

static void ProcessLayerRecursive(
        const json& layer,
        const fs::path& tiledDir,
        SceneData& scene,
        ResourceData& resources,
        const std::string& currentGroup,
        float parentOffsetX,
        float parentOffsetY,
        float parentParallaxX,
        float parentParallaxY)
{
    const std::string type = layer.value("type", "");
    const std::string name = layer.value("name", "");

    const float layerX = GetFloatOrDefault(layer, "x", 0.0f);
    const float layerY = GetFloatOrDefault(layer, "y", 0.0f);
    const float offsetX = GetFloatOrDefault(layer, "offsetx", 0.0f);
    const float offsetY = GetFloatOrDefault(layer, "offsety", 0.0f);

    const float totalOffsetX = parentOffsetX + layerX + offsetX;
    const float totalOffsetY = parentOffsetY + layerY + offsetY;

    const float parallaxX = parentParallaxX * GetFloatOrDefault(layer, "parallaxx", 1.0f);
    const float parallaxY = parentParallaxY * GetFloatOrDefault(layer, "parallaxy", 1.0f);

    if (type == "group") {
        const std::string nextGroup = name.empty() ? currentGroup : name;

        if (layer.contains("layers") && layer["layers"].is_array()) {
            for (const auto& child : layer["layers"]) {
                ProcessLayerRecursive(
                        child,
                        tiledDir,
                        scene,
                        resources,
                        nextGroup,
                        totalOffsetX,
                        totalOffsetY,
                        parallaxX,
                        parallaxY);
            }
        }
        return;
    }

    if (type == "imagelayer" && (currentGroup == "background" || currentGroup == "foreground")) {
        SceneImageLayer img;
        img.name = name;
        img.visible = layer.value("visible", true);
        img.opacity = GetFloatOrDefault(layer, "opacity", 1.0f);
        img.parallaxX = parallaxX;
        img.parallaxY = parallaxY;

        const std::string imageRel = layer.value("image", "");
        if (imageRel.empty()) {
            TraceLog(LOG_WARNING, "Image layer without image in group %s", currentGroup.c_str());
            return;
        }

        const fs::path imagePath = (tiledDir / imageRel).lexically_normal();
        img.imagePath = NormalizePath(imagePath);
        img.textureHandle = LoadTextureAsset(resources, img.imagePath.c_str());

        img.sourceSize.x = GetFloatOrDefault(layer, "imagewidth", 0.0f);
        img.sourceSize.y = GetFloatOrDefault(layer, "imageheight", 0.0f);

        img.worldPos.x = totalOffsetX * static_cast<float>(scene.baseAssetScale);
        img.worldPos.y = totalOffsetY * static_cast<float>(scene.baseAssetScale);

        img.worldSize.x = img.sourceSize.x * static_cast<float>(scene.baseAssetScale);
        img.worldSize.y = img.sourceSize.y * static_cast<float>(scene.baseAssetScale);

        if (currentGroup == "background") {
            scene.backgroundLayers.push_back(img);
        } else {
            scene.foregroundLayers.push_back(img);
        }
        return;
    }

    if (type == "objectgroup" && currentGroup == "navigation" && name == "navmesh") {
        if (!layer.contains("objects") || !layer["objects"].is_array()) {
            return;
        }

        for (const auto& obj : layer["objects"]) {
            if (!obj.value("visible", true)) {
                continue;
            }

            NavPolygon poly;
            const ScenePolygon worldPoly = BuildWorldPolygon(obj, totalOffsetX, totalOffsetY, scene.baseAssetScale);
            poly.vertices = worldPoly.vertices;

            if (poly.vertices.size() >= 3) {
                scene.navMesh.sourcePolygons.push_back(poly);
            }
        }
        return;
    }

    if (type == "objectgroup" && currentGroup == "navigation" && name == "blockers") {
        if (!layer.contains("objects") || !layer["objects"].is_array()) {
            return;
        }

        for (const auto& obj : layer["objects"]) {
            if (!obj.value("visible", true)) {
                continue;
            }

            NavPolygon poly;
            const ScenePolygon worldPoly = BuildWorldPolygon(obj, totalOffsetX, totalOffsetY, scene.baseAssetScale);
            poly.vertices = worldPoly.vertices;

            if (poly.vertices.size() >= 3) {
                scene.navMesh.blockerPolygons.push_back(poly);
            }
        }
        return;
    }

    if (type == "objectgroup" && name == "spawns") {
        if (!layer.contains("objects") || !layer["objects"].is_array()) {
            return;
        }

        for (const auto& obj : layer["objects"]) {
            if (!obj.value("visible", true) || !obj.value("point", false)) {
                continue;
            }

            SceneSpawnPoint spawn;
            spawn.id = obj.value("name", "");
            if (spawn.id.empty()) {
                TraceLog(LOG_ERROR, "Spawn missing name");
                continue;
            }

            if (!LoadFacingProperty(obj, spawn.facing)) {
                TraceLog(LOG_ERROR, "Spawn missing/invalid facing: %s", spawn.id.c_str());
                continue;
            }

            spawn.position.x = (totalOffsetX + GetFloatOrDefault(obj, "x", 0.0f)) * static_cast<float>(scene.baseAssetScale);
            spawn.position.y = (totalOffsetY + GetFloatOrDefault(obj, "y", 0.0f)) * static_cast<float>(scene.baseAssetScale);

            scene.spawns.push_back(spawn);
        }
        return;
    }

    if (type == "objectgroup" && name == "hotspots") {
        if (!layer.contains("objects") || !layer["objects"].is_array()) {
            return;
        }

        for (const auto& obj : layer["objects"]) {
            if (!obj.value("visible", true)) {
                continue;
            }

            SceneHotspot hotspot;
            hotspot.id = obj.value("name", "");
            if (hotspot.id.empty()) {
                TraceLog(LOG_ERROR, "Hotspot missing name");
                continue;
            }

            hotspot.displayName = GetStringPropertyOrDefault(obj, "displayName", "");
            hotspot.lookText = GetStringPropertyOrDefault(obj, "lookText", "");

            float walkToX = 0.0f;
            float walkToY = 0.0f;
            const bool hasWalkToX = GetFloatProperty(obj, "walkToX", walkToX);
            const bool hasWalkToY = GetFloatProperty(obj, "walkToY", walkToY);

            if (hotspot.displayName.empty() || hotspot.lookText.empty() ||
                !hasWalkToX || !hasWalkToY || !LoadFacingProperty(obj, hotspot.facing)) {
                TraceLog(LOG_ERROR, "Hotspot missing required properties: %s", hotspot.id.c_str());
                continue;
            }

            hotspot.walkTo.x = walkToX * static_cast<float>(scene.baseAssetScale);
            hotspot.walkTo.y = walkToY * static_cast<float>(scene.baseAssetScale);

            hotspot.shape = BuildWorldPolygon(obj, totalOffsetX, totalOffsetY, scene.baseAssetScale);
            if (hotspot.shape.vertices.size() < 3) {
                TraceLog(LOG_ERROR, "Hotspot polygon invalid: %s", hotspot.id.c_str());
                continue;
            }

            scene.hotspots.push_back(hotspot);
        }
        return;
    }

    if (type == "objectgroup" && name == "exits") {
        if (!layer.contains("objects") || !layer["objects"].is_array()) {
            return;
        }

        for (const auto& obj : layer["objects"]) {
            if (!obj.value("visible", true)) {
                continue;
            }

            SceneExit exitObj;
            exitObj.id = obj.value("name", "");
            if (exitObj.id.empty()) {
                TraceLog(LOG_ERROR, "Exit missing name");
                continue;
            }

            exitObj.displayName = GetStringPropertyOrDefault(obj, "displayName", "");
            exitObj.lookText = GetStringPropertyOrDefault(obj, "lookText", "");
            exitObj.targetScene = GetStringPropertyOrDefault(obj, "targetScene", "");
            exitObj.targetSpawn = GetStringPropertyOrDefault(obj, "targetSpawn", "");

            float walkToX = 0.0f;
            float walkToY = 0.0f;
            const bool hasWalkToX = GetFloatProperty(obj, "walkToX", walkToX);
            const bool hasWalkToY = GetFloatProperty(obj, "walkToY", walkToY);

            if (exitObj.displayName.empty() || exitObj.lookText.empty() ||
                exitObj.targetScene.empty() || exitObj.targetSpawn.empty() ||
                !hasWalkToX || !hasWalkToY || !LoadFacingProperty(obj, exitObj.facing)) {
                TraceLog(LOG_ERROR, "Exit missing required properties: %s", exitObj.id.c_str());
                continue;
            }

            exitObj.walkTo.x = walkToX * static_cast<float>(scene.baseAssetScale);
            exitObj.walkTo.y = walkToY * static_cast<float>(scene.baseAssetScale);

            exitObj.shape = BuildWorldPolygon(obj, totalOffsetX, totalOffsetY, scene.baseAssetScale);
            if (exitObj.shape.vertices.size() < 3) {
                TraceLog(LOG_ERROR, "Exit polygon invalid: %s", exitObj.id.c_str());
                continue;
            }

            scene.exits.push_back(exitObj);
        }
        return;
    }

    if (type == "objectgroup" && name == "props") {
        if (!layer.contains("objects") || !layer["objects"].is_array()) {
            return;
        }

        for (const auto& obj : layer["objects"]) {
            if (!obj.value("point", false)) {
                continue;
            }

            ScenePropData prop;
            prop.id = obj.value("name", "");
            if (prop.id.empty()) {
                TraceLog(LOG_ERROR, "Prop missing name");
                continue;
            }

            const std::string visualTypeStr = GetStringPropertyOrDefault(obj, "visualType", "");
            if (!ParseScenePropVisualType(visualTypeStr, prop.visualType)) {
                TraceLog(LOG_ERROR, "Prop missing/invalid visualType: %s", prop.id.c_str());
                continue;
            }

            const std::string depthModeStr = GetStringPropertyOrDefault(obj, "depthMode", "depthSorted");
            if (!ParseScenePropDepthMode(depthModeStr, prop.depthMode)) {
                TraceLog(LOG_ERROR, "Prop missing/invalid depthMode: %s", prop.id.c_str());
                continue;
            }

            const std::string assetRel = GetStringPropertyOrDefault(obj, "asset", "");
            if (assetRel.empty()) {
                TraceLog(LOG_ERROR, "Prop missing asset property: %s", prop.id.c_str());
                continue;
            }

            const fs::path assetPath = (tiledDir / assetRel).lexically_normal();

            if (prop.visualType == ScenePropVisualType::Sprite) {
                prop.spriteAssetHandle = LoadSpriteAsset(resources, assetPath.string().c_str());
                if (prop.spriteAssetHandle < 0) {
                    TraceLog(LOG_ERROR, "Failed loading sprite prop asset for %s: %s",
                             prop.id.c_str(), assetPath.string().c_str());
                    continue;
                }

                prop.defaultAnimation = GetStringPropertyOrDefault(obj, "animation", "");
                if (prop.defaultAnimation.empty()) {
                    TraceLog(LOG_ERROR, "Sprite prop missing animation property: %s", prop.id.c_str());
                    continue;
                }
            } else if (prop.visualType == ScenePropVisualType::Image) {
                prop.textureHandle = LoadTextureAsset(resources, assetPath.string().c_str());
                if (prop.textureHandle < 0) {
                    TraceLog(LOG_ERROR, "Failed loading image prop asset for %s: %s",
                             prop.id.c_str(), assetPath.string().c_str());
                    continue;
                }
            }

            bool flipX = false;
            if (GetBoolProperty(obj, "flipX", flipX)) {
                prop.flipX = flipX;
            }

            bool depthScaling = false;
            if (GetBoolProperty(obj, "depthScaling", depthScaling)) {
                prop.depthScaling = depthScaling;
            }

            prop.visible = obj.value("visible", true);

            prop.feetPos.x =
                    (totalOffsetX + GetFloatOrDefault(obj, "x", 0.0f)) *
                    static_cast<float>(scene.baseAssetScale);
            prop.feetPos.y =
                    (totalOffsetY + GetFloatOrDefault(obj, "y", 0.0f)) *
                    static_cast<float>(scene.baseAssetScale);

            scene.props.push_back(prop);
        }
        return;
    }

    if (type == "objectgroup" && name == "actors") {
        if (!layer.contains("objects") || !layer["objects"].is_array()) {
            return;
        }

        for (const auto& obj : layer["objects"]) {
            if (!obj.value("visible", true) || !obj.value("point", false)) {
                continue;
            }

            SceneActorPlacement placement;
            placement.actorId = obj.value("name", "");
            if (placement.actorId.empty()) {
                TraceLog(LOG_ERROR, "Actor placement missing name");
                continue;
            }

            if (!LoadFacingProperty(obj, placement.facing)) {
                TraceLog(LOG_ERROR, "Actor placement missing/invalid facing: %s", placement.actorId.c_str());
                continue;
            }

            placement.visible = obj.value("visible", true);

            placement.position.x =
                    (totalOffsetX + GetFloatOrDefault(obj, "x", 0.0f)) *
                    static_cast<float>(scene.baseAssetScale);
            placement.position.y =
                    (totalOffsetY + GetFloatOrDefault(obj, "y", 0.0f)) *
                    static_cast<float>(scene.baseAssetScale);

            scene.actorPlacements.push_back(placement);
        }
        return;
    }
}

bool ImportTiledSceneIntoSceneData(SceneData& scene, ResourceData& resources, const char* tiledFilePath)
{
    scene.backgroundLayers.clear();
    scene.foregroundLayers.clear();
    scene.navMesh = {};
    scene.spawns.clear();
    scene.hotspots.clear();
    scene.exits.clear();
    scene.props.clear();
    scene.actorPlacements.clear();

    const fs::path tiledPath = fs::path(tiledFilePath).lexically_normal();
    scene.tiledFilePath = tiledPath.string();

    json root;
    {
        std::ifstream in(tiledPath);
        if (!in.is_open()) {
            TraceLog(LOG_ERROR, "Failed to open Tiled file: %s", scene.tiledFilePath.c_str());
            return false;
        }
        in >> root;
    }

    if (!root.contains("layers") || !root["layers"].is_array()) {
        TraceLog(LOG_ERROR, "Tiled file missing layers array: %s", scene.tiledFilePath.c_str());
        return false;
    }

    const fs::path tiledDir = tiledPath.parent_path();

    for (const auto& layer : root["layers"]) {
        ProcessLayerRecursive(
                layer,
                tiledDir,
                scene,
                resources,
                "",
                0.0f,
                0.0f,
                1.0f,
                1.0f);
    }

    TraceLog(LOG_INFO,
             "Imported Tiled scene: %s (bg=%d fg=%d navPolys=%d spawns=%d hotspots=%d exits=%d props=%d actors=%d)",
             scene.tiledFilePath.c_str(),
             static_cast<int>(scene.backgroundLayers.size()),
             static_cast<int>(scene.foregroundLayers.size()),
             static_cast<int>(scene.navMesh.sourcePolygons.size()),
             static_cast<int>(scene.spawns.size()),
             static_cast<int>(scene.hotspots.size()),
             static_cast<int>(scene.exits.size()),
             static_cast<int>(scene.props.size()),
             static_cast<int>(scene.actorPlacements.size()));


    return true;
}
