package com.snap.valdi.views

import android.app.Activity
import android.app.TimePickerDialog
import android.content.ContextWrapper
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.MotionEvent
import android.content.Context
import android.content.res.Resources
import androidx.annotation.Keep
import android.widget.DatePicker
import android.widget.NumberPicker
import android.widget.TextView
import android.widget.TimePicker
import android.util.Xml.asAttributeSet
import android.os.Build
import android.util.AttributeSet
import android.util.TypedValue
import android.text.format.DateFormat
import com.snap.valdi.logger.Logger
import com.snap.valdi.context.ValdiContext
import com.snap.valdi.context.ValdiViewOwner
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.utils.error
import com.snap.valdi.utils.warn
import com.snap.valdi.utils.attributeSetFromXml
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.utils.ValdiMarshaller
import com.snap.valdi.utils.InternedString
import java.util.Calendar

@Keep
open class ValdiTimePicker(context: Context) : ViewGroup(context), ValdiTouchTarget {

    companion object {
        private val hourOfDayProperty = InternedString.create("hourOfDay")
        private val minuteOfHourProperty = InternedString.create("minuteOfHour")

        // Source of truth: STYLE_MAP in composer/coreui/src/components/pickers/DatePickerPreferredStyle.ts
        private const val STYLE_SPINNER = 1
        private const val STYLE_OVERLAY = 2

        fun timePickerAttributeSet(context: Context): AttributeSet? {
            return attributeSetFromXml(context, com.snapchat.client.R.xml.valdi_time_picker)
        }
    }

    private var currentHour: Int = 0
    private var currentMinute: Int = 0

    var hourOfDay: Int?
        get() = currentHour
        set(value) {
            currentHour = value ?: 0
            safeUpdate { underlyingTimePickerHour = currentHour }
            updateOverlayLabel()
        }

    var minuteOfHour: Int?
        get() = currentMinute
        set(value) {
            currentMinute = value ?: 0
            safeUpdate { underlyingTimePickerMinuteIndex = currentMinute / intervalMinutes }
            updateOverlayLabel()
        }

    var intervalMinutes: Int = 1
        set(value) {
            safeUpdate {
                setMinutesInterval(value)
                field = value
            }
        }

    private var underlyingTimePickerHour: Int
        get() {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                return timePicker.getHour()
            } else {
                return timePicker.getCurrentHour()
            }
        }
        set(value) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                timePicker.setHour(value)
            } else {
                timePicker.setCurrentHour(value)
            }
        }

    private var underlyingTimePickerMinuteIndex: Int
        get() {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                return timePicker.getMinute()
            } else {
                return timePicker.getCurrentMinute()
            }
        }
        set(value) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                timePicker.setMinute(value)
            } else {
                timePicker.setCurrentMinute(value)
            }
        }

    var onChangeFunction: ValdiFunction? = null

    var preferredStyle: Int = STYLE_SPINNER
        set(value) {
            if (field == value) return
            field = value
            applyStyle()
        }

    val timePicker = TimePicker(context, timePickerAttributeSet(context))
    private val overlayLabel = TextView(context).apply {
        setTextSize(TypedValue.COMPLEX_UNIT_SP, 16f)
        gravity = Gravity.CENTER_VERTICAL
        val outValue = TypedValue()
        context.theme.resolveAttribute(android.R.attr.selectableItemBackground, outValue, true)
        setBackgroundResource(outValue.resourceId)
        setOnClickListener { showTimeDialog() }
        visibility = GONE
    }

    val valdiContext: ValdiContext?
        get() = ViewUtils.findValdiContext(this)

    val logger: Logger?
        get() = valdiContext?.runtime?.logger

    private var isSettingValueCount = 0

    private fun safeUpdate(block: () -> Unit) {
        isSettingValueCount += 1
        try {
            block()
        } finally {
            isSettingValueCount -= 1
        }
    }

    init {
        addView(timePicker)
        addView(overlayLabel)

        val c = Calendar.getInstance()
        currentHour = c.get(Calendar.HOUR_OF_DAY)
        currentMinute = c.get(Calendar.MINUTE)

        timePicker.setIs24HourView(DateFormat.is24HourFormat(context))
        timePicker.setOnTimeChangedListener(object: TimePicker.OnTimeChangedListener {
            override fun onTimeChanged(view: TimePicker, hourOfDay: Int, minute: Int) {
                if (isSettingValueCount > 0) return

                currentHour = hourOfDay
                currentMinute = minute * intervalMinutes
                fireOnChange()
            }
        })
        timePicker.setDescendantFocusability(TimePicker.FOCUS_BLOCK_DESCENDANTS)
        updateOverlayLabel()
    }

    private fun applyStyle() {
        if (preferredStyle == STYLE_OVERLAY) {
            timePicker.visibility = GONE
            overlayLabel.visibility = VISIBLE
            updateOverlayLabel()
        } else {
            timePicker.visibility = VISIBLE
            overlayLabel.visibility = GONE
        }
        requestLayout()
    }

    private fun updateOverlayLabel() {
        val c = Calendar.getInstance()
        c.set(Calendar.HOUR_OF_DAY, currentHour)
        c.set(Calendar.MINUTE, currentMinute)
        overlayLabel.text = DateFormat.getTimeFormat(context).format(c.time)
    }

    private fun resolveActivity(): Activity? {
        var current: View? = this
        while (current != null) {
            var ctx = current.context
            while (ctx is ContextWrapper) {
                if (ctx is Activity) return ctx
                ctx = ctx.baseContext
            }
            current = current.parent as? View
        }
        return null
    }

    private fun showTimeDialog() {
        val activity = resolveActivity()
        if (activity == null || activity.isFinishing || activity.isDestroyed) return

        val is24Hour = DateFormat.is24HourFormat(activity)
        TimePickerDialog(activity, { _, hour, minute ->
            currentHour = hour
            currentMinute = minute
            safeUpdate {
                underlyingTimePickerHour = hour
                underlyingTimePickerMinuteIndex = minute / intervalMinutes
            }
            updateOverlayLabel()
            fireOnChange()
        }, currentHour, currentMinute, is24Hour).show()
    }

    private fun fireOnChange() {
        ViewUtils.notifyAttributeChanged(timePicker, hourOfDayProperty, currentHour)
        ViewUtils.notifyAttributeChanged(timePicker, minuteOfHourProperty, currentMinute)

        val fn = onChangeFunction ?: return
        ValdiMarshaller.use {
            val objectIndex = it.pushMap(2)
            it.putMapPropertyOptionalDouble(hourOfDayProperty, objectIndex, currentHour.toDouble())
            it.putMapPropertyOptionalDouble(minuteOfHourProperty, objectIndex, currentMinute.toDouble())
            fn.perform(it)
        }
    }

    override fun processTouchEvent(event: MotionEvent): ValdiTouchEventResult {
        if (this.dispatchTouchEvent(event)) {
            return ValdiTouchEventResult.ConsumeEventAndCancelOtherGestures
        }
        return ValdiTouchEventResult.IgnoreEvent
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec)
        val activeChild = if (preferredStyle == STYLE_OVERLAY) overlayLabel else timePicker
        activeChild.measure(widthMeasureSpec, heightMeasureSpec)
        setMeasuredDimension(activeChild.measuredWidth, activeChild.measuredHeight)
    }

    override fun onLayout(p0: Boolean, l: Int, t: Int, r: Int, b: Int) {
        val activeChild = if (preferredStyle == STYLE_OVERLAY) overlayLabel else timePicker
        activeChild.layout(0, 0, r - l, b - t)
    }

    override fun hasOverlappingRendering(): Boolean {
        return ViewUtils.hasOverlappingRendering(this)
    }

    private fun setMinutesInterval(intervalMinutes: Int) {
        if (intervalMinutes == 1) return
        timePicker.post {
            try {
                val minutePicker: NumberPicker = timePicker.findViewById(Resources.getSystem().getIdentifier(
                    "minute", "id", "android")) as NumberPicker
                minutePicker.minValue = 0
                minutePicker.maxValue = 60 / intervalMinutes - 1
                val displayedValues: MutableList<String> = ArrayList()
                var i = 0
                while (i < 60) {
                    displayedValues.add(String.format("%02d", i))
                    i += intervalMinutes
                }
                minutePicker.displayedValues = displayedValues.toTypedArray()
            } catch (e: Exception) {
                logger?.error("Failed to set minute interval increment for timepicker ${e}")
            }
        }
    }
}
