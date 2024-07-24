package com.google.android.filament.gltf.fw

import android.view.Choreographer
import android.view.Choreographer.FrameCallback
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.coroutineScope
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

class FrameChoreographer(
    private val doFrameCallback: FrameCallback
) {
    private val choreographer = Choreographer.getInstance()

    private val frameCallback = object : FrameCallback {
        override fun doFrame(frameTimeNanos: Long) {
            doFrameCallback.doFrame(frameTimeNanos)
            frameCount++
            choreographer.postFrameCallback(this)
        }
    }

    private var frameCount = 0

    private val _fpsFlow = MutableStateFlow(1)
    val fpsFlow: StateFlow<Int> = _fpsFlow

    private var fpsJob: Job? = null

    fun start() {
        choreographer.postFrameCallback(frameCallback)
    }

    fun stop() {
        choreographer.removeFrameCallback(frameCallback)
    }

    fun monitorFPSInResumedState() {
        frameCount = 0
        fpsJob = CoroutineScope(Dispatchers.Default).launch {
            delay(1000)
            val count = frameCount
            frameCount = 0
            _fpsFlow.tryEmit(count)
        }
    }

    fun stopMonitoringFPS() {
        frameCount = 0
        fpsJob?.cancel()
    }
}
