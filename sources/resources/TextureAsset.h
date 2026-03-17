#pragma once

#include "ResourceData.h"

TextureHandle LoadTextureAsset(
        ResourceData& resources,
        const char* filePath,
        ResourceScope scope = ResourceScope::Scene);

TextureResource* FindTextureResource(ResourceData& resources, TextureHandle handle);
const TextureResource* FindTextureResource(const ResourceData& resources, TextureHandle handle);
TextureHandle LoadTextureAssetFromImage(
        ResourceData& resources,
        const char* filePath,
        const Image& image,
        ResourceScope scope = ResourceScope::Scene);
