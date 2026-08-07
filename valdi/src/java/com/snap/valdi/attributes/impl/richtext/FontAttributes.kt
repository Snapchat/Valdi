package com.snap.valdi.attributes.impl.richtext

import android.content.Context
import android.content.res.Resources
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Typeface
import android.text.Layout
import android.text.TextPaint
import android.text.style.AlignmentSpan
import android.text.style.BackgroundColorSpan
import android.text.style.ForegroundColorSpan
import android.text.style.MetricAffectingSpan
import android.text.style.StrikethroughSpan
import android.text.style.UnderlineSpan
import android.util.TypedValue
import com.snap.valdi.attributes.impl.fonts.FontDescriptor
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.MissingFontsTracker
import com.snap.valdi.exceptions.AttributeError

class CustomTypefaceSpan(val typeface: Typeface) : MetricAffectingSpan() {
    override fun updateDrawState(tp: TextPaint) {
        tp.typeface = typeface
    }

    override fun updateMeasureState(tp: TextPaint) {
        tp.typeface = typeface
    }
}

data class FontAttributes(
    var textDecoration: TextDecoration?,
    var customUnderlineStyle: CustomUnderlineStyle?,
    var fontSize: Float,
    var fontName: String?,
    var lineHeight: Float?,
    var lineHeightAbsolute: Float?,
    var numberOfLines: Int?,
    var letterSpacing: Float?,
    var adjustsFontSizeToFitWidth: Boolean?,
    var minimumScaleFactor: Float?,
    var color: Int,
    var backgroundColor: Int? = null,
    var alignment: TextAlignment,
    var isUnscaled: Boolean = false,
    var outlineColor: Int? = null,
    var outlineWidth: Float,
    var animationTransform: TextAnimationTransform? = null
) {
    enum class RenderMode {
        BASE,
        OVERLAY,
    }

    companion object {
        val default = FontAttributes(
            null,
            null,
            12f,
            null,
            null,
            null,
            null,
            null,
            null,
            null,
            Color.BLACK,
            null,
            TextAlignment.LEFT,
            false,
            null,
            0F,
            null)
        val buttonDefault = FontAttributes(
            null,
            null,
            12f,
            null,
            null,
            null,
            null,
            null,
            null,
            null,
            Color.BLACK,
            null,
            TextAlignment.CENTER,
            false,
            null,
            0F,
            null
        )
        private const val PX_SUFFIX = "px"
        private const val PT_SUFFIX = "pt"
        private const val POSITION_OF_DYNAMIC_TYPE = 3
    }

    fun enumerateSpans(
        fontManager: FontManager,
        missingFontsTracker: MissingFontsTracker,
        disableTextReplacement: Boolean = false,
        includeTextDecoration: Boolean = true,
        renderMode: RenderMode = RenderMode.BASE,
        suppressAnimatedBase: Boolean = false,
        closure: (Any) -> Unit
    ) {
        closure(TextSizeSpan(resolveFontSize(fontManager.context)))

        closure(AlignmentSpan.Standard(when (alignment) {
            TextAlignment.LEFT -> Layout.Alignment.ALIGN_NORMAL
            TextAlignment.CENTER -> Layout.Alignment.ALIGN_CENTER
            TextAlignment.RIGHT -> Layout.Alignment.ALIGN_OPPOSITE
            else -> Layout.Alignment.ALIGN_NORMAL
        }))

        if (includeTextDecoration) {
            createTextDecorationLayoutSpan()?.let(closure)
        }

        val typeface = resolveTypeface(fontManager, missingFontsTracker)
        if (typeface != null) {
            closure(CustomTypefaceSpan(typeface))
        }

        if (backgroundColor != null) {
            closure(BackgroundColorSpan(backgroundColor!!))
        }

        if (animationTransform != null) {
            val shouldAnimateInOverlay = isActiveAnimationTransform(animationTransform)
            if (renderMode == RenderMode.OVERLAY) {
                if (!disableTextReplacement && outlineColor != null && outlineWidth > 0) {
                    closure(OutlineReplacementSpan(color, outlineColor!!, outlineWidth))
                } else {
                    closure(ForegroundColorSpan(color))
                }
            } else if (shouldAnimateInOverlay && suppressAnimatedBase) {
                if (disableTextReplacement || outlineColor == null || outlineWidth <= 0f) {
                    closure(InvisibleForegroundColorSpan())
                } else {
                    closure(InvisibleReplacementSpan())
                }
            } else if (!disableTextReplacement && outlineColor != null && outlineWidth > 0) {
                closure(OutlineReplacementSpan(color, outlineColor!!, outlineWidth))
            } else {
                closure(ForegroundColorSpan(color))
            }
        } else if (renderMode == RenderMode.OVERLAY) {
            if (!disableTextReplacement && outlineColor != null && outlineWidth > 0) {
                closure(OutlineReplacementSpan(color, outlineColor!!, outlineWidth))
            } else {
                closure(ForegroundColorSpan(color))
            }
        } else if (!disableTextReplacement && outlineColor != null && outlineWidth > 0) {
            closure(OutlineReplacementSpan(color, outlineColor!!, outlineWidth))
        } else {
            closure(ForegroundColorSpan(color))
        }
    }

    fun requiresDrawableUnderlineSpan(): Boolean {
        return when (textDecoration) {
            TextDecoration.UNDERLINE -> customUnderlineStyle != null
            TextDecoration.DASHED_UNDERLINE,
            TextDecoration.DOTTED_UNDERLINE -> true
            else -> false
        }
    }

    fun createTextDecorationLayoutSpan(): Any? {
        return createDrawableUnderlineSpan(null) ?: when (textDecoration) {
            TextDecoration.UNDERLINE -> UnderlineSpan()
            TextDecoration.STRIKETHROUGH -> StrikethroughSpan()
            TextDecoration.NONE,
            null -> null
            TextDecoration.DASHED_UNDERLINE,
            TextDecoration.DOTTED_UNDERLINE -> null
        }
    }

    fun createDrawableUnderlineSpan(animation: AttributedTextAnimation?): PatternUnderlineSpan? {
        return when (textDecoration) {
            TextDecoration.UNDERLINE -> customUnderlineStyle?.let { CustomUnderlineSpan(it, animation) }
            TextDecoration.DASHED_UNDERLINE -> customUnderlineStyle?.let {
                CustomUnderlineSpan(it, animation)
            } ?: DashedUnderlineSpan(animation)
            TextDecoration.DOTTED_UNDERLINE -> customUnderlineStyle?.let {
                CustomUnderlineSpan(it, animation)
            } ?: DottedUnderlineSpan(animation)
            TextDecoration.STRIKETHROUGH,
            TextDecoration.NONE,
            null -> null
        }
    }

    fun applyFont(font: String) {
        // We scale all fonts by default, unless specified otherwise in the font's 3rd parameter
        isUnscaled = false

        when (font.lowercase()) {
            "title1" -> {
                fontName = "title1"
                fontSize = 25f
            }
            "title2" -> {
                fontName = "title2"
                fontSize = 19f
            }
            "title3" -> {
                fontName = "title3"
                fontSize = 17f
            }
            "body" -> {
                fontName = "body"
                fontSize = 14f
            }
            else -> {
                val fontPieces = font.split(" ")
                fontName = fontPieces[0]
                if (fontPieces.size > 1) {
                    try {
                        fontSize = fontPieces[1]
                                .removeSuffix(PX_SUFFIX)
                                .removeSuffix(PT_SUFFIX)
                               .toFloat()
                    } catch (e: NumberFormatException) {
                        throw AttributeError("Found ${fontPieces[1]}, expected float for font size")
                    }

                    // The special "unscaled" type can be used to disable text scaling
                    if (fontPieces.size >= POSITION_OF_DYNAMIC_TYPE && fontPieces[2].lowercase() == "unscaled") {
                        isUnscaled = true
                    }
                }
            }
        }
    }

    fun applyTextDecoration(attributeVal: String) {
        textDecoration = when (attributeVal) {
            "underline" -> TextDecoration.UNDERLINE
            "dashed-underline" -> TextDecoration.DASHED_UNDERLINE
            "dotted-underline" -> TextDecoration.DOTTED_UNDERLINE
            "strikethrough" -> TextDecoration.STRIKETHROUGH
            else -> TextDecoration.NONE
        }
    }

    fun applyTextAlign(attributeVal: String) {
        alignment = when (attributeVal) {
            "center" -> TextAlignment.CENTER
            "right" -> TextAlignment.RIGHT
            "justified" -> TextAlignment.JUSTIFIED
            else -> TextAlignment.LEFT
        }
    }

    fun resolveTypeface(fontManager: FontManager, missingFontsTracker: MissingFontsTracker): Typeface? {
        // TODO: (nate) support line height
        // TODO: (nate) support inline letter-spacing adjustments
        if (fontName != null) {
            val fontDescriptor = FontDescriptor(name = fontName)
            val typeface = fontManager.get(fontDescriptor)

            if (typeface == null) {
                missingFontsTracker.onFontMissing(fontDescriptor)
            }

            return typeface
        }
        return null
    }

    // the font size of zero will cause a load of possible errors and crash in android, so we clamp it
    val resolvedFontSizeValue: Float
        get() {
            return Math.max(fontSize, 1f)
        }

    // the font unit will depends on wether or not we want to dynamically scale our labels (DIP vs SP)
    fun resolveFontSizeUnit(): Int {
        return if (!isUnscaled) {
            TypedValue.COMPLEX_UNIT_SP
        } else {
            TypedValue.COMPLEX_UNIT_DIP
        }
    }

    private fun resolveFontSize(context: Context): Float {
        val resources = context.resources ?: Resources.getSystem()
        return TypedValue.applyDimension(
            resolveFontSizeUnit(),
            fontSize,
            resources.displayMetrics
        )
    }

    /**
     * Creates the matching Paint object for a given FontAttributes
     * - we choose not to include alignment here, since positioning should be dependent on the layout
     */
    /**
     * Valdi's `letterSpacing` is an absolute size, while [Paint.letterSpacing] is in ems, so it has to
     * be divided by the resolved text size — the same conversion `TextViewHelper` applies when setting
     * it on the view. Omitting it entirely made these paints disagree with the layout's own paint:
     * `drawOnTop` strokes the outline over text Android laid out with letter spacing applied, so the
     * outline drifted further from the glyphs the longer the run, and `AnimatedTextReplacementSpan`
     * measured its width with the layout paint but drew with this one.
     */
    private fun Paint.applyLetterSpacing(resolvedTextSize: Float) {
        val spacing = this@FontAttributes.letterSpacing ?: return
        if (resolvedTextSize <= 0f) {
            return
        }
        letterSpacing = spacing / resolvedTextSize
    }

    fun toPaint(fontManager: FontManager, missingFontsTracker: MissingFontsTracker): Paint {
        val resolvedTextSize = resolveFontSize(fontManager.context)
        return Paint(Paint.ANTI_ALIAS_FLAG).apply {
            textSize = resolvedTextSize
            typeface = resolveTypeface(fontManager, missingFontsTracker)
            color = outlineColor ?: Color.TRANSPARENT
            strokeWidth = outlineWidth ?: 0f
            isUnderlineText = textDecoration == TextDecoration.UNDERLINE && customUnderlineStyle == null
            isStrikeThruText = textDecoration == TextDecoration.STRIKETHROUGH
            style = Paint.Style.STROKE
            applyLetterSpacing(resolvedTextSize)
        }
    }

    fun toFillPaint(fontManager: FontManager, missingFontsTracker: MissingFontsTracker): Paint {
        val resolvedTextSize = resolveFontSize(fontManager.context)
        return Paint(Paint.ANTI_ALIAS_FLAG).apply {
            textSize = resolvedTextSize
            typeface = resolveTypeface(fontManager, missingFontsTracker)
            color = this@FontAttributes.color
            isUnderlineText = textDecoration == TextDecoration.UNDERLINE && customUnderlineStyle == null
            isStrikeThruText = textDecoration == TextDecoration.STRIKETHROUGH
            style = Paint.Style.FILL
            applyLetterSpacing(resolvedTextSize)
        }
    }
}
