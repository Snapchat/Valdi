package com.snap.valdi.modules

import android.text.format.DateFormat
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.utils.ValdiMarshaller
import com.snap.valdi.utils.ValdiMarshallerJava
import java.util.Calendar
import java.util.Date
import java.util.GregorianCalendar
import java.util.Locale
import java.util.TimeZone
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
class ValdiDateFormattingModuleTest {

    private class RecordingCallback : ValdiFunction {
        val latch = CountDownLatch(1)
        var formatted: String? = null
        var thread: Thread? = null

        override fun perform(marshaller: ValdiMarshaller): Boolean {
            formatted = if (marshaller.isString(0)) marshaller.getString(0) else null
            thread = Thread.currentThread()
            latch.countDown()
            return false
        }
    }

    private var previousLocale: Locale = Locale.getDefault()
    private var previousTimeZone: TimeZone = TimeZone.getDefault()

    @Before
    fun setUp() {
        previousLocale = Locale.getDefault()
        previousTimeZone = TimeZone.getDefault()
        Locale.setDefault(Locale.US)
        TimeZone.setDefault(TimeZone.getTimeZone("America/Los_Angeles"))
    }

    @After
    fun tearDown() {
        Locale.setDefault(previousLocale)
        TimeZone.setDefault(previousTimeZone)
    }

    private val module by lazy {
        @Suppress("UNCHECKED_CAST")
        ValdiDateFormattingModule(getApplicationContext()).loadModule() as Map<String, ValdiFunction>
    }

    // Built in the zone setUp() installs: property initializers run before @Before.
    private val birthday: Long = Calendar.getInstance(TimeZone.getTimeZone("America/Los_Angeles")).apply {
        clear()
        set(2020, Calendar.JUNE, 3)
    }.timeInMillis

    private fun formatDate(epochMillis: Long, format: String, timeZone: String? = null): String {
        val marshaller = ValdiMarshallerJava().apply {
            pushDouble(epochMillis.toDouble())
            pushString(format)
            if (timeZone == null) {
                pushUndefined()
            } else {
                pushString(timeZone)
            }
        }
        module.getValue("formatDate").perform(marshaller)
        return marshaller.getString(-1)
    }

    private fun formatDateAsync(epochMillis: Long, format: String): RecordingCallback {
        val callback = RecordingCallback()
        val marshaller = ValdiMarshallerJava().apply {
            pushDouble(epochMillis.toDouble())
            pushString(format)
            pushUndefined()
            pushFunction(callback)
        }
        module.getValue("formatDateAsync").perform(marshaller)
        assertTrue("formatDateAsync never called back", callback.latch.await(5, TimeUnit.SECONDS))
        return callback
    }

    @Test
    fun formatDateAsyncMatchesFormatDate() {
        for (format in listOf("SHORT_NO_YEAR", "MEDIUM_NO_YEAR", "LONG_NO_YEAR", "LONG_NO_DAY", "MEDIUM", "TIME")) {
            assertEquals(format, formatDate(birthday, format), formatDateAsync(birthday, format).formatted)
        }
    }

    @Test
    fun formatDateAsyncCallsBackOffTheCallerThread() {
        val callback = formatDateAsync(birthday, "MEDIUM_NO_YEAR")

        assertEquals("Jun 3", callback.formatted)
        assertNotSame(Thread.currentThread(), callback.thread)
    }

    @Test
    fun shortAndLongDatesHonorExplicitTimeZone() {
        val timestamp = Calendar.getInstance(TimeZone.getTimeZone("UTC")).apply {
            clear()
            set(2026, Calendar.OCTOBER, 13, 0, 30)
        }.timeInMillis

        for ((format, formatter) in listOf(
            "SHORT" to DateFormat.getDateFormat(getApplicationContext()),
            "LONG" to DateFormat.getLongDateFormat(getApplicationContext()),
        )) {
            val utc = formatter.clone() as java.text.DateFormat
            utc.timeZone = TimeZone.getTimeZone("UTC")
            val losAngeles = formatter.clone() as java.text.DateFormat
            losAngeles.timeZone = TimeZone.getTimeZone("America/Los_Angeles")

            assertEquals(utc.format(Date(timestamp)), formatDate(timestamp, format, "UTC"))
            assertEquals(losAngeles.format(Date(timestamp)), formatDate(timestamp, format, "America/Los_Angeles"))
            assertNotEquals(utc.format(Date(timestamp)), losAngeles.format(Date(timestamp)))
        }
    }

    private fun utcMillis(year: Int, month: Int, day: Int, hour: Int = 12, minute: Int = 0): Long =
        GregorianCalendar(TimeZone.getTimeZone("UTC")).apply {
            clear()
            set(year, month, day, hour, minute)
        }.timeInMillis

    private fun assertMonthAndYear(month: String, year: String, actual: String) {
        assertTrue(actual, actual.contains(month) && actual.contains(year))
    }

    // Asserts components, not their order: Robolectric's ICU shadow returns the "yMMM" skeleton unchanged
    // from getBestDateTimePattern, so en-US renders "2021Jan" here instead of a device's "Jan 2021".
    @Test
    @Config(qualifiers = "en-rUS")
    fun shortNoDay_rendersAbbreviatedMonthAndYear() {
        assertMonthAndYear("Jan", "2021", formatDate(utcMillis(2021, Calendar.JANUARY, 5), "SHORT_NO_DAY", "UTC"))
    }

    /** Guards against a full numeric date ("01/31/2021"), which includes the day. */
    @Test
    @Config(qualifiers = "en-rUS")
    fun shortNoDay_omitsDayOfMonth() {
        val result = formatDate(utcMillis(2021, Calendar.JANUARY, 31), "SHORT_NO_DAY", "UTC")

        assertFalse(result, result.contains("31"))
        assertTrue(result, result.contains("2021"))
    }

    /** Distinguishes SHORT_NO_DAY from its LONG_NO_DAY sibling, which spells the month out. */
    @Test
    @Config(qualifiers = "en-rUS")
    fun shortNoDay_abbreviatesTheMonth() {
        val timestamp = utcMillis(2021, Calendar.JANUARY, 5)

        assertTrue(formatDate(timestamp, "SHORT_NO_DAY", "UTC").length < formatDate(timestamp, "LONG_NO_DAY").length)
    }

    /**
     * Asserts ordering rather than glyphs: CLDR wording shifts between releases, but a year-first
     * locale staying year-first is the property MemOne's scrubber hand-rolled per locale.
     */
    @Test
    @Config(qualifiers = "zh-rCN")
    fun shortNoDay_putsYearFirstInChinese() {
        assertTrue(formatDate(utcMillis(2021, Calendar.JANUARY, 5), "SHORT_NO_DAY", "UTC").startsWith("2021"))
    }

    @Test
    @Config(qualifiers = "ko-rKR")
    fun shortNoDay_putsYearFirstInKorean() {
        assertTrue(formatDate(utcMillis(2021, Calendar.JANUARY, 5), "SHORT_NO_DAY", "UTC").startsWith("2021"))
    }

    /**
     * The DateUtils-based formats silently drop the time zone argument. SHORT_NO_DAY must honor it,
     * since the fallback path in TimeUtils.ts does.
     */
    @Test
    @Config(qualifiers = "en-rUS")
    fun shortNoDay_honorsTimeZone() {
        val newYearUtc = utcMillis(2021, Calendar.JANUARY, 1, hour = 0, minute = 30)

        assertMonthAndYear("Jan", "2021", formatDate(newYearUtc, "SHORT_NO_DAY", "UTC"))
        assertMonthAndYear("Dec", "2020", formatDate(newYearUtc, "SHORT_NO_DAY", "America/Los_Angeles"))
    }
}
