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
import android.app.Activity
import android.content.res.Configuration
import android.os.Bundle
import android.util.Log
import android.view.Choreographer
import android.view.GestureDetector
import android.view.MotionEvent
import android.view.SurfaceView
import android.view.WindowManager
import android.widget.ProgressBar
import android.widget.TextView
import androidx.activity.OnBackPressedCallback
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.isVisible
import com.google.android.filament.Engine
import com.google.android.filament.Skybox
import com.google.android.filament.gltf.R
import com.google.android.filament.utils.KTX1Loader
import com.google.android.filament.utils.ModelViewer
import com.google.android.filament.utils.Utils
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import java.nio.ByteBuffer
import java.util.concurrent.Executors

fun logg(vararg msg: Any) {
    Log.d("fml", msg.joinToString("\t"))
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
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        logg("config changed")
    }

    private fun loadModel() {
//        MainScope().launch {
            try {
                showProgress()
                bimExecutor.loadModel(this@FWMainActivity, intent.getStringExtra("model")!!) { count ->
                    logg("nodeCount", count)
                    frameChoreographer.start()
                }
            } catch (e: Exception) {
                logg("Exception in loadModel", e.message.toString())
            }
//        }
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

    private fun showProgress() {
        progressDialogFragment = ProgressDialogFragment()
        progressDialogFragment?.callback = this

        progressDialogFragment?.show(supportFragmentManager, "")
        supportFragmentManager.executePendingTransactions()
    }

    override fun onProgressCancelled() {
        logg("destroyingViewer")
//        bimExecutor.destroyViewerSync()
        bimExecutor.destroyViewer {
            setResult(Activity.RESULT_OK)
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
