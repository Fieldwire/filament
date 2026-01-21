package com.google.android.filament.gltf.fw

import com.google.android.filament.*
import com.google.android.filament.gltfio.FilamentAsset
import com.google.android.filament.utils.Float3
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Manages triangle hiding by creating modified index buffers that exclude selected triangles.
 *
 * This mirrors the hideTriangle functionality from gltf_viewer.cpp:
 * - Keeps original vertex buffer (only positions needed - material handles appearance)
 * - Creates new index buffer excluding hidden triangles
 * - Uses setGeometryAt to update the renderable's geometry
 *
 * Usage:
 * - Call [hideTriangle] to hide a single triangle
 * - Call [hideTriangles] to hide multiple triangles from the same entity
 * - Materials, textures, and colors are preserved (they come from MaterialInstance)
 */
class TriangleHider(
    val engine: Engine,
    private val asset: FilamentAsset
) {
    // Tracks hidden triangle info per entity
    private val hiddenTrianglesMap = mutableMapOf<Int, HiddenTriangleInfo>()

    private data class HiddenTriangleInfo(
        val entity: Int,
        val hiddenTriangleIndices: MutableSet<Int>,
        var createdVertexBuffer: VertexBuffer?,   // VB we create (owned by us)
        var modifiedIndexBuffer: IndexBuffer?      // IB we create (owned by us)
    )

    /**
     * Hide a single triangle from the specified entity.
     * Gets mesh data internally from the asset's picking registry.
     * This mirrors the onClick pattern from gltf_viewer.cpp.
     *
     * @param entityId The entity to hide triangle from
     * @param triangleIndex The triangle index to hide
     * @return true if triangle was hidden, false if already hidden or error
     */
    fun hideTriangle(
        @Entity entityId: Int,
        triangleIndex: Int
    ): Boolean {
        // Get mesh data from asset's picking registry - same as gltf_viewer.cpp
        val positions = asset.getMeshPositions(entityId)
        val indices = asset.getMeshIndices(entityId)

        if (positions == null || indices == null) {
            return false
        }

        // Convert to List<Float3> format
        val positionsList = mutableListOf<Float3>()
        for (i in 0 until positions.size step 3) {
            if (i + 2 < positions.size) {
                positionsList.add(Float3(positions[i], positions[i + 1], positions[i + 2]))
            }
        }

        val indicesList = indices.toList()

        // Call the main hideTriangle implementation
        return hideTriangle(entityId, triangleIndex, positionsList, indicesList)
    }

    /**
     * Hide a single triangle from the specified entity.
     *
     * @param entityId The entity to hide triangle from
     * @param triangleIndex The triangle index to hide
     * @param positions All vertex positions for the entity (from picking mesh data)
     * @param indices All triangle indices for the entity (from picking mesh data)
     * @return true if triangle was hidden, false if already hidden
     */
    fun hideTriangle(
        @Entity entityId: Int,
        triangleIndex: Int,
        positions: List<Float3>,
        indices: List<Int>
    ): Boolean {
        val rcm = engine.renderableManager
        val renderableInst = rcm.getInstance(entityId)
        if (renderableInst == 0) {
            return false
        }

        val existingInfo = hiddenTrianglesMap[entityId]

        if (existingInfo == null) {
            // First time hiding a triangle for this entity
            return createHiddenTriangleInfo(entityId, triangleIndex, positions, indices, renderableInst, rcm)
        } else {
            // Already have hidden triangles - hide another one
            if (existingInfo.hiddenTriangleIndices.contains(triangleIndex)) {
                // Already hidden
                return false
            }

            existingInfo.hiddenTriangleIndices.add(triangleIndex)
            updateIndexBuffer(existingInfo, indices, renderableInst, rcm)
            return true
        }
    }

    /**
     * Hide multiple triangles from the same entity.
     */
    fun hideTriangles(
        @Entity entityId: Int,
        triangleIndices: List<Int>,
        positions: List<Float3>,
        indices: List<Int>
    ) {
        triangleIndices.forEach { triangleIndex ->
            hideTriangle(entityId, triangleIndex, positions, indices)
        }
    }

    /**
     * Hide a range of triangles by index range.
     * This is used when you have a mapping of triangle ranges to hide.
     *
     * @param entityId The entity to hide triangles from
     * @param startIdx Start index in the index buffer (must be multiple of 3)
     * @param endIdx End index in the index buffer (inclusive)
     * @return true if successful, false otherwise
     */
    fun hideTriangles(
        @Entity entityId: Int,
        startIdx: Int,
        endIdx: Int
    ): Boolean {
        // Get mesh data from the asset's picking registry
        val meshData = asset.getMeshDataForEntity(entityId) ?: return false

        // Calculate which triangle indices fall within the range
        val triangleIndices = mutableListOf<Int>()
        var idx = startIdx
        while (idx <= endIdx && idx + 2 < meshData.indices.size) {
            val triangleIdx = idx / 3
            triangleIndices.add(triangleIdx)
            idx += 3
        }

        if (triangleIndices.isEmpty()) {
            return false
        }

        // Hide the triangles
        hideTriangles(entityId, triangleIndices, meshData.positions, meshData.indices)
        return true
    }

    /**
     * Restore all hidden triangles (clears all hiding).
     * This is called before hiding new triangles to ensure only one set is hidden at a time.
     */
    fun restore() {
        clearAll()
    }

    /**
     * Clear hidden triangles for a specific entity (restore original geometry).
     */
    fun clearEntity(@Entity entityId: Int) {
        hiddenTrianglesMap[entityId]?.let { info ->
            // Destroy the vertex buffer and index buffer we created
            info.createdVertexBuffer?.let { engine.destroyVertexBuffer(it) }
            info.modifiedIndexBuffer?.let { engine.destroyIndexBuffer(it) }

            // TODO: Restore original geometry to the renderable
            // This would require keeping a reference to the original buffers
            // For now, just remove from map

            hiddenTrianglesMap.remove(entityId)
            logg("Cleared hidden triangles for entity $entityId")
        }
    }

    /**
     * Clear all hidden triangles.
     */
    fun clearAll() {
        hiddenTrianglesMap.keys.toList().forEach { clearEntity(it) }
    }

    /**
     * Helper data class for mesh data retrieved from picking registry
     */
    private data class MeshData(
        val positions: List<Float3>,
        val indices: List<Int>
    )

    /**
     * Get mesh data for an entity from the asset's JNI picking methods
     */
    private fun FilamentAsset.getMeshDataForEntity(entityId: Int): MeshData? {
        try {
            // Get positions and indices using existing JNI methods
            val positionsArray = this.getMeshPositions(entityId) ?: return null
            val indicesArray = this.getMeshIndices(entityId) ?: return null

            // Convert float array to List<Float3>
            val positions = mutableListOf<Float3>()
            var i = 0
            while (i < positionsArray.size) {
                positions.add(Float3(positionsArray[i], positionsArray[i + 1], positionsArray[i + 2]))
                i += 3
            }

            // Convert int array to List<Int>
            val indices = indicesArray.toList()

            return MeshData(positions, indices)
        } catch (e: Exception) {
            logg("Error getting mesh data for entity $entityId: ${e.message}")
            return null
        }
    }

    private fun logg(vararg args: Any?) {
        println("TriangleHider: ${args.joinToString(" ")}")
    }

    private fun createHiddenTriangleInfo(
        entityId: Int,
        triangleIndex: Int,
        positions: List<Float3>,
        indices: List<Int>,
        renderableInst: Int,
        rcm: RenderableManager
    ): Boolean {
        val vertexCount = positions.size

        // Create vertex buffer with only POSITION attribute (like gltf_viewer.cpp)
        // AssetLoaderExtended will generate face normals if needed
        val vb = VertexBuffer.Builder()
            .bufferCount(1)
            .vertexCount(vertexCount)
            .attribute(VertexBuffer.VertexAttribute.POSITION, 0, VertexBuffer.AttributeType.FLOAT3, 0, 12)
            .build(engine)

        // Copy position data
        val vbuf = ByteBuffer.allocateDirect(vertexCount * 12).order(ByteOrder.nativeOrder())
        positions.forEach { pos ->
            vbuf.putFloat(pos.x)
            vbuf.putFloat(pos.y)
            vbuf.putFloat(pos.z)
        }
        vbuf.flip()
        vb.setBufferAt(engine, 0, vbuf)

        // Create index buffer excluding the hidden triangle
        val originalIndexCount = indices.size
        val newIndexCount = originalIndexCount - 3
        val newIndices = IntArray(newIndexCount)

        var writeIdx = 0
        val triangleBase = triangleIndex * 3
        for (i in indices.indices) {
            if (i in triangleBase until triangleBase + 3) {
                continue
            }
            newIndices[writeIdx++] = indices[i]
        }

        val ib = createIndexBuffer(newIndices)

        // Update the renderable's geometry
        rcm.setGeometryAt(
            renderableInst,
            0,
            RenderableManager.PrimitiveType.TRIANGLES,
            vb,
            ib,
            0,
            newIndexCount
        )

        // Store info
        hiddenTrianglesMap[entityId] = HiddenTriangleInfo(
            entity = entityId,
            hiddenTriangleIndices = mutableSetOf(triangleIndex),
            createdVertexBuffer = vb,
            modifiedIndexBuffer = ib
        )

        logg("Hidden triangle $triangleIndex from entity $entityId. New index count: $newIndexCount")
        return true
    }

    private fun updateIndexBuffer(
        info: HiddenTriangleInfo,
        originalIndices: List<Int>,
        renderableInst: Int,
        rcm: RenderableManager
    ) {
        // Rebuild index buffer excluding all hidden triangles
        val originalIndexCount = originalIndices.size
        val newIndexCount = originalIndexCount - (info.hiddenTriangleIndices.size * 3)
        val newIndices = IntArray(newIndexCount)

        var writeIdx = 0
        var i = 0
        while (i < originalIndexCount) {
            val triangleIdx = i / 3
            if (!info.hiddenTriangleIndices.contains(triangleIdx)) {
                newIndices[writeIdx++] = originalIndices[i]
                newIndices[writeIdx++] = originalIndices[i + 1]
                newIndices[writeIdx++] = originalIndices[i + 2]
            }
            i += 3
        }

        // Destroy old index buffer
        info.modifiedIndexBuffer?.let { engine.destroyIndexBuffer(it) }

        // Create new index buffer
        val ib = createIndexBuffer(newIndices)
        info.modifiedIndexBuffer = ib

        // Update geometry (vertex buffer stays the same)
        rcm.setGeometryAt(
            renderableInst,
            0,
            RenderableManager.PrimitiveType.TRIANGLES,
            info.createdVertexBuffer!!,
            ib,
            0,
            newIndexCount
        )

        logg("Updated index buffer. Total hidden: ${info.hiddenTriangleIndices.size}. New index count: $newIndexCount")
    }

    private fun createIndexBuffer(indices: IntArray): IndexBuffer {
        val ibuf = ByteBuffer.allocateDirect(indices.size * 4).order(ByteOrder.nativeOrder())
        indices.forEach { ibuf.putInt(it) }
        ibuf.flip()

        val ib = IndexBuffer.Builder()
            .indexCount(indices.size)
            .bufferType(IndexBuffer.Builder.IndexType.UINT)
            .build(engine)
        ib.setBuffer(engine, ibuf)

        return ib
    }
}

