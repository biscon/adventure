#include "SceneRender.h"

#include <cmath>
#include <algorithm>
#include <sstream>
#include "resources/TextureAsset.h"
#include "resources/AsepriteAsset.h"
#include "raylib.h"
#include "scene/SceneHelpers.h"
#include "adventure/AdventureActorHelpers.h"
#include "render/EffectShaderRegistry.h"

static unsigned char MultiplyU8(unsigned char a, unsigned char b)
{
    const int value = static_cast<int>(a) * static_cast<int>(b);
    return static_cast<unsigned char>((value + 127) / 255);
}

static Color BuildEffectSpriteDrawColor(const EffectSpriteInstance& effect)
{
    Color c = effect.tint;
    c.a = MultiplyU8(c.a, static_cast<unsigned char>(std::round(255.0f * Clamp01(effect.opacity))));
    return c;
}

static int GetRaylibBlendMode(SceneEffectBlendMode mode)
{
    switch (mode) {
        case SceneEffectBlendMode::Add:
            return BLEND_ADDITIVE;
        case SceneEffectBlendMode::Multiply:
            return BLEND_MULTIPLIED;
        case SceneEffectBlendMode::Normal:
        default:
            return BLEND_ALPHA_PREMULTIPLY;
    }
}

static void SetShaderFloatIfValid(const Shader& shader, int loc, float value)
{
    if (loc < 0) {
        return;
    }

    SetShaderValue(shader, loc, &value, SHADER_UNIFORM_FLOAT);
}

static void SetShaderVec2IfValid(const Shader& shader, int loc, Vector2 value)
{
    if (loc < 0) {
        return;
    }

    const float v[2] = { value.x, value.y };
    SetShaderValue(shader, loc, v, SHADER_UNIFORM_VEC2);
}

static Rectangle GetRenderTargetSourceRect(const Texture2D& tex)
{
    return Rectangle{
            0.0f,
            0.0f,
            static_cast<float>(tex.width),
            -static_cast<float>(tex.height)
    };
}

static Rectangle GetRenderTargetDestRect(const Texture2D& tex)
{
    return Rectangle{
            0.0f,
            0.0f,
            static_cast<float>(tex.width),
            static_cast<float>(tex.height)
    };
}

static Rectangle BuildPolygonBounds(const ScenePolygon& polygon)
{
    Rectangle r{};

    if (polygon.vertices.empty()) {
        return r;
    }

    float minX = polygon.vertices[0].x;
    float minY = polygon.vertices[0].y;
    float maxX = polygon.vertices[0].x;
    float maxY = polygon.vertices[0].y;

    for (const Vector2& v : polygon.vertices) {
        if (v.x < minX) minX = v.x;
        if (v.y < minY) minY = v.y;
        if (v.x > maxX) maxX = v.x;
        if (v.y > maxY) maxY = v.y;
    }

    r.x = minX;
    r.y = minY;
    r.width = maxX - minX;
    r.height = maxY - minY;
    return r;
}

static float GetPolygonMaxY(const ScenePolygon& polygon)
{
    if (polygon.vertices.empty()) {
        return 0.0f;
    }

    float maxY = polygon.vertices[0].y;
    for (const Vector2& v : polygon.vertices) {
        if (v.y > maxY) {
            maxY = v.y;
        }
    }
    return maxY;
}

static void SetShaderIntIfValid(const Shader& shader, int loc, int value)
{
    if (loc < 0) {
        return;
    }

    SetShaderValue(shader, loc, &value, SHADER_UNIFORM_INT);
}

static void SetShaderPolygonIfValid(
        const Shader& shader,
        int usePolygonLoc,
        int polygonVertexCountLoc,
        int polygonPointsLoc,
        const SceneEffectRegionData& sceneEffect,
        const Vector2& cam)
{
    const int usePolygon = sceneEffect.usePolygon ? 1 : 0;
    SetShaderIntIfValid(shader, usePolygonLoc, usePolygon);

    if (!sceneEffect.usePolygon) {
        SetShaderIntIfValid(shader, polygonVertexCountLoc, 0);
        return;
    }

    const int vertexCount = static_cast<int>(sceneEffect.polygon.vertices.size());
    SetShaderIntIfValid(shader, polygonVertexCountLoc, vertexCount);

    if (polygonPointsLoc < 0 || vertexCount <= 0) {
        return;
    }

    float points[32 * 2] = {};
    for (int i = 0; i < vertexCount && i < 32; ++i) {
        points[i * 2 + 0] = sceneEffect.polygon.vertices[i].x - cam.x;
        points[i * 2 + 1] = sceneEffect.polygon.vertices[i].y - cam.y;
    }

    SetShaderValueV(shader, polygonPointsLoc, points, SHADER_UNIFORM_VEC2, vertexCount);
}

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

static void DrawSceneEffectSprite(
        const GameState& state,
        const SceneEffectSpriteData& sceneEffect,
        const EffectSpriteInstance& effect)
{
    if (!effect.visible || sceneEffect.textureHandle < 0) {
        return;
    }

    const TextureResource* texRes = FindTextureResource(state.resources, sceneEffect.textureHandle);
    if (texRes == nullptr || !texRes->loaded) {
        return;
    }

    const Vector2 cam = state.adventure.camera.position;

    Rectangle src{};
    src.x = 0.0f;
    src.y = 0.0f;
    src.width = sceneEffect.sourceSize.x;
    src.height = sceneEffect.sourceSize.y;

    Rectangle dst{};
    dst.x = sceneEffect.worldPos.x - cam.x;
    dst.y = sceneEffect.worldPos.y - cam.y;
    dst.width = sceneEffect.worldSize.x;
    dst.height = sceneEffect.worldSize.y;

    const SceneEffectShaderCategory shaderCategory = GetEffectShaderCategory(effect.shaderType);
    Color drawColor = BuildEffectSpriteDrawColor(effect);
    // if using a shader set draw color to white and use effect opacity for alpha
    if(shaderCategory != SceneEffectShaderCategory::None) {
        drawColor = WHITE;
        drawColor.a = static_cast<unsigned char>(std::round(255.0f * Clamp01(effect.opacity)));
    }

    EndBlendMode();
    BeginBlendMode(GetRaylibBlendMode(sceneEffect.blendMode));

    if (shaderCategory == SceneEffectShaderCategory::SelfTexture) {
        const EffectShaderEntry* shaderEntry = FindEffectShaderEntry(effect.shaderType);
        if (shaderEntry != nullptr) {
            const float timeSeconds = static_cast<float>(GetTime());

            BeginShaderMode(shaderEntry->shader);

            SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->timeLoc, timeSeconds);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->scrollSpeedLoc, effect.shaderParams.scrollSpeed);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->uvScaleLoc, effect.shaderParams.uvScale);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->distortionAmountLoc, effect.shaderParams.distortionAmount);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->noiseScrollSpeedLoc, effect.shaderParams.noiseScrollSpeed);
            SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->intensityLoc, effect.shaderParams.intensity);
            SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->phaseOffsetLoc, effect.shaderParams.phaseOffset);
            if (shaderEntry->tintLoc >= 0) {
                const float tint[3] = {
                        effect.shaderParams.tintR,
                        effect.shaderParams.tintG,
                        effect.shaderParams.tintB
                };
                SetShaderValue(shaderEntry->shader, shaderEntry->tintLoc, tint, SHADER_UNIFORM_VEC3);
            }

            DrawTexturePro(texRes->texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, drawColor);

            EndShaderMode();
        } else {
            DrawTexturePro(texRes->texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, drawColor);
        }
    } else {
        DrawTexturePro(texRes->texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, drawColor);
    }

    EndBlendMode();
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
}

static void DrawSceneEffectRegion(
        const GameState& state,
        const SceneEffectRegionData& sceneEffect,
        const EffectRegionInstance& effect)
{
    if (!effect.visible) {
        return;
    }

    const SceneEffectShaderCategory shaderCategory = GetEffectShaderCategory(effect.shaderType);
    if (shaderCategory == SceneEffectShaderCategory::SelfTexture && sceneEffect.textureHandle < 0) {
        return;
    }

    const TextureResource* texRes = nullptr;
    if (sceneEffect.textureHandle >= 0) {
        texRes = FindTextureResource(state.resources, sceneEffect.textureHandle);
        if (texRes == nullptr || !texRes->loaded) {
            return;
        }
    }

    const Vector2 cam = state.adventure.camera.position;

    const Rectangle effectBounds = sceneEffect.usePolygon
                                   ? BuildPolygonBounds(sceneEffect.polygon)
                                   : sceneEffect.worldRect;

    Rectangle src{};
    if (texRes != nullptr) {
        src.x = 0.0f;
        src.y = 0.0f;
        src.width = static_cast<float>(texRes->texture.width);
        src.height = static_cast<float>(texRes->texture.height);
    }

    Rectangle dst{};
    dst.x = effectBounds.x - cam.x;
    dst.y = effectBounds.y - cam.y;
    dst.width = effectBounds.width;
    dst.height = effectBounds.height;

    Color drawColor = WHITE;
    drawColor.a = static_cast<unsigned char>(std::round(255.0f * Clamp01(effect.opacity)));

    // if using a shader set draw color to white and use effect opacity for alpha
    if(shaderCategory == SceneEffectShaderCategory::None) {
        drawColor = effect.tint;
        drawColor.a = MultiplyU8(
                drawColor.a,
                static_cast<unsigned char>(std::round(255.0f * Clamp01(effect.opacity))));
    }

    EndBlendMode();
    BeginBlendMode(GetRaylibBlendMode(sceneEffect.blendMode));

    if (shaderCategory == SceneEffectShaderCategory::SelfTexture) {
        const EffectShaderEntry* shaderEntry = FindEffectShaderEntry(effect.shaderType);
        if (shaderEntry != nullptr) {
            const float timeSeconds = static_cast<float>(GetTime());
            const Vector2 sceneSize{ 1920.0f, 1080.0f };

            const Vector2 regionPos{ dst.x, dst.y };
            const Vector2 regionSize{ dst.width, dst.height };

            BeginShaderMode(shaderEntry->shader);

            SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->timeLoc, timeSeconds);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->scrollSpeedLoc, effect.shaderParams.scrollSpeed);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->uvScaleLoc, effect.shaderParams.uvScale);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->distortionAmountLoc, effect.shaderParams.distortionAmount);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->noiseScrollSpeedLoc, effect.shaderParams.noiseScrollSpeed);
            SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->intensityLoc, effect.shaderParams.intensity);
            SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->phaseOffsetLoc, effect.shaderParams.phaseOffset);

            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->sceneSizeLoc, sceneSize);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->regionPosLoc, regionPos);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->regionSizeLoc, regionSize);
            SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->softnessLoc, effect.shaderParams.softness);

            if (shaderEntry->tintLoc >= 0) {
                const float tint[3] = {
                        effect.shaderParams.tintR,
                        effect.shaderParams.tintG,
                        effect.shaderParams.tintB
                };
                SetShaderValue(shaderEntry->shader, shaderEntry->tintLoc, tint, SHADER_UNIFORM_VEC3);
            }

            SetShaderPolygonIfValid(
                    shaderEntry->shader,
                    shaderEntry->usePolygonLoc,
                    shaderEntry->polygonVertexCountLoc,
                    shaderEntry->polygonPointsLoc,
                    sceneEffect,
                    cam);

            DrawTexturePro(texRes->texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, drawColor);

            EndShaderMode();
        } else {
            DrawTexturePro(texRes->texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, drawColor);
        }
    } else {
        // scene-sample shaders are applied in the post pass, not here
    }

    EndBlendMode();
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
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

static void DrawBackEffectSprites(const GameState& state)
{
    const int effectCount = std::min(
            static_cast<int>(state.adventure.currentScene.effectSprites.size()),
            static_cast<int>(state.adventure.effectSprites.size()));

    for (int i = 0; i < effectCount; ++i) {
        const SceneEffectSpriteData& sceneEffect = state.adventure.currentScene.effectSprites[i];
        const EffectSpriteInstance& effect = state.adventure.effectSprites[i];

        if (!effect.visible) {
            continue;
        }

        if (sceneEffect.depthMode != ScenePropDepthMode::Back) {
            continue;
        }

        DrawSceneEffectSprite(state, sceneEffect, effect);
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

static void DrawFrontEffectSprites(const GameState& state)
{
    const int effectCount = std::min(
            static_cast<int>(state.adventure.currentScene.effectSprites.size()),
            static_cast<int>(state.adventure.effectSprites.size()));

    for (int i = 0; i < effectCount; ++i) {
        const SceneEffectSpriteData& sceneEffect = state.adventure.currentScene.effectSprites[i];
        const EffectSpriteInstance& effect = state.adventure.effectSprites[i];

        if (!effect.visible) {
            continue;
        }

        if (sceneEffect.depthMode != ScenePropDepthMode::Front) {
            continue;
        }

        if (sceneEffect.renderAsOverlay) {
            continue;
        }

        DrawSceneEffectSprite(state, sceneEffect, effect);
    }
}

static void DrawOverlayEffectSprites(const GameState& state)
{
    const int effectCount = std::min(
            static_cast<int>(state.adventure.currentScene.effectSprites.size()),
            static_cast<int>(state.adventure.effectSprites.size()));

    for (int i = 0; i < effectCount; ++i) {
        const SceneEffectSpriteData& sceneEffect = state.adventure.currentScene.effectSprites[i];
        const EffectSpriteInstance& effect = state.adventure.effectSprites[i];

        if (!effect.visible) {
            continue;
        }

        if (!sceneEffect.renderAsOverlay) {
            continue;
        }

        DrawSceneEffectSprite(state, sceneEffect, effect);
    }
}

static void DrawBackEffectRegions(const GameState& state)
{
    const int effectCount = std::min(
            static_cast<int>(state.adventure.currentScene.effectRegions.size()),
            static_cast<int>(state.adventure.effectRegions.size()));

    for (int i = 0; i < effectCount; ++i) {
        const SceneEffectRegionData& sceneEffect = state.adventure.currentScene.effectRegions[i];
        const EffectRegionInstance& effect = state.adventure.effectRegions[i];

        if (!effect.visible) {
            continue;
        }

        if (sceneEffect.depthMode != ScenePropDepthMode::Back) {
            continue;
        }

        DrawSceneEffectRegion(state, sceneEffect, effect);
    }
}

static void DrawFrontEffectRegions(const GameState& state)
{
    const int effectCount = std::min(
            static_cast<int>(state.adventure.currentScene.effectRegions.size()),
            static_cast<int>(state.adventure.effectRegions.size()));

    for (int i = 0; i < effectCount; ++i) {
        const SceneEffectRegionData& sceneEffect = state.adventure.currentScene.effectRegions[i];
        const EffectRegionInstance& effect = state.adventure.effectRegions[i];

        if (!effect.visible) {
            continue;
        }

        if (sceneEffect.depthMode != ScenePropDepthMode::Front) {
            continue;
        }

        if (sceneEffect.renderAsOverlay) {
            continue;
        }

        DrawSceneEffectRegion(state, sceneEffect, effect);
    }
}

static void DrawOverlayEffectRegions(const GameState& state)
{
    const int effectCount = std::min(
            static_cast<int>(state.adventure.currentScene.effectRegions.size()),
            static_cast<int>(state.adventure.effectRegions.size()));

    for (int i = 0; i < effectCount; ++i) {
        const SceneEffectRegionData& sceneEffect = state.adventure.currentScene.effectRegions[i];
        const EffectRegionInstance& effect = state.adventure.effectRegions[i];

        if (!effect.visible) {
            continue;
        }

        if (!sceneEffect.renderAsOverlay) {
            continue;
        }

        DrawSceneEffectRegion(state, sceneEffect, effect);
    }
}

bool ApplySceneSampleEffectRegionPass(
        const GameState& state,
        int effectRegionIndex,
        const RenderTexture2D& sourceTarget,
        RenderTexture2D& destTarget)
{
    if (effectRegionIndex < 0 ||
        effectRegionIndex >= static_cast<int>(state.adventure.currentScene.effectRegions.size()) ||
        effectRegionIndex >= static_cast<int>(state.adventure.effectRegions.size())) {
        return false;
    }

    const SceneEffectRegionData& sceneEffect = state.adventure.currentScene.effectRegions[effectRegionIndex];
    const EffectRegionInstance& effect = state.adventure.effectRegions[effectRegionIndex];

    if (!effect.visible) {
        return false;
    }

    if (GetEffectShaderCategory(effect.shaderType) != SceneEffectShaderCategory::SceneSample) {
        return false;
    }

    const EffectShaderEntry* shaderEntry = FindEffectShaderEntry(effect.shaderType);
    if (shaderEntry == nullptr) {
        return false;
    }

    const Vector2 cam = state.adventure.camera.position;

    const Rectangle effectBounds = sceneEffect.usePolygon
                                   ? BuildPolygonBounds(sceneEffect.polygon)
                                   : sceneEffect.worldRect;

    Vector2 regionPos{
            effectBounds.x - cam.x,
            effectBounds.y - cam.y
    };

    Vector2 regionSize{
            effectBounds.width,
            effectBounds.height
    };

    const float timeSeconds = static_cast<float>(GetTime());
    const Vector2 sceneSize{
            static_cast<float>(sourceTarget.texture.width),
            static_cast<float>(sourceTarget.texture.height)
    };

    BeginTextureMode(destTarget);
    ClearBackground(BLACK);

    BeginShaderMode(shaderEntry->shader);

    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->timeLoc, timeSeconds);
    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->scrollSpeedLoc, effect.shaderParams.scrollSpeed);
    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->uvScaleLoc, effect.shaderParams.uvScale);
    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->distortionAmountLoc, effect.shaderParams.distortionAmount);
    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->noiseScrollSpeedLoc, effect.shaderParams.noiseScrollSpeed);
    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->intensityLoc, effect.shaderParams.intensity);
    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->phaseOffsetLoc, effect.shaderParams.phaseOffset);
    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->sceneSizeLoc, sceneSize);
    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->regionPosLoc, regionPos);
    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->regionSizeLoc, regionSize);
    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->brightnessLoc, effect.shaderParams.brightness);
    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->contrastLoc, effect.shaderParams.contrast);
    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->saturationLoc, effect.shaderParams.saturation);

    if (shaderEntry->tintLoc >= 0) {
        const float tint[3] = {
                effect.shaderParams.tintR,
                effect.shaderParams.tintG,
                effect.shaderParams.tintB
        };
        SetShaderValue(shaderEntry->shader, shaderEntry->tintLoc, tint, SHADER_UNIFORM_VEC3);
    }

    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->softnessLoc, effect.shaderParams.softness);

    SetShaderPolygonIfValid(
            shaderEntry->shader,
            shaderEntry->usePolygonLoc,
            shaderEntry->polygonVertexCountLoc,
            shaderEntry->polygonPointsLoc,
            sceneEffect,
            cam);

    DrawTexturePro(
            sourceTarget.texture,
            GetRenderTargetSourceRect(sourceTarget.texture),
            GetRenderTargetDestRect(destTarget.texture),
            Vector2{0.0f, 0.0f},
            0.0f,
            WHITE);

    EndShaderMode();
    EndTextureMode();

    return true;
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
    DrawBackEffectSprites(state);
    DrawBackEffectRegions(state);

    enum class WorldDrawItemType {
        Actor,
        Prop,
        EffectSprite,
        EffectRegion
    };

    struct WorldDrawItem {
        float sortY = 0.0f;
        WorldDrawItemType type = WorldDrawItemType::Actor;
        int actorIndex = -1;
        int propIndex = -1;
        int effectIndex = -1;
        int effectRegionIndex = -1;
    };

    std::vector<WorldDrawItem> drawItems;

    for (int i = 0; i < static_cast<int>(state.adventure.actors.size()); ++i) {
        const ActorInstance& actor = state.adventure.actors[i];
        if (!actor.activeInScene || !actor.visible) {
            continue;
        }

        WorldDrawItem item;
        item.sortY = actor.feetPos.y;
        item.type = WorldDrawItemType::Actor;
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
        item.type = WorldDrawItemType::Prop;
        item.propIndex = i;
        drawItems.push_back(item);
    }

    const int effectCount = std::min(
            static_cast<int>(state.adventure.currentScene.effectSprites.size()),
            static_cast<int>(state.adventure.effectSprites.size()));

    for (int i = 0; i < effectCount; ++i) {
        const SceneEffectSpriteData& sceneEffect = state.adventure.currentScene.effectSprites[i];
        const EffectSpriteInstance& effect = state.adventure.effectSprites[i];
        if (!effect.visible) {
            continue;
        }

        if (sceneEffect.depthMode != ScenePropDepthMode::DepthSorted) {
            continue;
        }

        WorldDrawItem item;
        item.sortY = sceneEffect.worldPos.y + sceneEffect.worldSize.y;
        item.type = WorldDrawItemType::EffectSprite;
        item.effectIndex = i;
        drawItems.push_back(item);
    }


    // collect effect regions
    const int effectRegionCount = std::min(
            static_cast<int>(state.adventure.currentScene.effectRegions.size()),
            static_cast<int>(state.adventure.effectRegions.size()));

    for (int i = 0; i < effectRegionCount; ++i) {
        const SceneEffectRegionData& sceneEffect = state.adventure.currentScene.effectRegions[i];
        const EffectRegionInstance& effect = state.adventure.effectRegions[i];
        if (!effect.visible) {
            continue;
        }

        if (sceneEffect.depthMode != ScenePropDepthMode::DepthSorted) {
            continue;
        }

        if (sceneEffect.renderAsOverlay) {
            continue;
        }

        WorldDrawItem item;

        item.sortY = sceneEffect.usePolygon
                     ? GetPolygonMaxY(sceneEffect.polygon)
                     : (sceneEffect.worldRect.y + sceneEffect.worldRect.height);
        item.type = WorldDrawItemType::EffectRegion;
        item.effectRegionIndex = i;
        drawItems.push_back(item);
    }

    std::sort(drawItems.begin(), drawItems.end(),
              [](const WorldDrawItem& a, const WorldDrawItem& b) {
                  if (a.sortY != b.sortY) {
                      return a.sortY < b.sortY;
                  }

                  if (a.type != b.type) {
                      return static_cast<int>(a.type) < static_cast<int>(b.type);
                  }

                  switch (a.type) {
                      case WorldDrawItemType::Actor:
                          return a.actorIndex < b.actorIndex;

                      case WorldDrawItemType::Prop:
                          return a.propIndex < b.propIndex;

                      case WorldDrawItemType::EffectSprite:
                          return a.effectIndex < b.effectIndex;

                      case WorldDrawItemType::EffectRegion:
                          return a.effectRegionIndex < b.effectRegionIndex;

                      default:
                          return false;
                  }
              });

    for (const WorldDrawItem& item : drawItems) {
        switch (item.type) {
            case WorldDrawItemType::Actor:
                if (item.actorIndex >= 0 &&
                    item.actorIndex < static_cast<int>(state.adventure.actors.size())) {
                    DrawActor(state, state.adventure.actors[item.actorIndex]);
                }
                break;

            case WorldDrawItemType::Prop:
                if (item.propIndex >= 0 &&
                    item.propIndex < static_cast<int>(state.adventure.currentScene.props.size()) &&
                    item.propIndex < static_cast<int>(state.adventure.props.size())) {
                    DrawProp(
                            state,
                            state.adventure.currentScene.props[item.propIndex],
                            state.adventure.props[item.propIndex]);
                }
                break;

            case WorldDrawItemType::EffectSprite:
                if (item.effectIndex >= 0 &&
                    item.effectIndex < static_cast<int>(state.adventure.currentScene.effectSprites.size()) &&
                    item.effectIndex < static_cast<int>(state.adventure.effectSprites.size())) {
                    DrawSceneEffectSprite(
                            state,
                            state.adventure.currentScene.effectSprites[item.effectIndex],
                            state.adventure.effectSprites[item.effectIndex]);
                }
                break;

            case WorldDrawItemType::EffectRegion:
                if (item.effectRegionIndex >= 0 &&
                    item.effectRegionIndex < static_cast<int>(state.adventure.currentScene.effectRegions.size()) &&
                    item.effectRegionIndex < static_cast<int>(state.adventure.effectRegions.size())) {
                    DrawSceneEffectRegion(
                            state,
                            state.adventure.currentScene.effectRegions[item.effectRegionIndex],
                            state.adventure.effectRegions[item.effectRegionIndex]);
                }
                break;

            default:
                break;
        }
    }

    DrawFrontProps(state);
    DrawFrontEffectSprites(state);
    DrawFrontEffectRegions(state);

    for (const SceneImageLayer& layer : state.adventure.currentScene.foregroundLayers) {
        DrawSceneImageLayer(state, layer);
    }

    DrawOverlayEffectSprites(state);
    DrawOverlayEffectRegions(state);

    EndBlendMode();
}
