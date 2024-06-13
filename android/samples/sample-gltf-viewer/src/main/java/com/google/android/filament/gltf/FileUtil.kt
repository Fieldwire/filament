package com.google.android.filament.gltf

import android.content.Context
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

object FileUtil {

    fun copyFileFromAssetsToDisk(context: Context, destFileName: String, sourceFileName: String,) {
        // Specify the destination file on the device's disk
        val destinationFile = File(destFileName)

        if (destinationFile.exists()) return

        try {
            // Open an InputStream to the file in the assets folder
            val inputStream = context.assets.open(sourceFileName)

            destinationFile.parentFile?.mkdirs()
            destinationFile.createNewFile()

            val outputStream = destinationFile.outputStream()

            inputStream.copyTo(outputStream)

            // Close the streams
            inputStream.close()
            outputStream.close()

        } catch (e: IOException) {
            e.printStackTrace()
        }
    }
}
