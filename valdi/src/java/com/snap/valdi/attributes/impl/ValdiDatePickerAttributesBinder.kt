package com.snap.valdi.attributes.impl

import android.content.Context
import android.view.ViewGroup
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.attributes.AttributesBindingContext
import com.snap.valdi.attributes.impl.animations.ValdiAnimator
import com.snap.valdi.logger.Logger
import com.snap.valdi.views.ValdiDatePicker
import com.snap.valdi.callable.ValdiFunction

class ValdiDatePickerAttributesBinder(private val context: Context, val logger: Logger) : AttributesBinder<ValdiDatePicker> {

    override val viewClass: Class<ValdiDatePicker>
        get() = ValdiDatePicker::class.java

    fun applyDateSeconds(view: ValdiDatePicker, value: Float, animator: ValdiAnimator?) {
        view.dateSeconds = value
    }

    fun resetDateSeconds(view: ValdiDatePicker, animator: ValdiAnimator?) {
        view.dateSeconds = null
    }

    fun applyMinimumDateSeconds(view: ValdiDatePicker, value: Float, animator: ValdiAnimator?) {
        view.minimumDateSeconds = value
    }

    fun resetMinimumDateSeconds(view: ValdiDatePicker, animator: ValdiAnimator?) {
        view.minimumDateSeconds = null
    }

    fun applyMaximumDateSeconds(view: ValdiDatePicker, value: Float, animator: ValdiAnimator?) {
        view.maximumDateSeconds = value
    }

    fun resetMaximumDateSeconds(view: ValdiDatePicker, animator: ValdiAnimator?) {
        view.maximumDateSeconds = null
    }

    private fun applyOnChange(view: ValdiDatePicker, fn: ValdiFunction) {
        view.onChangeFunction = fn
    }

    private fun resetOnChange(view: ValdiDatePicker) {
        view.onChangeFunction = null
    }

    private fun applyPreferredStyle(view: ValdiDatePicker, value: Int, animator: ValdiAnimator?) {
        view.preferredStyle = value
    }

    private fun resetPreferredStyle(view: ValdiDatePicker, animator: ValdiAnimator?) {
        view.preferredStyle = 1 // spinner
    }

    private fun noopApplyColor(view: ValdiDatePicker, value: Int, animator: ValdiAnimator?) {
        // noop
        // DO NOT USE - @mli6 - temporary workaround pending release of iOS dark mode
    }

    private fun noopResetColor(view: ValdiDatePicker, animator: ValdiAnimator?) {
        // noop
        // DO NOT USE - @mli6 - temporary workaround pending release of iOS dark mode
    }

    override fun bindAttributes(attributesBindingContext: AttributesBindingContext<ValdiDatePicker>) {
        attributesBindingContext.bindFloatAttribute("dateSeconds", false, this::applyDateSeconds, this::resetDateSeconds)
        attributesBindingContext.bindFloatAttribute("minimumDateSeconds", false, this::applyMinimumDateSeconds, this::resetMinimumDateSeconds)
        attributesBindingContext.bindFloatAttribute("maximumDateSeconds", false, this::applyMaximumDateSeconds, this::resetMaximumDateSeconds)
        attributesBindingContext.bindFunctionAttribute("onChange", this::applyOnChange, this::resetOnChange)
        attributesBindingContext.bindIntAttribute("preferredStyle", true, this::applyPreferredStyle, this::resetPreferredStyle)
        attributesBindingContext.bindColorAttribute("color", false, this::noopApplyColor, this::noopResetColor)

        attributesBindingContext.setPlaceholderViewMeasureDelegate(lazy {
            ValdiDatePicker(context).apply {
                layoutParams = ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT
                )
            }
        })
    }
}
