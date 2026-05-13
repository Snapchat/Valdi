package com.snap.valdi.attributes.impl.richtext

import android.graphics.Canvas
import android.graphics.LinearGradient
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.PointF
import android.graphics.RadialGradient
import android.graphics.Shader
import android.graphics.Typeface
import android.os.Build
import android.text.Layout
import android.text.Spannable
import android.text.SpannableString
import android.text.Spanned
import android.util.Size
import android.widget.TextView
import androidx.core.widget.TextViewCompat
import com.snap.valdi.attributes.impl.fonts.FontDescriptor
import com.snap.valdi.attributes.impl.fonts.MissingFontsTracker
import com.snap.valdi.attributes.impl.gradients.ValdiGradient
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.utils.CoordinateResolver
import com.snap.valdi.utils.Disposable
import com.snap.valdi.utils.LoadCompletion
import com.snap.valdi.utils.runOnMainThreadIfNeeded
import com.snap.valdi.views.ValdiEditText
import com.snap.valdi.views.TextViewUtils
import com.snap.valdi.views.ValdiTextView
import com.snap.valdi.views.touches.AttributedTextTapGestureRecognizer
import kotlin.math.max

class TextViewHelper(private val view: TextView,
                     private val textConverter: RichTextConverter,
                     private val defaultAttributes: FontAttributes,
                     private val valueAttributeId: Int) : MissingFontsTracker {

    companion object {
        private val fontMetrics: Paint.FontMetrics = Paint.FontMetrics()
        var lastMeasuredText: CharSequence? = null
        var lastMeasuredFontAttributes: FontAttributes? = null

        const val GRADIENT_TOP_BOTTOM = 0
        const val GRADIENT_TR_BL = 1
        const val GRADIENT_RIGHT_LEFT = 2
        const val GRADIENT_BR_TL = 3
        const val GRADIENT_BOTTOM_TOP = 4
        const val GRADIENT_BL_TR = 5
        const val GRADIENT_LEFT_RIGHT = 6
        const val GRADIENT_TL_BR = 7
        private const val IMAGE_ATTACHMENT_BREAK_CHAR = '\u2009'

        fun isTextValueEqual(
            valdiText: Any?,
            viewText: CharSequence,
            disableTextReplacement: Boolean = false,
        ): Boolean {
            return when (valdiText) {
                is String -> valdiText == viewText.toString()
                is AttributedText -> viewText.contentEquals(buildComparableTextValue(valdiText, disableTextReplacement))
                else -> false
            }
        }

        private fun buildComparableTextValue(
            valdiText: AttributedText,
            disableTextReplacement: Boolean,
        ): String {
            val builder = StringBuilder()
            for (index in 0 until valdiText.getPartsSize()) {
                builder.append(valdiText.getContentAtIndex(index))
                if (valdiText.getImageAttachmentAtIndex(index) != null && !disableTextReplacement) {
                    builder.append(IMAGE_ATTACHMENT_BREAK_CHAR)
                }
            }
            return builder.toString()
        }
    }

    private val coordinateResolver = CoordinateResolver(view.context)

    /**
     * In some cases, we want the number of lines to not be manipulated through the normal font composite attribute
     * This is to avoid conflicts when a view uses another attribute that conflicts and does not uses "numberOfLines"
     */
    var managesNumberOfLines = true

    /**
     * For TextViews that don't support selection, we allow text replacement by default
     * For EditText where selection is paramount, we disable text replacement in spannables, and draw an outline on top as needed
     */
    var disableTextReplacement = false

    var fontAttributes: FontAttributes? = null
        set(value) {
            if (field != value) {
                field = value
                fontAttributesDirty = true
                fontAutofitDirty = true
                onDirty()
            }
        }

    var textValue: Any? = null
        set(value) {
            val shouldUpdate = when (value) {
                is AttributedText -> {
                    val isSameTextValue = field == value
                    val hasAnimationTransform =
                        if (isSameTextValue) textValueHasAnimationTransform else value.hasAnimationTransform()
                    val comparableTextValue = if (
                        isSameTextValue &&
                        textValueComparableDisableTextReplacement == disableTextReplacement
                    ) {
                        textValueComparableText
                    } else {
                        buildComparableTextValue(value, disableTextReplacement).also {
                            textValueComparableText = it
                            textValueComparableDisableTextReplacement = disableTextReplacement
                        }
                    }
                    if (!isSameTextValue) {
                        textValueHasAnimationTransform = hasAnimationTransform
                    }
                    hasAnimationTransform || !isSameTextValue || !view.text.contentEquals(comparableTextValue)
                }
                else ->
                    field != value ||
                        // textValue can get out of sync with the view so we may need to compare them
                        !isTextValueEqual(value, view.text, disableTextReplacement)
            }
            if (shouldUpdate) {
                field = value
                textValueDirty = true
                isAttributedText = value is AttributedText
                if (value !is AttributedText) {
                    textValueHasAnimationTransform = false
                    textValueComparableText = null
                    textValueComparableDisableTextReplacement = false
                }
                onDirty()
            }
        }

    var textGradient: ValdiGradient? = null
        set(value) {
            if (field != value) {
                field = value
                textGradientDirty = true
                onDirty()
            }
        }

    var selection: Pair<Int, Int>? = null
        set(value) {
            if (field != value) {
                field = value
                selectionDirty = true
                onDirty()
            }
        }
    private var selectionDirty = false

    private var textValueHasAnimationTransform = false
    private var textValueComparableText: String? = null
    private var textValueComparableDisableTextReplacement = false
    private var fontAttributesDirty = true
    private var fontAutofitDirty = true

    private var textValueDirty = false
    private var isAttributedText = false

    private var needsUpdateOnLayoutCallbacks = false

    private var textGradientDirty = false
    private lateinit var initialGradientSize: Size

    private var fontLoadDisposables: MutableMap<FontDescriptor, Disposable>? = null
    private var overlayAttributedTextSpannable: Spannable? = null
    private var parsedAttributedTextSource: AttributedText? = null
    private var parsedAttributedText: RichTextConverter.ParsedAttributedText? = null
    private var overlayLayoutCache: RichTextConverter.OverlayLayoutCache? = null
    private var attributedTextShapeSignature: String? = null
    private var hadActiveAnimationTransform: Boolean = false

    fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        updateTextAttributes()
        TextViewHelper.lastMeasuredText = view.text
        TextViewHelper.lastMeasuredFontAttributes = fontAttributes
    }

    fun onLayout(changed: Boolean) {
        updateTextAttributes()
        updateTextAutofit()
        updateTextGradient(changed)
        updateOnLayoutCallbacks()
    }

    private fun updateOnLayoutCallbacks() {
        // TODO(3065): Also update on view size changed
        if (view.getText() !is Spanned || !needsUpdateOnLayoutCallbacks) {
            return
        }
        val layout = view.getLayout() ?: return
        val layoutText = layout.text as? Spanned ?: return

        val attributeLayoutSpans = layoutText.getSpans(
            0, layoutText.length,
            OnLayoutSpan::class.java
        )

        for (span in attributeLayoutSpans) {
            val start = layoutText.getSpanStart(span)
            val end = layoutText.getSpanEnd(span)

            if (start < 0 || end < 0 || start > end || end > layoutText.length) {
                continue
            }

            val lineStart = layout.getLineForOffset(start)
            val xStart = coordinateResolver.fromPixel(layout.getPrimaryHorizontal(start))
            val yStart = coordinateResolver.fromPixel(layout.getLineTop(lineStart).toDouble())

            val lineEnd = layout.getLineForOffset(end)
            val xEnd = coordinateResolver.fromPixel(layout.getPrimaryHorizontal(end))
            val yEnd = coordinateResolver.fromPixel(layout.getLineBottom(lineEnd).toDouble())

            // TODO(3944): Add support for multiline text
            span.onLayout(xStart.toDouble(), yStart.toDouble(), (xEnd - xStart).toDouble(), (yEnd - yStart).toDouble())
        }
        needsUpdateOnLayoutCallbacks = false
    }

    private fun updateTextAttributes() {
        if (isAttributedText) {
            if (fontAttributesDirty || textValueDirty) {
                fontAttributesDirty = false
                textValueDirty = false
                applyFontAttributes(fontAttributes ?: defaultAttributes)
                applyAttributedText(textValue as AttributedText)
            }
        } else {
            if (fontAttributesDirty) {
                fontAttributesDirty = false
                applyFontAttributes(fontAttributes ?: defaultAttributes)
            }

            if (textValueDirty) {
                textValueDirty = false
                applyTextSimple(textValue as? String)
            }
        }

        if (view is ValdiEditText && selectionDirty) {
            selectionDirty = false
            selection?.let { (first, second) ->
                view.setSelectionClamped(first, second)
            }
        }
    }

    private fun updateTextAutofit() {
        if (fontAutofitDirty) {
            fontAutofitDirty = false
            applyFontAutofit(fontAttributes ?: defaultAttributes)
        }
    }

    private fun updateTextGradient(forceUpdate: Boolean = false) {
        if (textGradientDirty || (forceUpdate && textGradient != null)) {
            textGradientDirty = false
            applyTextGradient(textGradient)
        }
    }

    private fun onDirty() {
        if (!view.isLayoutRequested) {
            view.requestLayout()
        }
    }

    private fun applyTextSimple(text: String?) {
        overlayAttributedTextSpannable = null
        parsedAttributedText = null
        clearOverlayLayoutCache()
        attributedTextShapeSignature = null
        if (view is ValdiTextView) {
            view.clearAttributedText()
        }
        if (view is ValdiEditText) {
            view.setTextAndSelection(text ?: "")
        } else {
            view.text = text
        }
    }

    private fun removeAttributedTextTapGestureRecognizer() {
        val gestureRecognizers = ViewUtils.getGestureRecognizers(this.view) ?: return
        gestureRecognizers.removeGestureRecognizer(AttributedTextTapGestureRecognizer::class.java)
    }

    private fun addAttributedTextTapGestureRecognizer(spannable: Spannable) {
        val gestureRecognizers = ViewUtils.getOrCreateGestureRecognizers(this.view)
        var attributedTextTapGestureRecognizer = gestureRecognizers
                .getGestureRecognizer(AttributedTextTapGestureRecognizer::class.java)
        if (attributedTextTapGestureRecognizer == null) {
            attributedTextTapGestureRecognizer = AttributedTextTapGestureRecognizer(this.view)
            gestureRecognizers.addGestureRecognizer(attributedTextTapGestureRecognizer)
        }

        attributedTextTapGestureRecognizer.spannable = spannable
    }

    fun convertAttributedText(text: AttributedText): Spannable {
        val fontAttributes = this.fontAttributes ?: defaultAttributes
        val density = view.resources.displayMetrics.density
        return textConverter.convert(
            attributedText = text,
            startingAttributes = fontAttributes,
            missingFontsTracker = this,
            disableTextReplacement = disableTextReplacement,
            suppressAnimatedBase = false,
            renderMode = FontAttributes.RenderMode.BASE,
            density = density,
        )
    }

    fun convertOverlayAttributedText(text: AttributedText): Spannable {
        val fontAttributes = this.fontAttributes ?: defaultAttributes
        val density = view.resources.displayMetrics.density
        return textConverter.convert(
            text,
            fontAttributes,
            this,
            disableTextReplacement = disableTextReplacement,
            suppressAnimatedBase = false,
            renderMode = FontAttributes.RenderMode.OVERLAY,
            density = density,
        )
    }

    fun drawOnTopAttributedText(canvas: Canvas, layout: Layout, text: AttributedText): Boolean {
        val overlaySpannable = overlayAttributedTextSpannable ?: convertOverlayAttributedText(text).also {
            overlayAttributedTextSpannable = it
        }
        val parsedText = if (parsedAttributedTextSource === text) {
            parsedAttributedText
        } else {
            null
        } ?: textConverter.parseAttributedText(text, this.fontAttributes ?: defaultAttributes).also {
            parsedAttributedTextSource = text
            parsedAttributedText = it
        }
        val cachedOverlayLayout = overlayLayoutCache
        val currentBreakStrategy = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) view.breakStrategy else 0
        val currentHyphenationFrequency = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) view.hyphenationFrequency else 0
        val currentJustificationMode = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) view.justificationMode else 0
        val overlayLayout = if (cachedOverlayLayout != null &&
            cachedOverlayLayout.overlaySpannable === overlaySpannable &&
            cachedOverlayLayout.width == layout.width &&
            cachedOverlayLayout.alignment == layout.alignment &&
            cachedOverlayLayout.lineSpacingExtra == view.lineSpacingExtra &&
            cachedOverlayLayout.lineSpacingMultiplier == view.lineSpacingMultiplier &&
            cachedOverlayLayout.includeFontPadding == view.includeFontPadding &&
            cachedOverlayLayout.breakStrategy == currentBreakStrategy &&
            cachedOverlayLayout.hyphenationFrequency == currentHyphenationFrequency &&
            cachedOverlayLayout.justificationMode == currentJustificationMode &&
            cachedOverlayLayout.textDirection == TextViewUtils.resolveTextDirectionHeuristic(view)
        ) {
            cachedOverlayLayout
        } else {
            textConverter.buildOverlayLayoutCache(layout, view, overlaySpannable)?.also {
                if (cachedOverlayLayout !== it) {
                    cachedOverlayLayout?.recycle()
                }
                overlayLayoutCache = it
            }
        } ?: return false

        textConverter.drawOnTop(canvas, layout, overlayLayout, view, parsedText, this)
        return true
    }

    fun forceRebindAttributedText(text: AttributedText) {
        val fontAttributes = this.fontAttributes ?: defaultAttributes
        forceRebindAttributedText(text, buildAttributedTextShapeSignature(text, fontAttributes))
    }

    private fun applyAttributedText(text: AttributedText) {
        val fontAttributes = this.fontAttributes ?: defaultAttributes
        val nextShapeSignature = buildAttributedTextShapeSignature(text, fontAttributes)
        val hasActiveAnimation = text.hasActiveAnimationTransform()

        if (attributedTextShapeSignature == nextShapeSignature &&
            hadActiveAnimationTransform == hasActiveAnimation
        ) {
            if (view is ValdiEditText) {
                view.updateAttributedText(text)
            } else if (view is ValdiTextView) {
                if (!view.updateAttributedText(text)) {
                    forceRebindAttributedText(text, nextShapeSignature)
                    return
                }
            }
            refreshAnimatedBaseVisibilitySpans(text, hasActiveAnimation)
        } else {
            forceRebindAttributedText(text, nextShapeSignature)
        }
    }

    private fun forceRebindAttributedText(text: AttributedText, shapeSignature: String) {
        parsedAttributedTextSource = text
        parsedAttributedText = textConverter.parseAttributedText(text, this.fontAttributes ?: defaultAttributes)
        val spannable = convertAttributedText(text)
        overlayAttributedTextSpannable = null
        clearOverlayLayoutCache()
        attributedTextShapeSignature = shapeSignature
        val hasActiveAnimation = text.hasActiveAnimationTransform()
        hadActiveAnimationTransform = hasActiveAnimation
        if (view is ValdiEditText) {
            view.setTextAndSelection(text, spannable)
        } else if (view is ValdiTextView) {
            view.setAttributedText(text, spannable)
        } else {
            view.text = SpannableString(spannable)
        }
        refreshAnimatedBaseVisibilitySpans(text, hasActiveAnimation)

        needsUpdateOnLayoutCallbacks = true

        val spans = spannable.getSpans(0, spannable.length, OnTapSpan::class.java)
        if (spans.isNullOrEmpty()) {
            removeAttributedTextTapGestureRecognizer()
        } else {
            addAttributedTextTapGestureRecognizer(spannable)
        }
    }

    private fun buildAttributedTextShapeSignature(text: AttributedText, fontAttributes: FontAttributes): String {
        return buildString {
            append(fontAttributes.fontName).append('|')
            append(fontAttributes.fontSize).append('|')
            append(fontAttributes.lineHeight).append('|')
            append(fontAttributes.letterSpacing).append('|')
            append(fontAttributes.color).append('|')
            append(fontAttributes.outlineColor).append('|')
            append(fontAttributes.outlineWidth).append('|')
            append(fontAttributes.textDecoration).append('|')
            append(fontAttributes.alignment).append('|')
            append(fontAttributes.numberOfLines).append('|')
            append(fontAttributes.isUnscaled).append('|')

            val partsSize = text.getPartsSize()
            append(partsSize).append('|')
            for (index in 0 until partsSize) {
                // The fast path is only shape-based. Closures and attachments are compared by identity
                // because animated frame updates reuse the rendered content and only mutate transform values.
                append(text.getContentAtIndex(index)).append('|')
                append(text.getFontAtIndex(index)).append('|')
                append(text.getColorAtIndex(index)).append('|')
                append(text.getOutlineColorAtIndex(index)).append('|')
                append(text.getOutlineWidthAtIndex(index)).append('|')
                append(text.getTextDecorationAtIndex(index)).append('|')
                append(text.getImageAttachmentAtIndex(index)?.let { System.identityHashCode(it) }).append('|')
                append(text.getOnTapAtIndex(index)?.let { System.identityHashCode(it) }).append('|')
                append(text.getOnLayoutAtIndex(index)?.let { System.identityHashCode(it) }).append('|')
                append(text.getAnimationTransformAtIndex(index) != null).append('|')
            }
        }
    }

    private fun refreshAnimatedBaseVisibilitySpans(text: AttributedText, hasActiveAnimation: Boolean) {
        if (view !is ValdiEditText || !hasActiveAnimation) {
            clearAnimatedBaseVisibilitySpans()
            return
        }

        val spannable = view.text as? Spannable ?: return
        val desiredRanges = mutableListOf<Triple<Int, Int, Boolean>>()
        val spannableLength = spannable.length
        val effectiveAttributes = parsedAttributedText?.attributes
        val partRangesByIndex = spannable
            .getSpans(0, spannableLength, RichTextConverter.PartIndexSpan::class.java)
            .associate { span ->
                span.partIndex to Pair(spannable.getSpanStart(span), spannable.getSpanEnd(span))
            }
        var fallbackStart = 0
        for (index in 0 until text.getPartsSize()) {
            val fallbackEnd = fallbackStart + text.getContentAtIndex(index).length
            val (rangeStart, rangeEnd) = partRangesByIndex[index]
                ?: Pair(fallbackStart, fallbackEnd)
            val clampedStart = rangeStart.coerceAtMost(spannableLength)
            val clampedEnd = rangeEnd.coerceAtMost(spannableLength)
            if (clampedStart < clampedEnd && isActiveAnimationTransform(text.getAnimationTransformAtIndex(index))) {
                val outlineColor = effectiveAttributes?.getOrNull(index)?.outlineColor ?: text.getOutlineColorAtIndex(index)
                val outlineWidth = effectiveAttributes?.getOrNull(index)?.outlineWidth ?: text.getOutlineWidthAtIndex(index)
                val usesReplacementSpan =
                    !disableTextReplacement &&
                    outlineColor != null &&
                    outlineWidth > 0f
                desiredRanges.add(Triple(clampedStart, clampedEnd, usesReplacementSpan))
            }
            fallbackStart = fallbackEnd +
                if (text.getImageAttachmentAtIndex(index) != null && !disableTextReplacement) 1 else 0
        }

        val existingRanges = buildAnimatedBaseVisibilitySpanRanges(spannable)
            .sortedWith(compareBy<Triple<Int, Int, Boolean>> { it.first }.thenBy { it.second }.thenBy { it.third })

        if (existingRanges == desiredRanges) {
            return
        }

        clearAnimatedBaseVisibilitySpans(spannable)
        desiredRanges.forEach { (rangeStart, rangeEnd, usesReplacementSpan) ->
            val span = if (usesReplacementSpan) InvisibleReplacementSpan() else InvisibleForegroundColorSpan()
            spannable.setSpan(span, rangeStart, rangeEnd, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
        }
    }

    private fun clearAnimatedBaseVisibilitySpans(existingSpannable: Spannable? = view.text as? Spannable) {
        val spannable = existingSpannable ?: return
        spannable.getSpans(0, spannable.length, InvisibleReplacementSpan::class.java).forEach { span ->
            spannable.removeSpan(span)
        }
        spannable.getSpans(0, spannable.length, InvisibleForegroundColorSpan::class.java).forEach { span ->
            spannable.removeSpan(span)
        }
    }

    private fun buildAnimatedBaseVisibilitySpanRanges(spannable: Spannable): List<Triple<Int, Int, Boolean>> {
        val replacementRanges = spannable
            .getSpans(0, spannable.length, InvisibleReplacementSpan::class.java)
            .map { Triple(spannable.getSpanStart(it), spannable.getSpanEnd(it), true) }
        val transparentRanges = spannable
            .getSpans(0, spannable.length, InvisibleForegroundColorSpan::class.java)
            .map { Triple(spannable.getSpanStart(it), spannable.getSpanEnd(it), false) }
        return replacementRanges + transparentRanges
    }

    internal fun clearOverlayLayoutCache() {
        overlayLayoutCache?.recycle()
        overlayLayoutCache = null
    }

    // Apply all known specified text rendering attributes
    private fun applyFontAttributes(attributes: FontAttributes) {
        view.typeface = attributes.resolveTypeface(textConverter.fontManager, this)
        view.setTextColor(attributes.color) // TODO - this could be its own attribute instead
        applyLetterSpacing(attributes)
        applyTextSize(attributes)
        applyLineHeight(attributes)
        applyNumberOfLines(attributes)
        applyTextAlignment(attributes)
        applyPaintFlags(attributes)
    }

    // After measuring the text, we can optionally apply some auto-fitting fields
    private fun applyFontAutofit(attributes: FontAttributes) {
        // Apply text-shrinking behavior if enabled and needed
        val adjustsFontSizeToFitWidth = attributes.adjustsFontSizeToFitWidth ?: false
        val numberOfLines = attributes.numberOfLines ?: 1
        if (adjustsFontSizeToFitWidth && numberOfLines > 0) {
            applyTextShrinking(attributes)
            applyLineHeight(attributes)
        }
    }

    private fun createRadialGradient(
        textWidth: Float,
        textHeight: Float,
        valdiGradient: ValdiGradient
    ): RadialGradient {
        val centerPoint = PointF(textWidth / 2, textHeight / 2)
        val radius = max(textWidth, textHeight) / 2

        return RadialGradient(
            centerPoint.x, centerPoint.y, radius,
            valdiGradient.colors, valdiGradient.locations,
            Shader.TileMode.CLAMP)
    }

    private fun createLinearGradient(
        textWidth: Float,
        textHeight: Float,
        valdiGradient: ValdiGradient
    ): LinearGradient {
        var startP = PointF(0f, 0f)
        var endP = PointF(0f, textHeight)

        when (valdiGradient.orientation) {
            GRADIENT_TOP_BOTTOM -> {
                startP = PointF(0f, 0f)
                endP = PointF(0f, textHeight)
            }

            GRADIENT_TR_BL -> {
                startP = PointF(textWidth, 0f)
                endP = PointF(0f, textHeight)
            }

            GRADIENT_RIGHT_LEFT -> {
                startP = PointF(textWidth, 0f)
                endP = PointF(0f, 0f)
            }

            GRADIENT_BR_TL -> {
                startP = PointF(textWidth, textHeight)
                endP = PointF(0f, 0f)
            }

            GRADIENT_BOTTOM_TOP -> {
                startP = PointF(0f, textHeight)
                endP = PointF(0f, 0f)
            }

            GRADIENT_BL_TR -> {
                startP = PointF(0f, textHeight)
                endP = PointF(textWidth, 0f)
            }

            GRADIENT_LEFT_RIGHT -> {
                startP = PointF(0f, 0f)
                endP = PointF(textWidth, 0f)
            }

            GRADIENT_TL_BR -> {
                startP = PointF(0f, 0f)
                endP = PointF(textWidth, textHeight)
            }
        }

        return LinearGradient(
            startP.x, startP.y, endP.x, endP.y,
            valdiGradient.colors, valdiGradient.locations,
            Shader.TileMode.CLAMP)
    }

    private fun applyTextGradient(textGradient: ValdiGradient?) {
        if (textGradient == null || textGradient.colors.size <= 1) {
            view.paint.shader = null
            return
        }

        if (view.paint.shader == null) {
            val shader = if (textGradient.gradientType == ValdiGradient.GradientType.RADIAL) {
                createRadialGradient(view.width.toFloat(), view.height.toFloat(), textGradient)
            } else {
                createLinearGradient(view.width.toFloat(), view.height.toFloat(), textGradient)
            }

            initialGradientSize = Size(view.width, view.height)
            view.paint.shader = shader
        } else {
            val initialWidth = if (initialGradientSize.width == 0) 1 else initialGradientSize.width
            val initialHeight =
                if (initialGradientSize.height == 0) 1 else initialGradientSize.height

            val scaleX: Float = view.width.toFloat() / initialWidth
            val scaleY: Float = view.height.toFloat() / initialHeight

            val updateMatrix = Matrix()
            updateMatrix.setScale(scaleX, scaleY)
            view.paint.shader.setLocalMatrix(updateMatrix)
        }
    }

    // Set the number of lines allowed for text wrapping
    private fun applyNumberOfLines(attributes: FontAttributes) {
        if (!managesNumberOfLines) {
            return
        }
        val numberOfLines = attributes.numberOfLines ?: 1
        if (numberOfLines <= 0) {
            view.maxLines = Int.MAX_VALUE
        } else {
            view.maxLines = numberOfLines
        }
    }

    // Compute the line height after the view.textSize has been resolved
    private fun applyLineHeight(attributes: FontAttributes) {
        val lineHeightRatio = attributes.lineHeight
        if (lineHeightRatio != null) {
            view.paint.getFontMetrics(fontMetrics)
            val lineOverflow = (fontMetrics.bottom - fontMetrics.top) / (fontMetrics.descent - fontMetrics.ascent)
            val lineHeightExtra = ((lineHeightRatio - 1) * view.textSize * lineOverflow).toInt()
            view.setLineSpacing(0.0f, lineHeightRatio)
            view.setPadding(0, lineHeightExtra, 0, 0)
        } else {
            view.setLineSpacing(0.0f, 1.0f)
            view.setPadding(0, 0, 0, 0)
        }
    }

    // Apply alignment attribute (change alignment and justification modes)
    private fun applyTextAlignment(attributes: FontAttributes) {
        view.textAlignment = when (attributes.alignment) {
            TextAlignment.LEFT -> TextView.TEXT_ALIGNMENT_VIEW_START
            TextAlignment.CENTER -> TextView.TEXT_ALIGNMENT_CENTER
            TextAlignment.RIGHT -> TextView.TEXT_ALIGNMENT_VIEW_END
            else -> TextView.TEXT_ALIGNMENT_VIEW_START
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            if (attributes.alignment == TextAlignment.JUSTIFIED) {
                view.justificationMode = Layout.JUSTIFICATION_MODE_INTER_WORD
            } else {
                view.justificationMode = Layout.JUSTIFICATION_MODE_NONE
            }
        }
    }

    // Apply text decoration attribute (change paint flags)
    private fun applyPaintFlags(attributes: FontAttributes) {
        var paintFlags = 0
        var flagUnderline = Paint.UNDERLINE_TEXT_FLAG
        var flagStrike = Paint.STRIKE_THRU_TEXT_FLAG
        val textDecoration = attributes.textDecoration
        if (textDecoration != null) {
            paintFlags = when (textDecoration) {
                TextDecoration.UNDERLINE -> flagUnderline
                TextDecoration.STRIKETHROUGH -> flagStrike
                TextDecoration.NONE -> 0
            }
        }
        val cleanPaintFlags = view.paintFlags and flagUnderline.inv() and flagStrike.inv()
        view.paintFlags = cleanPaintFlags or paintFlags
    }

    // Apply the letter spacing so we can properly measure
    private fun applyLetterSpacing(attributes: FontAttributes) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            val fontSizeValue = attributes.resolvedFontSizeValue
            val letterSpacing = (attributes.letterSpacing ?: 0f)
            view.letterSpacing = letterSpacing / fontSizeValue // convert spacing to font relative size (EM)
        }
    }

    // Apply the base text size so we can measure the text rendered
    private fun applyTextSize(attributes: FontAttributes) {
        val fontSizeValue = attributes.resolvedFontSizeValue
        val fontSizeUnit = attributes.resolveFontSizeUnit()
        // Reset auto-sizing so we can properly measure with max text size (it will be re-set after measurement)
        if (TextViewCompat.getAutoSizeTextType(view) != TextViewCompat.AUTO_SIZE_TEXT_TYPE_NONE) {
            TextViewCompat.setAutoSizeTextTypeWithDefaults(view, TextViewCompat.AUTO_SIZE_TEXT_TYPE_NONE)
        }
        // Because we disabled the auto-sizing, we can now set the actual base text size (otherwise its a no-op)
        view.setTextSize(fontSizeUnit, fontSizeValue)
    }

    // Apply text shrinking behavior (adjustsFontSizeToFitWidth)
    private fun applyTextShrinking(attributes: FontAttributes) {
        val fontSizeValue = attributes.resolvedFontSizeValue
        val fontSizeUnit = attributes.resolveFontSizeUnit()
        // on iOS, minimumScaleFactor==0 is a valid (and default) value,
        // but not on android, so here we clamp it to avoid crashing
        val minimumScaleFactor = attributes.minimumScaleFactor ?: 0f
        val minSize = Math.max((minimumScaleFactor * fontSizeValue).toInt(), 1)
        val maxSize = fontSizeValue.toInt()
        TextViewCompat.setAutoSizeTextTypeUniformWithConfiguration(view, minSize, maxSize, 1, fontSizeUnit)
    }

    override fun onFontMissing(fontDescriptor: FontDescriptor) {
        var fontLoadDisposables = this.fontLoadDisposables
        if (fontLoadDisposables == null) {
            fontLoadDisposables = hashMapOf()
            this.fontLoadDisposables = fontLoadDisposables
        }

        if (fontLoadDisposables.contains(fontDescriptor)) {
            return
        }

        val disposable = textConverter.fontManager.load(fontDescriptor, object : LoadCompletion<Typeface> {
            override fun onSuccess(item: Typeface) {
                runOnMainThreadIfNeeded {
                    onMissingFontLoadSuccess(fontDescriptor)
                }
            }

            override fun onFailure(error: Throwable) {
                runOnMainThreadIfNeeded {
                    onMissingFontLoadFailure(fontDescriptor, error)
                }
            }
        })

        if (disposable != null) {
            fontLoadDisposables[fontDescriptor] = disposable
        }
    }

    private fun onMissingFontLoadSuccess(fontDescriptor: FontDescriptor) {
        val fontLoadDisposables = this.fontLoadDisposables ?: return
        fontLoadDisposables.remove(fontDescriptor)

        if (fontLoadDisposables.isNotEmpty()) {
            return
        }

        val viewNode = ViewUtils.findViewNode(view) ?: return
        // Once we finished loading all the missing fonts, we can invalidate the measured size
        // to trigger a new layout pass on the node
        textValueDirty = true
        onDirty()
        viewNode.invalidateLayout()
    }

    private fun onMissingFontLoadFailure(fontDescriptor: FontDescriptor, error: Throwable) {
        this.fontLoadDisposables?.remove(fontDescriptor)
        val viewNode = ViewUtils.findViewNode(view) ?: return
        viewNode.notifyApplyAttributeFailed(valueAttributeId, "Failed to load font with descriptor: $fontDescriptor: ${error.message}")
    }

}
