package com.snap.valdi.attributes.impl.richtext

import android.graphics.Paint
import android.os.Build
import android.text.Layout
import android.text.StaticLayout
import android.text.TextPaint
import android.text.TextUtils
import android.util.TypedValue
import android.view.View
import com.snap.valdi.attributes.MeasureDelegate
import com.snap.valdi.attributes.MeasuredSize
import com.snap.valdi.attributes.ViewLayoutAttributes
import com.snap.valdi.attributes.impl.fonts.FontDescriptor
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.MissingFontsTracker
import com.snap.valdi.logger.Logger
import com.snap.valdi.utils.ValdiTextDirectionHeuristic
import com.snap.valdi.views.TextViewUtils
import com.snapchat.client.valdi.utils.CppObjectWrapper
import kotlin.math.ceil

/**
 * Measures Valdi text nodes directly with a [StaticLayout] built from the node's layout
 * attributes, replacing placeholder-TextView measurement. Stateless per measure and callable
 * from arbitrary threads, per the [MeasureDelegate] contract.
 *
 * Attributed text is resolved through [ValdiProcessedText.parse] rather than a TextView-bound
 * [TextViewHelper]: the point of this delegate is to measure without instantiating a view, and
 * `parse` is the view-free half of that conversion.
 *
 * Known limitation: `adjustsFontSizeToFitWidth`/`minimumScaleFactor` are ignored, so text that
 * auto-shrinks at render time is always measured at its base font size. The placeholder path
 * measured the first pass un-shrunken too; it only picked up a (stale) shrunken size when
 * re-measuring a node whose mounted view had already been laid out. Base-size metrics are an
 * upper bound on the shrunk rendering, so the measured box stays loose rather than clipping.
 */
class TextViewMeasureDelegate(
    private val fontManager: FontManager,
    private val defaultAttributes: FontAttributes,
    private val logger: Logger,
) : MeasureDelegate {

    private val missingFontsTracker = object : MissingFontsTracker {
        override fun onFontMissing(fontDescriptor: FontDescriptor) = Unit
    }

    override fun onMeasure(
        attributes: ViewLayoutAttributes,
        widthMeasureSpec: Int,
        heightMeasureSpec: Int,
        isRightToLeft: Boolean,
    ): MeasuredSize {
        val fontAttributes = attributes.getValue("fontAttributes") as? FontAttributes ?: defaultAttributes
        val text = resolveText(attributes.getValue("value"), fontAttributes)

        if (text.isEmpty()) {
            val resolvedHeightMeasureSpec = TextViewUtils.resolveEmptyTextHeightMeasureSpec(heightMeasureSpec)
            return MeasuredSize(
                resolveSize(0, widthMeasureSpec),
                resolveSize(0, resolvedHeightMeasureSpec),
            )
        }

        val paint = buildPaint(fontAttributes)
        val desiredTextWidth = ceil(Layout.getDesiredWidth(text, paint).toDouble()).toInt()
        val measuredWidth = resolveSize(desiredTextWidth, widthMeasureSpec)
        val layoutWidth = measuredWidth.coerceAtLeast(1)
        val maxLines = resolveMaxLines(fontAttributes)
        val lineHeightRatio = fontAttributes.lineHeight ?: 1f

        val layout = buildStaticLayout(
            text = text,
            paint = paint,
            width = layoutWidth,
            alignment = resolveAlignment(fontAttributes),
            lineSpacingMultiplier = lineHeightRatio,
            includeFontPadding = false,
            maxLines = maxLines,
        )

        val measuredLineCount = maxLines?.let { layout.lineCount.coerceAtMost(it) } ?: layout.lineCount
        val textHeight = if (measuredLineCount > 0) {
            layout.getLineBottom(measuredLineCount - 1)
        } else {
            0
        }
        val desiredHeight = textHeight + resolveLineHeightTopPadding(fontAttributes, paint)

        return MeasuredSize(
            measuredWidth,
            resolveSize(desiredHeight, heightMeasureSpec),
        )
    }

    private fun resolveText(value: Any?, fontAttributes: FontAttributes): CharSequence {
        val attributedText = when (value) {
            is String -> return value
            is AttributedText -> value
            // Same unwrap as bindTextAttribute's preprocessor, for values that reach the
            // measure snapshot unpreprocessed.
            is CppObjectWrapper -> AttributedTextCpp(value)
            else -> return ""
        }
        return ValdiProcessedText.parse(
            fontManager = fontManager,
            attributedText = attributedText,
            startingAttributes = fontAttributes,
            missingFontsTracker = missingFontsTracker,
            logger = logger,
            density = fontManager.context.resources.displayMetrics.density,
        ).spannable
    }

    private fun buildPaint(fontAttributes: FontAttributes): TextPaint {
        val fontSizeValue = fontAttributes.resolvedFontSizeValue
        val resources = fontManager.context.resources
        val textSize = TypedValue.applyDimension(
            fontAttributes.resolveFontSizeUnit(),
            fontSizeValue,
            resources.displayMetrics,
        )

        return TextPaint(Paint.ANTI_ALIAS_FLAG).apply {
            this.textSize = textSize
            typeface = fontAttributes.resolveTypeface(fontManager, missingFontsTracker)
            color = fontAttributes.color
            isUnderlineText = fontAttributes.textDecoration == TextDecoration.UNDERLINE
            isStrikeThruText = fontAttributes.textDecoration == TextDecoration.STRIKETHROUGH
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                letterSpacing = (fontAttributes.letterSpacing ?: 0f) / fontSizeValue
            }
        }
    }

    private fun buildStaticLayout(
        text: CharSequence,
        paint: TextPaint,
        width: Int,
        alignment: Layout.Alignment,
        lineSpacingMultiplier: Float,
        includeFontPadding: Boolean,
        maxLines: Int?,
    ): StaticLayout {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            val builder = StaticLayout.Builder.obtain(text, 0, text.length, paint, width)
                .setAlignment(alignment)
                .setLineSpacing(0f, lineSpacingMultiplier)
                .setIncludePad(includeFontPadding)
                // The rendered views resolve TEXT_DIRECTION_LOCALE through
                // ValdiTextDirectionHeuristic, so bidi base direction comes from the text content
                // and locale, not the Yoga layout direction. Measuring with an absolute direction
                // would wrap differently.
                .setTextDirection(ValdiTextDirectionHeuristic)
                .setEllipsize(TextUtils.TruncateAt.END)

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                // TextView renders with fallback line spacing by default on P+, so measure with
                // it too; otherwise text drawn with taller fallback fonts (e.g. Arabic/Urdu)
                // exceeds the measured height and gets clipped.
                builder.setUseLineSpacingFromFallbacks(true)
            }

            maxLines?.let(builder::setMaxLines)
            return builder.build()
        }

        // Pre-M fallback for open-source embedders (the library's minSdk is below 23; the
        // Snapchat app never reaches this). maxLines is still honored because onMeasure caps the
        // counted lines before reading getLineBottom, and ellipsization does not affect measured
        // bounds. There is no pre-M API to supply a text direction heuristic, so this measures
        // with FIRSTSTRONG_LTR and bidi text under an RTL locale may wrap differently from the
        // rendered TextView.
        @Suppress("DEPRECATION")
        return StaticLayout(
            text,
            0,
            text.length,
            paint,
            width,
            alignment,
            lineSpacingMultiplier,
            0f,
            includeFontPadding,
        )
    }

    private fun resolveSize(desiredSize: Int, measureSpec: Int): Int {
        val specSize = View.MeasureSpec.getSize(measureSpec)
        return when (View.MeasureSpec.getMode(measureSpec)) {
            View.MeasureSpec.EXACTLY -> specSize
            View.MeasureSpec.AT_MOST -> desiredSize.coerceAtMost(specSize)
            else -> desiredSize
        }
    }

    private fun resolveMaxLines(fontAttributes: FontAttributes): Int? {
        val numberOfLines = fontAttributes.numberOfLines ?: 1
        return if (numberOfLines <= 0) null else numberOfLines
    }

    private fun resolveAlignment(fontAttributes: FontAttributes): Layout.Alignment {
        return when (fontAttributes.alignment) {
            TextAlignment.CENTER -> Layout.Alignment.ALIGN_CENTER
            TextAlignment.RIGHT -> Layout.Alignment.ALIGN_OPPOSITE
            else -> Layout.Alignment.ALIGN_NORMAL
        }
    }

    private fun resolveLineHeightTopPadding(fontAttributes: FontAttributes, paint: TextPaint): Int {
        val lineHeightRatio = fontAttributes.lineHeight ?: return 0
        val fontMetrics = paint.fontMetrics
        val fontHeight = fontMetrics.descent - fontMetrics.ascent
        if (fontHeight <= 0f) {
            return 0
        }
        val lineOverflow = (fontMetrics.bottom - fontMetrics.top) / fontHeight
        return ((lineHeightRatio - 1) * paint.textSize * lineOverflow).toInt()
    }
}
