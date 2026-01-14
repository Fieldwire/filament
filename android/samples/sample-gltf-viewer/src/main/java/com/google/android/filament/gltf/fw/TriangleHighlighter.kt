package com.google.android.filament.gltf.fw

import android.content.Context
import com.google.android.filament.*
import com.google.android.filament.utils.Float3
import com.google.android.filament.utils.cross
import com.google.android.filament.utils.normalize
import java.nio.ByteBuffer

/**
 * Builds and manages a standalone overlay renderable to highlight a single triangle.
 *
 * Limitations:
 * - This class needs the triangle's model-space vertices (v0, v1, v2). Android gltfio does not
 *   currently expose triangle-level picking, so callers must provide these vertices, e.g., via
 *   their own mesh data or a JNI bridge to gltfio PickingRegistry.
 *
 * Usage:
 * - Call [showTriangle] with the hit renderable entity, its model-space triangle vertices, and
 *   the Filament Engine.
 * - The overlay renderable is parented to the provided parent entity (renderable) so it persists
 *   under rotate / translate / scale.
 * - Call [clear] to remove it.
 */
class TriangleHighlighter(
    private val context: Context,
    private val viewer: FWModelViewer,
) {
    private var overlayEntity: Int = 0
    private var vb: VertexBuffer? = null
    private var ib: IndexBuffer? = null
    private var materialInstance: MaterialInstance? = null

    init {
        materialInstance = loadUnlitHighlightMaterial(viewer.engine)
    }

    fun isVisible(): Boolean = overlayEntity != 0

    /**
     * Show a triangle overlay.
     * @param parentEntity The renderable entity to parent to (inherits transforms, including rotation).
     * @param v0 First vertex in model space.
     * @param v1 Second vertex in model space.
     * @param v2 Third vertex in model space.
     */
    fun showTriangle(@Entity parentEntity: Int, v0: Float3, v1: Float3, v2: Float3) {
        clear()
        val engine = viewer.engine

        // Build model-space VB with a tiny normal lift to avoid coplanar z-fighting.
        data class TriV(val x: Float, val y: Float, val z: Float)
        fun TriV.toArray() = floatArrayOf(x, y, z)

        val n = normalize(cross(v1 - v0, v2 - v0))
        val lift = n * 1e-4f
        val p0 = TriV(v0.x + lift.x, v0.y + lift.y, v0.z + lift.z)
        val p1 = TriV(v1.x + lift.x, v1.y + lift.y, v1.z + lift.z)
        val p2 = TriV(v2.x + lift.x, v2.y + lift.y, v2.z + lift.z)

        // Pack positions into a direct buffer.
        val verts = floatArrayOf(*p0.toArray(), *p1.toArray(), *p2.toArray())
        val vbuf = ByteBuffer.allocateDirect(verts.size * 4).order(java.nio.ByteOrder.nativeOrder())
        verts.forEach { vbuf.putFloat(it) }
        vbuf.flip()

        vb = VertexBuffer.Builder()
            .bufferCount(1)
            .vertexCount(3)
            .attribute(VertexBuffer.VertexAttribute.POSITION, 0, VertexBuffer.AttributeType.FLOAT3, 0, 12)
            .build(engine)
        vb!!.setBufferAt(engine, 0, vbuf)

        val indices = shortArrayOf(0, 1, 2)
        val ibuf = ByteBuffer.allocateDirect(indices.size * 2).order(java.nio.ByteOrder.nativeOrder())
        indices.forEach { ibuf.putShort(it) }
        ibuf.flip()
        ib = IndexBuffer.Builder().indexCount(3).bufferType(IndexBuffer.Builder.IndexType.USHORT).build(engine)
        ib!!.setBuffer(engine, ibuf)

        overlayEntity = EntityManager.get().create()

        // Ensure transform component and parent to the renderable so rotations persist.
        val tcm = engine.transformManager
        if (!tcm.hasComponent(overlayEntity)) {
            tcm.create(overlayEntity)
        }
        val parentInst = tcm.getInstance(parentEntity)
        val overlayInst = tcm.getInstance(overlayEntity)
        if (parentInst != 0 && overlayInst != 0) {
            tcm.setParent(overlayInst, parentInst)
            tcm.setTransform(overlayInst, kIdentityTransform)
        }

        // Build renderable: unlit, double-sided, culling disabled.
        val mi = materialInstance ?: return
        val builder = RenderableManager.Builder(1)
            .geometry(0, RenderableManager.PrimitiveType.TRIANGLES, vb!!, ib!!, 0, 3)
            .material(0, mi)
            .culling(false)
            .receiveShadows(false)
            .castShadows(false)
            .priority(7)
        builder.build(engine, overlayEntity)

        viewer.scene.addEntity(overlayEntity)
    }

    fun clear() {
        if (overlayEntity != 0) {
            viewer.scene.removeEntity(overlayEntity)
            viewer.engine.destroyEntity(overlayEntity)
            overlayEntity = 0
        }
        vb?.let { viewer.engine.destroyVertexBuffer(it) }
        ib?.let { viewer.engine.destroyIndexBuffer(it) }
        vb = null
        ib = null
    }

    private fun loadUnlitHighlightMaterial(engine: Engine): MaterialInstance? {
        return try {
            val buffer = readAssetAsByteBuffer(context, ObjectHighlighter.MAT_FILE)
            val mat = Material.Builder().payload(buffer, buffer.remaining()).build(engine)
            mat.defaultInstance
        } catch (t: Throwable) {
            null
        }
    }

    companion object {
        private val kIdentityTransform = floatArrayOf(
            1f, 0f, 0f, 0f,
            0f, 1f, 0f, 0f,
            0f, 0f, 1f, 0f,
            0f, 0f, 0f, 1f
        )
    }
}

