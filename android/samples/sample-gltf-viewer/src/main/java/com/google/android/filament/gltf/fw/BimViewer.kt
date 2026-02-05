package com.google.android.filament.gltf.fw

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Color
import android.os.Looper
import android.view.GestureDetector
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.core.content.ContextCompat
import com.google.android.filament.Engine
import com.google.android.filament.Skybox
import com.google.android.filament.View
import com.google.android.filament.gltf.logg
import com.google.android.filament.gltfio.FilamentAsset
import com.google.android.filament.utils.Float3
import com.google.android.filament.utils.KTX1Loader
import com.google.android.filament.utils.Manipulator
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.nio.ByteBuffer

@SuppressLint("ClickableViewAccessibility")
class BimViewer(
    private val surfaceView: SurfaceView,
    private val callback: Callback
) {

    private val context: Context get() = surfaceView.context

    private val cameraManipulator: Manipulator get() = Manipulator.Builder()
        .viewport(surfaceView.width, surfaceView.height)
        .flightPanSpeed(0.0004f, 0.0004f)
        .flightStartPosition(0f, 0f, 1f)
        .build(Manipulator.Mode.FREE_FLIGHT_2)

    init {
        setupTouchListener()
    }

    private val multiplier = 2L

    private val modelViewer: FWModelViewer = FWModelViewer(
        surfaceView = surfaceView,
        manipulator = cameraManipulator,
        engine = Engine.Builder().backend(Engine.Backend.OPENGL).config(
            Engine.Config().apply {
                // Remember to change the configs in release/config file
                commandBufferSizeMB = 2 * multiplier * 3
                perRenderPassArenaSizeMB = 2 * multiplier + 2
                minCommandBufferSizeMB = 2 * multiplier
                perFrameCommandsSizeMB = 2 * multiplier
                driverHandleArenaSizeMB = 8 * multiplier
            }
        ).build(),
        executor = {
            when (Looper.myLooper() == Looper.getMainLooper()) {
                true -> callback.execute(it)
                // If the current thread is not main then it must be
                // the thread from executor so execute it immediately
                else -> it()
            }
        },
        generateNormals = true
    )

    // To highlight the renderable entity on selection
    private val entityHighlighter: ObjectHighlighter
    // To handle isolate/hide/showAll menu actions
    private val visibilityHandler: VisibilityHandler
    // Menus: Properties/Isolate/Hide
    // To highlight a single triangle overlay
    private val triangleHighlighter: TriangleHighlighter
    // Triangle hiding utility (JNI wrapper to gltfio::TriangleHider)
    private var triangleHiderNative: Long = 0
    // Triangle mapping for grouped triangle highlighting
    private var triangleMapping: TriangleMapping? = null

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
        triangleHighlighter = TriangleHighlighter(entityHighlighter, modelViewer)
    }

    suspend fun loadModel(context: Context, fileName: String) {
        logg("loadingModel", fileName)
        val (modelBuffer, lightBuffer) = withContext(Dispatchers.IO) {
            Pair(
                readAssetAsByteBuffer(context, "models/$fileName.glb"),
                readAssetAsByteBuffer(context, IBL_FILE)
            )
        }
        logg("ModelBuffer", modelBuffer, "lightBuffer", lightBuffer)
        modelViewer.loadModelGlb(modelBuffer)
        setIndirectLight(lightBuffer)

        // Initialize TriangleHider using JNI
        modelViewer.asset?.let { asset ->
            triangleHiderNative = asset.createTriangleHider(modelViewer.engine)
            logg("TriangleHider initialized (native)")
        }

        // Try to load triangle mapping if it exists
        try {
            triangleMapping = TriangleMapping(context, "json/mapping_$fileName.json")
            logg("TriangleMapping loaded successfully")
        } catch (e: Exception) {
            logg("No triangle mapping file found or error loading it: ${e.message}")
            triangleMapping = null
        }

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
        // Cleanup triangle hider
        if (triangleHiderNative != 0L) {
            modelViewer.asset?.destroyTriangleHider(triangleHiderNative)
            triangleHiderNative = 0
        }

        modelViewer.destroyViewer()
    }

    private fun transformToInitialPosition() {
        modelViewer.transformToUnitCube(Float3(0f, 0f, ZOOM))
        modelViewer.setCameraManipulator(cameraManipulator)
    }

    private fun setIndirectLight(lightBuffer: ByteBuffer) {
        modelViewer.scene.indirectLight = KTX1Loader.createIndirectLight(modelViewer.engine, lightBuffer).indirectLight.apply {
            // Adjust this value to increase the brightness of the model
            this?.intensity = 30_000f
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
                enabled = false
                quality = View.QualityLevel.MEDIUM
            }

            // MSAA is needed with dynamic resolution MEDIUM
            multiSampleAntiAliasingOptions = multiSampleAntiAliasingOptions.apply {
                enabled = false
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

//            callback.execute {
                // If there was a previous selection, remove it & don't start another selection
//                if (entityHighlighter.unhighlight()) {
//                    triangleHighlighter.clear()
//                    return@execute
//                }

                val x = event.x.toInt()
                val y = event.y.toInt()
                val flippedY = surfaceView.height - 1 - event.y.toInt()
                logg("SurfaceView width: ${surfaceView.width} height: ${surfaceView.height} event.x: ${event.x} event.y: ${event.y}")

//                kotlinTrianglePicking(x, flippedY)

            try {

                cppTrianglePicking(x, flippedY)
            } catch (e: Exception) {
                logg("Error in cppTrianglePicking: ${e.message}")
            }

                // Fallback: highlight entire renderable entity
                // Filament's pick expects y flipped to viewport origin at bottom-left
                /*modelViewer.view.pick(x, flippedY, surfaceView.handler) { result ->
                    if (entityHighlighter.highlight(result.renderable)) {
                        // showContextMenu(x, y)
                    }
                }*/
//            }
            // Tap event is always consumed
            return true
        }
    }

    private fun cppTrianglePicking(x: Int, y: Int) {
        val asset = modelViewer.asset
        if (asset == null) return

        // Use existing screen-space pick to get entity and triangle index

        val hit = hitTinybvh(asset, x, y)

        hide(hit)
    }

    private fun hitTinybvh(
        asset: FilamentAsset,
        x: Int,
        y: Int
    ): FilamentAsset.Hit? {
        val hit = if (lastHiddenStartIdx != -1 && lastHiddenEndIdx != -1) {
            asset.pickScreenSkippingRangeTinybvh(modelViewer.view, x, y, lastHiddenStartIdx, lastHiddenEndIdx)
        } else {
            asset.pickScreenTinybvh(modelViewer.view, x, y)
        }

//        logg("Pick result: ${hit?.entity} triangle: ${hit?.triangle} ")
        return hit
    }

    private var lastHiddenStartIdx = -1
    private var lastHiddenEndIdx = -1

    private fun hide(hit: FilamentAsset.Hit?) {
        val asset = modelViewer.asset ?: return

        if (hit == null || hit.entity == 0 || hit.triangle < 0) {
            return
        }

        // Get entity and triangle from hit parameter
        val entity = hit.entity
        val triangle = hit.triangle

        // Get entity name for mapping lookup
        val entityName = asset.getName(entity) ?: entity.toString()
//        logg("Hiding triangles for entity: $entityName, triangle: $triangle")

        // Get the triangle range from mapping
        val mapping = triangleMapping
        if (mapping != null) {
            val range = mapping.getTriangleRange(entityName, triangle)
            if (range != null) {
                var (startIdx, endIdx) = range
//                logg("Triangle range from mapping: $startIdx to $endIdx")

                // If endIdx is Int.MAX_VALUE, set it to indices.size from FilamentAsset.getIndicesSize
                if (endIdx == Int.MAX_VALUE) {
                    val indicesSize = asset.getIndicesSize(entity)
                    if (indicesSize > 0) {
                        endIdx = indicesSize - 1
//                        logg("Adjusted endIdx from Int.MAX_VALUE to actual size: $endIdx")
                    }
                }

                // Hide all the triangles in between startIdx and endIdx
             /*   var hiddenCount = 0
                for (triIdx in startIdx..endIdx step 3) {
                    val success = asset.hideTriangleWithoutCache(entity, triIdx / 3)
                    if (success) {
                        hiddenCount++
                    }
                }*/

                lastHiddenStartIdx = startIdx
                lastHiddenEndIdx = endIdx
                asset.hideVerticesInRangeWithoutCache(entity, startIdx, endIdx)

//                logg("Successfully hid triangles in range $startIdx to $endIdx")
            } else {
                // No mapping found, hide single triangle
//                logg("No triangle mapping found, hiding single triangle")
                val success = asset.hideTriangleWithoutCache(entity, triangle)
//                if (success) {
//                    logg("Successfully hid triangle $triangle")
//                } else {
//                    logg("Failed to hide triangle $triangle")
//                }
            }
        } else {
            // No triangle mapping available, hide single triangle
//            logg("No triangle mapping available, hiding single triangle")
            val success = asset.hideTriangleWithoutCache(entity, triangle)
//            if (success) {
//                logg("Successfully hid triangle $triangle")
//            } else {
//                logg("Failed to hide triangle $triangle")
//            }
        }
    }

    private fun highlight(hit: FilamentAsset.Hit?) {
        val asset = modelViewer.asset ?: return
        if (hit != null && hit.entity != 0 && hit.triangle >= 0) {
            // Get entity name for mapping lookup
            val entityName = asset.getName(hit.entity) ?: hit.entity.toString()
            logg("Picked entity: $entityName, triangle: ${hit.triangle}")

            // Check if we have a triangle mapping
            val mapping = triangleMapping
            if (mapping != null) {
                // Get the triangle range from mapping
                logg("getting triangle range from mapping")
                val range = mapping.getTriangleRange(entityName, hit.triangle)
                logg("Triangle range from mapping: $range")
                if (range != null) {
                    var (startIdx, endIdx) = range
                    logg("Triangle range: $startIdx to $endIdx")

                    // If endIdx is Int.MAX_VALUE, get the actual indices size from the asset
                    if (endIdx == Int.MAX_VALUE) {
                        val indicesSize = asset.getIndicesSize(hit.entity)
                        if (indicesSize > 0) {
                            endIdx = indicesSize - 1
                            logg("Adjusted endIdx from Int.MAX_VALUE to actual size: $endIdx")
                        }
                    }

                    // Collect all triangles in the range
                    val trianglesToHighlight = mutableListOf<Triple<Float3, Float3, Float3>>()
                    for (triIdx in startIdx..endIdx step 3) {
                        val tri = asset.getTriangleModelSpaceForHit(hit.entity, triIdx)
                        if (tri != null && tri.size == 9) {
                            val v0 = Float3(tri[0], tri[1], tri[2])
                            val v1 = Float3(tri[3], tri[4], tri[5])
                            val v2 = Float3(tri[6], tri[7], tri[8])
                            trianglesToHighlight.add(Triple(v0, v1, v2))
                        }
                    }

                    if (trianglesToHighlight.isNotEmpty()) {
                        logg("Highlighting ${trianglesToHighlight.size} triangles")
                        triangleHighlighter.showTriangles(hit.entity, trianglesToHighlight)

                        // Alternative: Use TriangleHider to actually hide triangles (modifies geometry)
                        // Note: Only one entity can be hidden at a time (auto-restores previous)
                        // triangleHider?.hideTriangles(hit.entity, startIdx, endIdx)
                        // To restore: triangleHider?.restore()

                        return
                    }
                }
            }

            // Fallback: highlight single triangle if no mapping or mapping failed
            val tri = asset.getTriangleModelSpaceForHit(hit.entity, hit.triangle)
            if (tri != null && tri.size == 9) {
                val v0 = Float3(tri[0], tri[1], tri[2])
                val v1 = Float3(tri[3], tri[4], tri[5])
                val v2 = Float3(tri[6], tri[7], tri[8])
                triangleHighlighter.showTriangle(hit.entity, v0, v1, v2)
                return
            }
        }
    }

    companion object {
        private const val IBL_FILE = "envs/default_env/default_env_ibl.ktx"
        // Some models take a couple of frames to appear on screen
        // Wait for 60 frames (~1 sec on most devices) before informing any callbacks
        // TODO: This is not foolproof - some models take longer than 60frames to appear on screen
        // Find a solution that works for models of all sizes & complexities. It may not be straightforward
        // as GPU renders pixels asynchronously on a different (render) thread.
        private const val FIRST_FRAME_THRESHOLD = 60

        const val ZOOM = -1f
    }

    interface Callback {
        fun onModelRendered()
        fun onClickProperties(entityId: String?)
        fun execute(block: () -> Unit)
    }
}
