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
import com.google.android.filament.utils.Renderabler
import com.google.android.filament.utils.TextureType
import com.google.android.filament.utils.Transformer
import com.google.android.filament.utils.Utils
import com.google.android.filament.utils.loadTexture
import java.nio.ByteBuffer
import java.nio.channels.Channels

private val glbFile = "560_MB"

class ShowHideMainActivity : Activity() {

    private lateinit var surfaceView: SurfaceView
    private lateinit var choreographer: Choreographer
    private lateinit var modelViewer: ModelViewer

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
        modelViewer = ModelViewer(surfaceView)
        surfaceView.setOnTouchListener(modelViewer)
        loadGlb(glbFile)
        loadMaterial()
        createIndirectLight()
        setupEntityTapListener()
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

    private fun readCompressedAsset(assetName: String): ByteBuffer {
        val input = assets.open(assetName)
        val bytes = ByteArray(input.available())
        input.read(bytes)
        return ByteBuffer.wrap(bytes)
    }

    private fun loadGlb(name: String) {
        val buffer = readAsset("models/${name}.glb")
        modelViewer.loadModelGlb(buffer)
        modelViewer.transformToUnitCube()
    }

    private fun readAsset(assetName: String): ByteBuffer {
        val input = assets.open(assetName)
        val bytes = ByteArray(input.available())
        input.read(bytes)
        return ByteBuffer.wrap(bytes)
    }

    private val hiddenEntities = hashSetOf<Int>()

    private fun setupEntityTapListener() {
        /*modelViewer.gestureDetector.onTapRenderable = { clickedRenderable, x, y ->
            if (modelViewer.scene.hasEntity(clickedRenderable)) {
                val visibility = if (hiddenEntities.contains(clickedRenderable)) {
                    hiddenEntities.remove(clickedRenderable)
                    0xff
                } else {
                    hiddenEntities.add(clickedRenderable)
                    0x00
                }
                engine.renderableManager.setLayerMask(
                    engine.renderableManager.getInstance(clickedRenderable),
                    0xff,
                     visibility
                )
            } else {
                modelViewer.scene.entities.forEach {
                    engine.renderableManager.setLayerMask(
                        engine.renderableManager.getInstance(it),
                        0xff,
                        0xff
                    )
                }
            }
        }*/
    }

    private fun readUncompressedAsset(assetName: String): ByteBuffer {
        assets.openFd(assetName).use { fd ->
            val input = fd.createInputStream()
            val dst = ByteBuffer.allocate(fd.length.toInt())

            val src = Channels.newChannel(input)
            src.read(dst)
            src.close()

            return dst.apply { rewind() }
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
