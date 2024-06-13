package com.google.android.filament.gltf

import android.annotation.SuppressLint
import android.app.Activity
import android.os.Bundle
import android.view.Choreographer
import android.view.SurfaceView
import com.google.android.filament.Box
import com.google.android.filament.Colors
import com.google.android.filament.Material
import com.google.android.filament.TextureSampler
import com.google.android.filament.utils.Float3
import com.google.android.filament.utils.Float4
import com.google.android.filament.utils.KTX1Loader
import com.google.android.filament.utils.Mesher
import com.google.android.filament.utils.ModelViewer
import com.google.android.filament.utils.PlaneModelViewer
import com.google.android.filament.utils.Renderabler
import com.google.android.filament.utils.TextureType
import com.google.android.filament.utils.Transformer
import com.google.android.filament.utils.Utils
import com.google.android.filament.utils.loadTexture
import com.google.android.filament.utils.max
import com.google.android.filament.utils.rotation
import com.google.android.filament.utils.scale
import com.google.android.filament.utils.translation
import com.google.android.filament.utils.transpose
import java.nio.ByteBuffer
import java.nio.channels.Channels

private val glbFile = "70_MB"

class PlaneMainActivity : Activity() {

    private lateinit var surfaceView: SurfaceView
    private lateinit var choreographer: Choreographer
    private lateinit var modelViewer: PlaneModelViewer

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
        modelViewer = PlaneModelViewer(surfaceView)
        surfaceView.setOnTouchListener(modelViewer)
        loadGlb(glbFile)
        loadMaterial()
        createIndirectLight()
        createRhombus()
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
        readCompressedAsset("materials/baked_color.filamat").let {
            material = Material.Builder().payload(it, it.remaining()).build(engine)
            /*val pinTexture = loadTexture(engine, resources, R.drawable.pin, TextureType.DATA)
            // A texture sampler does not need to be kept around or destroyed
            val sampler = TextureSampler()
            sampler.anisotropy = 8.0f*/
            material.defaultInstance.setParameter("baseColor", Colors.RgbType.SRGB, 0.71f, 0.0f, 0.0f)
        }
    }

    private fun readCompressedAsset(assetName: String): ByteBuffer {
        val input = assets.open(assetName)
        val bytes = ByteArray(input.available())
        input.read(bytes)
        return ByteBuffer.wrap(bytes)
    }

    private fun loadGlb(name: String) {
        val buffer = readCompressedAsset("models/${name}.glb")
        modelViewer.loadModelGlb(buffer)
        modelViewer.transformToUnitCube()
    }

    private fun createRhombus() {
        val buffers = mesher.createRhombus()
        val renderable = renderabler.create(buffers, material.defaultInstance, priority = 7)
        val tm = engine.transformManager
        val transform = translation(Float3(0f, 0f, -3f))
        tm.setTransform(tm.getInstance(renderable), transpose(transform).toFloatArray())

        modelViewer.scene.addEntity(renderable)
        modelViewer.planEntity = renderable
        logg("Rhombus $renderable")
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
