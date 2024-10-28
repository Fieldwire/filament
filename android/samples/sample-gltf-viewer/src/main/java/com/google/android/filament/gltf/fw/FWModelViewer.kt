package com.google.android.filament.gltf.fw


import android.view.MotionEvent
import android.view.Surface
import android.view.SurfaceView
import android.view.TextureView
import com.google.android.filament.Camera
import com.google.android.filament.Colors
import com.google.android.filament.Engine
import com.google.android.filament.Entity
import com.google.android.filament.EntityManager
import com.google.android.filament.Fence
import com.google.android.filament.IndirectLight
import com.google.android.filament.LightManager
import com.google.android.filament.Renderer
import com.google.android.filament.Scene
import com.google.android.filament.Skybox
import com.google.android.filament.SwapChain
import com.google.android.filament.View
import com.google.android.filament.Viewport
import com.google.android.filament.android.DisplayHelper
import com.google.android.filament.android.UiHelper
import com.google.android.filament.gltfio.Animator
import com.google.android.filament.gltfio.AssetLoader
import com.google.android.filament.gltfio.FilamentAsset
import com.google.android.filament.gltfio.MaterialProvider
import com.google.android.filament.gltfio.ResourceLoader
import com.google.android.filament.gltfio.UbershaderProvider
import com.google.android.filament.utils.Float3
import com.google.android.filament.utils.GestureDetector
import com.google.android.filament.utils.Manipulator
import com.google.android.filament.utils.max
import com.google.android.filament.utils.scale
import com.google.android.filament.utils.translation
import com.google.android.filament.utils.transpose
import kotlinx.coroutines.Job
import java.nio.Buffer

private const val kNearPlane = 0.0001f
private const val kFarPlane = 1000.0f // 1 km
private const val kAperture = 16f
private const val kShutterSpeed = 1f / 125f
private const val kSensitivity = 100f


/**
 * Helps render glTF models into a [SurfaceView] or [TextureView] with an orbit controller.
 *
 * `ModelViewer` owns a Filament engine, renderer, swapchain, view, and scene. It allows clients
 * to access these objects via read-only properties. The viewer can display only one glTF scene
 * at a time, which can be scaled and translated into the viewing frustum by calling
 * [transformToUnitCube]. All ECS entities can be accessed and modified via the [asset] property.
 *
 * For GLB files, clients can call [loadModelGlb] and pass in a [Buffer] with the contents of the
 * GLB file. For glTF files, clients can call [loadModelGltf] and pass in a [Buffer] with the JSON
 * contents, as well as a callback for loading external resources.
 *
 * `ModelViewer` reduces much of the boilerplate required for simple Filament applications, but
 * clients still have the responsibility of adding an [IndirectLight] and [Skybox] to the scene.
 * Additionally, clients should:
 *
 * 1. Pass the model viewer into [SurfaceView.setOnTouchListener] or call its [onTouchEvent]
 *    method from your touch handler.
 * 2. Call [render] and [Animator.applyAnimation] from a `Choreographer` frame callback.
 *
 * NOTE: if its associated SurfaceView or TextureView has become detached from its window, the
 * ModelViewer becomes invalid and must be recreated.
 *
 * See `sample-gltf-viewer` for a usage example.
 */
class FWModelViewer(
    val engine: Engine,
    val uiHelper: UiHelper,
    // All calls to filament must be executed on the same thread so execute
    // UI callbacks (such as from SurfaceView) on the executor so they are run in same thread
    // as other filament calls
    private val uiCallbackExecutor: Function1<() -> Unit, Unit>,
    generateNormals: Boolean,
) : android.view.View.OnTouchListener {
    var asset: FilamentAsset? = null
        private set

    var animator: Animator? = null
        private set

    @Suppress("unused")
    val progress
        get() = resourceLoader.asyncGetLoadProgress()

    var normalizeSkinningWeights = true

    var cameraFocalLength = 28f
        set(value) {
            field = value
            updateCameraProjection()
        }

    var cameraNear = kNearPlane
        set(value) {
            field = value
            updateCameraProjection()
        }

    var cameraFar = kFarPlane
        set(value) {
            field = value
            updateCameraProjection()
        }

    val scene: Scene
    val view: View
    val camera: Camera
    val renderer: Renderer
    @Entity val light: Int

    private lateinit var displayHelper: DisplayHelper
    private lateinit var cameraManipulator: Manipulator
    private lateinit var gestureDetector: GestureDetector
    private var surfaceView: SurfaceView? = null
    private var textureView: TextureView? = null

    private var fetchResourcesJob: Job? = null

    private var swapChain: SwapChain? = null
    private var assetLoader: AssetLoader
    private var materialProvider: MaterialProvider
    private var resourceLoader: ResourceLoader
    private val readyRenderables = IntArray(128) // add up to 128 entities at a time

    private val eyePos = DoubleArray(3)
    private val target = DoubleArray(3)
    private val upward = DoubleArray(3)

    init {
        renderer = engine.createRenderer()
        scene = engine.createScene()
        camera = engine.createCamera(engine.entityManager.create()).apply {
            setExposure(kAperture, kShutterSpeed, kSensitivity)
        }
        view = engine.createView()
        view.scene = scene
        view.camera = camera

        materialProvider = UbershaderProvider(engine)
        // Just pass in an empty string, see https://github.com/google/filament/issues/7905#issuecomment-2161779784,
        // to opt for the extended asset loader until https://github.com/google/filament/issues/7782 is done
        assetLoader = AssetLoader(engine, materialProvider, EntityManager.get(), if (generateNormals) "" else null, "UnknownObject")
        resourceLoader = ResourceLoader(engine, normalizeSkinningWeights)

        // Always add a direct light source since it is required for shadowing.
        // We highly recommend adding an indirect light as well.

        light = EntityManager.get().create()

        val (r, g, b) = Colors.cct(6_500.0f)
        LightManager.Builder(LightManager.Type.DIRECTIONAL)
            .color(r, g, b)
            .intensity(100_000.0f)
            .direction(0.0f, -1.0f, 0.0f)
            .castShadows(true)
            .build(engine, light)

        scene.addEntity(light)
    }

    constructor(
        surfaceView: SurfaceView,
        engine: Engine = Engine.create(),
        uiHelper: UiHelper = UiHelper(UiHelper.ContextErrorPolicy.DONT_CHECK),
        manipulator: Manipulator? = null,
        executor: Function1<() -> Unit, Unit>,
        generateNormals: Boolean = false
    ) : this(engine, uiHelper, executor, generateNormals) {
        cameraManipulator = manipulator ?: Manipulator.Builder()
            .targetPosition(kDefaultObjectPosition.x, kDefaultObjectPosition.y, kDefaultObjectPosition.z)
            .viewport(surfaceView.width, surfaceView.height)
            .build(Manipulator.Mode.ORBIT)

        this.surfaceView = surfaceView
        gestureDetector = GestureDetector(surfaceView, cameraManipulator)
        displayHelper = DisplayHelper(surfaceView.context)
        uiHelper.renderCallback = SurfaceCallback()
        uiHelper.attachTo(surfaceView)
    }

    /**
     * Loads a monolithic binary glTF and populates the Filament scene.
     */
    fun loadModelGlb(buffer: Buffer) {
        destroyModel()
        asset = assetLoader.createAsset(buffer)
        asset?.let { asset ->
            resourceLoader.asyncBeginLoad(asset)
            animator = asset.instance.animator
            asset.releaseSourceData()
        }
    }

    /**
     * Loads a JSON-style glTF file and populates the Filament scene.
     *
     * The given callback is triggered for each requested resource.
     */
    fun loadModelGltf(buffer: Buffer, callback: (String) -> Buffer?) {
        destroyModel()
        asset = assetLoader.createAsset(buffer)
        asset?.let { asset ->
            for (uri in asset.resourceUris) {
                val resourceBuffer = callback(uri)
                if (resourceBuffer == null) {
                    this.asset = null
                    return
                }
                resourceLoader.addResourceData(uri, resourceBuffer)
            }
            resourceLoader.asyncBeginLoad(asset)
            animator = asset.instance.animator
            asset.releaseSourceData()
        }
    }

    /**
     * Sets up a root transform on the current model to make it fit into a unit cube.
     *
     * @param centerPoint Coordinate of center point of unit cube, defaults to < 0, 0, -4 >
     */
    fun transformToUnitCube(centerPoint: Float3 = kDefaultObjectPosition) {
        asset?.let { asset ->
            val tm = engine.transformManager
            var center = asset.boundingBox.center.let { v -> Float3(v[0], v[1], v[2]) }
            val halfExtent = asset.boundingBox.halfExtent.let { v -> Float3(v[0], v[1], v[2]) }
            val maxExtent = 2.0f * max(halfExtent)
            val scaleFactor = 2.0f / maxExtent
            center -= centerPoint / scaleFactor
            val transform = scale(Float3(scaleFactor)) * translation(-center)
            tm.setTransform(tm.getInstance(asset.root), transpose(transform).toFloatArray())
        }
    }

    /**
     * Resets the camera to initial position. Calling this after [transformToUnitCube]
     * will transform the model to initial world position
     *
     * Resetting the manipulator is the easiest & safest option instead of resetting the
     * individual properties in different Manipulator (Orbit, FreeFlight, Map) implementations
     */
    fun setCameraManipulator(cameraManipulator: Manipulator) {
        surfaceView?.let {
            this.cameraManipulator = cameraManipulator
            gestureDetector = GestureDetector(it, cameraManipulator)
        }
    }

    /**
     * Frees all entities associated with the most recently-loaded model.
     */
    private fun destroyModel() {
        fetchResourcesJob?.cancel()
        resourceLoader.asyncCancelLoad()
        resourceLoader.evictResourceData()
        asset?.let { asset ->
            this.scene.removeEntities(asset.entities)
            assetLoader.destroyAsset(asset)
            this.asset = null
            this.animator = null
        }
    }

    /**
     * Renders the model and updates the Filament camera.
     *
     * @param frameTimeNanos time in nanoseconds when the frame started being rendered,
     *                       typically comes from {@link android.view.Choreographer.FrameCallback}
     */
    fun render(frameTimeNanos: Long) {
        val nativeSurface = swapChain ?: return

        // Allow the resource loader to finalize textures that have become ready.
        resourceLoader.asyncUpdateLoad()

        // Add renderable entities to the scene as they become ready.
        asset?.let { populateScene(it) }

        // Extract the camera basis from the helper and push it to the Filament camera.
        cameraManipulator.getLookAt(eyePos, target, upward)
        camera.lookAt(
            eyePos[0], eyePos[1], eyePos[2],
            target[0], target[1], target[2],
            upward[0], upward[1], upward[2])

        // Render the scene, unless the renderer wants to skip the frame.
        if (renderer.beginFrame(nativeSurface, frameTimeNanos)) {
            renderer.render(view)
            renderer.endFrame()
        }
    }

    private fun populateScene(asset: FilamentAsset) {
        val rcm = engine.renderableManager
        var count = 0
        val popRenderables = { count = asset.popRenderables(readyRenderables); count != 0 }
        while (popRenderables()) {
            for (i in 0 until count) {
                val ri = rcm.getInstance(readyRenderables[i])
                rcm.setScreenSpaceContactShadows(ri, true)
            }
            scene.addEntities(readyRenderables.take(count).toIntArray())
        }
        scene.addEntities(asset.lightEntities)
    }

    fun destroyViewer() {
        uiHelper.detach()

        destroyModel()
        assetLoader.destroy()
        materialProvider.destroyMaterials()
        materialProvider.destroy()
        resourceLoader.destroy()

        engine.destroyEntity(light)
        engine.destroyRenderer(renderer)
        engine.destroyView(this@FWModelViewer.view)
        engine.destroyScene(scene)
        engine.destroyCameraComponent(camera.entity)
        EntityManager.get().destroy(camera.entity)

        EntityManager.get().destroy(light)

        engine.destroy()
        logg("viewer destroyed")
    }

    /**
     * Handles a [MotionEvent] to enable one-finger orbit, two-finger pan, and pinch-to-zoom.
     */
    fun onTouchEvent(event: MotionEvent) {
        // don't process touch gestures beyond two fingers
        if (event.pointerCount > 2) return

        gestureDetector.onTouchEvent(event)
    }

    @SuppressWarnings("ClickableViewAccessibility")
    override fun onTouch(view: android.view.View, event: MotionEvent): Boolean {
        onTouchEvent(event)
        return true
    }

    private fun updateCameraProjection() {
        val width = view.viewport.width
        val height = view.viewport.height
        val aspect = width.toDouble() / height.toDouble()
        camera.setLensProjection(cameraFocalLength.toDouble(), aspect,
            cameraNear.toDouble(), cameraFar.toDouble())
    }

    inner class SurfaceCallback : UiHelper.RendererCallback {
        override fun onNativeWindowChanged(surface: Surface) {
            uiCallbackExecutor.invoke {
                swapChain?.let { engine.destroySwapChain(it) }
                swapChain = engine.createSwapChain(surface)
                surfaceView?.let { displayHelper.attach(renderer, it.display) }
                textureView?.let { displayHelper.attach(renderer, it.display) }
            }
        }

        override fun onDetachedFromSurface() {
            uiCallbackExecutor.invoke {
                displayHelper.detach()
                swapChain?.let {
                    engine.destroySwapChain(it)
                    engine.flushAndWait()
                    swapChain = null
                }
            }
        }

        override fun onResized(width: Int, height: Int) {
            uiCallbackExecutor.invoke {
                view.viewport = Viewport(0, 0, width, height)
                cameraManipulator.setViewport(width, height)
                updateCameraProjection()
                synchronizePendingFrames(engine)
            }
        }
    }

    private fun synchronizePendingFrames(engine: Engine) {
        // Wait for all pending frames to be processed before returning. This is to
        // avoid a race between the surface being resized before pending frames are
        // rendered into it.
        val fence = engine.createFence()
        fence.wait(Fence.Mode.FLUSH, Fence.WAIT_FOR_EVER)
        engine.destroyFence(fence)
    }

    companion object {
        private val kDefaultObjectPosition = Float3(0.0f, 0.0f, 0f)
    }
}
