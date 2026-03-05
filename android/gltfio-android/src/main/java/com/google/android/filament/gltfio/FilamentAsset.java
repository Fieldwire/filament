/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.android.filament.gltfio;

import androidx.annotation.IntRange;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.google.android.filament.Box;
import com.google.android.filament.Engine;
import com.google.android.filament.Entity;

/**
 * Owns a bundle of Filament objects that have been created by <code>AssetLoader</code>.
 *
 * <p>For usage instructions, see the documentation for {@link AssetLoader}.</p>
 *
 * <p>This class owns a hierarchy of entities that have been loaded from a glTF asset. Every entity
 * has a <code>TransformManager</code> component, and some entities also have compnents managed by
 * <code>NameComponentManager</code>, <code>RenderableManager</code>, and others.</p>
 *
 * <p>In addition to the aforementioned entities, an asset has strong ownership over a list of
 * <code>VertexBuffer</code>, <code>IndexBuffer</code>, and <code>Texture</code>.</p>
 *
 * <p>Clients can use {@link ResourceLoader} to create textures, compute tangent quaternions, and
 * upload data into vertex buffers and index buffers.</p>
 *
 * @see ResourceLoader
 * @see FilamentInstance
 * @see AssetLoader
 */
public class FilamentAsset {
    private long mNativeObject;
    private FilamentInstance mPrimaryInstance;
    private Engine mEngine;

    FilamentAsset(Engine engine, long nativeObject) {
        mEngine = engine;
        mNativeObject = nativeObject;
    }

    public FilamentInstance getInstance() {
        if (mPrimaryInstance != null) {
            return mPrimaryInstance;
        }
        long nativeInstance = nGetInstance(getNativeObject());
        mPrimaryInstance = new FilamentInstance(this, nativeInstance);
        return mPrimaryInstance;
    }

    long getNativeObject() {
        return mNativeObject;
    }

    /**
     * Gets the transform root for the asset, which has no matching glTF node.
     */
    public @Entity int getRoot() {
        return nGetRoot(mNativeObject);
    }

    /**
     * Pops a ready renderable off the queue, or returns 0 if no renderables have become ready.
     *
     * NOTE: To determine the progress percentage or completion status, please use
     * ResourceLoader#asyncGetLoadProgress.
     *
     * This helper method allows clients to progressively add renderables to the scene as textures
     * gradually become ready through asynchronous loading.
     *
     * See also ResourceLoader#asyncBeginLoad.
     */
    public @Entity int popRenderable() {
        return nPopRenderable(mNativeObject);
    }

    /**
     * Pops one or more renderables off the queue, or returns the available number.
     *
     * Returns the number of entities written into the given array. If the given array
     * is null, returns the number of available renderables.
     */
    public int popRenderables(@Nullable @Entity int[] entities) {
        return nPopRenderables(mNativeObject, entities);
    }

    /**
     * Gets the list of entities, one for each glTF node.
     *
     * <p>All of these have a transform component. Some of the returned entities may also have a
     * renderable or light component.</p>
     */
    public @NonNull @Entity int[] getEntities() {
        int[] result = new int[nGetEntityCount(mNativeObject)];
        nGetEntities(mNativeObject, result);
        return result;
    }

    /**
     * Gets only the entities that have light components.
     */
    public @NonNull @Entity int[] getLightEntities() {
        int[] result = new int[nGetLightEntityCount(mNativeObject)];
        nGetLightEntities(mNativeObject, result);
        return result;
    }

    /**
     * Gets only the entities that have renderable components.
     */
    public @NonNull @Entity int[] getRenderableEntities() {
        int[] result = new int[nGetRenderableEntityCount(mNativeObject)];
        nGetRenderableEntities(mNativeObject, result);
        return result;
    }

    /**
     * Gets only the entities that have camera components.
     *
     * <p>
     * Note about aspect ratios:<br>
     *
     * gltfio always uses an aspect ratio of 1.0 when setting the projection matrix for perspective
     * cameras. gltfio then sets the camera's scaling matrix with the aspect ratio specified in the
     * glTF file (if present).<br>
     *
     * The camera's scaling matrix allows clients to adjust the aspect ratio independently from the
     * camera's projection.
     * </p>
     *
     * @see com.google.android.filament.Camera#setScaling
     */
    public @NonNull @Entity int[] getCameraEntities() {
        int[] result = new int[nGetCameraEntityCount(mNativeObject)];
        nGetCameraEntities(mNativeObject, result);
        return result;
    }

    /**
     * Returns the first entity with the given name, or 0 if none exist.
     */
    public @Entity int getFirstEntityByName(String name) {
        return nGetFirstEntityByName(mNativeObject, name);
    }

    /**
     * Gets a list of entities with the given name.
     */
    public @NonNull @Entity int[] getEntitiesByName(String name) {
        int[] result = new int[nGetEntitiesByName(mNativeObject, name, null)];
        nGetEntitiesByName(mNativeObject, name, result);
        return result;
    }

    /**
     * Gets a list of entities whose names start with the given prefix.
     */
    public @NonNull @Entity int[] getEntitiesByPrefix(String prefix) {
        int[] result = new int[nGetEntitiesByPrefix(mNativeObject, prefix, null)];
        nGetEntitiesByPrefix(mNativeObject, prefix, result);
        return result;
    }

    /**
     * Gets the bounding box computed from the supplied min / max values in glTF accessors.
     *
     * This does not return a bounding box over all FilamentInstance, it's just a straightforward
     * AAAB that can be determined at load time from the asset data.
     */
    public @NonNull Box getBoundingBox() {
        float[] box = new float[6];
        nGetBoundingBox(mNativeObject, box);
        return new Box(box[0], box[1], box[2], box[3], box[4], box[5]);
    }

    /**
     * Returns the total number of triangles in the asset.
     *
     * <p>This counts all triangles across all primitives in all meshes by querying the
    /**
     * Gets the total number of triangles in all renderables.
     *
     * <p>Only primitives of type TRIANGLES are counted. The count is computed on-demand
     * by iterating through all renderable entities and their primitives.</p>
     *
     * @return Total triangle count across all renderables in the asset.
     */
    public long getTriangleCount() {
        return nGetTriangleCount(mNativeObject);
    }

    /**
     * Gets the <code>NameComponentManager</code> label for the given entity, if it exists.
     */
    public String getName(@Entity int entity) {
        return nGetName(getNativeObject(), entity);
    }

    /**
     * Gets the glTF extras string for the asset or a specific node.
     *
     * @param entity the entity corresponding to the glTF node, or 0 to get the asset-level string.
     * @return the requested extras string, or null if it does not exist.
     */
    public @Nullable String getExtras(@Entity int entity) {
        return nGetExtras(mNativeObject, entity);
    }

    /**
     * Gets the names of all morph targets in the given entity.
     */
    public @NonNull String[] getMorphTargetNames(@Entity int entity) {
        String[] names = new String[nGetMorphTargetCount(mNativeObject, entity)];
        nGetMorphTargetNames(mNativeObject, entity, names);
        return names;
    }

    /**
     * Gets resource URIs for all externally-referenced buffers.
     */
    public @NonNull String[] getResourceUris() {
        String[] uris = new String[nGetResourceUriCount(mNativeObject)];
        nGetResourceUris(mNativeObject, uris);
        return uris;
    }

    /**
     * Reclaims CPU-side memory for URI strings, binding lists, and raw animation data.
     *
     * This should only be called after ResourceLoader#loadResources() or
     * ResourceLoader#asyncBeginLoad(). If this is an instanced asset, this prevents creation of new
     * instances.
     */
    public void releaseSourceData() {
        nReleaseSourceData(mNativeObject);
    }

    public Engine getEngine() { return mEngine; }

    /**
     * Result from ray-triangle intersection test.
     */
    public static class PickingHit {
        /** The entity that was hit */
        public final @Entity int entityId;
        /** Index of the hit triangle (-1 if no hit) */
        public final int triangleIndex;
        /** Distance along the ray to the hit point */
        public final float distance;

        public PickingHit(int entityId, int triangleIndex, float distance) {
            this.entityId = entityId;
            this.triangleIndex = triangleIndex;
            this.distance = distance;
        }

        /** Returns true if a hit was found (triangleIndex >= 0) */
        public boolean hasHit() {
            return triangleIndex >= 0;
        }

        @NonNull
        @Override
        public String toString() {
            return "PickingHit{" +
                    "entityId=" + entityId +
                    ", triangleIndex=" + triangleIndex +
                    ", distance=" + distance +
                    '}';
        }
    }

    /**
     * Perform ray-triangle intersection test against a specific entity's mesh.
     * Computes ray from screen coordinates and optionally skips triangles in specified ranges.
     *
     * @param view The View to use for screen-to-ray conversion
     * @param entityId The entity ID to test against
     * @param screenX Screen X coordinate
     * @param screenY Screen Y coordinate
     * @param skipRanges Optional array of triangle index ranges to skip during intersection.
     *                   Format: [start0, end0, start1, end1, ...] where each pair defines
     *                   an inclusive range of triangle indices. Can be null for no skipping.
     * @return PickingHit with hit information (triangleIndex = -1 if no hit)
     */
    @NonNull
    public PickingHit pick(long view, @Entity int entityId, int screenX, int screenY,
                          @Nullable int[] skipRanges) {
        int[] result = nPick(mNativeObject, view, mEngine.getNativeObject(),
                            entityId, screenX, screenY, skipRanges);
        return new PickingHit(
            result[0],                              // entityId
            result[1],                              // triangleIndex
            Float.intBitsToFloat(result[2])         // distance
        );
    }

    /**
     * Perform ray-triangle intersection test without skip ranges.
     */
    @NonNull
    public PickingHit pick(long view, @Entity int entityId, int screenX, int screenY) {
        return pick(view, entityId, screenX, screenY, null);
    }

    /**
     * Mesh data for an entity containing positions and indices.
     * The ByteBuffers directly reference native C++ memory - no copy is made.
     *
     * <p><b>MEMORY OWNERSHIP:</b> The ByteBuffers are <b>weak references</b> to C++ memory.
     * The JVM does NOT own this memory and cannot prevent it from being freed.
     * You do NOT need to manually free/close the ByteBuffers - they will be garbage collected.
     * However, GC'ing the ByteBuffer does NOT free the C++ memory (it's owned by PickingRegistry).</p>
     *
     * <p><b>LIFETIME WARNING:</b> The buffers are only valid while the FilamentAsset exists
     * and releaseSourceData() has not been called. Accessing the buffers after the asset
     * is destroyed will cause undefined behavior (crash or garbage data).</p>
     *
     * <p><b>SAFE USAGE PATTERN:</b></p>
     * <pre>
     * // GOOD: Use immediately and don't store
     * val mesh = asset.getMeshData(entityId)
     * mesh?.let { processImmediately(it) }
     *
     * // BAD: Don't cache the ByteBuffer beyond current scope
     * val cachedBuffer = asset.getMeshData(entityId)?.positions  // DANGEROUS!
     * // ... later, asset might be destroyed ...
     * cachedBuffer?.get(0)  // CRASH!
     * </pre>
     */
    public static class MeshData {
        /**
         * Direct ByteBuffer containing positions as floats (x, y, z per vertex).
         * Read as FloatBuffer: positions.order(ByteOrder.nativeOrder()).asFloatBuffer()
         * Each position is 12 bytes (3 floats * 4 bytes)
         */
        @Nullable
        public final java.nio.ByteBuffer positions;

        /**
         * Direct ByteBuffer containing indices as uint32.
         * Read as IntBuffer: indices.order(ByteOrder.nativeOrder()).asIntBuffer()
         * Each index is 4 bytes
         */
        @Nullable
        public final java.nio.ByteBuffer indices;

        /**
         * Direct ByteBuffer containing expanded indices as uint32.
         * Read as IntBuffer: expandedIndices.order(ByteOrder.nativeOrder()).asIntBuffer()
         * Each index is 4 bytes
         * This is useful for rendering operations where vertex sharing is not needed.
         */
        @Nullable
        public final java.nio.ByteBuffer expandedIndices;

        MeshData(java.nio.ByteBuffer positions,
                 java.nio.ByteBuffer indices,
                 java.nio.ByteBuffer expandedIndices) {
            this.positions = positions;
            this.indices = indices;
            this.expandedIndices = expandedIndices;
        }
    }

    /**
     * Gets mesh data (positions, indices, and expanded indices) for an entity.
     *
     * <p>The returned {@link MeshData} contains three direct {@link java.nio.ByteBuffer}s that
     * reference native C++ memory owned by the {@code PickingRegistry} — no copy is made.</p>
     *
     * <ul>
     *   <li><b>positions</b> — float3 per vertex (x, y, z), 12 bytes each.
     *       Vertex count = {@code positions.capacity() / 12}.</li>
     *   <li><b>indices</b> — uint32 per index slot, 4 bytes each, 3 slots per triangle.
     *       Triangle count = {@code indices.capacity() / 4 / 3}.</li>
     *   <li><b>expandedIndices</b> — uint32, same layout as indices but remapped to the
     *       expanded (de-duplicated) vertex buffer used by the GPU renderable. Present only
     *       when the asset was loaded via {@code AssetLoaderExtended}; may be null otherwise.</li>
     * </ul>
     *
     * <p><b>IMPORTANT:</b> The buffers are only valid while the {@link FilamentAsset} exists.
     * Do not cache or use them after the asset is destroyed.</p>
     *
     * <p>Example usage:</p>
     * <pre>
     * MeshData mesh = asset.getMeshData(entityId);
     * if (mesh != null && mesh.positions != null && mesh.indices != null) {
     *     FloatBuffer positions = mesh.positions
     *         .order(ByteOrder.nativeOrder())
     *         .asFloatBuffer();
     *     int vertexCount = mesh.positions.capacity() / 12; // 3 floats * 4 bytes
     *
     *     IntBuffer indices = mesh.indices
     *         .order(ByteOrder.nativeOrder())
     *         .asIntBuffer();
     *     int triangleCount = mesh.indices.capacity() / 4 / 3; // uint32, 3 per triangle
     *
     *     for (int i = 0; i < triangleCount; i++) {
     *         int i0 = indices.get(i * 3);
     *         int i1 = indices.get(i * 3 + 1);
     *         int i2 = indices.get(i * 3 + 2);
     *     }
     * }
     * </pre>
     *
     * @param entityId The entity to get mesh data for
     * @return {@link MeshData} containing positions, indices, and expandedIndices,
     *         or null if the entity is not registered in the PickingRegistry or
     *         any buffer size exceeds {@link Integer#MAX_VALUE}
     */
    @Nullable
    public MeshData getMeshData(@Entity int entityId) {
        long[] info = nGetMeshDataInfo(mNativeObject, entityId);
        if (info == null) {
            return null;
        }

        // info: [positionsPtr, positionsSize, indicesPtr, indicesSize, expandedIndicesPtr, expandedIndicesSize]
        long positionsPtr = info[0];
        long positionsSize = info[1];
        long indicesPtr = info[2];
        long indicesSize = info[3];
        long expandedIndicesPtr = info[4];
        long expandedIndicesSize = info[5];

        // Guard against silent truncation: ByteBuffer capacity is int-sized (max 2,147,483,647 bytes).
        // Fail fast with null rather than producing a ByteBuffer with a truncated capacity
        // that silently reads garbage beyond the true end of the data.
        //   positions:        max ~178M vertices     (Integer.MAX_VALUE / 12 bytes per float3)
        //   indices:          max ~178M triangles    (Integer.MAX_VALUE / 4 bytes per uint32 / 3 indices per triangle)
        //   expandedIndices:  max ~178M triangles    (same as indices)
        if (positionsSize > Integer.MAX_VALUE) {
            android.util.Log.e("FilamentAsset",
                    "getMeshData: positionsSize " + positionsSize + " exceeds Integer.MAX_VALUE for entity " + entityId);
            return null;
        }
        if (indicesSize > Integer.MAX_VALUE) {
            android.util.Log.e("FilamentAsset",
                    "getMeshData: indicesSize " + indicesSize + " exceeds Integer.MAX_VALUE for entity " + entityId);
            return null;
        }
        if (expandedIndicesSize > Integer.MAX_VALUE) {
            android.util.Log.e("FilamentAsset",
                    "getMeshData: expandedIndicesSize " + expandedIndicesSize + " exceeds Integer.MAX_VALUE for entity " + entityId);
            return null;
        }

        // Safe to cast to int now — all sizes verified above to fit within Integer.MAX_VALUE.
        java.nio.ByteBuffer positions = positionsPtr != 0 && positionsSize > 0
            ? nNewDirectByteBuffer(positionsPtr, (int) positionsSize) : null;
        java.nio.ByteBuffer indices = indicesPtr != 0 && indicesSize > 0
            ? nNewDirectByteBuffer(indicesPtr, (int) indicesSize) : null;
        java.nio.ByteBuffer expandedIndices = expandedIndicesPtr != 0 && expandedIndicesSize > 0
            ? nNewDirectByteBuffer(expandedIndicesPtr, (int) expandedIndicesSize) : null;

        return new MeshData(positions, indices, expandedIndices);
    }

    void clearNativeObject() {
        mPrimaryInstance = null;
        mNativeObject = 0;
    }

    private static native int nGetRoot(long nativeAsset);
    private static native int nPopRenderable(long nativeAsset);
    private static native int nPopRenderables(long nativeAsset, int[] result);

    private static native long nGetTriangleCount(long nativeAsset);

    private static native int nGetEntityCount(long nativeAsset);
    private static native void nGetEntities(long nativeAsset, int[] result);

    private static native int nGetFirstEntityByName(long nativeAsset, String name);
    private static native int nGetEntitiesByName(long nativeAsset, String name, int[] result);
    private static native int nGetEntitiesByPrefix(long nativeAsset, String prefix, int[] result);

    private static native int nGetLightEntityCount(long nativeAsset);
    private static native void nGetLightEntities(long nativeAsset, int[] result);

    private static native int nGetRenderableEntityCount(long nativeAsset);
    private static native void nGetRenderableEntities(long nativeAsset, int[] result);

    private static native int nGetCameraEntityCount(long nativeAsset);
    private static native void nGetCameraEntities(long nativeAsset, int[] result);

    private static native int nGetMorphTargetCount(long nativeAsset, int entity);
    private static native void nGetMorphTargetNames(long nativeAsset, int entity, String[] result);

    private static native void nGetBoundingBox(long nativeAsset, float[] box);
    private static native String nGetName(long nativeAsset, int entity);
    private static native String nGetExtras(long nativeAsset, int entity);

    private static native long nGetInstance(long nativeAsset);

    private static native int nGetResourceUriCount(long nativeAsset);
    private static native void nGetResourceUris(long nativeAsset, String[] result);

    private static native void nReleaseSourceData(long nativeAsset);

    private static native int[] nPick(long nativeAsset, long nativeView, long nativeEngine,
                                      int entityId, int screenX, int screenY, int[] skipRanges);

    private static native long nGetPickingRegistry(long nativeAsset);

    private static native long[] nGetMeshDataInfo(long nativeAsset, int entityId);

    // Creates a direct ByteBuffer from a native pointer.
    // capacity is int — callers must verify the size fits within Integer.MAX_VALUE before calling.
    private static native java.nio.ByteBuffer nNewDirectByteBuffer(long address, int capacity);
}
