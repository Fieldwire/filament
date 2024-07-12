@file:Suppress("LateinitUsage")
package com.google.android.filament.gltf.fw

import android.content.Context
import android.os.Handler
import android.os.Looper
import com.google.android.filament.Entity
import com.google.android.filament.Material
import com.google.android.filament.MaterialInstance
import com.google.android.filament.utils.ModelViewer

// Highlights the renderable entity (or object)
class ObjectHighlighter(private val context: Context, private val modelViewer: ModelViewer) {

    // Used to highlight the selected renderable entity
    // To change the highlight color, modify `tools/material/highlight.mat` & follow the instruction
    // in that file to create a compiled `.filamat` file
    private lateinit var highlightMaterial: MaterialInstance

    // Entity is nothing but an individual object that creates a scene including camera, light and renderables.
    // Of these three entities, only renderable entities are displayed on the screen. This stores the currently
    // selected entity (renderable) to reset the appearance later
    @Entity
    var highlightedEntity: Int? = null
        private set

    // Stores the material for all the primitives in the selected renderable entity
    // Refer to the diagram in notes/filament/gltf to know about primitives in gltf
    private var actualEntityMaterials: List<MaterialInstance>? = null

    init {
        loadObjectHighlightMaterial()
    }

    fun highlight(@Entity renderableEntity: Int): Boolean {
        // Asset contains all entities - Light entity, Camera entity & Renderable entities
        // Camera & Light entities aren't displayed on the screen
        // Renderable entities make up the entire model and are displayed on the screen
        val asset = modelViewer.asset ?: return false

        // Contains check on int array is pretty fast, benchmark result shows it takes about 0.0009 second
        // for 100K elements on a mid range android device
        if (asset.renderableEntities.contains(renderableEntity)) {
            // Retain the selected entity to clear later
            highlightedEntity = renderableEntity

            modelViewer.engine.renderableManager.run {
                val renderableInstance = getInstance(renderableEntity)

                // Get the primitives (geometries) count for this renderable/mesh
                // Refer to the diagram in notes/filament/gltf to know about primitives
                val primitivesCount = getPrimitiveCount(renderableInstance)

                // Stores the actual material of all primitives in the this renderable entity
                val materials = ArrayList<MaterialInstance>(primitivesCount)

                for (primitiveIndex in 0 until primitivesCount) {
                    materials.add(getMaterialInstanceAt(renderableInstance, primitiveIndex))
                    // Set the same highlight material to all primitives
                    setMaterialInstanceAt(renderableInstance, primitiveIndex, highlightMaterial)
                }

                // Retain the actual material for all the primitives to restore later
                actualEntityMaterials = materials
            }
            return true
        }

        return false
    }

    fun unhighlight(): Boolean {
        val tmpSelectedEntity = highlightedEntity
        val tmpActualMaterials = actualEntityMaterials
        // There's an active selection
        if (tmpSelectedEntity != null) {
            if (tmpActualMaterials != null) {
                modelViewer.engine.renderableManager.run {
                    val renderableInstance = getInstance(tmpSelectedEntity)
                    tmpActualMaterials.forEachIndexed { idx, material ->
                        setMaterialInstanceAt(renderableInstance, idx, material)
                    }
                }
            }
            highlightedEntity = null
            actualEntityMaterials = null
            return true
        }
        return false
    }

    private fun loadObjectHighlightMaterial() {
        val engine = modelViewer.engine
        readAssetAsByteBuffer(context, MAT_FILE).let { buffer ->
            val material = Material.Builder().payload(buffer, buffer.remaining()).build(engine)
            material.compile(
                Material.CompilerPriorityQueue.HIGH,
                Material.UserVariantFilterBit.ALL,
                Handler(Looper.getMainLooper())
            ) { /* We're compiling the simplest mat file & it will be done before the model is displayed */ }

            highlightMaterial = material.defaultInstance

            engine.flush()
        }
    }

    companion object {
        private const val MAT_FILE = "materials/object_highlight.filamat"
    }
}
