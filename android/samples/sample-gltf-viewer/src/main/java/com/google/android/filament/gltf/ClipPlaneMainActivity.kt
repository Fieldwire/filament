package com.google.android.filament.gltf

import android.annotation.SuppressLint
import android.app.Activity
import android.os.Bundle
import android.view.Choreographer
import android.view.SurfaceView
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.Switch
import com.google.android.filament.Colors
import com.google.android.filament.Material
import com.google.android.filament.utils.ClipModelViewer
import com.google.android.filament.utils.Float3
import com.google.android.filament.utils.KTX1Loader
import com.google.android.filament.utils.Mesher
import com.google.android.filament.utils.PlaneModelViewer
import com.google.android.filament.utils.Renderabler
import com.google.android.filament.utils.Transformer
import com.google.android.filament.utils.Utils
import com.google.android.filament.utils.translation
import com.google.android.filament.utils.transpose
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.nio.ByteBuffer

private val glbFile = "21_KB"
class ClipPlaneMainActivity : Activity() {

    private lateinit var surfaceView: SurfaceView
    private lateinit var choreographer: Choreographer
    private lateinit var modelViewer: ClipModelViewer

    private lateinit var material: Material

    private val engine by lazy { modelViewer.engine }

    companion object {
        init {
            Utils.init()
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.layout_gltf)

        actionBar?.title = intent.getStringExtra("model")

        surfaceView = findViewById(R.id.surfaceView)
        choreographer = Choreographer.getInstance()
        modelViewer = ClipModelViewer(surfaceView)
        surfaceView.setOnTouchListener(modelViewer)

        val textAngle = findViewById<EditText>(R.id.textAngle)
        val textHeight = findViewById<EditText>(R.id.textHeight)
        findViewById<Button>(R.id.submit).setOnClickListener {
            modelViewer.setAngleAndHeight(
                angle = textAngle.text.toString().toFloat(),
                height = textHeight.text.toString().toFloat()
            )
        }
        val switch = findViewById<Switch>(R.id.switchClip)
        // modelViewer.fullClip = switch.isChecked

        switch.setOnCheckedChangeListener { buttonView, isChecked ->
            //modelViewer.fullClip = isChecked
        }

        val checkClip = findViewById<CheckBox>(R.id.checkClip)
        modelViewer.enableClipping = checkClip.isChecked

        checkClip.setOnCheckedChangeListener { buttonView, isChecked ->
            modelViewer.enableClipping = isChecked
        }

        loadModel()
    }

    private fun createIndirectLight() {
        val engine = modelViewer.engine
        val scene = modelViewer.scene
        val ibl = "default_env"
        readCompressedAsset("envs/$ibl/${ibl}_ibl.ktx").let {
            scene.indirectLight = KTX1Loader.createIndirectLight(engine, it)
            scene.indirectLight!!.intensity = 11_000f
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
//            material.defaultInstance.setParameter("baseColor", Colors.RgbType.SRGB, 0.5f, 0.5f, 1f)
        }
        modelViewer.material = material
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
    }

    private val frameCallback = object : Choreographer.FrameCallback {
        override fun doFrame(currentTime: Long) {
            choreographer.postFrameCallback(this)
            modelViewer.render(currentTime)
        }
    }

    private fun loadModel() {
        loadMaterial()
        createIndirectLight()
        loadGlb(glbFile)
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
