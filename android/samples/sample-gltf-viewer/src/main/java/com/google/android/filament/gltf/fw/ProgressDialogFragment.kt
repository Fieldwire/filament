package com.google.android.filament.gltf.fw

import android.app.AlertDialog
import android.app.Dialog
import android.app.DialogFragment
import android.os.Bundle
import android.view.KeyEvent
import androidx.core.content.ContentProviderCompat.requireContext


class ProgressDialogFragment : DialogFragment() {

    var callback: Callback? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        retainInstance = true
        isCancelable = false
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val ui = ProgressDialogUi()
        val view = ui.createView(context)
        callback?.getLoadingMessage()?.let { ui.textView.text = it }

        return AlertDialog
                .Builder(context)
                .setView(view)
                .setOnKeyListener { dialog, keyCode, _ ->
                    if (keyCode == KeyEvent.KEYCODE_BACK) {
                        dialog.dismiss()
                        callback?.onProgressCancelled()
                    }

                    true
                }
                .create()
    }

    interface Callback {
        fun onProgressCancelled()
        fun getLoadingMessage(): String
    }
}
