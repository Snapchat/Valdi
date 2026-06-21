package com.snap.valdi.attributes.impl

import android.content.Context
import android.graphics.Color
import android.text.TextUtils
import android.widget.TextView
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.attributes.AttributesBindingContext
import com.snap.valdi.attributes.conversions.ColorConversions
import com.snap.valdi.attributes.impl.animations.ValdiAnimator
import com.snap.valdi.attributes.impl.gradients.ValdiGradient
import com.snap.valdi.attributes.impl.richtext.CustomUnderlineStyle
import com.snap.valdi.attributes.impl.richtext.RichTextConverter
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.attributes.impl.richtext.TextViewHelper
import com.snap.valdi.exceptions.AttributeError
import com.snap.valdi.exceptions.ValdiException
import com.snap.valdi.utils.CoordinateResolver
import com.snap.valdi.views.ValdiTextHolder
import com.snapchat.client.valdi_core.AttributeType
import com.snapchat.client.valdi_core.CompositeAttributePart

/**
 * Binds attributes for the TextView's view class
 */
class TextViewAttributesBinder(
        private val context: Context,
        private val textConverter: RichTextConverter,
        private val defaultAttributes: FontAttributes
) : AttributesBinder<TextView> {
    private val scaledDensity = context.resources.displayMetrics.scaledDensity

    private var valueAttributeId = 0

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

    private val coordinateResolver = CoordinateResolver(context)

    override val viewClass: Class<TextView>
        get() = TextView::class.java

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

    private fun getTextViewHelper(view: TextView): TextViewHelper {
        if (view !is ValdiTextHolder) {
            throw ValdiException("TextView class ${view.javaClass.name} does not implement ValdiTextHolder")
        }
        var helper = view.textViewHelper
        if (helper == null) {
            helper = TextViewHelper(view, textConverter, defaultAttributes, valueAttributeId)
            view.textViewHelper = helper
        }

        return helper
    }

    fun applyFontAttributes(view: TextView, value: Any?, animator: ValdiAnimator?) {
        getTextViewHelper(view).fontAttributes = value as? FontAttributes
    }

    fun resetFontAttributes(view: TextView, animator: ValdiAnimator?) {
        getTextViewHelper(view).fontAttributes = null
    }

    fun applyValue(view: TextView, value: Any?, animator: ValdiAnimator?) {
        getTextViewHelper(view).textValue = value
    }

    fun resetValue(view: TextView, animator: ValdiAnimator?) {
        getTextViewHelper(view).textValue = null
    }

    fun applyTextGradient(view: TextView, value: Array<Any>, animator: ValdiAnimator?) {
        getTextViewHelper(view).textGradient = ValdiGradient.fromGradientData(value)
    }

    fun resetTextGradient(view: TextView, animator: ValdiAnimator?) {
        getTextViewHelper(view).textGradient = null
    }

    fun applyTextShadow(view: TextView, value: Any?, animator: ValdiAnimator?) {
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

        // When the radius is 0 on Android, the shadow doesn't render
        // When the radius is 0 on iOS, if there's an offset, the shadow will still render
        if (radius == 0) {
            if (widthOffset.equals(0f) && heightOffset.equals(0f)) {
                resetTextShadow(view, animator)
                return
            }
            // This is the pixel equivalent of epsilon so that we get some shadow instead of no
            // shadow
            radius = 1
        }

        if (opacity < 1) {
            // If the passed in opacity is less than 1, use it instead of the color's alpha
            // for feature parity with iOS
            // Fancy bit math because the Android APIs to do this don't go far enough back
            // Color.valueOf was added in API 26 we support back to API 21
            var bitmask = 0x00ffffff
            // Shift up to opacity bits
            var shiftedOpacity = (opacity * 255).toInt() shl 24
            var clearedAlpha = color and bitmask
            color = shiftedOpacity or clearedAlpha
        }

        view.setShadowLayer(radius.toFloat(), widthOffset.toFloat(), heightOffset.toFloat(), color);
    }

    fun resetTextShadow(view: TextView, animator: ValdiAnimator?) {
        view.setShadowLayer(0f, 0f, 0f, 0);
    }

    fun applyTextOverflow(view: TextView, value: String, animator: ValdiAnimator?) {
        view.ellipsize = when (value) {
            "ellipsis" -> TextUtils.TruncateAt.END
            "clip" -> null
            else -> throw AttributeError("Invalid textOverflow value")
        }
    }

    fun resetTextOverflow(view: TextView, animator: ValdiAnimator?) {
        view.ellipsize = TextUtils.TruncateAt.END
    }

    override fun bindAttributes(attributesBindingContext: AttributesBindingContext<TextView>) {
        attributesBindingContext.bindCompositeAttribute("fontAttributes", FONT_ATTRIBUTES_PARTS, this::applyFontAttributes, this::resetFontAttributes)
        attributesBindingContext.registerPreprocessor("customUnderlineStyle", true, this::preprocessCustomUnderlineStyle)
        attributesBindingContext.registerPreprocessor("fontAttributes", true, this::preprocessFontAttributes)

        attributesBindingContext.bindTextAttribute("value", true, this::applyValue, this::resetValue)
        attributesBindingContext.bindUntypedAttribute("textShadow", false, this::applyTextShadow, this::resetTextShadow)
        attributesBindingContext.bindStringAttribute("textOverflow", true, this::applyTextOverflow, this::resetTextOverflow)
        attributesBindingContext.bindArrayAttribute("textGradient", false, this::applyTextGradient, this::resetTextGradient)

        this.valueAttributeId = attributesBindingContext.getBoundAttributeId("value")
    }

}
