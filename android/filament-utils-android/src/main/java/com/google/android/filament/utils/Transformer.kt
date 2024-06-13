package com.google.android.filament.utils

import com.google.android.filament.Box
import com.google.android.filament.Engine
import com.google.android.filament.utils.Float3
import com.google.android.filament.utils.max
import com.google.android.filament.utils.scale
import com.google.android.filament.utils.translation
import com.google.android.filament.utils.transpose

class Transformer(val engine: Engine) {

    companion object {
        private val kDefaultObjectPosition = Float3(0.0f, 0.0f, -4.0f)
    }

    fun transform(boundingBox: Box, renderable: Int) {
        val tm = engine.transformManager
        var center = boundingBox.center.let { v -> Float3(v[0], v[1], v[2]) }
        val halfExtent = boundingBox.halfExtent.let { v -> Float3(v[0], v[1], v[2]) }
        val maxExtent = 2.0f * max(halfExtent)
        val scaleFactor = 5.0f / maxExtent
        center -= kDefaultObjectPosition / scaleFactor
        val transform = scale(Float3(scaleFactor)) * translation(-center)
        tm.setTransform(tm.getInstance(renderable), transpose(transform).toFloatArray())
    }
}
