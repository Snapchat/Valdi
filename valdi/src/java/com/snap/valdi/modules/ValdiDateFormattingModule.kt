package com.snap.valdi.modules

import android.content.Context
import android.text.format.DateFormat
import android.text.format.DateUtils
import com.snap.valdi.utils.ExecutorsUtil
import com.snap.valdi.utils.ValdiMarshaller
import java.util.Calendar
import java.util.Date
import java.util.TimeZone
import java.util.concurrent.Executor

/**
 * Duktape does not support Javascript date formatting. This native module can be used to format dates using Android's
 * built-in date formatter.
 */
class ValdiDateFormattingModule(
    private val context: Context,
    computationExecutor: Executor? = null,
) : ValdiBridgeModule() {
    companion object {
        private const val DEFAULT_FORMAT = "MEDIUM"

        // Only for hosts that don't pass a computation executor.
        private val fallbackExecutor by lazy {
            ExecutorsUtil.newSingleThreadCachedExecutor { r ->
                Thread(r).apply {
                    name = "Valdi Date Formatting"
                    priority = Thread.NORM_PRIORITY
                }
            }
        }
    }

    // The first call in a process loads ICU locale data, which can take seconds on devices under memory
    // pressure. formatDateAsync keeps that off the JS thread.
    private val asyncExecutor: Executor by lazy { computationExecutor ?: fallbackExecutor }

    private class Formats(context: Context) {
        val shortDate = DateFormat.getDateFormat(context)
        val mediumDate = DateFormat.getMediumDateFormat(context)
        val longDate = DateFormat.getLongDateFormat(context)
        val time = DateFormat.getTimeFormat(context)
    }

    private val formats = lazy { Formats(context) }

    override fun getModulePath(): String {
        return "DateFormatting"
    }

    override fun loadModule(): Any {
        return mapOf(
                "formatDate" to makeBridgeMethod(this::formatDate),
                "formatDateAsync" to makeBridgeMethod(this::formatDateAsync)
        )
    }

    private fun formatDate(marshaller: ValdiMarshaller) {
        val epochMillis = marshaller.getDouble(0).toLong()
        val format = if (marshaller.isString(1)) marshaller.getString(1) else DEFAULT_FORMAT
        val timeZone = if (marshaller.isString(2)) marshaller.getString(2) else null

        marshaller.pushString(formatEpochMillis(epochMillis, format, timeZone))
    }

    /**
     * Same arguments as formatDate, followed by a callback that receives (formatted) on success or
     * (undefined, errorMessage) on failure. The callback runs on the JS thread.
     */
    private fun formatDateAsync(marshaller: ValdiMarshaller) {
        val epochMillis = marshaller.getDouble(0).toLong()
        val format = if (marshaller.isString(1)) marshaller.getString(1) else DEFAULT_FORMAT
        val timeZone = if (marshaller.isString(2)) marshaller.getString(2) else null
        val callback = marshaller.getFunction(3)

        asyncExecutor.execute {
            val result = try {
                Result.success(formatEpochMillis(epochMillis, format, timeZone))
            } catch (e: Exception) {
                Result.failure(e)
            }

            ValdiMarshaller.use {
                result.fold(
                    onSuccess = { formatted -> it.pushString(formatted) },
                    onFailure = { error ->
                        it.pushUndefined()
                        it.pushString(error.message ?: error.javaClass.name)
                    }
                )
                callback.perform(it)
            }
        }

        marshaller.pushUndefined()
    }

    private fun formatEpochMillis(epochMillis: Long, format: String, timeZone: String?): String {
        val date = Date(epochMillis)

        return when (format) {

            // EN: 4/20/21 -> FR: 20/04/2021
            "SHORT" -> formatWithTimeZone(formats.value.shortDate, timeZone, date)

            // EN: 4/20 -> FR: 20/04
            "SHORT_NO_YEAR" -> DateUtils.formatDateTime(
                context,
                epochMillis,
                DateUtils.FORMAT_SHOW_DATE
                or DateUtils.FORMAT_NO_YEAR
                or DateUtils.FORMAT_NUMERIC_DATE
            )

            // EN: Apr 20, 2021 -> FR: 20 avr. 2021
            "MEDIUM" -> formatWithTimeZone(formats.value.mediumDate, timeZone, date)

            // EN: Apr 20 -> FR: 20 avr.
            "MEDIUM_NO_YEAR" -> DateUtils.formatDateTime(
                context,
                epochMillis,
                DateUtils.FORMAT_SHOW_DATE
                or DateUtils.FORMAT_NO_YEAR
                or DateUtils.FORMAT_ABBREV_MONTH
            )

            // EN: April 20, 2021 -> FR: 20 avril 2021
            "LONG" -> formatWithTimeZone(formats.value.longDate, timeZone, date)

            // EN: April 20 -> FR: 20 avril
            "LONG_NO_YEAR" -> DateUtils.formatDateTime(
                context,
                epochMillis,
                DateUtils.FORMAT_SHOW_DATE
                or DateUtils.FORMAT_NO_YEAR
            )

            // EN: April 2021 -> FR: avril 2021
            "LONG_NO_DAY" -> DateUtils.formatDateTime(
                context,
                epochMillis,
                DateUtils.FORMAT_SHOW_DATE
                or DateUtils.FORMAT_SHOW_YEAR
                or DateUtils.FORMAT_NO_MONTH_DAY
            )

            // EN: Tuesday -> FR: mardi
            "DAY_OF_WEEK" ->  getCalendar(date, timeZone).getDisplayName(Calendar.DAY_OF_WEEK, Calendar.LONG, context.resources.configuration.locale)

            // EN: Tue -> FR: mar,
            "DAY_OF_WEEK_SHORT" -> getCalendar(date, timeZone).getDisplayName(Calendar.DAY_OF_WEEK, Calendar.SHORT, context.resources.configuration.locale)

            // EN: 5:33 PM -> FR: 17:33
            "TIME" -> formatWithTimeZone(formats.value.time, timeZone, date)

            // Fallback
            else -> formatWithTimeZone(formats.value.mediumDate, timeZone, date)
        }
    }

    private fun getCalendar(date: Date, timeZone: String?): Calendar {
      val calendar = Calendar.getInstance()
      timeZone?.let { calendar.timeZone = TimeZone.getTimeZone(it) }
      calendar.time = date
      return calendar
    }

    private fun formatWithTimeZone(dateFormat: java.text.DateFormat, timeZone: String?, date: Date): String {
        return synchronized(dateFormat) {
            if (timeZone == null) {
                return dateFormat.format(date)
            }

            val oldTimeZone = dateFormat.timeZone
            dateFormat.timeZone = TimeZone.getTimeZone(timeZone)
            val str = dateFormat.format(date)
            dateFormat.timeZone = oldTimeZone
            str
        }
    }
}
