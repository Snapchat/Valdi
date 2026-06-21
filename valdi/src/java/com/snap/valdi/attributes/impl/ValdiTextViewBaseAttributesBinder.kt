package com.snap.valdi.attributes.impl

import android.content.Context
import android.graphics.Color
import android.text.TextUtils
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.attributes.AttributesBindingContext
import com.snap.valdi.attributes.conversions.ColorConversions
import com.snap.valdi.attributes.impl.animations.ValdiAnimator
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.gradients.ValdiGradient
import com.snap.valdi.attributes.impl.richtext.CustomUnderlineStyle
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.attributes.impl.richtext.TextViewHelper
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.exceptions.AttributeError
import com.snap.valdi.utils.CoordinateResolver
import com.snap.valdi.views.ValdiTextSelection
import com.snap.valdi.views.ValdiTextViewBase
import com.snapchat.client.valdi_core.AttributeType
import com.snapchat.client.valdi_core.CompositeAttributePart
import kotlin.math.roundToInt

/**
 * Attribute binder for properties shared by all Android text controls backed by
 * ValdiTextViewBase.
 *
 * Registering this binder against the base class keeps label, textview, and
 * textfield text behavior in one place: attributed text parsing, font
 * attributes, selection, shadows, gradients, overflow, and shared callbacks.
 */
class ValdiTextViewBaseAttributesBinder(
    context: Context,
    private val fontManager: FontManager,
    private val defaultAttributes: FontAttributes,
) : AttributesBinder<ValdiTextViewBase> {
    private val coordinateResolver = CoordinateResolver(context)
    private var valueAttributeId = 0

    override val viewClass: Class<ValdiTextViewBase>
        get() = ValdiTextViewBase::class.java

    companion object {
        val FONT_ATTRIBUTES_PARTS = arrayListOf(
            CompositeAttributePart("color", AttributeType.COLOR, true, false),
            CompositeAttributePart("textDecoration", AttributeType.STRING, true, false),
            CompositeAttributePart("textAlign", AttributeType.STRING, true, false),
            CompositeAttributePart("font", AttributeType.STRING, true, true),
            CompositeAttributePart("lineHeightMultiple", AttributeType.DOUBLE, true, true),
            CompositeAttributePart("lineHeight", AttributeType.DOUBLE, true, true),
            CompositeAttributePart("numberOfLines", AttributeType.DOUBLE, true, true),
            CompositeAttributePart("letterSpacing", AttributeType.DOUBLE, true, true),
            CompositeAttributePart("adjustsFontSizeToFitWidth", AttributeType.BOOLEAN, true, false),
            CompositeAttributePart("minimumScaleFactor", AttributeType.DOUBLE, true, false),
            CompositeAttributePart("customUnderlineStyle", AttributeType.STRING, true, false),
        )
    }

    fun preprocessFontAttributes(values: Any?): Any {
        val valuesArray = values as? Array<*> ?: throw AttributeError("Expecting array for spannable string")

        val color = valuesArray[0] as? Long
        val textDecoration = valuesArray[1] as? String
        val textAlign = valuesArray[2] as? String
        val font = valuesArray[3] as? String
        val lineHeightMultiple = valuesArray[4] as? Double
        val lineHeight = valuesArray[5] as? Double
        val numberOfLines = valuesArray[6] as? Double
        val letterSpacing = valuesArray[7] as? Double
        val adjustsFontSizeToFitWidth = valuesArray[8] as? Boolean
        val minimumScaleFactor = valuesArray[9] as? Double
        val customUnderlineStyle = when (val rawCustomUnderlineStyle = valuesArray[10]) {
            is CustomUnderlineStyle -> rawCustomUnderlineStyle
            is String -> CustomUnderlineStyle.parse(rawCustomUnderlineStyle)
            else -> null
        }

        val attributes = defaultAttributes.copy()
        if (color != null) {
            attributes.color = ColorConversions.fromRGBA(color)
        }
        if (textDecoration != null) {
            attributes.applyTextDecoration(textDecoration)
        }
        if (textAlign != null) {
            attributes.applyTextAlign(textAlign)
        }
        if (font != null) {
            attributes.applyFont(font)
        }

        attributes.lineHeightMultiple = lineHeightMultiple?.toFloat()
        attributes.lineHeight = lineHeight?.toFloat()
        attributes.numberOfLines = numberOfLines?.toInt()
        attributes.letterSpacing = letterSpacing?.toFloat()
        attributes.adjustsFontSizeToFitWidth = adjustsFontSizeToFitWidth
        attributes.minimumScaleFactor = minimumScaleFactor?.toFloat()
        attributes.customUnderlineStyle = customUnderlineStyle
        return attributes
    }

    fun preprocessCustomUnderlineStyle(value: Any?): Any {
        val styleString = value as? String ?: throw AttributeError("customUnderlineStyle must be a string")
        return CustomUnderlineStyle.parse(styleString)
    }

    private fun getTextViewHelper(view: ValdiTextViewBase): TextViewHelper {
        return view.getOrCreateTextViewHelper(fontManager, defaultAttributes, valueAttributeId)
    }

    fun applyFontAttributes(view: ValdiTextViewBase, value: Any?, animator: ValdiAnimator?) {
        getTextViewHelper(view).fontAttributes = value as? FontAttributes
    }

    fun resetFontAttributes(view: ValdiTextViewBase, animator: ValdiAnimator?) {
        getTextViewHelper(view).fontAttributes = null
    }

    fun applyValue(view: ValdiTextViewBase, value: Any?, animator: ValdiAnimator?) {
        getTextViewHelper(view).textValue = value
    }

    fun resetValue(view: ValdiTextViewBase, animator: ValdiAnimator?) {
        getTextViewHelper(view).textValue = null
    }

    fun applyTextGradient(view: ValdiTextViewBase, value: Array<Any>, animator: ValdiAnimator?) {
        getTextViewHelper(view).textGradient = ValdiGradient.fromGradientData(value)
    }

    fun resetTextGradient(view: ValdiTextViewBase, animator: ValdiAnimator?) {
        getTextViewHelper(view).textGradient = null
    }

    fun applyTextShadow(view: ValdiTextViewBase, value: Any?, animator: ValdiAnimator?) {
        if (value !is Array<*>) {
            resetTextShadow(view, animator)
            return
        }

        if (value.size < 5) {
            throw AttributeError("textShadow components should have 5 entries")
        }

        var color = ColorConversions.fromRGBA(value[0] as? Long ?: 0)
        var radius = coordinateResolver.toPixel((value[1] as? Double ?: 0.0))
        val opacity = value[2] as? Double ?: 0.0
        val widthOffset = coordinateResolver.toPixel(value[3] as? Double ?: 0.0)
        val heightOffset = coordinateResolver.toPixel(value[4] as? Double ?: 0.0)

        if (radius == 0) {
            if (widthOffset.equals(0f) && heightOffset.equals(0f)) {
                resetTextShadow(view, animator)
                return
            }
            radius = 1
        }

        if (opacity < 1) {
            val bitmask = 0x00ffffff
            val shiftedOpacity = (opacity * 255).toInt() shl 24
            val clearedAlpha = color and bitmask
            color = shiftedOpacity or clearedAlpha
        }

        view.backingTextView.setShadowLayer(radius.toFloat(), widthOffset.toFloat(), heightOffset.toFloat(), color)
    }

    fun resetTextShadow(view: ValdiTextViewBase, animator: ValdiAnimator?) {
        view.backingTextView.setShadowLayer(0f, 0f, 0f, 0)
    }

    fun applyTextOverflow(view: ValdiTextViewBase, value: String, animator: ValdiAnimator?) {
        view.backingTextView.ellipsize = when (value) {
            "ellipsis" -> TextUtils.TruncateAt.END
            "clip" -> null
            else -> throw AttributeError("Invalid textOverflow value")
        }
    }

    fun resetTextOverflow(view: ValdiTextViewBase, animator: ValdiAnimator?) {
        view.backingTextView.ellipsize = TextUtils.TruncateAt.END
    }

    fun applySelectable(view: ValdiTextViewBase, value: Boolean, animator: ValdiAnimator?) {
        view.setValdiSelectable(value)
    }

    fun resetSelectable(view: ValdiTextViewBase, animator: ValdiAnimator?) {
        applySelectable(view, false, animator)
    }

    fun applySelection(view: ValdiTextViewBase, selection: Any?, animator: ValdiAnimator?) {
        if (selection !is Array<*>) {
            resetSelection(view, animator)
            return
        }
        if (selection.size != ValdiTextSelection.EXPECTED_SELECTION_DATA_SIZE) {
            throw AttributeError("Selection should have two values in the given array: start + end")
        }
        val start = (selection[0] as? Double)?.roundToInt() ?: 0
        val end = (selection[1] as? Double)?.roundToInt() ?: 0
        getTextViewHelper(view).selection = Pair(start, end)
    }

    fun resetSelection(view: ValdiTextViewBase, animator: ValdiAnimator?) {
        view.setValdiSelection(0, 0)
    }

    fun applyOnSelectionChange(view: ValdiTextViewBase, action: ValdiFunction) {
        view.onSelectionChangeFunction = action
    }

    fun resetOnSelectionChange(view: ValdiTextViewBase) {
        view.onSelectionChangeFunction = null
    }

    override fun bindAttributes(attributesBindingContext: AttributesBindingContext<ValdiTextViewBase>) {
        attributesBindingContext.bindCompositeAttribute("fontAttributes", FONT_ATTRIBUTES_PARTS, this::applyFontAttributes, this::resetFontAttributes)
        attributesBindingContext.registerPreprocessor("customUnderlineStyle", true, this::preprocessCustomUnderlineStyle)
        attributesBindingContext.registerPreprocessor("fontAttributes", true, this::preprocessFontAttributes)

        attributesBindingContext.bindTextAttribute("value", true, this::applyValue, this::resetValue)
        attributesBindingContext.bindUntypedAttribute("textShadow", false, this::applyTextShadow, this::resetTextShadow)
        attributesBindingContext.bindStringAttribute("textOverflow", true, this::applyTextOverflow, this::resetTextOverflow)
        attributesBindingContext.bindArrayAttribute("textGradient", false, this::applyTextGradient, this::resetTextGradient)
        attributesBindingContext.bindBooleanAttribute("selectable", false, this::applySelectable, this::resetSelectable)
        attributesBindingContext.bindUntypedAttribute("selection", false, this::applySelection, this::resetSelection)
        attributesBindingContext.bindFunctionAttribute("onSelectionChange", this::applyOnSelectionChange, this::resetOnSelectionChange)
        valueAttributeId = attributesBindingContext.getBoundAttributeId("value")
    }
}
