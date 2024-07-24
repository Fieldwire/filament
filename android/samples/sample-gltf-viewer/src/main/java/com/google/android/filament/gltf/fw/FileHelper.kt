package com.google.android.filament.gltf.fw

import android.content.Context
import java.io.File
import java.io.FileNotFoundException
import java.io.IOException
import java.nio.ByteBuffer

@Throws(FileNotFoundException::class, IOException::class)
fun readAssetAsByteBuffer(context: Context, assetName: String): ByteBuffer {
    return context.assets.open(assetName).use { inputStream ->
        val bytes = ByteArray(inputStream.available())
        inputStream.read(bytes)
        ByteBuffer.wrap(bytes)
    }.also {
        logg("assetAsByteBuffer", it)
    }
}

@Throws(FileNotFoundException::class, IOException::class)
fun readFileAsByteBuffer(fileName: String): ByteBuffer {
    return File(fileName).inputStream().use { inputStream ->
        val bytes = ByteArray(inputStream.available())
        inputStream.read(bytes)
        ByteBuffer.wrap(bytes)
    }
}
