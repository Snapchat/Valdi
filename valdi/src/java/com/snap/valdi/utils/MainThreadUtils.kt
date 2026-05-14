@file:JvmName("MainThreadUtils")

package com.snap.valdi.utils

import android.os.Handler
import android.os.Looper

// Lazy main thread reference so we don't load Looper at class init (fails on JVM unit tests).
private val mainThreadOrNull: Thread? by lazy {
    try {
        Looper.getMainLooper()?.thread
    } catch (e: NoClassDefFoundError) {
        null
    } catch (e: ClassNotFoundException) {
        null
    } catch (e: NullPointerException) {
        // Robolectric / SDK stubs: Looper exists but getMainLooper() or .thread can be null
        null
    }
}

// When not on Android (e.g. JVM tests), treat the first thread that asks as "main" so
// strict-mode checks (e.g. assertResolutionNotOnMainThreadIfNeeded) still trigger.
private object JvmMainThreadFallback {
    @Volatile
    var mainThread: Thread? = null
}

// Convenience methods to run runnables in the UI thread.

fun runOnMainThreadIfNeeded(task: () -> Unit) {
    if (isMainThread()) {
        task()
    } else {
        dispatchOnMainThread(task)
    }
}

fun dispatchOnMainThread(task: () -> Unit) {
    handler.post(task)
}

fun isMainThread(): Boolean {
    val main = mainThreadOrNull
    if (main != null) {
        return Thread.currentThread() === main
    }
    // JVM unit test: no Android Looper. Use first caller as "main" so strict-mode still throws.
    synchronized(JvmMainThreadFallback) {
        if (JvmMainThreadFallback.mainThread == null) {
            JvmMainThreadFallback.mainThread = Thread.currentThread()
        }
        return Thread.currentThread() === JvmMainThreadFallback.mainThread
    }
}

fun assertMainThread() {
    if (!isMainThread()) {
        throw RuntimeException("This action can only be performed from the main thread")
    }
}

fun assertNotMainThread() {
    if (isMainThread()) {
        throw RuntimeException("This action should never be performed from the main thread")
    }
}

fun runOnMainThreadDelayed(delayMs: Long, task: () -> Unit) {
    handler.postDelayed(task, delayMs)
}

fun runOnMainThreadDelayed(delayMs: Long, task: Runnable) {
    handler.postDelayed(task, delayMs)
}

fun getValdiHandler(): Handler = handler

private val handler by lazy {
    object : Handler(Looper.getMainLooper()) {}
}
