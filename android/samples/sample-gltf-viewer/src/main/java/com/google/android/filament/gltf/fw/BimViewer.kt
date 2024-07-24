package com.google.android.filament.gltf.fw

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Color
import android.view.GestureDetector
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import com.google.android.filament.Engine
import com.google.android.filament.Skybox
import com.google.android.filament.View
import com.google.android.filament.utils.Float3
import com.google.android.filament.utils.KTX1Loader
import com.google.android.filament.utils.Manipulator
import com.google.android.filament.utils.ModelViewer
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import java.nio.ByteBuffer

@SuppressLint("ClickableViewAccessibility")
class BimViewer(
    private val surfaceView: SurfaceView,
    private val callback: Callback
) {

    private val context: Context get() = surfaceView.context

    private val cameraManipulator: Manipulator get() = Manipulator.Builder()
        .targetPosition(0f, 0f, -4f)
        .viewport(surfaceView.width, surfaceView.height)
        .flightPanSpeed(0.0004f, 0.0004f)
        .build(Manipulator.Mode.FREE_FLIGHT_2)

    init {
        setupTouchListener()
    }

    private val modelViewer: ModelViewer = ModelViewer(
        surfaceView = surfaceView,
        manipulator = cameraManipulator,
        engine = Engine.Builder().config(
            Engine.Config().apply {
                // Remember to change the configs in release/config file
                commandBufferSizeMB = 34 * 6
                perRenderPassArenaSizeMB = 35
                minCommandBufferSizeMB = 32
                perFrameCommandsSizeMB = 32
                driverHandleArenaSizeMB = 32
            }
        ).build()
    )

    // To highlight the renderable entity on selection
    private val entityHighlighter: ObjectHighlighter
    // To handle isolate/hide/showAll menu actions
    private val visibilityHandler: VisibilityHandler
    // Menus: Properties/Isolate/Hide

    private var frameCount = 0
    private var isModelRendered = false

    init {
        // Don't need the direct light which lights up a specific part of the model based on its position
        // on the scene
        modelViewer.scene.removeEntity(modelViewer.light)
        setBackgroundColor(context)
        disablePostProcessing()

        entityHighlighter = ObjectHighlighter(context.applicationContext, modelViewer)
        visibilityHandler = VisibilityHandler(modelViewer)
    }

    suspend fun loadModel(context: Context, fileName: String) {
        logg("loadingModel", fileName)
        val (modelBuffer, lightBuffer) = withContext(Dispatchers.IO) {
            Pair(
                readAssetAsByteBuffer(context, "models/$fileName"),
                readAssetAsByteBuffer(context, IBL_FILE)
            )
        }
        logg("ModelBuffer", modelBuffer, "lightBuffer", lightBuffer)
        modelViewer.loadModelGlb(modelBuffer)
        setIndirectLight(lightBuffer)
        // To fit the model in the screen
        transformToInitialPosition()
    }

    fun doFrame(frameTimeNanos: Long) {
        modelViewer.render(frameTimeNanos)
        if (!isModelRendered && isFirstFrameRendered()) {
            isModelRendered = true
            callback.onModelRendered()
        }
    }

    fun nodeCount(): Int {
        return modelViewer.asset?.entities?.size ?: -1
    }

    fun resetModel() {
        transformToInitialPosition()
        entityHighlighter.unhighlight()
    }

    fun destroyViewer() {
        modelViewer.destroy()
    }

    private fun transformToInitialPosition() {
        modelViewer.transformToUnitCube(Float3(0f, 0f, -4f))
        modelViewer.setCameraManipulator(cameraManipulator)
    }

    private fun setIndirectLight(lightBuffer: ByteBuffer) {
        modelViewer.scene.indirectLight = KTX1Loader.createIndirectLight(modelViewer.engine, lightBuffer).apply {
            // Adjust this value to increase the brightness of the model
            intensity = 20_000f
        }
    }

    private fun setBackgroundColor(context: Context) {
        val color = ContextCompat.getColor(context, android.R.color.white)
        val background = floatArrayOf(
            Color.red(color) / 255f,
            Color.green(color) / 255f,
            Color.blue(color) / 255f,
            Color.alpha(color) / 255f
        )
        // Skybox fills the untouched pixels (pixels not taken up by model) with this color
        modelViewer.scene.skybox = Skybox.Builder().color(background).build(modelViewer.engine)

        // On Lenovo Tab without this option, draw calls from previous frame is not cleared
        // resulting in multiple appearance of the same model when moved
        modelViewer.renderer.clearOptions = modelViewer.renderer.clearOptions.apply {
            clear = true
        }
    }

    private fun disablePostProcessing() {
        modelViewer.view.run {
            isPostProcessingEnabled = false
            setShadowingEnabled(false)
            setScreenSpaceRefractionEnabled(false)

            // on mobile, better use lower quality color buffer
            renderQuality = renderQuality.apply {
                hdrColorBuffer = View.QualityLevel.MEDIUM
            }

            // Below properties are copied from sample-gltf-viewer android sample
            // https://github.com/Fieldwire/filament/blob/main/android/samples/sample-gltf-viewer/src/main/java/com/google/android/filament/gltf/MainActivity.kt
            // dynamic resolution often helps a lot
            dynamicResolutionOptions = dynamicResolutionOptions.apply {
                enabled = true
                quality = View.QualityLevel.MEDIUM
            }

            // MSAA is needed with dynamic resolution MEDIUM
            multiSampleAntiAliasingOptions = multiSampleAntiAliasingOptions.apply {
                enabled = true
            }

            // FXAA is pretty cheap and helps a lot
            antiAliasing = View.AntiAliasing.FXAA
        }
    }

    private fun isFirstFrameRendered(): Boolean {
        if (frameCount >= FIRST_FRAME_THRESHOLD) {
            return true
        }
        frameCount++
        return false
    }

    private fun setupTouchListener() {
        surfaceView.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(holder: SurfaceHolder) {
                // surfaceView.handler will be non-null only after the surface is created
                val singleTapDetector = GestureDetector(context, SingleTapListener())
                surfaceView.setOnTouchListener { _, event ->
                    modelViewer.onTouchEvent(event)
                    singleTapDetector.onTouchEvent(event)
                    true
                }
            }
            override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {}
            override fun surfaceDestroyed(holder: SurfaceHolder) {}
        })
    }

    private inner class SingleTapListener : GestureDetector.SimpleOnGestureListener() {
        override fun onSingleTapUp(event: MotionEvent): Boolean {
            // If there was a previous selection, remove it & don't start another selection
            if (entityHighlighter.unhighlight()) {
                // Event consumed
                return true
            }

            val x = event.x.toInt()
            val y = event.y.toInt()
            modelViewer.view.pick(x, surfaceView.height - y, surfaceView.handler) { result ->
                if (entityHighlighter.highlight(result.renderable)) {
                    showContextMenu(x, y)
                }
            }
            // Tap event is always consumed
            return true
        }
    }

    private fun showContextMenu(x: Int, y: Int) {

    }

    companion object {
        private const val IBL_FILE = "envs/default_env/default_env_ibl.ktx"
        // Some models take a couple of frames to appear on screen
        // Wait for 60 frames (~1 sec on most devices) before informing any callbacks
        // TODO: This is not foolproof - some models take longer than 60frames to appear on screen
        // Find a solution that works for models of all sizes & complexities. It may not be straightforward
        // as GPU renders pixels asynchronously on a different (render) thread.
        private const val FIRST_FRAME_THRESHOLD = 60
    }

    interface Callback {
        fun onModelRendered()
        fun onClickProperties(entityId: String?)
    }
}
