package com.snap.valdi.modules

import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.logger.LogLevel
import com.snap.valdi.logger.Logger
import com.snap.valdi.utils.ValdiMarshallerJava
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
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

    private fun formatNumberWithCurrency(value: Double, currencyCode: String): String {
        val module = ValdiNumberFormattingModule(getApplicationContext(), logger)

        @Suppress("UNCHECKED_CAST")
        val method = (module.loadModule() as Map<String, ValdiFunction>)
            .getValue("formatNumberWithCurrency")

        val marshaller = ValdiMarshallerJava().apply {
            pushDouble(value)
            pushString(currencyCode)
        }

        method.perform(marshaller)

        return marshaller.getString(-1)
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
