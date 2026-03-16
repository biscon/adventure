#include "Resources.h"

static void RebuildTextureIndices(ResourceData& resources)
{
    resources.textureIndexByHandle.clear();
    resources.textureHandleByPath.clear();

    for (size_t i = 0; i < resources.textures.size(); ++i) {
        resources.textureIndexByHandle[resources.textures[i].handle] = i;
        resources.textureHandleByPath[resources.textures[i].path] = resources.textures[i].handle;
    }
}

void UnloadSceneResources(ResourceData& resources)
{
    for (auto it = resources.spriteAssets.begin(); it != resources.spriteAssets.end(); ) {
        if (it->scope == ResourceScope::Scene) {
            it = resources.spriteAssets.erase(it);
        } else {
            ++it;
        }
    }

    resources.spriteAssetIndexByHandle.clear();
    resources.spriteAssetHandleByPath.clear();
    for (size_t i = 0; i < resources.spriteAssets.size(); ++i) {
        resources.spriteAssetIndexByHandle[resources.spriteAssets[i].handle] = i;
        resources.spriteAssetHandleByPath[resources.spriteAssets[i].sidecarPath] =
                resources.spriteAssets[i].handle;
    }

    for (auto it = resources.textures.begin(); it != resources.textures.end(); ) {
        if (it->scope == ResourceScope::Scene) {
            if (it->loaded && it->texture.id != 0) {
                UnloadTexture(it->texture);
            }
            it = resources.textures.erase(it);
        } else {
            ++it;
        }
    }

    RebuildTextureIndices(resources);
}

static void UnloadAllTextureAssets(ResourceData& resources)
{
    for (auto& tex : resources.textures) {
        if (tex.loaded && tex.texture.id != 0) {
            UnloadTexture(tex.texture);
            tex.texture = {};
            tex.loaded = false;
        }
    }

    resources.textures.clear();
    resources.textureIndexByHandle.clear();
    resources.textureHandleByPath.clear();
    resources.nextTextureHandle = 1;
}

void UnloadAllResources(ResourceData& resources)
{
    resources.spriteAssets.clear();
    resources.spriteAssetIndexByHandle.clear();
    resources.spriteAssetHandleByPath.clear();
    resources.nextSpriteAssetHandle = 1;

    UnloadAllTextureAssets(resources);
}

