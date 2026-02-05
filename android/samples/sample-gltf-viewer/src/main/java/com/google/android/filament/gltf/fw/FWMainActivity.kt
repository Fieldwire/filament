/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.android.filament.gltf.fw

import android.annotation.SuppressLint
import android.content.Intent
import android.content.res.Configuration
import android.os.Bundle
import android.util.Log
import android.view.WindowManager
import android.widget.TextView
import androidx.activity.OnBackPressedCallback
import androidx.appcompat.app.AppCompatActivity
import com.google.android.filament.gltf.R
import com.google.android.filament.utils.Utils

fun logg(vararg msg: Any, tag: String = "fml") {
    Log.d(tag, msg.joinToString("\t"))
}

class FWMainActivity : AppCompatActivity(), BimExecutor.Callback, ProgressDialogFragment.Callback {

    companion object {
        // Load the library for the utility layer, which in turn loads gltfio and the Filament core.
        init { Utils.init() }
        private const val TAG = "gltf-viewer"
    }

    private var progressDialogFragment: ProgressDialogFragment? = null
    private lateinit var frameChoreographer: FrameChoreographer

    private lateinit var bimExecutor: BimExecutor
    private lateinit var titlebarHint: TextView


    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.simple_layout)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        logg("onCreate")

        // Configure overlay Toolbar as the ActionBar so it renders on top of content (if present)
        val toolbar = findViewById<androidx.appcompat.widget.Toolbar?>(R.id.top_toolbar)
        if (toolbar != null) {
            setSupportActionBar(toolbar)
            val titleFromIntent = intent.getStringExtra("model") ?: ""
            supportActionBar?.title = titleFromIntent
            // Add back button to the top app bar using a built-in Android drawable
            toolbar.setNavigationIcon(android.R.drawable.ic_media_previous)
            toolbar.setNavigationOnClickListener { finishWithResult(RESULT_OK) }
        }

        titlebarHint = findViewById(R.id.user_hint)

        bimExecutor = BimExecutor(
            findViewById(R.id.main_sv),
            this
        )
        frameChoreographer = FrameChoreographer(bimExecutor)

        loadModel()

        addOnConfigurationChangedListener {
            logg("configChangedListener")
        }

        onBackPressedDispatcher.addCallback(object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                finishWithResult(RESULT_OK)
            }
        })
    }

    private fun finishWithResult(resultCode: Int, intent: Intent? = null) {
        frameChoreographer.stop()
        bimExecutor.destroyViewer {
            hideProgress()
            setResult(resultCode, intent)
            finish()
        }
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        logg("config changed")
    }

    private fun loadModel() {
        try {
            bimExecutor.loadModel(this@FWMainActivity, intent.getStringExtra("model")!!) { count ->
                frameChoreographer.start()
            }
        } catch (e: Exception) {
            logg("Exception in loadModel", e.message.toString())
        }
    }

    override fun onModelRendered() {
        logg("modelRendered")
        hideProgress()
    }

    override fun onClickProperties(entityId: String?) {

    }

    override fun onExecutorException(throwable: Throwable) {
        logg("executorException", throwable)
    }

    override fun onProgressCancelled() {
        logg("destroyingViewer")
        bimExecutor.destroyViewer {
            setResult(RESULT_OK)
            finish()
            logg("destroyed")
        }
    }

    override fun getLoadingMessage(): String {
        return "loading model"
    }

    override fun onDestroy() {
        super.onDestroy()
        logg("ondestroy")
    }

    private fun hideProgress() {
        progressDialogFragment?.dismissAllowingStateLoss()
        supportFragmentManager.executePendingTransactions()
        progressDialogFragment = null
    }
}
