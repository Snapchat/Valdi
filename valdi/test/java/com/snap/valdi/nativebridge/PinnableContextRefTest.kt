package com.snap.valdi.nativebridge

import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Test
import java.util.concurrent.CountDownLatch
import java.util.concurrent.atomic.AtomicInteger
import kotlin.concurrent.thread

class PinnableContextRefTest {

    private class RecordingRef(handle: Long) : ContextNative.PinnableContextRef(handle) {
        val activePins = AtomicInteger(0)
        val releasedHandles = mutableListOf<Long>()
        val pinsHeldAtRelease = mutableListOf<Int>()

        override fun destroyHandle(handle: Long) {
            synchronized(releasedHandles) {
                releasedHandles.add(handle)
                pinsHeldAtRelease.add(activePins.get())
            }
        }
    }

    @Test
    fun destroyWithoutPinsReleasesImmediately() {
        val ref = RecordingRef(42L)

        ref.destroy()

        assertEquals(listOf(42L), ref.releasedHandles)
        assertEquals(0L, ref.nativeHandle)
        assertEquals(0L, ref.pin())
    }

    @Test
    fun destroyWhilePinnedDefersReleaseUntilLastUnpin() {
        val ref = RecordingRef(42L)

        assertEquals(42L, ref.pin())
        assertEquals(42L, ref.pin())
        ref.destroy()

        assertEquals(emptyList<Long>(), ref.releasedHandles)
        assertEquals(0L, ref.nativeHandle)
        assertEquals(0L, ref.pin())

        ref.unpin()
        assertEquals(emptyList<Long>(), ref.releasedHandles)

        ref.unpin()
        assertEquals(listOf(42L), ref.releasedHandles)
    }

    @Test
    fun repeatedDestroyReleasesOnce() {
        val ref = RecordingRef(42L)

        ref.pin()
        ref.destroy()
        ref.destroy()
        ref.unpin()
        ref.destroy()

        assertEquals(listOf(42L), ref.releasedHandles)
    }

    @Test
    fun concurrentPinsNeverObserveARelease() {
        repeat(200) {
            val ref = RecordingRef(42L)
            val start = CountDownLatch(1)
            val callers = (0 until 4).map {
                thread {
                    start.await()
                    repeat(500) {
                        val handle = ref.pin()
                        if (handle != 0L) {
                            ref.activePins.incrementAndGet()
                            assertEquals(42L, handle)
                            ref.activePins.decrementAndGet()
                            ref.unpin()
                        }
                    }
                }
            }
            val destroyer = thread {
                start.await()
                ref.destroy()
            }

            start.countDown()
            callers.forEach { it.join() }
            destroyer.join()

            assertEquals(listOf(42L), ref.releasedHandles)
            assertEquals(listOf(0), ref.pinsHeldAtRelease)
        }
    }
}
