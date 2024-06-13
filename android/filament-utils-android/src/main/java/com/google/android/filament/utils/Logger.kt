package com.google.android.filament.utils

import android.util.Log

fun logg(vararg msg: Any) {
    Log.d("MDL", msg.joinToString("\t"))
}

fun logg2(vararg msg: Any) {
    Log.d("MDL2", msg.joinToString("\t"))
}
