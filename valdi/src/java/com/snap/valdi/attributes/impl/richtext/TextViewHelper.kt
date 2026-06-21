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
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.core.widget.TextViewCompat
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.FontDescriptor
import com.snap.valdi.attributes.impl.fonts.MissingFontsTracker
import com.snap.valdi.attributes.impl.gradients.ValdiGradient
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.utils.CoordinateResolver
import com.snap.valdi.utils.Disposable
import com.snap.valdi.utils.LoadCompletion
import com.snap.valdi.utils.runOnMainThreadIfNeeded
import com.snap.valdi.views.ValdiTextHolder
import com.snap.valdi.views.ValdiTextAnimationGroup
import com.snap.valdi.views.ValdiEditTextInput
import com.snap.valdi.views.ValdiEditTextMultiline
import com.snap.valdi.views.touches.AttributedTextTapGestureRecognizer
import kotlin.math.max

class TextViewHelper(private val view: TextView,
                     private val fontManager: FontManager,
                     private val defaultAttributes: FontAttributes,
                     private val valueAttributeId: Int,
                     private val textHolder: ValdiTextHolder? = view as? ValdiTextHolder) : MissingFontsTracker {

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

        fun isTextValueEqual(valdiText: Any?, viewText: CharSequence): Boolean {
            return if (valdiText is String) {
                valdiText == viewText.toString()
            } else {
                false
            }
        }
    }

    private val coordinateResolver = CoordinateResolver(view.context)

    /**
     * In some cases, we want the number of lines to not be manipulated through the normal font composite attribute
     * This is to avoid conflicts when a view uses another attribute that conflicts and does not uses "numberOfLines"
     */
    var managesNumberOfLines = true

    var defaultNumberOfLines = 1

    /**
     * For TextViews that don't support selection, we allow text replacement by default
     * For EditText where selection is paramount, we disable text replacement in spannables, and draw an outline on top as needed
     */
    var disableTextReplacement = false

    /**
     * When true, match iOS: after text is set programmatically, keep the caret at the end of the
     * text instead of Android's default of moving it to the start, unless an explicit selection is
     * provided. Gated by the VALDI_EDITTEXT_RESET_SELECTION_MATCHES_IOS COF; applies to ValdiEditText.
     */
    var matchIosTextSetCaret = false

    var fontAttributes: FontAttributes? = null
        set(value) {
            if (field != value) {
                val text = textValue as? String
                val didRequireAttributedText = textRequiresAttributedText(text, field)
                val willRequireAttributedText = textRequiresAttributedText(text, value)
                field = value
                fontAttributesDirty = true
                fontAutofitDirty = true
                if (didRequireAttributedText != willRequireAttributedText) {
                    textValueDirty = true
                }
                onDirty()
            }
        }

    var textValue: Any? = null
        set(value) {
            if (
                    field != value  ||
                    // textValue can get out of sync with the view so we may need to compare them
                    !isTextValueEqual(value, view.text)
            ) {
                field = value
                textValueDirty = true
                isAttributedText = value is AttributedText
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

    private var fontAttributesDirty = true
    private var fontAutofitDirty = true

    private var textValueDirty = false
    private var isAttributedText = false
    val processedText: ValdiProcessedText?
        get() = processedTextValue
    private var processedTextValue: ValdiProcessedText? = null
    private var attributedTextAnimator: AttributedTextAnimator? = null
    private var animationFrameCallbackPosted = false
    private var animationFrameCallback: Runnable? = null
    private var textAnimationGroup: ValdiTextAnimationGroup? = null
    private var textAnimationTimeline: AttributedTextAnimationTimeline? = null
    var textAnimationPartCount: Int = 0
        private set
    private var textAnimationBasePartIndex: Int = 0
    internal val textAnimationView: TextView
        get() = view

    private var needsUpdateOnLayoutCallbacks = false

    private var textGradientDirty = false
    private lateinit var initialGradientSize: Size

    private var fontLoadDisposables: MutableMap<FontDescriptor, Disposable>? = null

    fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        updateTextAnimationGroupRegistration()
        updateTextAttributes()
        TextViewHelper.lastMeasuredText = view.text
        TextViewHelper.lastMeasuredFontAttributes = fontAttributes
    }

    fun updateInlineAttachments(): Boolean {
        return processedTextValue?.updateInlineAttachments() == true
    }

    fun refreshProcessedTextStorage() {
        val processedText = processedTextValue ?: return
        if (view is ValdiEditTextInput) {
            view.refreshTextAndSelection()
        } else {
            setTextViewProcessedText(processedText)
        }

        needsUpdateOnLayoutCallbacks = true
    }

    fun onLayout(changed: Boolean) {
        updateTextAttributes()
        updateTextAutofit()
        updateTextGradient(changed)
        updateOnLayoutCallbacks()
    }

    fun applyCurrentNumberOfLines() {
        applyNumberOfLines(fontAttributes ?: defaultAttributes)
    }

    private fun updateOnLayoutCallbacks() {
        // TODO(3065): Also update on view size changed
        val processedText = processedTextValue
        if (processedText == null || !processedText.hasOnLayout || !needsUpdateOnLayoutCallbacks) {
            return
        }
        val layout = view.getLayout() ?: return
        // TODO(3944): Add support for multiline text
        processedText.updateOnLayoutCallbacks(layout, coordinateResolver)
        needsUpdateOnLayoutCallbacks = false
    }

    private fun updateTextAttributes() {
        val editText = view as? ValdiEditTextInput
        val setTextGenerationBefore = editText?.setTextGeneration ?: 0
        val text = textValue as? String
        if (isAttributedText || textRequiresAttributedText(text)) {
            if (fontAttributesDirty || textValueDirty) {
                fontAttributesDirty = false
                textValueDirty = false
                applyFontAttributes(fontAttributes ?: defaultAttributes)
                applyAttributedText(textValue as? AttributedText, text)
            }
        } else {
            if (fontAttributesDirty) {
                fontAttributesDirty = false
                applyFontAttributes(fontAttributes ?: defaultAttributes)
            }

            if (textValueDirty) {
                textValueDirty = false
                applyTextSimple(text)
            }
        }

        if (editText != null) {
            val currentSelection = selection
            val textWasSet = editText.setTextGeneration != setTextGenerationBefore
            if (currentSelection != null) {
                if (selectionDirty || (matchIosTextSetCaret && textWasSet)) {
                    editText.setSelectionClamped(currentSelection.first, currentSelection.second)
                }
            } else if (matchIosTextSetCaret && textWasSet) {
                editText.setSelectionClamped(Int.MAX_VALUE, Int.MAX_VALUE)
            }
            selectionDirty = false
        } else if (textHolder != null && selectionDirty) {
            selectionDirty = false
            selection?.let { (first, second) ->
                textHolder.setValdiSelection(first, second)
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
        processedTextValue = null
        clearTextAnimationState()
        if (view is ValdiEditTextInput) {
            view.setTextAndSelection(text ?: "")
        } else {
            view.text = text
        }
    }

    private fun textRequiresAttributedText(text: String?): Boolean {
        return textRequiresAttributedText(text, fontAttributes)
    }

    private fun textRequiresAttributedText(text: String?, attributes: FontAttributes?): Boolean {
        if (text.isNullOrEmpty() || attributes == null) {
            return false
        }

        return when (attributes.textDecoration) {
            // Plain underline can use TextView's native paint flag unless custom geometry requires a span.
            TextDecoration.UNDERLINE -> attributes.customUnderlineStyle != null
            TextDecoration.DASHED_UNDERLINE,
            TextDecoration.DOTTED_UNDERLINE -> true
            else -> false
        }
    }

    private fun removeAttributedTextTapGestureRecognizer() {
        val gestureRecognizers = ViewUtils.getGestureRecognizers(this.view) ?: return
        gestureRecognizers.removeGestureRecognizer(AttributedTextTapGestureRecognizer::class.java)
    }

    private fun addAttributedTextTapGestureRecognizer() {
        val gestureRecognizers = ViewUtils.getOrCreateGestureRecognizers(this.view)
        var attributedTextTapGestureRecognizer = gestureRecognizers
                .getGestureRecognizer(AttributedTextTapGestureRecognizer::class.java)
        if (attributedTextTapGestureRecognizer == null) {
            attributedTextTapGestureRecognizer = AttributedTextTapGestureRecognizer(this.view)
            gestureRecognizers.addGestureRecognizer(attributedTextTapGestureRecognizer)
        }

        attributedTextTapGestureRecognizer.processedText = processedTextValue
    }

    fun convertAttributedText(text: AttributedText, attributedTextAnimator: AttributedTextAnimator?): ValdiProcessedText {
        val fontAttributes = this.fontAttributes ?: defaultAttributes
        val density = view.resources.displayMetrics.density
        return ValdiProcessedText.parse(
            fontManager,
            text,
            fontAttributes,
            this,
            attributedTextAnimator,
            disableTextReplacement,
            density
        )
    }

    fun needsDrawOnTopAttributedText(): Boolean {
        val processedText = processedTextValue ?: return false
        return disableTextReplacement && processedText.hasOuterOutline
    }

    fun postInvalidateOnAnimationIfNeeded() {
        val attributedTextAnimator = attributedTextAnimator
        if (attributedTextAnimator?.hasAnimationRuns() != true) {
            cancelAnimationFrameLoop()
            return
        }

        val textAnimationGroup = textAnimationGroup
        if (textAnimationGroup != null) {
            textAnimationGroup.startTextAnimationFrameLoopIfNeeded()
            return
        }

        postAnimationFrameCallback()
    }

    fun updateTextAnimationGroupRegistration() {
        val group = nearestTextAnimationGroup()
        if (group == textAnimationGroup) {
            group?.let {
                applyTextAnimationTimeline(it.textAnimationTimeline, 0)
                it.markParticipantBaseIndexesDirty()
            }
            return
        }

        textAnimationGroup?.unregisterParticipant(this)
        textAnimationGroup = group
        if (group != null) {
            group.registerParticipant(this)
        } else {
            applyTextAnimationTimeline(null, 0)
        }
    }

    fun unregisterTextAnimationGroup() {
        textAnimationGroup?.unregisterParticipant(this)
    }

    fun applyTextAnimationTimeline(timeline: AttributedTextAnimationTimeline?, basePartIndex: Int) {
        textAnimationTimeline = timeline
        textAnimationBasePartIndex = basePartIndex
        attributedTextAnimator?.groupedTimeline = timeline
        attributedTextAnimator?.basePartIndex = basePartIndex
        if (timeline != null) {
            cancelAnimationFrameLoop()
        }
    }

    fun clearTextAnimationGroupRegistration() {
        textAnimationGroup = null
        applyTextAnimationTimeline(null, 0)
    }

    fun prepareGroupedTextAnimationFrame() {
        attributedTextAnimator?.prepareGroupedFrame()
    }

    fun updateGroupedTextAnimationFrame(): Boolean {
        val animator = attributedTextAnimator ?: return false
        val spannable = view.text as? Spannable ?: return false
        val hasActiveAnimations = animator.update(spannable)
        if (hasActiveAnimations) {
            view.invalidate()
        }
        return hasActiveAnimations
    }

    private fun nearestTextAnimationGroup(): ValdiTextAnimationGroup? {
        var parent = view.parent
        while (parent is ViewGroup) {
            if (parent is ValdiTextAnimationGroup) {
                return parent
            }
            parent = parent.parent
        }
        return null
    }

    private fun postAnimationFrameCallback() {
        if (animationFrameCallbackPosted) {
            return
        }

        animationFrameCallbackPosted = true
        view.postOnAnimation(animationFrameCallback())
    }

    private fun animationFrameCallback(): Runnable {
        val existingFrameCallback = animationFrameCallback
        if (existingFrameCallback != null) {
            return existingFrameCallback
        }

        return Runnable {
            animationFrameCallbackPosted = false
            val attributedTextAnimator = attributedTextAnimator ?: return@Runnable
            val spannable = view.text as? Spannable ?: return@Runnable
            val hasActiveAnimations = attributedTextAnimator.update(spannable)
            if (hasActiveAnimations) {
                postAnimationFrameCallback()
            }
        }.also {
            animationFrameCallback = it
        }
    }

    private fun cancelAnimationFrameLoop() {
        if (animationFrameCallbackPosted) {
            animationFrameCallback?.let { view.removeCallbacks(it) }
            animationFrameCallbackPosted = false
        }
    }

    fun drawOnTopAttributedText(canvas: Canvas, layout: Layout) {
        val processedText = processedTextValue ?: return
        processedText.drawOnTop(
            canvas,
            layout,
            fontManager,
            this
        )
    }

    private fun applyAttributedText(attributedText: AttributedText?, text: String?) {
        val processedText = if (attributedText != null) {
            val animationTransformsSize = attributedText.getAnimationTransformsSize()
            val animator = if (animationTransformsSize > 0) {
                textAnimationPartCount = animationTransformsSize
                updateTextAnimationGroupRegistration()
                getOrCreateAttributedTextAnimator().also {
                    it.groupedTimeline = textAnimationTimeline
                    it.basePartIndex = textAnimationBasePartIndex
                    it.beginSync()
                }
            } else {
                clearTextAnimationState()
                null
            }
            val convertedText = try {
                convertAttributedText(attributedText, animator)
            } finally {
                animator?.endSync()
            }
            convertedText
        } else {
            clearTextAnimationState()
            null
        }
        processedTextValue = processedText
        val spannable = processedText?.spannable ?: convertTextToSpannable(text ?: "")

        if (view is ValdiEditTextInput) {
            if (attributedText != null) {
                view.setTextAndSelection(attributedText, spannable)
            } else {
                view.setTextAndSelection(spannable)
            }
        } else {
            if (processedText != null) {
                setTextViewProcessedText(processedText)
            } else {
                view.text = spannable
            }
        }

        needsUpdateOnLayoutCallbacks = true

        val activeSpannable = view.text as? Spannable ?: spannable
        if (attributedText != null) {
            attributedTextAnimator?.update(activeSpannable)
        }
        postInvalidateOnAnimationIfNeeded()

        if (processedText?.hasOnTap == true) {
            addAttributedTextTapGestureRecognizer()
        } else {
            removeAttributedTextTapGestureRecognizer()
        }
    }

    private fun setTextViewProcessedText(processedText: ValdiProcessedText) {
        if (processedText.hasAnimationTransform) {
            view.setText(processedText.spannable, TextView.BufferType.SPANNABLE)
        } else {
            view.text = processedText.spannable
        }
    }

    private fun getOrCreateAttributedTextAnimator(): AttributedTextAnimator {
        val animator = attributedTextAnimator
        if (animator != null) {
            return animator
        }

        return AttributedTextAnimator().also {
            it.groupedTimeline = textAnimationTimeline
            it.basePartIndex = textAnimationBasePartIndex
            attributedTextAnimator = it
        }
    }

    private fun clearAttributedTextAnimator() {
        val animator = attributedTextAnimator ?: return
        animator.clear(view.text as? Spannable)
        attributedTextAnimator = null
    }

    private fun clearTextAnimationState() {
        textAnimationPartCount = 0
        unregisterTextAnimationGroup()
        clearAttributedTextAnimator()
        cancelAnimationFrameLoop()
    }

    private fun convertTextToSpannable(text: String): Spannable {
        val spannable = SpannableString(text)
        if (text.isEmpty()) {
            return spannable
        }

        val attributes = fontAttributes ?: defaultAttributes
        attributes.enumerateSpans(fontManager, this, disableTextReplacement) {
            spannable.setSpan(it, 0, spannable.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
        }
        return spannable
    }

    // Apply all known specified text rendering attributes
    private fun applyFontAttributes(attributes: FontAttributes) {
        view.typeface = attributes.resolveTypeface(fontManager, this)
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
        val numberOfLines = attributes.numberOfLines ?: defaultNumberOfLines
        if (numberOfLines <= 0) {
            view.maxLines = Int.MAX_VALUE
        } else {
            view.maxLines = numberOfLines
        }
        if (textHolder is ValdiEditTextMultiline) {
            textHolder.backingEditTextInput.onNumberOfLinesChanged()
        }
    }

    // Compute the line height after the view.textSize has been resolved
    private fun applyLineHeight(attributes: FontAttributes) {
        val lineHeight = attributes.lineHeight
        if (lineHeight != null) {
            view.paint.getFontMetrics(fontMetrics)
            val resolvedLineHeight = coordinateResolver.toPixelF(lineHeight)
            val fontLineHeight = fontMetrics.descent - fontMetrics.ascent
            val lineSpacingExtra = resolvedLineHeight - fontLineHeight
            view.setLineSpacing(lineSpacingExtra, 1.0f)
            view.setPadding(0, (lineSpacingExtra / 2.0f).toInt(), 0, 0)
            return
        }

        val lineHeightMultiple = attributes.lineHeightMultiple
        if (lineHeightMultiple != null) {
            view.paint.getFontMetrics(fontMetrics)
            val lineOverflow = (fontMetrics.bottom - fontMetrics.top) / (fontMetrics.descent - fontMetrics.ascent)
            val lineHeightExtra = ((lineHeightMultiple - 1) * view.textSize * lineOverflow).toInt()
            view.setLineSpacing(0.0f, lineHeightMultiple)
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
                // Custom underline geometry is drawn by CustomUnderlineSpan; keep the native flag off.
                TextDecoration.UNDERLINE -> if (attributes.customUnderlineStyle == null) flagUnderline else 0
                TextDecoration.STRIKETHROUGH -> flagStrike
                TextDecoration.DASHED_UNDERLINE -> 0
                TextDecoration.DOTTED_UNDERLINE -> 0
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

        val disposable = fontManager.load(fontDescriptor, object : LoadCompletion<Typeface> {
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
