#include "SceneRender.h"

#include <cmath>
#include <algorithm>
#include <sstream>
#include "resources/TextureAsset.h"
#include "resources/AsepriteAsset.h"
#include "raylib.h"
#include "scene/SceneHelpers.h"
#include "adventure/AdventureActorHelpers.h"

static void DrawSceneImageLayer(const GameState& state, const SceneImageLayer& layer)
{
    if (!layer.visible || layer.textureHandle < 0) {
        return;
    }

    const TextureResource* texRes = FindTextureResource(state.resources, layer.textureHandle);
    if (texRes == nullptr || !texRes->loaded) {
        return;
    }

    const Vector2 cam = state.adventure.camera.position;

    Rectangle src{};
    src.x = 0.0f;
    src.y = 0.0f;
    src.width = layer.sourceSize.x;
    src.height = layer.sourceSize.y;

    Rectangle dst{};
    dst.x = layer.worldPos.x - cam.x * layer.parallaxX;
    dst.y = layer.worldPos.y - cam.y * layer.parallaxY;
    dst.width = layer.worldSize.x;
    dst.height = layer.worldSize.y;

    const unsigned char alpha = static_cast<unsigned char>(std::round(255.0f * Clamp01(layer.opacity)));
    const Color tint = Color{255, 255, 255, alpha};

    DrawTexturePro(texRes->texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, tint);
}

static void DrawActor(const GameState& state, const ActorInstance& actor)
{
    const ActorDefinitionData* actorDef =
            FindActorDefinitionByIndex(state, actor.actorDefIndex);
    if (actorDef == nullptr || actorDef->spriteAssetHandle < 0) {
        return;
    }

    const SpriteAssetResource* asset =
            FindSpriteAssetResource(state.resources, actorDef->spriteAssetHandle);
    if (asset == nullptr || !asset->loaded) {
        return;
    }

    const TextureResource* texRes = FindTextureResource(state.resources, asset->textureHandle);
    if (texRes == nullptr || !texRes->loaded) {
        return;
    }

    const float depthScale = ComputeDepthScale(state.adventure.currentScene, actor.feetPos.y);
    const float finalScale = asset->baseDrawScale * depthScale;

    const Vector2 cam = state.adventure.camera.position;
    const Vector2 screenFeet = {
            actor.feetPos.x - cam.x,
            actor.feetPos.y - cam.y
    };

    for (const std::string& layerName : asset->layerNames) {
        int clipIndex = FindClipIndex(*asset, layerName, actor.currentAnimation);
        if (clipIndex < 0) {
            continue;
        }

        const SpriteClip& clip = asset->clips[clipIndex];

        int frameIndex = -1;
        if (actor.scriptAnimationActive) {
            frameIndex = GetOneShotFrameIndex(*asset, clip, actor.animationTimeMs);
        } else {
            frameIndex = GetLoopingFrameIndex(*asset, clip, actor.animationTimeMs);
        }

        if (frameIndex < 0) {
            continue;
        }

        const SpriteFrame& frame = asset->frames[frameIndex];

        Rectangle src = frame.sourceRect;
        if (actor.flipX) {
            src.width = -src.width;
        }

        const float drawWidth = frame.sourceSize.x * finalScale;
        const float drawHeight = frame.sourceSize.y * finalScale;

        Rectangle dst{};
        dst.x = screenFeet.x;
        dst.y = screenFeet.y;
        dst.width = drawWidth;
        dst.height = drawHeight;

        const bool hasFeetPivot =
                (asset->feetPivot.x > 0.0f || asset->feetPivot.y > 0.0f);

        const float pivotX = hasFeetPivot
                             ? asset->feetPivot.x
                             : frame.sourceSize.x * 0.5f;
        const float pivotY = hasFeetPivot
                             ? asset->feetPivot.y
                             : frame.sourceSize.y;

        Vector2 origin{};
        origin.y = pivotY * finalScale;

        if (actor.flipX) {
            origin.x = (frame.sourceSize.x - pivotX) * finalScale;
        } else {
            origin.x = pivotX * finalScale;
        }

        DrawTexturePro(texRes->texture, src, dst, origin, 0.0f, WHITE);
    }
}

static void DrawSpriteProp(
        const GameState& state,
        const ScenePropData& sceneProp,
        const PropInstance& prop)
{
    const SpriteAssetResource* asset =
            FindSpriteAssetResource(state.resources, sceneProp.spriteAssetHandle);
    if (asset == nullptr || !asset->loaded) {
        return;
    }

    const TextureResource* texRes =
            FindTextureResource(state.resources, asset->textureHandle);
    if (texRes == nullptr || !texRes->loaded) {
        return;
    }

    if (prop.currentAnimation.empty()) {
        return;
    }

    const float depthScale = sceneProp.depthScaling
                             ? ComputeDepthScale(state.adventure.currentScene, prop.feetPos.y)
                             : 1.0f;
    const float finalScale = asset->baseDrawScale * depthScale;


    const Vector2 cam = state.adventure.camera.position;
    const Vector2 screenFeet{
            prop.feetPos.x - cam.x,
            prop.feetPos.y - cam.y
    };

    for (const std::string& layerName : asset->layerNames) {
        const int clipIndex = FindClipIndex(*asset, layerName, prop.currentAnimation);
        if (clipIndex < 0) {
            continue;
        }

        const SpriteClip& clip = asset->clips[clipIndex];

        int frameIndex = -1;
        if (prop.oneShotActive) {
            frameIndex = GetOneShotFrameIndex(*asset, clip, prop.animationTimeMs);
        } else {
            frameIndex = GetLoopingFrameIndex(*asset, clip, prop.animationTimeMs);
        }

        if (frameIndex < 0) {
            continue;
        }

        const SpriteFrame& frame = asset->frames[frameIndex];

        Rectangle src = frame.sourceRect;
        if (prop.flipX) {
            src.width = -src.width;
        }

        Rectangle dst{};
        dst.x = screenFeet.x;
        dst.y = screenFeet.y;
        dst.width = frame.sourceSize.x * finalScale;
        dst.height = frame.sourceSize.y * finalScale;

        const bool hasFeetPivot =
                (asset->feetPivot.x > 0.0f || asset->feetPivot.y > 0.0f);

        const float pivotX = hasFeetPivot
                             ? asset->feetPivot.x
                             : frame.sourceSize.x * 0.5f;
        const float pivotY = hasFeetPivot
                             ? asset->feetPivot.y
                             : frame.sourceSize.y;

        Vector2 origin{};
        origin.y = pivotY * finalScale;
        origin.x = prop.flipX
                   ? (frame.sourceSize.x - pivotX) * finalScale
                   : pivotX * finalScale;

        DrawTexturePro(texRes->texture, src, dst, origin, 0.0f, WHITE);
    }
}

static void DrawImageProp(
        const GameState& state,
        const ScenePropData& sceneProp,
        const PropInstance& prop)
{
    const TextureResource* texRes =
            FindTextureResource(state.resources, sceneProp.textureHandle);
    if (texRes == nullptr || !texRes->loaded) {
        return;
    }

    const float depthScale = sceneProp.depthScaling
                             ? ComputeDepthScale(state.adventure.currentScene, prop.feetPos.y)
                             : 1.0f;
    const float finalScale = static_cast<float>(state.adventure.currentScene.baseAssetScale) * depthScale;


    const Vector2 cam = state.adventure.camera.position;
    const Vector2 screenFeet{
            prop.feetPos.x - cam.x,
            prop.feetPos.y - cam.y
    };


    Rectangle src{};
    src.x = 0.0f;
    src.y = 0.0f;
    src.width = static_cast<float>(texRes->texture.width);
    src.height = static_cast<float>(texRes->texture.height);

    if (prop.flipX) {
        src.width = -src.width;
    }

    Rectangle dst{};
    dst.x = screenFeet.x;
    dst.y = screenFeet.y;
    dst.width = static_cast<float>(texRes->texture.width) * finalScale;
    dst.height = static_cast<float>(texRes->texture.height) * finalScale;

    Vector2 origin{};
    origin.x = (static_cast<float>(texRes->texture.width) * 0.5f) * finalScale;
    origin.y = static_cast<float>(texRes->texture.height) * finalScale;

    DrawTexturePro(texRes->texture, src, dst, origin, 0.0f, WHITE);
}

static void DrawProp(
        const GameState& state,
        const ScenePropData& sceneProp,
        const PropInstance& prop)
{
    if (!prop.visible) {
        return;
    }

    switch (sceneProp.visualType) {
        case ScenePropVisualType::Sprite:
            DrawSpriteProp(state, sceneProp, prop);
            break;

        case ScenePropVisualType::Image:
            DrawImageProp(state, sceneProp, prop);
            break;

        case ScenePropVisualType::None:
        default:
            break;
    }
}

static void DrawBackProps(const GameState& state)
{
    const int propCount = std::min(
            static_cast<int>(state.adventure.currentScene.props.size()),
            static_cast<int>(state.adventure.props.size()));

    for (int i = 0; i < propCount; ++i) {
        const ScenePropData& sceneProp = state.adventure.currentScene.props[i];
        const PropInstance& prop = state.adventure.props[i];

        if (!prop.visible) {
            continue;
        }

        if (sceneProp.depthMode != ScenePropDepthMode::Back) {
            continue;
        }

        DrawProp(state, sceneProp, prop);
    }
}

static void DrawFrontProps(const GameState& state)
{
    const int propCount = std::min(
            static_cast<int>(state.adventure.currentScene.props.size()),
            static_cast<int>(state.adventure.props.size()));

    for (int i = 0; i < propCount; ++i) {
        const ScenePropData& sceneProp = state.adventure.currentScene.props[i];
        const PropInstance& prop = state.adventure.props[i];

        if (!prop.visible) {
            continue;
        }

        if (sceneProp.depthMode != ScenePropDepthMode::Front) {
            continue;
        }

        DrawProp(state, sceneProp, prop);
    }
}

void RenderAdventureScene(const GameState& state)
{
    if (!state.adventure.currentScene.loaded) {
        return;
    }

    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);

    for (const SceneImageLayer& layer : state.adventure.currentScene.backgroundLayers) {
        DrawSceneImageLayer(state, layer);
    }

    DrawBackProps(state);

    struct WorldDrawItem {
        float sortY = 0.0f;
        bool isActor = false;
        int actorIndex = -1;
        int propIndex = -1;
    };

    std::vector<WorldDrawItem> drawItems;

    for (int i = 0; i < static_cast<int>(state.adventure.actors.size()); ++i) {
        const ActorInstance& actor = state.adventure.actors[i];
        if (!actor.activeInScene || !actor.visible) {
            continue;
        }

        WorldDrawItem item;
        item.sortY = actor.feetPos.y;
        item.isActor = true;
        item.actorIndex = i;
        drawItems.push_back(item);
    }

    const int propCount = std::min(
            static_cast<int>(state.adventure.currentScene.props.size()),
            static_cast<int>(state.adventure.props.size()));

    for (int i = 0; i < propCount; ++i) {
        const ScenePropData& sceneProp = state.adventure.currentScene.props[i];
        const PropInstance& prop = state.adventure.props[i];
        if (!prop.visible) {
            continue;
        }

        if (sceneProp.depthMode != ScenePropDepthMode::DepthSorted) {
            continue;
        }

        WorldDrawItem item;
        item.sortY = prop.feetPos.y;
        item.isActor = false;
        item.propIndex = i;
        drawItems.push_back(item);
    }

    std::sort(drawItems.begin(), drawItems.end(),
              [](const WorldDrawItem& a, const WorldDrawItem& b) {
                  if (a.sortY != b.sortY) {
                      return a.sortY < b.sortY;
                  }

                  if (a.isActor != b.isActor) {
                      return !a.isActor;
                  }

                  if (a.isActor) {
                      return a.actorIndex < b.actorIndex;
                  }

                  return a.propIndex < b.propIndex;
              });

    for (const WorldDrawItem& item : drawItems) {
        if (item.isActor) {
            if (item.actorIndex >= 0 &&
                item.actorIndex < static_cast<int>(state.adventure.actors.size())) {
                DrawActor(state, state.adventure.actors[item.actorIndex]);
            }
        } else {
            if (item.propIndex >= 0 &&
                item.propIndex < static_cast<int>(state.adventure.currentScene.props.size()) &&
                item.propIndex < static_cast<int>(state.adventure.props.size())) {
                DrawProp(
                        state,
                        state.adventure.currentScene.props[item.propIndex],
                        state.adventure.props[item.propIndex]);
            }
        }
    }

    DrawFrontProps(state);

    for (const SceneImageLayer& layer : state.adventure.currentScene.foregroundLayers) {
        DrawSceneImageLayer(state, layer);
    }

    EndBlendMode();
}
