package com.snap.valdi.views

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Rect
import android.os.Build
import android.text.InputType
import android.text.Spannable
import android.text.SpannableString
import android.text.SpannableStringBuilder
import android.text.TextUtils
import android.text.method.KeyListener
import android.view.Gravity
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import androidx.annotation.Keep
import androidx.appcompat.widget.AppCompatEditText
import com.snap.valdi.attributes.impl.ValdiTextViewBackgroundEffects
import com.snap.valdi.attributes.impl.ValdiTextViewBackgroundEffectsLayoutManager
import com.snap.valdi.attributes.impl.richtext.AttributedText
import com.snap.valdi.attributes.impl.richtext.TextViewHelper
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.callable.performSync
import com.snap.valdi.exceptions.ValdiException
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.logger.Logger
import com.snap.valdi.utils.InternedString
import com.snap.valdi.utils.ValdiMarshaller
import com.snap.valdi.utils.error

@Keep
open class ValdiEditText(context: Context) : ValdiTextViewBase(context, ValdiEditTextInput(context)) {

    init {
        backingEditTextInput.textHolder = this
    }

    val backingEditTextInput: ValdiEditTextInput
        get() = backingTextView as ValdiEditTextInput

    override fun configureTextViewHelper(helper: TextViewHelper) {
        helper.managesNumberOfLines = false
        helper.disableTextReplacement = true
    }

    override var onSelectionChangeFunction: ValdiFunction?
        get() = backingEditTextInput.onSelectionChangeFunction
        set(value) {
            backingEditTextInput.onSelectionChangeFunction = value
        }

    override fun setValdiSelectable(selectable: Boolean) {
        backingEditTextInput.setValdiSelectable(selectable)
    }

    override fun setValdiSelection(start: Int, end: Int) {
        backingEditTextInput.setValdiSelection(start, end)
    }

    override fun setTextAccessibility(text: CharSequence?) {
        backingEditTextInput.setTextAccessibility(text)
    }

    override fun prepareForRecycling() {
        super.prepareForRecycling()
        backingEditTextInput.setText("")
    }

    // Maps to the typescript's EditTextUnfocusReason
    enum class UnfocusReason(val value: Int) {
        Unknown(0),
        ReturnKeyPress(1),
        DismissKeyPress(2),
    }
}

class ValdiEditTextInput(context: Context) : AppCompatEditText(context), ValdiTouchTarget {
    protected val owner: ValdiTextViewBase?
        get() = parent as? ValdiTextViewBase

    protected val editTextOwner: ValdiEditText?
        get() = owner as? ValdiEditText

    lateinit var textHolder: ValdiTextHolder

    var backgroundEffects: ValdiTextViewBackgroundEffects? = null
    private val backgroundEffectsLayoutManager by lazy {
        ValdiTextViewBackgroundEffectsLayoutManager(this, textHolder)
    }

    private val logger: Logger?
        get() {
            return owner?.let { ViewUtils.findValdiContext(it)?.logger }
        }

    private var isAttributedText = false
    private var attributedText: AttributedText? = null
    var setTextGeneration = 0
        private set

    var valdiInputType: Int = 0
        private set

    private var valdiEditable = true
    private var editableKeyListener: KeyListener? = null
    private var valdiSelectable = true

    val isValdiEditable: Boolean
        get() = valdiEditable

    val isValdiSelectable: Boolean
        get() = valdiSelectable

    var closesWhenReturnKeyPressedDefault = true
    var closesWhenReturnKeyPressed = true

    var onWillChangeFunction: ValdiFunction? = null
    var onChangeFunction: ValdiFunction? = null
    var onEditBeginFunction: ValdiFunction? = null
    var onEditEndFunction: ValdiFunction? = null
    var onReturnFunction: ValdiFunction? = null
    var onWillDeleteFunction: ValdiFunction? = null
    var onSelectionChangeFunction: ValdiFunction? = null

    private var ignoreNewlines: Boolean = false

    var selectTextOnFocus: Boolean = false

    private var characterLimit: Int? = null

    protected var isSettingTextCount = 0

    private var lastUnfocusReason = ValdiEditText.UnfocusReason.Unknown

    private var lastFocusState = false
    var pressesReturnOnLineBreak = false
    var allowsSameViewGestureRecognizersWhenNotEditable = false

    init {
        layoutParams = ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)
        maxLines = 1
        ellipsize = TextUtils.TruncateAt.END
        includeFontPadding = false
        setValdiInputType(InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_CAP_SENTENCES or InputType.TYPE_TEXT_FLAG_AUTO_CORRECT)
        isFocusableInTouchMode = true
        gravity = Gravity.CENTER_VERTICAL
        textDirection = View.TEXT_DIRECTION_LOCALE
        textAlignment = View.TEXT_ALIGNMENT_VIEW_START
        setHintTextColor(Color.GRAY)
        setTextColor(Color.BLACK)
        setBackground(null)
        setPadding(0, 0, 0, 0)
        imeOptions = EditorInfo.IME_ACTION_DONE

        this.setOnEditorActionListener { _, actionId, _ ->
            this.onEditorActionCallback(actionId)
        }
        this.setOnKeyListener { _, keyCode, keyEvent ->
            this.onKeyCallback(keyCode, keyEvent)
        }
    }

    // Necessary for drawing
    override fun onDraw(canvas: Canvas) {
        backgroundEffects?.let {
            backgroundEffectsLayoutManager.drawBackgroundEffects(canvas, it)
        }

        super.onDraw(canvas)
    }

    fun setValdiInputType(value: Int) {
        valdiInputType = value
        super.setInputType(value)
        editableKeyListener = keyListener
        if (!valdiEditable) {
            applyValdiEditableState()
        }
    }

    fun setValdiEditable(editable: Boolean) {
        valdiEditable = editable
        applyValdiEditableState()
        owner?.textViewHelper?.applyCurrentNumberOfLines()
    }

    fun setValdiSelectable(selectable: Boolean) {
        valdiSelectable = selectable
        applyValdiEditableState()
        owner?.textViewHelper?.applyCurrentNumberOfLines()
    }

    private fun applyValdiEditableState() {
        if (valdiEditable) {
            setTextIsSelectable(false)
            keyListener = editableKeyListener
            super.setRawInputType(valdiInputType)
            setShowSoftInputOnFocusCompat(true)
            isCursorVisible = true
            isFocusable = true
            isFocusableInTouchMode = true
        } else {
            editableKeyListener = keyListener ?: editableKeyListener
            keyListener = null
            setShowSoftInputOnFocusCompat(false)
            isCursorVisible = false
            setTextIsSelectable(valdiSelectable)
            isFocusable = true
            isFocusableInTouchMode = true
        }
    }

    private fun setShowSoftInputOnFocusCompat(value: Boolean) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            showSoftInputOnFocus = value
        }
    }

    override fun requestFocus(direction: Int, previouslyFocusedRect: Rect?): Boolean {
        val owner = owner ?: return super.requestFocus(direction, previouslyFocusedRect)
        val keyboardManager = ViewUtils.getKeyboardManager(owner)
        return if (keyboardManager != null) {
            keyboardManager.onRequestFocus(this) {
                super.requestFocus(direction, previouslyFocusedRect)
            }
        } else {
            super.requestFocus(direction, previouslyFocusedRect)
        }
    }

    override fun onFocusChanged(focused: Boolean, direction: Int, previouslyFocusedRect: Rect?) {
        super.onFocusChanged(focused, direction, previouslyFocusedRect)
        val owner = owner ?: return

        ViewUtils.notifyAttributeChanged(owner, focusedAttribute, focused)

        if (focused) {
            callEventCallback(onEditBeginFunction)
        } else {
            callEventCallback(onEditEndFunction, reasonId=lastUnfocusReason.value)
            ViewUtils.getKeyboardManager(owner)?.hideKeyboard(this)
        }
        lastUnfocusReason = ValdiEditText.UnfocusReason.Unknown

        if (focused && selectTextOnFocus) {
            this.post {
                val text = text.toString() ?: ""
                setTextAndSelection(text, 0, text.length)
            }
        }

        this.post {
            lastFocusState = focused
        }
    }

    override fun onDetachedFromWindow() {
        val owner = owner
        if (lastFocusState) {
            owner?.let { ViewUtils.getKeyboardManager(it)?.hideKeyboard(this) }
        }
        super.onDetachedFromWindow()
    }

    override fun onTextChanged(text: CharSequence, start: Int, lengthBefore: Int, lengthAfter: Int) {
        super.onTextChanged(text, start, lengthBefore, lengthAfter)
        val owner = owner ?: return

        if (pressesReturnOnLineBreak && isSettingTextCount == 0) {
            val end = start + lengthAfter - 1
            if (end >= 0 && text.length > end && text[end] == '\n') {
                onPressedReturn()
            }
        }

        if (isSettingTextCount == 0) {

            val originalText = text.toString()
            val originalSelectionStart = selectionStart
            val originalSelectionEnd = selectionEnd

            var updatedText = originalText
            var updatedSelectionStart = selectionStart
            var updatedSelectionEnd = selectionEnd

            val onWillChangeFunction = this.onWillChangeFunction
            if (onWillChangeFunction != null) {
                callProcessorCallback(onWillChangeFunction) { text, selectionStart, selectionEnd ->
                    updatedText = text
                    updatedSelectionStart = selectionStart
                    updatedSelectionEnd = selectionEnd
                }
            }

            updatedText = clampProcessTextIfNeeded(updatedText)

            // We expect AttributedText to be managed within JS, therefore we don't manually update on changes
            if ((!updatedText.equals(originalText) || originalSelectionStart != updatedSelectionStart || originalSelectionEnd != updatedSelectionEnd) && !isAttributedText) {
                setTextAndSelection(updatedText, updatedSelectionStart, updatedSelectionEnd)
            }

            ViewUtils.notifyAttributeChanged(owner, valueProperty, updatedText)

            callEventCallback(onChangeFunction)

            ViewUtils.invalidateLayout(owner)
        }
    }

    override fun onSelectionChanged(selStart: Int, selEnd: Int) {
        super.onSelectionChanged(selStart, selEnd)
        val owner = owner ?: return
        ValdiTextSelection.notifySelectionChanged(owner, selStart, selEnd)
        ValdiTextSelection.callSelectionChangeCallback(onSelectionChangeFunction, text ?: "", selStart, selEnd)
    }

    override fun onKeyPreIme(keyCode: Int, keyEvent: KeyEvent): Boolean {
        if (keyEvent.keyCode == KeyEvent.KEYCODE_BACK && keyEvent.action == KeyEvent.ACTION_UP) {
            doUnfocus(ValdiEditText.UnfocusReason.DismissKeyPress)
        }
        return super.onKeyPreIme(keyCode, keyEvent)
    }

    private fun callEventCallback(callback: ValdiFunction?, reasonId: Int? = null) {
        if (callback == null) {
            return
        }
        ValdiMarshaller.use {
            val objectIndex = marshallEvent(it)
            if (reasonId != null) {
                it.putMapPropertyInt(reasonProperty, objectIndex, reasonId)
            }
            callback.perform(it)
        }
    }

    private fun callProcessorCallback(callback: ValdiFunction, done: (text: String, start: Int, end: Int) -> Unit) {
        ValdiMarshaller.use {
            marshallEvent(it)
            if (callback.performSync(it, false) && it.isMap(-1)) {
                try {
                    var text = it.getMapPropertyString(textProperty, -1)
                    val selectionStart = it.getMapPropertyDouble(selectionStartProperty, -1)
                    val selectionEnd = it.getMapPropertyDouble(selectionEndProperty, -1)
                    done(text, selectionStart.toInt(), selectionEnd.toInt())
                } catch (exc: ValdiException) {
                    logger?.error("Failed to unmarshall EditTextEvent: ${exc.message}")
                }
            }
        }
    }

    private fun marshallEvent(marshaller: ValdiMarshaller): Int {
        val objectIndex = marshaller.pushMap(3)
        marshaller.putMapPropertyString(textProperty, objectIndex, text.toString())
        marshaller.putMapPropertyDouble(selectionStartProperty, objectIndex, selectionStart.toDouble())
        marshaller.putMapPropertyDouble(selectionEndProperty, objectIndex, selectionEnd.toDouble())
        return objectIndex
    }

    override fun setText(text: CharSequence?, type: BufferType?) {
        isSettingTextCount += 1
        try {
            setTextGeneration += 1
            super.setText(text, type)
        } finally {
            isSettingTextCount -= 1
        }
    }

    fun setSelectionClamped(start: Int, end: Int) {
        ValdiTextSelection.setSelectionClamped(this, start, end)
    }

    fun setValdiSelection(start: Int, end: Int) {
        setSelectionClamped(start, end)
    }

    fun setTextAccessibility(text: CharSequence?) {
        // setText has extra checks to prevent listener callbacks from being called
        // to prevent infinite loops and unexpected side effects.
        // For accessibility reasons, we want all of those side effects.
        super.setText(text, null)
    }

    fun setTextAndSelection(attributedText: AttributedText, spannable: Spannable) {
        setAttributedText(attributedText, spannable)
    }

    fun setTextAndSelection(spannable: Spannable) {
        attributedText = null
        setSpannableAndSelection(spannable)
    }

    fun setTextAndSelection(value: String, start: Int = selectionStart, end: Int = selectionEnd) {
        isAttributedText = false
        attributedText = null
        val textClamped = clampProcessTextIfNeeded(value)
        setText(textClamped)
        setSelectionClamped(start, end)
    }

    fun refreshTextAndSelection() {
        if (isAttributedText) {
            val spannable: Spannable = this.text ?: SpannableString("") as Spannable
            setSpannableAndSelection(spannable, selectionStart, selectionEnd, skipSetTextOptimization = true)
        } else {
            setTextAndSelection(text?.toString() ?: "", selectionStart, selectionEnd)
        }
    }

    private fun setAttributedText(nextAttributedText: AttributedText, spannable: Spannable) {
        isAttributedText = true
        attributedText = nextAttributedText
        setSpannableAndSelection(spannable)
    }

    private fun setSpannableAndSelection(spannable: Spannable, start: Int = selectionStart, end: Int = selectionEnd, skipSetTextOptimization: Boolean = false) {
        isAttributedText = true
        val textClamped = clampProcessSpannableIfNeeded(spannable)
        val lengthClamped = textClamped.length ?: 0
        val superText = super.getText()

        // Calling setText is expensive so we only call it if the text has changed.
        // If text has not changed then we apply the spans without calling setText.
        // See: https://developer.android.com/develop/ui/views/text-and-emoji/spans#change-internal-attributes
        if (superText == null || superText.toString() != spannable.toString() || skipSetTextOptimization) {
            setText(spannable, BufferType.SPANNABLE)
        } else {
            val newSpans = spannable.getSpans(0, spannable.length, Object::class.java)

            // When calling `setSpan` we first remove existing spans if their types are present.
            superText.getSpans(0, spannable.length, Object::class.java).forEach { span ->
                val isInNewSpans = newSpans.find { newSpan ->
                    newSpan::class == span::class
                }
                if (isInNewSpans != null) {
                    superText.removeSpan(span)
                }
            }

            // apply new spans
            newSpans.forEach { span ->
                superText.setSpan(
                    span,
                    spannable.getSpanStart(span),
                    spannable.getSpanEnd(span),
                    spannable.getSpanFlags(span),
                )
            }
        }

        val startClamped = Math.max(0, Math.min(lengthClamped, start))
        val endClamped = Math.max(startClamped, Math.min(lengthClamped, end))
        setSelection(startClamped, endClamped)
    }

    private fun clampProcessTextIfNeeded(rawValue: String): String {
        var value = rawValue
        if (ignoreNewlines) {
            value = value.replace("\n", "")
        }
        val characterLimit = this.characterLimit
        if (characterLimit != null && characterLimit >= 0) {
            // TODO(979) - check the length based on localized length, not string binary length (emojis, chinese chars, etc)
            value = value.substring(0, Math.max(0, Math.min(value.length, characterLimit)))
        }
        return value
    }

    private fun clampProcessSpannableIfNeeded(rawValue: Spannable): Spannable {
        val value = SpannableStringBuilder(rawValue)
        if (ignoreNewlines) {
            value.replace(Regex("\n"), "")
        }
        val characterLimit = this.characterLimit
        if (characterLimit != null && characterLimit >= 0 && value.length > characterLimit) {
            // TODO(979) check the length based on localized length, not string binary length (emojis, chinese chars, etc)
            value.delete(characterLimit, value.length)
        }
        return value
    }

    fun setCharacterLimit(value: Int?) {
        characterLimit = value
        refreshTextAndSelection()
    }

    fun setIgnoreNewlines(value: Boolean) {
        ignoreNewlines = value
        refreshTextAndSelection()
    }

    fun onNumberOfLinesChanged() {
        if (maxLines != Int.MAX_VALUE) {
            ellipsize = TextUtils.TruncateAt.END
        } else {
            ellipsize = null
        }
        if (!isValdiEditable) {
            setTextIsSelectable(isValdiSelectable)
            keyListener = null
            isCursorVisible = false
        }
    }

    fun allowLineReturns(value: Boolean) {
        if (value) {
            setValdiInputType(valdiInputType or InputType.TYPE_TEXT_FLAG_MULTI_LINE)
            owner?.textViewHelper?.applyCurrentNumberOfLines()
            setHorizontallyScrolling(false)
            setIgnoreNewlines(false)
        } else {
            setValdiInputType(valdiInputType and InputType.TYPE_TEXT_FLAG_MULTI_LINE.inv())
            owner?.textViewHelper?.applyCurrentNumberOfLines()
            setHorizontallyScrolling(false)
            setIgnoreNewlines(true)
        }
    }

    fun onPressedReturn() {
        if (owner == null) {
            return
        }
        if (closesWhenReturnKeyPressed) {
            doUnfocus(ValdiEditText.UnfocusReason.ReturnKeyPress)
        }
        callEventCallback(onReturnFunction)
    }

    private fun onEditorActionCallback(actionId: Int): Boolean {
        if (
            actionId == EditorInfo.IME_ACTION_DONE ||
            actionId == EditorInfo.IME_ACTION_GO ||
            actionId == EditorInfo.IME_ACTION_NEXT ||
            actionId == EditorInfo.IME_ACTION_SEARCH ||
            actionId == EditorInfo.IME_ACTION_SEND ||
            actionId == EditorInfo.IME_ACTION_UNSPECIFIED
        ) {
            onPressedReturn()
            return true
        }
        return false
    }

    private fun onKeyCallback(keyCode: Int, keyEvent: KeyEvent): Boolean {
        if (keyCode == KeyEvent.KEYCODE_DEL && keyEvent.action == KeyEvent.ACTION_UP) {
            callEventCallback(onWillDeleteFunction)
        }
        return false
    }

    fun doFocus() {
        val owner = owner ?: return
        if (!hasFocus()) {
            ViewUtils.getKeyboardManager(owner)?.requestFocusAndShowKeyboard(this)
        }
    }

    fun doUnfocus(reason: ValdiEditText.UnfocusReason) {
        if (hasFocus()) {
            lastUnfocusReason = reason
            ViewUtils.resetFocusToRootViewOf(this)
        }
    }

    override fun allowsSameViewGestureRecognizers(): Boolean {
        return allowsSameViewGestureRecognizersWhenNotEditable && !isValdiEditable
    }

    override fun processTouchEvent(event: MotionEvent): ValdiTouchEventResult {
        if (allowsSameViewGestureRecognizers()) {
            this.dispatchTouchEvent(event)
            return ValdiTouchEventResult.IgnoreEvent
        }

        // If the view is not focuseable, we deny it any kind of events
        if (!isFocusable || !isFocusableInTouchMode) {
            return ValdiTouchEventResult.IgnoreEvent
        }
        // Tap ups will always be swallowed to match iOS behaviour
        if (event.actionMasked == MotionEvent.ACTION_UP) {
            this.dispatchTouchEvent(event)
            return ValdiTouchEventResult.ConsumeEventAndCancelOtherGestures
        }
        // Read the input's state before the event
        val beforeFocused = this.isFocused
        val beforeSelectionStart = this.selectionStart
        val beforeSelectionEnd = this.selectionEnd
        val beforeText = this.text
        // Dispatch the event to the actual android view
        val eventIsUsedByDispatch = this.dispatchTouchEvent(event)
        // If the event was ignored by the underlying view, don't swallow it
        if (!eventIsUsedByDispatch) {
            return ValdiTouchEventResult.IgnoreEvent
        }
        // If the intput state has changed during the event, we want to cancel all other valdi gestures
        val afterFocused = this.isFocused
        if (afterFocused != beforeFocused) {
            return ValdiTouchEventResult.ConsumeEventAndCancelOtherGestures
        }
        val afterSelectionStart = this.selectionStart
        if (afterSelectionStart != beforeSelectionStart) {
            return ValdiTouchEventResult.ConsumeEventAndCancelOtherGestures
        }
        val afterSelectionEnd = this.selectionEnd
        if (afterSelectionEnd != beforeSelectionEnd) {
            return ValdiTouchEventResult.ConsumeEventAndCancelOtherGestures
        }
        val afterText = this.text
        if (afterText != beforeText) {
            return ValdiTouchEventResult.ConsumeEventAndCancelOtherGestures
        }
        // Otherwise, we'll give the other valdi gestures a chance to run when the touch was a no-op
        return ValdiTouchEventResult.IgnoreEvent
    }

    companion object {
        private val focusedAttribute = InternedString.create("focused")
        private val valueProperty = InternedString.create("value")
        private val textProperty = InternedString.create("text")
        private val selectionStartProperty = InternedString.create("selectionStart")
        private val selectionEndProperty = InternedString.create("selectionEnd")
        private val reasonProperty = InternedString.create("reason")
    }
}
