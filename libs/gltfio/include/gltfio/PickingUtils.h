#pragma once

#include <gltfio/FilamentAsset.h>
#include <gltfio/Picking.h>
#include <filament/Engine.h>
#include <filament/TransformManager.h>

namespace filament {
namespace gltfio {

// Updates PickingRegistry with the latest world transforms for all renderables in the asset,
// and the asset root, using the asset's Engine/TransformManager.
// Returns the number of renderable transforms updated.
inline size_t updatePickingTransforms(filament::gltfio::FilamentAsset* asset) {
    if (!asset) return 0;
    filament::gltfio::PickingRegistry* reg = asset->getPickingRegistry();
    if (!reg) return 0;
    filament::Engine* engine = asset->getEngine();
    if (!engine) return 0;
    auto& tcm = engine->getTransformManager();
    size_t renderableCount = asset->getRenderableEntityCount();
    const utils::Entity* renderables = asset->getRenderableEntities();
    size_t updatedCount = 0;
    if (renderables) {
        for (size_t i = 0; i < renderableCount; ++i) {
            auto inst = tcm.getInstance(renderables[i]);
            if (inst) {
                reg->updateTransform(renderables[i], tcm.getWorldTransform(inst));
                updatedCount++;
            }
        }
    }
    auto rootInst = tcm.getInstance(asset->getRoot());
    if (rootInst) {
        reg->updateTransform(asset->getRoot(), tcm.getWorldTransform(rootInst));
    }
    return updatedCount;
}

} // namespace gltfio
} // namespace filament

