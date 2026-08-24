package com.snap.valdi.modules

import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.logger.LogLevel
import com.snap.valdi.logger.Logger
import com.snap.valdi.utils.ValdiMarshallerJava
import java.util.Locale
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
class ValdiNumberFormattingModuleTest {

    private class RecordingLogger : Logger {
        val warnings = mutableListOf<String>()
        override fun log(level: Int, message: String?) = record(level, message)
        override fun log(level: Int, err: Throwable?, message: String?) = record(level, message)
        private fun record(level: Int, message: String?) {
            if (level == LogLevel.WARN && message != null) warnings.add(message)
        }
    }

    private val logger = RecordingLogger()
    private var previousLocale: Locale = Locale.getDefault()

    @Before
    fun setUp() {
        previousLocale = Locale.getDefault()
        // Pin the locale so grouping and decimal separators are deterministic across test hosts.
        Locale.setDefault(Locale.US)
    }

    @After
    fun tearDown() {
        Locale.setDefault(previousLocale)
    }

    private fun method(name: String): ValdiFunction {
        val module = ValdiNumberFormattingModule(getApplicationContext(), logger)

        @Suppress("UNCHECKED_CAST")
        return (module.loadModule() as Map<String, ValdiFunction>).getValue(name)
    }

    private fun formatNumber(value: Double, fractionDigits: Int? = null): String {
        val marshaller = ValdiMarshallerJava().apply {
            pushDouble(value)
            fractionDigits?.let { pushDouble(it.toDouble()) }
        }
        method("formatNumber").perform(marshaller)
        return marshaller.getString(-1)
    }

    private fun formatNumberWithCurrency(
        value: Double,
        currencyCode: String,
        minFractionDigits: Int? = null,
        maxFractionDigits: Int? = null,
    ): String {
        val marshaller = ValdiMarshallerJava().apply {
            pushDouble(value)
            pushString(currencyCode)
            if (minFractionDigits != null || maxFractionDigits != null) {
                pushDouble((minFractionDigits ?: 0).toDouble())
                pushDouble((maxFractionDigits ?: 0).toDouble())
            }
        }
        method("formatNumberWithCurrency").perform(marshaller)
        return marshaller.getString(-1)
    }

    @Test
    fun formatsWithGroupingByDefault() {
        assertEquals("1,234,567", formatNumber(1234567.0))
        assertEquals("1,234.5", formatNumber(1234.5))
    }

    @Test
    fun appliesFixedFractionDigits() {
        assertEquals("1,234.50", formatNumber(1234.5, 2))
        assertEquals("1", formatNumber(1.0, 0))
    }

    @Test
    fun repeatedCallsReturnSameResult() {
        val first = formatNumber(9999.0, 3)
        assertEquals(first, formatNumber(9999.0, 3))
        assertEquals("9,999.000", first)
    }

    @Test
    fun distinctFractionDigitsBeyondCacheCapacityStayCorrect() {
        // The formatter LRU caps at 32 entries; sweep well past that so evicted keys must be rebuilt.
        val firstResult = formatNumber(1.0, 0)
        for (digits in 0..40) {
            val expected = if (digits == 0) "1" else "1." + "0".repeat(digits)
            assertEquals(expected, formatNumber(1.0, digits))
        }
        // digits=0 was evicted during the sweep; re-request it and confirm it rebuilds identically.
        assertEquals(firstResult, formatNumber(1.0, 0))
        assertEquals("1", firstResult)
    }

    @Test
    fun switchingLocaleReformatsInsteadOfReturningCachedValue() {
        // Populate the cache under US, then switch the default locale. A cache that dropped locale
        // from its key would return the US string; correct keying must reformat with German separators.
        Locale.setDefault(Locale.US)
        assertEquals("1,234,567", formatNumber(1234567.0))
        assertEquals("1,234.5", formatNumber(1234.5))

        Locale.setDefault(Locale.GERMANY)
        assertEquals("1.234.567", formatNumber(1234567.0))
        assertEquals("1.234,5", formatNumber(1234.5))
    }

    @Test
    fun formatsCurrencyWithDefaultFractionDigits() {
        assertEquals("$1,234.50", formatNumberWithCurrency(1234.5, "USD"))
    }

    @Test
    fun currencyRepeatedCallsReturnSameResult() {
        val first = formatNumberWithCurrency(5.0, "USD")
        assertEquals(first, formatNumberWithCurrency(5.0, "USD"))
    }

    @Test
    fun distinctCurrencyOptionsBeyondCacheCapacityStayConsistent() {
        // Vary maxFractionDigits past the 32-entry cap to exercise currency-cache eviction.
        val firstCombo = formatNumberWithCurrency(1.5, "USD", 0, 0)
        for (maxDigits in 0..40) {
            val once = formatNumberWithCurrency(1.5, "USD", 0, maxDigits)
            val twice = formatNumberWithCurrency(1.5, "USD", 0, maxDigits)
            assertTrue(once.isNotEmpty())
            assertEquals(once, twice)
        }
        // The first combo was evicted; re-request it and confirm it rebuilds to the same value.
        assertEquals(firstCombo, formatNumberWithCurrency(1.5, "USD", 0, 0))
    }

    @Test
    fun emptyCurrencyCode_usesFallback() {
        assertEquals("12.5 ", formatNumberWithCurrency(12.5, ""))
    }

    @Test
    fun malformedCurrencyCode_usesFallback() {
        assertEquals("3.99 12A", formatNumberWithCurrency(3.99, "12A"))
    }

    @Test
    fun invalidCurrencyCode_logsWarning() {
        formatNumberWithCurrency(3.99, "12A")
        assertTrue(logger.warnings.any { it.contains("12A") })
    }
}
