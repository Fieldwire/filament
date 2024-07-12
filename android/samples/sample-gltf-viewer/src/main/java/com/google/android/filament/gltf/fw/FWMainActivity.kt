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
import android.os.Bundle
import android.util.Log
import android.view.Choreographer
import android.view.GestureDetector
import android.view.MotionEvent
import android.view.SurfaceView
import android.view.WindowManager
import android.widget.ProgressBar
import android.widget.TextView
import com.google.android.filament.Engine
import com.google.android.filament.Skybox
import com.google.android.filament.gltf.R
import com.google.android.filament.utils.KTX1Loader
import com.google.android.filament.utils.ModelViewer
import com.google.android.filament.utils.Utils
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import java.nio.ByteBuffer
import java.util.concurrent.Executors

fun logg(vararg msg: Any) {
    Log.d("fml", msg.joinToString("\t"))
}

class FWMainActivity : Activity() {

    private val executor = Executors.newSingleThreadExecutor().asCoroutineDispatcher()

    companion object {
        // Load the library for the utility layer, which in turn loads gltfio and the Filament core.
        init { Utils.init() }
        private const val TAG = "gltf-viewer"
    }

    private lateinit var surfaceView: SurfaceView
    private lateinit var choreographer: Choreographer
    private var progressDialogFragment: ProgressDialogFragment? = null

    private val frameScheduler = FrameCallback()
    private lateinit var modelViewer: ModelViewer
    private lateinit var titlebarHint: TextView

    private val singleTapListener = SingleTapListener()
    private lateinit var singleTapDetector: GestureDetector

    private lateinit var objectHighlighter: ObjectHighlighter
    private lateinit var visibilityHandler: VisibilityHandler

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.simple_layout)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        titlebarHint = findViewById(R.id.user_hint)
        surfaceView = findViewById(R.id.main_sv)

        choreographer = Choreographer.getInstance()

        singleTapDetector = GestureDetector(applicationContext, singleTapListener, surfaceView.handler)

        modelViewer = ModelViewer(surfaceView, Engine.Builder().config(
            Engine.Config().apply {
                commandBufferSizeMB = 34 * 6
                perRenderPassArenaSizeMB = 35
                minCommandBufferSizeMB = 32
                perFrameCommandsSizeMB = 32
                driverHandleArenaSizeMB = 32
            }
        )
            .build())

        surfaceView.setOnTouchListener { _, event ->
            modelViewer.onTouchEvent(event)
            singleTapDetector.onTouchEvent(event)
            true
        }

        objectHighlighter = ObjectHighlighter(this@FWMainActivity, modelViewer)
        visibilityHandler = VisibilityHandler(modelViewer)

        showProgress()

        createDefaultRenderables()
        createIndirectLight()
        setBackgroundColor()
        disablePostProcessing()
    }

    private fun setBackgroundColor() {
        val background = floatArrayOf(1f, 1f, 1f, 1f)
        // Skybox fills the untouched pixels (pixels not taken up by model) with this color
        modelViewer.scene.skybox = Skybox.Builder().color(background).build(modelViewer.engine)
        modelViewer.renderer.clearOptions = modelViewer.renderer.clearOptions.apply {
            clear = true
        }
    }

    private fun disablePostProcessing() {
        modelViewer.view.run {
            isPostProcessingEnabled = false
            setShadowingEnabled(false)
            setScreenSpaceRefractionEnabled(false)
        }
    }

    private fun createIndirectLight() {
        val engine = modelViewer.engine
        val scene = modelViewer.scene
        val ibl = "default_env"
        readAssetAsByteBuffer(this,"envs/$ibl/${ibl}_ibl.ktx").let {
            scene.indirectLight = KTX1Loader.createIndirectLight(engine, it)
            scene.indirectLight!!.intensity = 30_000.0f
        }
    }

    private fun showProgress() {
        val dialogFragment = ProgressDialogFragment()
        dialogFragment.show(fragmentManager, "")
        fragmentManager.executePendingTransactions()
        progressDialogFragment = dialogFragment
    }

    private fun hideProgress() {
        progressDialogFragment?.dismiss()
        progressDialogFragment = null
    }

    private fun createDefaultRenderables() {
        val buffer = assets.open("models/100_MB.glb").use { input ->
            val bytes = ByteArray(input.available())
            input.read(bytes)
            ByteBuffer.wrap(bytes)
        }

//        modelViewer.loadModelGltfAsync(buffer) { uri -> readCompressedAsset( "models/$uri") }

        modelViewer.loadModelGlb(buffer)
        updateRootTransform()
        modelViewer.asset?.run {
            logg("Renderables:${renderableEntities.size} Light:${lightEntities.size} Camera:${cameraEntities.size}")
        }

        hideProgress()
    }

    override fun onResume() {
        super.onResume()
        choreographer.postFrameCallback(frameScheduler)
    }

    override fun onPause() {
        super.onPause()
        choreographer.removeFrameCallback(frameScheduler)
    }

    private fun updateRootTransform() {
        modelViewer.transformToUnitCube()
    }

    inner class FrameCallback : Choreographer.FrameCallback {
        override fun doFrame(frameTimeNanos: Long) {
            choreographer.postFrameCallback(this)
            modelViewer.render(frameTimeNanos)
        }
    }

    // Just for testing purposes
    inner class SingleTapListener : GestureDetector.SimpleOnGestureListener() {
        override fun onSingleTapUp(event: MotionEvent): Boolean {
            logg("tap event calling pick")
            // If there was a previous selection, dismiss it & don't start another selection
            if (objectHighlighter.unhighlight()) {
                //showAllEntities()
                // Event consumed
                return true
            }

            val x = event.x.toInt()
            val y = event.y.toInt()

            modelViewer.view.pick(x, surfaceView.height - y, surfaceView.handler) { result ->
                val renderable = result.renderable
                logg( "picked ${result.renderable} fragCoord:${result.fragCoords.joinToString()} depth:${result.depth} name:${modelViewer.asset!!.getName(renderable)}")
                if (objectHighlighter.highlight(renderable)) {
                    logg("highlighted ${result.renderable}")
//                    visibilityHandler.handle(Action.Isolate(renderable))
                }
            }

            // We always consume the single tap event
            return true
        }
    }

    private fun showAllEntities() {
        val rm = modelViewer.engine.renderableManager
        modelViewer.asset!!.renderableEntities.forEach { entity ->
            val instance = rm.getInstance(entity)
            rm.setLayerMask(instance, 1, 1)
        }
    }
}
