package com.snap.valdi.modules

import android.content.Context
import com.snap.valdi.logger.LogLevel
import com.snap.valdi.logger.Logger
import com.snap.valdi.utils.ValdiMarshaller
import java.text.NumberFormat
import java.util.Currency
import java.util.Locale

/**
 * Duktape does not support Javascript number formatting. This native module can be used to format numbers using
 * Android's built-in number formatter.
 */
class ValdiNumberFormattingModule(
    private val context: Context,
    private val logger: Logger,
) : ValdiBridgeModule() {

    private data class PlainKey(val locale: Locale, val fractionDigits: Int)

    private data class CurrencyKey(
        val locale: Locale,
        val currencyCode: String,
        val minFractionDigits: Int?,
        val maxFractionDigits: Int?,
    )

    // getInstance()/getCurrencyInstance() rebuild the formatter (and its locale symbols) on every
    // call, which dominates the cost on hot render paths. Cache the configured formatters, keyed on
    // the current locale. NumberFormat isn't thread-safe, so each thread gets its own cache. A bounded
    // LRU keeps memory flat regardless of how many distinct format options are requested. The caches
    // are static so one ThreadLocal is shared across module instances rather than leaking a new one
    // per Valdi runtime context.
    private companion object {
        private const val MAX_CACHE_ENTRIES = 32

        private fun <K> lruCache() = object : LinkedHashMap<K, NumberFormat>(MAX_CACHE_ENTRIES, 0.75f, true) {
            override fun removeEldestEntry(eldest: Map.Entry<K, NumberFormat>) = size > MAX_CACHE_ENTRIES
        }

        // initialValue() override, not ThreadLocal.withInitial(), which is API 26+.
        val plainFormatters = object : ThreadLocal<LinkedHashMap<PlainKey, NumberFormat>>() {
            override fun initialValue() = lruCache<PlainKey>()
        }

        val currencyFormatters = object : ThreadLocal<LinkedHashMap<CurrencyKey, NumberFormat>>() {
            override fun initialValue() = lruCache<CurrencyKey>()
        }
    }

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

        val key = PlainKey(Locale.getDefault(), numFractionDigits)
        val format = plainFormatters.get().getOrPut(key) {
            // Build from key.locale, not getDefault() again, so the instance can't disagree with the key.
            NumberFormat.getInstance(key.locale).apply {
                isGroupingUsed = true
                if (numFractionDigits != -1) {
                    minimumFractionDigits = numFractionDigits
                    maximumFractionDigits = numFractionDigits
                }
            }
        }

        marshaller.pushString(format.format(value))
    }

    private fun formatNumberWithCurrency(marshaller: ValdiMarshaller) {
        val value = marshaller.getDouble(0)

        val currencyCode = if (marshaller.isString(1)) marshaller.getString(1) else ""

        val minFractionDigits = if (marshaller.isDouble(2)) marshaller.getDouble(2).toInt() else null

        val maxFractionDigits = if (marshaller.isDouble(3)) marshaller.getDouble(3).toInt() else null
        try {
            val key = CurrencyKey(Locale.getDefault(), currencyCode, minFractionDigits, maxFractionDigits)
            val format = currencyFormatters.get().getOrPut(key) {
                NumberFormat.getCurrencyInstance(key.locale).apply {
                    isGroupingUsed = true
                    currency = Currency.getInstance(currencyCode)
                    minFractionDigits?.let { minimumFractionDigits = it }
                    maxFractionDigits?.let { maximumFractionDigits = it }
                }
            }
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
