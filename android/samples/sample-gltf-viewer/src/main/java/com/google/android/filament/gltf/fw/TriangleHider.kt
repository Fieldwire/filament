package com.google.android.filament.gltf.fw

import android.util.Log
import com.google.android.filament.*
import com.google.android.filament.gltfio.FilamentAsset
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Hides a subset of triangles by building a new IndexBuffer while keeping the original VertexBuffer unchanged.
 *
 * This class updates the renderable geometry with only the visible triangles, effectively hiding
 * the triangles in the specified range.
 *
 * Approach:
 * 1. Keep the original VertexBuffer (positions/normals/UV) unchanged
 * 2. Build a new IndexBuffer containing only the indices of triangles you want visible
 * 3. Update the geometry using setGeometryAt() - preserves all renderable attributes automatically
 *
 * Restoration:
 * - Stores references to the original VertexBuffer and IndexBuffer before modification
 * - Stores the MaterialInstance to preserve appearance (setGeometryAt resets material to default)
 * - Simply calls setGeometryAt() with the original buffers to restore
 * - Reapplies the material to restore the original appearance
 *
 * The original vertex and index data is retrieved from the PickingRegistry's MeshData.
 * Only one entity can be hidden at a time (auto-restores previous when hiding a new one).
 *
 * Usage:
 * ```kotlin
 * val hider = TriangleHider(engine, asset)
 *
 * // Hide triangles in index range [300, 600]
 * hider.hideTriangles(entity, 300, 600)
 *
 * // Restore original geometry
 * hider.restore()
 * ```
 *
 * @param engine The Filament Engine instance
 * @param asset The FilamentAsset containing the mesh data
 */
class TriangleHider(
    private val engine: Engine,
    private val asset: FilamentAsset
) {
    companion object {
        private const val TAG = "TriangleHider"
    }

    // Only one renderable can be hidden at a time - store original buffers for restoration
    private var hiddenEntity: Int = 0
    private var originalIndexBuffer: IndexBuffer? = null
    private var originalVertexBuffer: VertexBuffer? = null
    private var originalIndexCount: Int = 0
    private var originalMaterialInstance: MaterialInstance? = null

    /**
     * Hide triangles in the specified index range.
     *
     * This keeps the original VertexBuffer unchanged and builds a new IndexBuffer
     * containing only the indices of triangles NOT in the hidden range.
     *
     * @param entity The entity whose triangles to hide
     * @param hideStartIdx Start index (inclusive) of triangles to hide
     * @param hideEndIdx End index (inclusive) of triangles to hide
     * @return True if successful, false otherwise
     */
    fun hideTriangles(@Entity entity: Int, hideStartIdx: Int, hideEndIdx: Int): Boolean {
        try {
            // Auto-restore previous entity if hiding a different one
            if (hiddenEntity != 0 && hiddenEntity != entity) {
                Log.d(TAG, "Auto-restoring previous entity $hiddenEntity before hiding $entity")
                restore()
            }

            // Prevent duplicate operations on same entity
            if (hiddenEntity == entity) {
                Log.w(TAG, "Entity $entity already has hidden triangles. Restore first before hiding again.")
                return false
            }

            // Get mesh data from PickingRegistry
            val allIndices = asset.getMeshIndices(entity)
            if (allIndices == null || allIndices.isEmpty()) {
                Log.w(TAG, "No mesh indices found for entity $entity")
                return false
            }

            val positions = asset.getMeshPositions(entity)
            if (positions == null || positions.isEmpty()) {
                Log.w(TAG, "No mesh positions found for entity $entity")
                return false
            }

            // Validate range
            if (hideStartIdx < 0 || hideEndIdx >= allIndices.size || hideStartIdx > hideEndIdx) {
                Log.w(TAG, "Invalid hide range: [$hideStartIdx, $hideEndIdx], total indices: ${allIndices.size}")
                return false
            }

            Log.d(TAG, "Hiding triangles [$hideStartIdx, $hideEndIdx] out of ${allIndices.size} total indices")

            // Get current renderable info before modifying
            val rm = engine.renderableManager
            val instance = rm.getInstance(entity)
            if (instance == 0) {
                Log.w(TAG, "No renderable instance found for entity $entity")
                return false
            }

            // Capture material BEFORE setGeometryAt because it doesn't preserve it
            originalMaterialInstance = rm.getMaterialInstanceAt(instance, 0)
            Log.d(TAG, "Captured material: ${originalMaterialInstance != null}")

            // IMPORTANT: Store original buffers BEFORE modifying the renderable
            // We'll use these to restore later - much simpler than recreating from data
            originalVertexBuffer = createVertexBuffer(positions, positions.size / 3)
            if (originalVertexBuffer == null) {
                Log.e(TAG, "Failed to create original VertexBuffer copy")
                return false
            }

            originalIndexBuffer = createIndexBuffer(allIndices)
            if (originalIndexBuffer == null) {
                Log.e(TAG, "Failed to create original IndexBuffer copy")
                originalVertexBuffer?.let { engine.destroyVertexBuffer(it) }
                originalVertexBuffer = null
                return false
            }

            originalIndexCount = allIndices.size
            Log.d(TAG, "Stored original buffers: VB with ${positions.size / 3} vertices, IB with ${allIndices.size} indices")

            // Build new index buffer with only visible triangles (excluding the hidden range)
            val visibleIndices = buildVisibleIndices(allIndices, hideStartIdx, hideEndIdx)
            if (visibleIndices.isEmpty()) {
                Log.w(TAG, "No visible triangles remain after hiding range")
                return false
            }

            Log.d(TAG, "Built new IndexBuffer with ${visibleIndices.size} indices (${visibleIndices.size / 3} triangles)")

            // Create new IndexBuffer
            val newIndexBuffer = createIndexBuffer(visibleIndices)
            if (newIndexBuffer == null) {
                Log.e(TAG, "Failed to create new IndexBuffer")
                return false
            }

            // Create VertexBuffer from positions (keeping it unchanged from original)
            val vertexCount = positions.size / 3
            val vertexBuffer = createVertexBuffer(positions, vertexCount)
            if (vertexBuffer == null) {
                Log.e(TAG, "Failed to create VertexBuffer")
                engine.destroyIndexBuffer(newIndexBuffer)
                return false
            }

            // Update geometry on the EXISTING renderable - much more efficient than destroy+rebuild
            // Note: setGeometryAt does NOT preserve material, so we need to reapply it
            rm.setGeometryAt(
                instance,
                0,  // primitiveIndex
                RenderableManager.PrimitiveType.TRIANGLES,
                vertexBuffer,
                newIndexBuffer,
                0,  // offset
                visibleIndices.size  // count
            )

            // Reapply the material to restore appearance (setGeometryAt resets it to default)
            originalMaterialInstance?.let {
                rm.setMaterialInstanceAt(instance, 0, it)
                Log.d(TAG, "Reapplied material to preserve appearance")
            }

            // Store the entity - original buffers are already stored above
            hiddenEntity = entity

            Log.d(TAG, "Successfully hid triangles for entity $entity")
            return true

        } catch (e: Exception) {
            Log.e(TAG, "Error hiding triangles for entity $entity", e)
            return false
        }
    }

    /**
     * Build a new index array containing only the visible triangles (excluding the hidden range).
     */
    private fun buildVisibleIndices(allIndices: IntArray, hideStartIdx: Int, hideEndIdx: Int): IntArray {
        val visibleIndices = mutableListOf<Int>()

        // Add indices before the hidden range
        for (i in 0 until hideStartIdx) {
            visibleIndices.add(allIndices[i])
        }

        // Add indices after the hidden range
        for (i in (hideEndIdx + 1) until allIndices.size) {
            visibleIndices.add(allIndices[i])
        }

        return visibleIndices.toIntArray()
    }

    /**
     * Create an IndexBuffer from the given indices.
     */
    private fun createIndexBuffer(indices: IntArray): IndexBuffer? {
        try {
            // Determine if we need UINT or USHORT
            val maxIndex = indices.maxOrNull() ?: 0
            val useUInt = maxIndex > 65535

            val indexBuffer = if (useUInt) {
                // Use 32-bit indices
                val buffer = ByteBuffer.allocateDirect(indices.size * 4)
                    .order(ByteOrder.nativeOrder())
                for (idx in indices) {
                    buffer.putInt(idx)
                }
                buffer.flip()

                IndexBuffer.Builder()
                    .indexCount(indices.size)
                    .bufferType(IndexBuffer.Builder.IndexType.UINT)
                    .build(engine)
                    .apply {
                        setBuffer(engine, buffer)
                    }
            } else {
                // Use 16-bit indices
                val buffer = ByteBuffer.allocateDirect(indices.size * 2)
                    .order(ByteOrder.nativeOrder())
                for (idx in indices) {
                    buffer.putShort(idx.toShort())
                }
                buffer.flip()

                IndexBuffer.Builder()
                    .indexCount(indices.size)
                    .bufferType(IndexBuffer.Builder.IndexType.USHORT)
                    .build(engine)
                    .apply {
                        setBuffer(engine, buffer)
                    }
            }

            return indexBuffer

        } catch (e: Exception) {
            Log.e(TAG, "Error creating IndexBuffer", e)
            return null
        }
    }

    /**
     * Create a VertexBuffer from position data.
     * This keeps the original vertex data unchanged.
     */
    private fun createVertexBuffer(positions: FloatArray, vertexCount: Int): VertexBuffer? {
        try {
            val buffer = ByteBuffer.allocateDirect(positions.size * 4)
                .order(ByteOrder.nativeOrder())
            for (pos in positions) {
                buffer.putFloat(pos)
            }
            buffer.flip()

            val vertexBuffer = VertexBuffer.Builder()
                .bufferCount(1)
                .vertexCount(vertexCount)
                .attribute(
                    VertexBuffer.VertexAttribute.POSITION,
                    0,
                    VertexBuffer.AttributeType.FLOAT3,
                    0,
                    12
                )
                .build(engine)

            vertexBuffer.setBufferAt(engine, 0, buffer)
            return vertexBuffer

        } catch (e: Exception) {
            Log.e(TAG, "Error creating VertexBuffer", e)
            return null
        }
    }

    /**
     * Restore the original geometry by updating it with the original buffers.
     * Simply swaps back the original IndexBuffer and VertexBuffer using setGeometryAt.
     *
     * @return True if successful, false otherwise
     */
    fun restore(): Boolean {
        if (hiddenEntity == 0) {
            Log.w(TAG, "No entity is currently hidden")
            return false
        }

        val origVB = originalVertexBuffer
        val origIB = originalIndexBuffer

        if (origVB == null || origIB == null) {
            Log.e(TAG, "Original buffers are null")
            clear()
            return false
        }

        val entityToRestore = hiddenEntity

        try {
            Log.d(TAG, "Restoring original geometry for entity $entityToRestore")

            // Get the renderable instance
            val rm = engine.renderableManager
            val instance = rm.getInstance(entityToRestore)

            if (instance == 0) {
                Log.e(TAG, "No renderable instance found for entity $entityToRestore")
                clear()
                return false
            }

            // Update geometry on the EXISTING renderable with original buffers
            // Note: setGeometryAt does NOT preserve material, so we need to reapply it
            rm.setGeometryAt(
                instance,
                0,  // primitiveIndex
                RenderableManager.PrimitiveType.TRIANGLES,
                origVB,
                origIB,
                0,  // offset
                originalIndexCount  // count
            )

            // Reapply the original material to restore appearance
            originalMaterialInstance?.let {
                rm.setMaterialInstanceAt(instance, 0, it)
                Log.d(TAG, "Reapplied original material")
            }

            // Clear stored references (buffers are now owned by the renderable again)
            hiddenEntity = 0
            originalIndexBuffer = null
            originalVertexBuffer = null
            originalIndexCount = 0
            originalMaterialInstance = null

            Log.d(TAG, "Successfully restored original geometry for entity $entityToRestore")
            return true

        } catch (e: Exception) {
            Log.e(TAG, "Error restoring geometry for entity $entityToRestore", e)
            clear()
            return false
        }
    }

    /**
     * Check if there is currently a hidden entity.
     *
     * @return True if an entity has hidden triangles, false otherwise
     */
    fun hasHiddenTriangles(): Boolean {
        return hiddenEntity != 0
    }

    /**
     * Get the currently hidden entity ID.
     *
     * @return The entity ID, or 0 if no entity is hidden
     */
    fun getHiddenEntity(): Int {
        return hiddenEntity
    }

    /**
     * Clear the current hidden entity and restore its original geometry.
     * This is the same as calling restore().
     */
    fun clear() {
        if (hiddenEntity != 0) {
            restore()
        } else {
            // Clean up any orphaned references
            originalIndexBuffer = null
            originalVertexBuffer = null
            originalIndexCount = 0
            originalMaterialInstance = null
        }
    }
}

