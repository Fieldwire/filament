package com.google.android.filament.utils

import com.google.android.filament.Box
import com.google.android.filament.Engine
import com.google.android.filament.EntityManager
import com.google.android.filament.MaterialInstance
import com.google.android.filament.RenderableManager
import com.google.android.filament.utils.Float3
import com.google.android.filament.utils.max
import com.google.android.filament.utils.scale
import com.google.android.filament.utils.translation
import com.google.android.filament.utils.transpose

class Renderabler(val engine: Engine) {

    fun create(
        buffers: Buffers,
        materialInstance: MaterialInstance,
        priority: Int = 0,
        boundingBox: Box = Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.01f)
    ): Int {

        val (vertexBuffer, indexBuffer) = buffers

        val renderable = EntityManager.get().create()

        // We then create a renderable component on that entity
        // A renderable is made of several primitives; in this case we declare only 1
        RenderableManager.Builder(1)
            .instances(2)
            .priority(priority)
            // Overall bounding box of the renderable
            .culling(false)
            .castShadows(false)
            .receiveShadows(false)
            .fog(false)
            // Sets the mesh data of the first primitive
            .geometry(0, RenderableManager.PrimitiveType.TRIANGLES, vertexBuffer, indexBuffer, 0, 6)
            // Sets the material of the first primitive
            .material(0, materialInstance)
            .build(engine, renderable)

        return renderable
    }
}
