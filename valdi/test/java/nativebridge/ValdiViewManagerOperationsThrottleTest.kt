package com.snap.valdi.nativebridge

import android.graphics.Bitmap
import android.os.Looper
import android.os.SystemClock
import android.view.View
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.ViewRef
import com.snap.valdi.attributes.BooleanAttributeHandlerDelegate
import com.snap.valdi.attributes.impl.animations.ValdiAnimator
import com.snap.valdi.logger.Logger
import com.snap.valdi.utils.BitmapPool
import com.snap.valdi.utils.CoordinateResolver
import com.snap.valdi.utils.ViewRefSupport
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.time.Duration
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.Config

/**
 * A single render can hand the main thread a very large batch of view operations. With a processing
 * budget configured, one flush must stop once the budget is spent and finish the rest on later
 * frames, so the looper keeps turning instead of the app being reported as not responding.
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
class ValdiViewManagerOperationsThrottleTest {

    private val logger = object : Logger {
        override fun log(level: Int, message: String?) {}
        override fun log(level: Int, err: Throwable?, message: String?) {}
    }

    // Each operation costs one millisecond of (virtual) main-thread time.
    private class SlowBooleanAttribute : BooleanAttributeHandlerDelegate() {
        var applied = 0

        override fun onApply(view: View, value: Boolean, animator: ValdiAnimator?) {
            applied++
            SystemClock.sleep(1)
        }

        override fun onReset(view: View, animator: ValdiAnimator?) {}
    }

    private fun makeApplyBoolOperations(count: Int): ByteBuffer {
        // header, delegate index, view ref index, value
        val buffer = ByteBuffer.allocate(count * 4 * Int.SIZE_BYTES).order(ByteOrder.LITTLE_ENDIAN)
        repeat(count) {
            buffer.putInt(OP_APPLY_ATTRIBUTE_BOOL or (1 shl 8))
            buffer.putInt(0)
            buffer.putInt(1)
            buffer.putInt(1)
        }
        buffer.flip()
        return buffer
    }

    @Test
    fun largeFlushIsSplitAcrossFramesWithinTheProcessingBudget() {
        val context = getApplicationContext<android.content.Context>()
        val bitmapPool = BitmapPool(context, Bitmap.Config.ARGB_8888, logger)
        val support = ViewRefSupport(logger, bitmapPool, CoordinateResolver(1f))
        val view = View(context)
        val attribute = SlowBooleanAttribute()
        val manager = ValdiViewManagerOperationsManager(logger, BUDGET_MS)

        val attachedValues = arrayOf<Any>(attribute, ViewRef(view, true, support))
        manager.appendViewOperations(makeApplyBoolOperations(OPERATION_COUNT), attachedValues)
        manager.flushViewOperations(false)

        assertTrue("first flush applied nothing", attribute.applied > 0)
        assertTrue(
            "first flush ran ${attribute.applied} ops (~${attribute.applied}ms) past a ${BUDGET_MS}ms budget",
            attribute.applied <= BUDGET_MS + 1,
        )

        // The remainder is finished on later frames rather than dropped.
        var frames = 0
        while (attribute.applied < OPERATION_COUNT && frames < OPERATION_COUNT) {
            shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(FRAME_MS))
            frames++
        }
        assertEquals(OPERATION_COUNT, attribute.applied)
    }

    private companion object {
        const val OP_APPLY_ATTRIBUTE_BOOL = 11
        const val OPERATION_COUNT = 10_000
        const val BUDGET_MS = 16
        const val FRAME_MS = 17L
    }
}
