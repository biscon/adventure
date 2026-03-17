#include "TextureAsset.h"

#include <filesystem>
#include "raylib.h"

static std::string NormalizePath(const char* filePath)
{
    return std::filesystem::path(filePath).lexically_normal().string();
}

static Texture2D LoadTexturePreMultiplied(const char* fileName)
{
    Image img = LoadImage(fileName);
    if (img.data == nullptr) {
        TraceLog(LOG_ERROR, "Failed to load image: %s", fileName);
        return Texture2D{};
    }

    ImageAlphaPremultiply(&img);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);

    if (tex.id != 0) {
        SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    }

    return tex;
}

TextureHandle LoadTextureAsset(
        ResourceData& resources,
        const char* filePath,
        ResourceScope scope)
{
    const std::string normPath = NormalizePath(filePath);

    auto existing = resources.textureHandleByPath.find(normPath);
    if (existing != resources.textureHandleByPath.end()) {
        return existing->second;
    }

    Texture2D tex = LoadTexturePreMultiplied(normPath.c_str());
    if (tex.id == 0) {
        return -1;
    }

    TextureResource res;
    res.handle = resources.nextTextureHandle++;
    res.path = normPath;
    res.texture = tex;
    res.loaded = true;
    res.scope = scope;

    const size_t index = resources.textures.size();
    resources.textures.push_back(res);
    resources.textureIndexByHandle[res.handle] = index;
    resources.textureHandleByPath[normPath] = res.handle;

    TraceLog(LOG_INFO, "Loaded texture: %s", normPath.c_str());
    return res.handle;
}

TextureResource* FindTextureResource(ResourceData& resources, TextureHandle handle)
{
    auto it = resources.textureIndexByHandle.find(handle);
    if (it == resources.textureIndexByHandle.end()) {
        return nullptr;
    }
    return &resources.textures[it->second];
}

const TextureResource* FindTextureResource(const ResourceData& resources, TextureHandle handle)
{
    auto it = resources.textureIndexByHandle.find(handle);
    if (it == resources.textureIndexByHandle.end()) {
        return nullptr;
    }
    return &resources.textures.at(it->second);
}

TextureHandle LoadTextureAssetFromImage(
        ResourceData& resources,
        const char* filePath,
        const Image& image,
        ResourceScope scope)
{
    const std::string normPath = NormalizePath(filePath);

    auto existing = resources.textureHandleByPath.find(normPath);
    if (existing != resources.textureHandleByPath.end()) {
        return existing->second;
    }

    Texture2D tex = LoadTextureFromImage(image);
    if (tex.id == 0) {
        TraceLog(LOG_ERROR, "Failed to create texture from preloaded image: %s", normPath.c_str());
        return -1;
    }

    SetTextureFilter(tex, TEXTURE_FILTER_POINT);

    TextureResource res;
    res.handle = resources.nextTextureHandle++;
    res.path = normPath;
    res.texture = tex;
    res.loaded = true;
    res.scope = scope;

    const size_t index = resources.textures.size();
    resources.textures.push_back(res);
    resources.textureIndexByHandle[res.handle] = index;
    resources.textureHandleByPath[normPath] = res.handle;

    TraceLog(LOG_INFO, "Loaded texture from predecoded image: %s", normPath.c_str());
    return res.handle;
}
