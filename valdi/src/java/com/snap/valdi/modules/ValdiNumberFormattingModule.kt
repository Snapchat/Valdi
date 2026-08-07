package com.snap.valdi.modules

import android.content.Context
import com.snap.valdi.logger.LogLevel
import com.snap.valdi.logger.Logger
import com.snap.valdi.utils.ValdiMarshaller
import java.text.DecimalFormat
import java.text.NumberFormat
import java.util.Currency

/**
 * Duktape does not support Javascript number formatting. This native module can be used to format numbers using
 * Android's built-in number formatter.
 */
class ValdiNumberFormattingModule(
    private val context: Context,
    private val logger: Logger,
) : ValdiBridgeModule() {

    override fun getModulePath(): String {
        return "NumberFormatting"
    }

    override fun loadModule(): Any {
        return mapOf(
                "formatNumber" to makeBridgeMethod(this::formatNumber),
                "formatNumberWithCurrency" to makeBridgeMethod(this::formatNumberWithCurrency)
        )
    }

    private fun formatNumber(marshaller: ValdiMarshaller) {
        val value = marshaller.getDouble(0)

        val numFractionDigits = (if (marshaller.isDouble(1)) marshaller.getDouble(1).toInt()  else -1)

        val format = if (numFractionDigits > 0) DecimalFormat.getInstance() else NumberFormat.getInstance()
        format.isGroupingUsed = true
        if (numFractionDigits != -1) {
            format.minimumFractionDigits = numFractionDigits
            format.maximumFractionDigits = numFractionDigits
        }

        marshaller.pushString(format.format(value))
    }

    private fun formatNumberWithCurrency(marshaller: ValdiMarshaller) {
        val value = marshaller.getDouble(0)

        val currencyCode = if (marshaller.isString(1)) marshaller.getString(1) else ""

        val minFractionDigits = if (marshaller.isDouble(2)) marshaller.getDouble(2).toInt() else null

        val maxFractionDigits = if (marshaller.isDouble(3)) marshaller.getDouble(3).toInt() else null
        try {
            val format = NumberFormat.getCurrencyInstance()
            format.isGroupingUsed = true
            format.currency = Currency.getInstance(currencyCode)
            minFractionDigits?.let { format.minimumFractionDigits = it }
            maxFractionDigits?.let { format.maximumFractionDigits = it }
            marshaller.pushString(format.format(value))
        }
        catch (e: IllegalArgumentException) {
            // Currency.getInstance throws IllegalArgumentException (not NumberFormatException) for
            // invalid or unsupported codes. If the currency code is invalid, don't crash, just
            // output the number.
            logger.log(LogLevel.WARN, e, "formatNumberWithCurrency: invalid currency code '$currencyCode', falling back to plain number")
            marshaller.pushString("${value.toString()} ${currencyCode.toString()}")
        }
    }
}
