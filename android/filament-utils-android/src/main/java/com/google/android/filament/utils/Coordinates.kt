package com.google.android.filament.utils

fun unproject(
    x: Float,
    y: Float,
    z: Float = 0f,
    projMatrix: Mat4,
    viewMatrix: Mat4,
    screenWidth: Int,
    screenHeight: Int
): Float4 {

    val vec = Float4(x, y, z, 1f)
    return projMatrix.times(vec).xyzw

    return Float4(x, y, z)

    logg("Proj matrix", projMatrix, "View matrix", viewMatrix)

    val inverseProjectionMat = inverse(projMatrix)
    logg("Inverse projection matrix", inverseProjectionMat)

    val inverseViewMatrix = inverse(viewMatrix)
    logg("Inverse view matrix", inverseViewMatrix)

    // Normalize the screen coordinates to the range [-1, 1]
    val normalizedX = (2.0f * x) / screenWidth - 1.0f
    val normalizedY = 1.0f - (2.0f * y) / screenHeight
    logg("NormX $normalizedX", "NormY $normalizedY")

    var normalized = Float4(normalizedX, normalizedY, 0f, 1f)

    // Invert the transformation to get the 3D coordinates
    normalized = inverseProjectionMat.times(normalized)
    logg("Normalized projection $normalized")

//    normalized = inverseViewMatrix.times(normalized)
//    logg("Normalized view $normalized")

    normalized.div(normalized.w)
    logg("Normalized div $normalized")

    return normalized
}


val FloatArray.mat4: Mat4 get() {
    val floats = asList().chunked(4).map {
        Float4(it[0], it[1], it[2], it[3])
    }
    return Mat4(floats[0], floats[1], floats[2], floats[3])
}

val DoubleArray.mat4: Mat4 get() {
    return asList().map { it.toFloat() }.mat4
}

val List<Float>.mat4: Mat4 get() {
    val floats = chunked(4).map {
        Float4(it[0], it[1], it[2], it[3])
    }
    return Mat4(floats[0], floats[1], floats[2], floats[3])
}
