@file:Suppress("LateinitUsage")
package com.google.android.filament.gltf.fw

import android.content.Context
import android.view.Choreographer.FrameCallback
import android.view.SurfaceView
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.launch
import java.util.concurrent.Executors

/**
 * Executes all calls to [FWModelViewer] in a different, single, thread.
 *
 * While loading the model initially, Filament blocks the thread on which Filament#Engine is created
 * (which happens in ModelViewer), causing the UI to be blocked during that time, especially for larger models
 * that could take 10s of seconds. Filament requires that all calls to Engine must occur on the same thread
 * on which it is created. Therefore, this class creates the ModelViewer (and thus the Engine) on a different thread.
 * It executes all calls to ModelViewer on that thread and handles calls that do not involve Filament
 * on the calling thread.
 */
class BimExecutor(
    surfaceView: SurfaceView,
    private val callback: Callback
) : FrameCallback, BimViewer.Callback {

    private val dispatcher = Executors.newSingleThreadExecutor { runnable ->
        Thread(runnable).apply { priority = Thread.MAX_PRIORITY }
    }

    private val singleThreadScope = CoroutineScope(dispatcher.asCoroutineDispatcher())
    private val mainThreadScope = MainScope()

    private lateinit var bimViewer: BimViewer

    init {
        exec {
            bimViewer = BimViewer(surfaceView, this)
        }
    }

    fun loadModel(context: Context, fileName: String, onLoaded: (Int) -> Unit) {
        exec {
            bimViewer.loadModel(context, fileName)
            execInMain { onLoaded(bimViewer.nodeCount()) }
        }
    }

    override fun doFrame(frameTimeNanos: Long) {
        exec {
            bimViewer.doFrame(frameTimeNanos)
        }
    }

    override fun execute(block: () -> Unit) {
        exec { block() }
    }

    fun resetModel() {
        exec() { bimViewer.resetModel() }
    }

    fun destroyViewer(onDestroyed: () -> Unit) {
        exec() {
            bimViewer.destroyViewer()
            execInMain { onDestroyed() }
        }
    }

    fun destroyViewerSync() {
        bimViewer.destroyViewer()
    }

    override fun onModelRendered() {
        execInMain { callback.onModelRendered() }
    }

    override fun onClickProperties(entityId: String?) {
        execInMain { callback.onClickProperties(entityId) }
    }

    private fun exec(action: suspend () -> Unit) {
        singleThreadScope.launch {
            try {
                action()
            } catch (e: Throwable) {
                callback.onExecutorException(e)
            }
        }
    }

    private fun execInMain(action: () -> Unit) {
        mainThreadScope.launch {
            try {
                action()
            } catch (e: Throwable) {
                callback.onExecutorException(e)
            }
        }
    }

    interface Callback {
        fun onModelRendered()
        fun onClickProperties(entityId: String?)
        fun onExecutorException(throwable: Throwable)
    }
}
