package com.google.android.filament.gltf

import android.util.Log

fun logg(vararg msg: Any) {
    Log.d("fml", msg.joinToString("\t"))
}
