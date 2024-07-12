package com.google.android.filament.gltf.fw

import android.content.Context
import android.view.View
import android.widget.TextView
import com.google.android.filament.gltf.R

class ProgressDialogUi {

    lateinit var textView: TextView

    fun createView(context: Context): View {
        val view = View.inflate(context, R.layout.layout_progress_dialog, null)

        textView = view.findViewById(R.id.progress_text)

        return view
    }
}
