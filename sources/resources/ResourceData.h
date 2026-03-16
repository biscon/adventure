#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "raylib.h"

using TextureHandle = int;
using SpriteAssetHandle = int;

enum class ResourceScope {
    Global,
    Scene
};

enum class AnimationPlaybackDirection {
    Forward,
    Reverse,
    PingPong
};

struct TextureResource {
    TextureHandle handle = -1;
    std::string path;
    Texture2D texture{};
    bool loaded = false;
    ResourceScope scope = ResourceScope::Scene;
};

struct SpriteFrame {
    Rectangle sourceRect{};
    Vector2 spriteSourcePos{};
    Vector2 spriteSourceSize{};
    Vector2 sourceSize{};
    float durationMs = 0.0f;
    bool trimmed = false;
};

struct SpriteClip {
    std::string name;
    std::string layerName;
    std::vector<int> frameIndices;

    AnimationPlaybackDirection direction = AnimationPlaybackDirection::Forward;
    int repeatCount = 0; // 0 = no explicit repeat from file
};

struct SpriteAssetResource {
    SpriteAssetHandle handle = -1;
    std::string sidecarPath;
    std::string asepriteJsonPath;
    std::string imagePath;

    TextureHandle textureHandle = -1;

    Vector2 feetPivot{};
    float baseDrawScale = 1.0f;
    bool useRightForLeft = true;

    std::vector<std::string> layerNames;
    std::vector<SpriteFrame> frames;
    std::vector<SpriteClip> clips;

    std::unordered_map<std::string, int> clipIndexByKey; // "layer::tag" -> clip index

    bool loaded = false;
    ResourceScope scope = ResourceScope::Scene;
};

struct ResourceData {
    int nextTextureHandle = 1;
    int nextSpriteAssetHandle = 1;

    std::vector<TextureResource> textures;
    std::unordered_map<TextureHandle, size_t> textureIndexByHandle;
    std::unordered_map<std::string, TextureHandle> textureHandleByPath;

    std::vector<SpriteAssetResource> spriteAssets;
    std::unordered_map<SpriteAssetHandle, size_t> spriteAssetIndexByHandle;
    std::unordered_map<std::string, SpriteAssetHandle> spriteAssetHandleByPath;
};
