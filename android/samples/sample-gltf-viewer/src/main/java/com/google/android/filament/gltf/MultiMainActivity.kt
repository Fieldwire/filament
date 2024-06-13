package com.google.android.filament.gltf

import android.annotation.SuppressLint
import android.app.Activity
import android.os.Bundle
import android.view.Choreographer
import android.view.SurfaceView
import com.google.android.filament.Box
import com.google.android.filament.Material
import com.google.android.filament.TextureSampler
import com.google.android.filament.utils.KTX1Loader
import com.google.android.filament.utils.Mesher
import com.google.android.filament.utils.ModelViewer
import com.google.android.filament.utils.MultipleModelViewer
import com.google.android.filament.utils.Renderabler
import com.google.android.filament.utils.TextureType
import com.google.android.filament.utils.Transformer
import com.google.android.filament.utils.Utils
import com.google.android.filament.utils.loadTexture
import java.nio.ByteBuffer
import java.nio.channels.Channels

private val glbFile1 = "small"
private val glbFile2 = "70_MB"

class MultiMainActivity : Activity() {

    private lateinit var surfaceView: SurfaceView
    private lateinit var choreographer: Choreographer
    private lateinit var modelViewer: MultipleModelViewer

    private lateinit var material: Material

    private val engine by lazy { modelViewer.engine }
    private val mesher by lazy { Mesher(engine) }
    private val renderabler by lazy { Renderabler(engine) }
    private val transformer by lazy { Transformer(engine) }

    companion object {
        init {
            Utils.init()
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        surfaceView = SurfaceView(this).apply { setContentView(this) }
        choreographer = Choreographer.getInstance()
        modelViewer = MultipleModelViewer(surfaceView)
        surfaceView.setOnTouchListener(modelViewer)
        loadGlb(glbFile1)
        // loadMaterial()
        createIndirectLight()
        // setupEntityTapListener()
    }

    private fun createIndirectLight() {
        val engine = modelViewer.engine
        val scene = modelViewer.scene
        val ibl = "default_env"
        readCompressedAsset("envs/$ibl/${ibl}_ibl.ktx").let {
            scene.indirectLight = KTX1Loader.createIndirectLight(engine, it)
            scene.indirectLight!!.intensity = 35_000f
        }
        readCompressedAsset("envs/$ibl/${ibl}_skybox.ktx").let {
            scene.skybox = KTX1Loader.createSkybox(engine, it)
        }
    }

    private fun loadMaterial() {
        readCompressedAsset("materials/textured_pbr.filamat").let {
            material = Material.Builder().payload(it, it.remaining()).build(engine)

            val pinTexture = loadTexture(engine, resources, R.drawable.pin, TextureType.DATA)

            // A texture sampler does not need to be kept around or destroyed
            val sampler = TextureSampler()
            sampler.anisotropy = 8.0f

            material.defaultInstance.setParameter("baseColor", pinTexture, sampler)
        }
    }

    private fun loadGlb(vararg files: String) {
        files.map { assetName ->
            readCompressedAsset("models/${assetName}.glb")
        }.also {
            modelViewer.loadModelGlbs(it)
            modelViewer.transformToUnitCube()
        }
    }

    private fun readCompressedAsset(assetName: String): ByteBuffer {
        val input = assets.open(assetName)
        val bytes = ByteArray(input.available())
        input.read(bytes)
        return ByteBuffer.wrap(bytes)
    }

    private fun setupEntityTapListener() {
        modelViewer.gestureDetector.onTapRenderable = { clickedRenderable, x, y ->

            val screenWidth = modelViewer.view.viewport.width
            val screenHeight = modelViewer.view.viewport.height

            val normalizedX = (2.0f * x) / screenWidth - 1.0f
            val normalizedY = 1.0f - (2.0f * y) / screenHeight

            val buffers = mesher.createSquareMeshWithTexture(normalizedX, normalizedY, 0f)
            val boundingBox = Box(1 / x, 1 / y, 0.0f, 1.0f, 1.0f, 0.1f)
            val renderable = renderabler.create(buffers, material.defaultInstance)
            /*
            // FW: No change with / without parent
            if (clickedRenderable > 0) {
                engine.transformManager.setParent(renderable, clickedRenderable)
            }
            */
            transformer.transform(boundingBox, renderable)

            // Add the entity to the scene to render it
            logg("Adding renderable $renderable")

            modelViewer.scene.addEntity(renderable)
        }
    }

    private val frameCallback = object : Choreographer.FrameCallback {
        override fun doFrame(currentTime: Long) {
            choreographer.postFrameCallback(this)
            modelViewer.render(currentTime)
        }
    }

    override fun onResume() {
        super.onResume()
        choreographer.postFrameCallback(frameCallback)
    }

    override fun onPause() {
        super.onPause()
        choreographer.removeFrameCallback(frameCallback)
    }

    override fun onDestroy() {
        super.onDestroy()
        choreographer.removeFrameCallback(frameCallback)
    }
}
