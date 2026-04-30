package com.snap.valdi.views

import android.app.Activity
import android.app.DatePickerDialog
import android.content.ContextWrapper
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.MotionEvent
import android.content.Context
import android.widget.TextView
import androidx.annotation.Keep
import android.widget.DatePicker
import android.util.AttributeSet
import android.util.TypedValue
import com.snap.valdi.logger.Logger
import com.snap.valdi.context.ValdiContext
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.utils.attributeSetFromXml
import com.snap.valdi.callable.ValdiFunction
import java.text.DateFormat
import java.util.Calendar
import java.util.Date
import com.snap.valdi.utils.ValdiMarshaller
import com.snap.valdi.utils.InternedString

@Keep
open class ValdiDatePicker(context: Context) : ViewGroup(context), ValdiTouchTarget {

    companion object {
        private val dateSecondsProperty = InternedString.create("dateSeconds")

        // Source of truth: STYLE_MAP in composer/coreui/src/components/pickers/DatePickerPreferredStyle.ts
        private const val STYLE_SPINNER = 1
        private const val STYLE_OVERLAY = 2

        fun datePickerAttributeSet(context: Context): AttributeSet? {
            return attributeSetFromXml(context, com.snapchat.client.R.xml.valdi_date_picker)
        }
    }

    private var currentYear: Int
    private var currentMonth: Int
    private var currentDay: Int

    var dateSeconds: Float?
        get() {
            val c = Calendar.getInstance()
            c.set(Calendar.YEAR, currentYear)
            c.set(Calendar.MONTH, currentMonth)
            c.set(Calendar.DAY_OF_MONTH, currentDay)
            return (c.getTimeInMillis() / 1000.0).toFloat()
        }
        set(value) {
            val date: Date = if (value == null) Date() else Date((value * 1000).toLong())
            val c = Calendar.getInstance()
            c.time = date
            currentYear = c.get(Calendar.YEAR)
            currentMonth = c.get(Calendar.MONTH)
            currentDay = c.get(Calendar.DAY_OF_MONTH)
            safeUpdate {
                datePicker.updateDate(currentYear, currentMonth, currentDay)
            }
            updateOverlayLabel()
        }

    var minimumDateSeconds: Float?
        get() {
            val minDate = datePicker.getMinDate()
            return (minDate / 1000.0).toFloat()
        }
        set(value) {
            if (value == null) {
                datePicker.setMinDate(Long.MIN_VALUE)
                return
            }
            datePicker.setMinDate((value * 1000).toLong())
        }

    var maximumDateSeconds: Float?
        get() {
            val maxDate = datePicker.getMaxDate()
            return (maxDate / 1000.0).toFloat()
        }
        set(value) {
            if (value == null) {
                datePicker.setMaxDate(Long.MAX_VALUE)
                return
            }
            datePicker.setMaxDate((value * 1000).toLong())
        }

    var onChangeFunction: ValdiFunction? = null

    var preferredStyle: Int = STYLE_SPINNER
        set(value) {
            if (field == value) return
            field = value
            applyStyle()
        }

    private val datePicker = DatePicker(context, datePickerAttributeSet(context))
    private val overlayLabel = TextView(context).apply {
        setTextSize(TypedValue.COMPLEX_UNIT_SP, 16f)
        gravity = Gravity.CENTER_VERTICAL
        val outValue = TypedValue()
        context.theme.resolveAttribute(android.R.attr.selectableItemBackground, outValue, true)
        setBackgroundResource(outValue.resourceId)
        setOnClickListener { showDateDialog() }
        visibility = GONE
    }

    private val valdiContext: ValdiContext?
        get() = ViewUtils.findValdiContext(this)

    private val logger: Logger?
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
        val c = Calendar.getInstance()
        c.time = Date()
        currentYear = c.get(Calendar.YEAR)
        currentMonth = c.get(Calendar.MONTH)
        currentDay = c.get(Calendar.DAY_OF_MONTH)

        addView(datePicker)
        addView(overlayLabel)

        datePicker.init(currentYear, currentMonth, currentDay, object: DatePicker.OnDateChangedListener {
            override fun onDateChanged(view: DatePicker, year: Int, monthOfYear: Int, dayOfMonth: Int) {
                if (isSettingValueCount > 0) return

                currentYear = year
                currentMonth = monthOfYear
                currentDay = dayOfMonth

                fireOnChange()
            }
        })
        datePicker.setDescendantFocusability(DatePicker.FOCUS_BLOCK_DESCENDANTS)
        updateOverlayLabel()
    }

    private fun applyStyle() {
        if (preferredStyle == STYLE_OVERLAY) {
            datePicker.visibility = GONE
            overlayLabel.visibility = VISIBLE
            updateOverlayLabel()
        } else {
            datePicker.visibility = VISIBLE
            overlayLabel.visibility = GONE
        }
        requestLayout()
    }

    private fun updateOverlayLabel() {
        val c = Calendar.getInstance()
        c.set(currentYear, currentMonth, currentDay)
        overlayLabel.text = DateFormat.getDateInstance(DateFormat.MEDIUM).format(c.time)
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

    private fun showDateDialog() {
        val activity = resolveActivity()
        if (activity == null || activity.isFinishing || activity.isDestroyed) return

        val outerDatePicker = this.datePicker
        DatePickerDialog(activity, { _, year, month, day ->
            currentYear = year
            currentMonth = month
            currentDay = day
            safeUpdate { outerDatePicker.updateDate(year, month, day) }
            updateOverlayLabel()
            fireOnChange()
        }, currentYear, currentMonth, currentDay).apply {
            val minMs = outerDatePicker.minDate
            val maxMs = outerDatePicker.maxDate
            if (minMs != Long.MIN_VALUE) datePicker.minDate = minMs
            if (maxMs != Long.MAX_VALUE) datePicker.maxDate = maxMs
        }.show()
    }

    private fun fireOnChange() {
        ViewUtils.notifyAttributeChanged(datePicker, dateSecondsProperty, dateSeconds)

        val fn = onChangeFunction ?: return
        ValdiMarshaller.use {
            val objectIndex = it.pushMap(1)
            it.putMapPropertyOptionalDouble(dateSecondsProperty, objectIndex, dateSeconds?.toDouble())
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
        val activeChild = if (preferredStyle == STYLE_OVERLAY) overlayLabel else datePicker
        activeChild.measure(widthMeasureSpec, heightMeasureSpec)
        setMeasuredDimension(activeChild.measuredWidth, activeChild.measuredHeight)
    }

    override fun onLayout(p0: Boolean, l: Int, t: Int, r: Int, b: Int) {
        val activeChild = if (preferredStyle == STYLE_OVERLAY) overlayLabel else datePicker
        activeChild.layout(0, 0, r - l, b - t)
    }

    override fun hasOverlappingRendering(): Boolean {
        return ViewUtils.hasOverlappingRendering(this)
    }
}
