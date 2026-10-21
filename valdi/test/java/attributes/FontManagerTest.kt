package com.snap.valdi.attributes

import android.content.Context
import android.graphics.Typeface
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.attributes.impl.fonts.FontDataProvider
import com.snap.valdi.attributes.impl.fonts.FontDescriptor
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.FontWeight
import com.snap.valdi.attributes.impl.fonts.TypefaceResLoader
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
class FontManagerTest {

    private class RecordingListener : FontManager.Listener {
        val requested = mutableListOf<FontDescriptor>()

        override fun onFontRequested(descriptor: FontDescriptor) {
            requested.add(descriptor)
        }

        override fun onTypefaceRegistered(descriptor: FontDescriptor,
                                          isFallback: Boolean,
                                          dataProvider: FontDataProvider,
                                          registerImmediatelyWithSnapDrawing: Boolean) {}
    }

    private val listener = RecordingListener()
    private lateinit var fontManager: FontManager

    @Before
    fun setUp() {
        val typefaceResLoader = object : TypefaceResLoader {
            override fun loadTypeface(context: Context, resId: Int): Typeface = Typeface.DEFAULT
        }
        fontManager = FontManager(getApplicationContext(), typefaceResLoader)
        fontManager.listener = listener
    }

    // PROFILE-34366: Drawing.getFont stalled the JS thread for 5s+ because every lookup flushed the
    // Valdi worker queue, even for fonts that were already registered.
    @Test
    fun getRegisteredFontByNameDoesNotFlushPendingLoads() {
        fontManager.register(FontDescriptor(name = "avenir-next-bold"), Typeface.DEFAULT_BOLD)

        val typeface = fontManager.get(FontDescriptor(name = "avenir-next-bold"))

        assertSame(Typeface.DEFAULT_BOLD, typeface)
        assertEquals(emptyList<FontDescriptor>(), listener.requested)
    }

    @Test
    fun getRegisteredFontByFamilyDoesNotFlushPendingLoads() {
        val descriptor = FontDescriptor(family = "avenir-next", weight = FontWeight.BOLD)
        fontManager.register(descriptor, Typeface.DEFAULT_BOLD)

        val typeface = fontManager.get(FontDescriptor(family = "avenir-next", weight = FontWeight.BOLD))

        assertSame(Typeface.DEFAULT_BOLD, typeface)
        assertEquals(emptyList<FontDescriptor>(), listener.requested)
    }

    @Test
    fun getUnregisteredFontFlushesPendingLoads() {
        val descriptor = FontDescriptor(name = "module-font")

        fontManager.get(descriptor)

        assertEquals(listOf(descriptor), listener.requested)
    }

    @Test
    fun concurrentLookupsOfPendingFontBothWaitForRegistration() {
        val descriptor = FontDescriptor(name = "module-font")
        val firstFlushStarted = CountDownLatch(1)
        val secondLookupReachedFlushOrFinished = CountDownLatch(1)
        val registrationDone = CountDownLatch(1)
        val flushCount = AtomicInteger()
        // Stands in for the worker-queue flush: blocks until the pending load op has registered the font.
        fontManager.listener = object : FontManager.Listener {
            override fun onFontRequested(descriptor: FontDescriptor) {
                if (flushCount.incrementAndGet() == 1) {
                    firstFlushStarted.countDown()
                } else {
                    secondLookupReachedFlushOrFinished.countDown()
                }
                registrationDone.await(5, TimeUnit.SECONDS)
            }

            override fun onTypefaceRegistered(descriptor: FontDescriptor,
                                              isFallback: Boolean,
                                              dataProvider: FontDataProvider,
                                              registerImmediatelyWithSnapDrawing: Boolean) {}
        }
        val executor = Executors.newFixedThreadPool(2)
        try {
            val first = executor.submit<Typeface> { fontManager.loadSynchronously(descriptor) }
            assertTrue(firstFlushStarted.await(5, TimeUnit.SECONDS))
            val second = executor.submit<Typeface> {
                try {
                    fontManager.loadSynchronously(descriptor)
                } finally {
                    secondLookupReachedFlushOrFinished.countDown()
                }
            }
            assertTrue(secondLookupReachedFlushOrFinished.await(5, TimeUnit.SECONDS))

            fontManager.register(descriptor, Typeface.DEFAULT_BOLD)
            registrationDone.countDown()

            assertSame(Typeface.DEFAULT_BOLD, first.get(5, TimeUnit.SECONDS))
            assertSame(Typeface.DEFAULT_BOLD, second.get(5, TimeUnit.SECONDS))
        } finally {
            executor.shutdownNow()
        }
    }

    @Test
    fun getFamilyWithoutExactWeightFlushesPendingLoads() {
        fontManager.register(FontDescriptor(family = "avenir-next", weight = FontWeight.NORMAL), Typeface.DEFAULT)
        val descriptor = FontDescriptor(family = "avenir-next", weight = FontWeight.BOLD)

        val typeface = fontManager.get(descriptor)

        assertSame(Typeface.DEFAULT, typeface)
        assertEquals(listOf(descriptor), listener.requested)
    }
}
