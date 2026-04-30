package com.snap.valdi.attributes.impl

import android.content.Context
import android.text.InputType
import android.graphics.Color
import android.text.InputFilter
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.attributes.AttributesBindingContext
import com.snap.valdi.attributes.impl.animations.ValdiAnimator
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.attributes.impl.richtext.RichTextConverter
import com.snap.valdi.attributes.impl.richtext.TextViewHelper
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.exceptions.ValdiException
import com.snap.valdi.views.ValdiEditText
import com.snap.valdi.views.ValdiTextHolder
import com.snap.valdi.exceptions.AttributeError
import kotlin.math.roundToInt

/**
 * Binds attributes for the EditText's view class
 */
class EditTextAttributesBinder(
    private val context: Context,
    private val textConverter: RichTextConverter,
    private val defaultAttributes: FontAttributes,
    private val resetSelectionMatchesIos: Boolean = false,
) : AttributesBinder<ValdiEditText> {

    private var valueAttributeId = 0
    private val scaledDensity = context.resources.displayMetrics.scaledDensity

    override val viewClass: Class<ValdiEditText>
        get() = ValdiEditText::class.java

    override fun bindAttributes(attributesBindingContext: AttributesBindingContext<ValdiEditText>) {
        attributesBindingContext.bindStringAttribute(
            "placeholder",
            true,
            this::applyHint,
            this::resetHint
        )
        attributesBindingContext.bindBooleanAttribute(
            "focused",
            false,
            this::applyFocus,
            this::resetFocus
        )
        attributesBindingContext.bindBooleanAttribute(
            "enabled",
            false,
            this::applyEnabled,
            this::resetEnabled
        )
        attributesBindingContext.bindFunctionAttribute(
            "onWillChange",
            this::applyOnWillChange,
            this::resetOnWillChange
        )
        attributesBindingContext.bindFunctionAttribute(
            "onChange",
            this::applyOnChange,
            this::resetOnChange
        )
        attributesBindingContext.bindFunctionAttribute(
            "onEditBegin",
            this::applyOnEditBegin,
            this::resetOnEditBegin
        )
        attributesBindingContext.bindFunctionAttribute(
            "onEditEnd",
            this::applyOnEditEnd,
            this::resetOnEditEnd
        )
        attributesBindingContext.bindFunctionAttribute(
            "onReturn",
            this::applyOnReturn,
            this::resetOnReturn
        )
        attributesBindingContext.bindFunctionAttribute(
            "onWillDelete",
            this::applyOnWillDelete,
            this::resetOnWillDelete
        )
        attributesBindingContext.bindTextAttribute(
            "value",
            true,
            this::applyValue,
            this::resetValue
        )
        attributesBindingContext.bindIntAttribute(
            "characterLimit",
            true,
            this::applyCharacterLimit,
            this::resetCharacterLimit
        )
        attributesBindingContext.bindBooleanAttribute(
            "closesWhenReturnKeyPressed",
            false,
            this::applyClosesWhenReturnKeyPressed,
            this::resetClosesWhenReturnKeyPressed
        )
        attributesBindingContext.bindStringAttribute(
            "returnKeyText",
            false,
            this::applyReturnKeyText,
            this::resetReturnKeyText
        )
        attributesBindingContext.bindColorAttribute(
            "placeholderColor",
            false,
            this::applyHintTextColor,
            this::resetHintTextColor
        )
        attributesBindingContext.bindStringAttribute(
            "autocapitalization",
            false,
            this::applyAutoCapitalization,
            this::resetAutoCapitalization
        )
        attributesBindingContext.bindStringAttribute(
            "autocorrection",
            false,
            this::applyAutocorrection,
            this::resetAutocorrection
        )
        attributesBindingContext.bindStringAttribute(
            "contentType",
            false,
            this::applyContentType,
            this::resetContentType
        )
        attributesBindingContext.bindBooleanAttribute(
            "selectTextOnFocus",
            false,
            this::applySelectTextOnFocus,
            this::resetSelectTextOnFocus
        )
        attributesBindingContext.bindColorAttribute(
            "tintColor", // iOS-only
            false,
            this::applyTintColor,
            this::resetTintColor
        )
        attributesBindingContext.bindStringAttribute(
            "keyboardAppearance", // iOS-only
            false,
            this::applyKeyboardAppearance,
            this::resetKeyboardAppearance
        )

        attributesBindingContext.setPlaceholderViewMeasureDelegate(lazy {
            ValdiEditText(context).apply {
                layoutParams = ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT
                )
            }
        })

        attributesBindingContext.bindUntypedAttribute(
            "selection",
            false,
            this::applySelection,
            this::resetSelection
        )
        attributesBindingContext.bindFunctionAttribute(
            "onSelectionChange",
            this::applyOnSelectionChange,
            this::resetOnSelectionChange
        )

        attributesBindingContext.bindBooleanAttribute(
            "enableInlinePredictions",
            false,
            this::applyEnableInlinePredictionsNoop,
            this::resetEnableInlinePredictionsNoop,
        )

        attributesBindingContext.bindColorAttribute(
                "backgroundEffectColor",
                true,
                this::applyBackgroundEffectColor,
                this::resetBackgroundEffectColor,
        )

        attributesBindingContext.bindFloatAttribute(
                "backgroundEffectBorderRadius",
                true,
                this::applyBackgroundEffectBorderRadius,
                this::resetBackgroundEffectBorderRadius,
        )

        attributesBindingContext.bindFloatAttribute(
                "backgroundEffectPadding",
                true,
                this::applyBackgroundEffectPadding,
                this::resetBackgroundEffectPadding,
        )


        this.valueAttributeId = attributesBindingContext.getBoundAttributeId("value")
    }

    private fun getTextViewHelper(view: ValdiEditText): TextViewHelper {
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

    private fun applyHint(view: ValdiEditText, value: String, animator: ValdiAnimator?) {
        view.hint = value
    }

    private fun resetHint(view: ValdiEditText, animator: ValdiAnimator?) {
        view.hint = null
    }

    private fun applyHintTextColor(view: ValdiEditText, value: Int, animator: ValdiAnimator?) {
        view.setHintTextColor(value)
    }

    private fun resetHintTextColor(view: ValdiEditText, animator: ValdiAnimator?) {
        view.setHintTextColor(Color.GRAY)
    }

    private fun applyAutoCapitalization(editText: ValdiEditText, value: String, animator: ValdiAnimator?) {
        val clearedInputType = editText.inputType and (
            InputType.TYPE_TEXT_FLAG_CAP_SENTENCES or
            InputType.TYPE_TEXT_FLAG_CAP_WORDS or
            InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS
        ).inv()
        when (value) {
            "sentences" -> {
                editText.inputType = clearedInputType or InputType.TYPE_TEXT_FLAG_CAP_SENTENCES
            }
            "words" -> {
                editText.inputType = clearedInputType or InputType.TYPE_TEXT_FLAG_CAP_WORDS
            }
            "characters" -> {
                editText.inputType = clearedInputType or InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS
            }
            "none" -> {
                editText.inputType = clearedInputType
            }
        }
    }

    private fun resetAutoCapitalization(editText: ValdiEditText, animator: ValdiAnimator?) {
        applyAutoCapitalization(editText, "sentences", animator)
    }

    private fun applyFocus(view: ValdiEditText, value: Boolean, animator: ValdiAnimator?) {
        if (value) {
            view.doFocus()
        } else {
            view.doUnfocus(ValdiEditText.UnfocusReason.Unknown)
        }
    }

    private fun resetFocus(view: ValdiEditText, animator: ValdiAnimator?) {
        applyFocus(view, false, animator)
    }

    private fun applyEnabled(view: ValdiEditText, value: Boolean, animator: ValdiAnimator?) {
        view.isFocusable = value
        view.isFocusableInTouchMode = value
    }

    private fun resetEnabled(view: ValdiEditText, animator: ValdiAnimator?) {
        applyEnabled(view, true, animator)
    }

    private fun applyOnWillChange(view: ValdiEditText, action: ValdiFunction) {
        view.onWillChangeFunction = action
    }

    private fun resetOnWillChange(view: ValdiEditText) {
        view.onWillChangeFunction = null
    }

    private fun applyOnChange(view: ValdiEditText, action: ValdiFunction) {
        view.onChangeFunction = action
    }

    private fun resetOnChange(view: ValdiEditText) {
        view.onChangeFunction = null
    }

    private fun applyOnEditBegin(view: ValdiEditText, function: ValdiFunction) {
        view.onEditBeginFunction = function
    }

    private fun resetOnEditBegin(view: ValdiEditText) {
        view.onEditBeginFunction = null
    }

    private fun applyOnEditEnd(view: ValdiEditText, function: ValdiFunction) {
        view.onEditEndFunction = function
    }

    private fun resetOnEditEnd(view: ValdiEditText) {
        view.onEditEndFunction = null
    }

    private fun applyOnReturn(view: ValdiEditText, function: ValdiFunction) {
        view.onReturnFunction = function
    }

    private fun resetOnReturn(view: ValdiEditText) {
        view.onReturnFunction = null
    }

    private fun applyOnWillDelete(view: ValdiEditText, function: ValdiFunction) {
        view.onWillDeleteFunction = function
    }

    private fun resetOnWillDelete(view: ValdiEditText) {
        view.onWillDeleteFunction = null
    }

    private fun applyValue(editText: ValdiEditText, value: Any?, animator: ValdiAnimator?) {
        val textViewHelper = getTextViewHelper(editText)
        textViewHelper.textValue = value
    }

    private fun resetValue(editText: ValdiEditText, animator: ValdiAnimator?) {
        editText.textViewHelper = null
        editText.setText("")
    }

    private fun applyCharacterLimit(editText: ValdiEditText, value: Int?, animator: ValdiAnimator?) {
        editText.setCharacterLimit(value)
        if (value == null) {
            editText.filters = emptyArray()
        } else {
            editText.filters = arrayOf(InputFilter.LengthFilter(value.toInt()))
        }
    }

    private fun resetCharacterLimit(editText: ValdiEditText, animator: ValdiAnimator?) {
        applyCharacterLimit(editText, null, animator)
    }

    private fun applyClosesWhenReturnKeyPressed(editText: ValdiEditText, value: Boolean, animator: ValdiAnimator?) {
        editText.closesWhenReturnKeyPressed = value
    }

    private fun resetClosesWhenReturnKeyPressed(editText: ValdiEditText, animator: ValdiAnimator?) {
        applyClosesWhenReturnKeyPressed(editText, editText.closesWhenReturnKeyPressedDefault, animator)
    }

    fun applyReturnKeyText(editText: ValdiEditText, value: String, animator: ValdiAnimator?) {
        editText.imeOptions = when (value) {
            "go" -> EditorInfo.IME_ACTION_GO
            "join" -> EditorInfo.IME_ACTION_NEXT
            "next" -> EditorInfo.IME_ACTION_NEXT
            "search" -> EditorInfo.IME_ACTION_SEARCH
            "send" -> EditorInfo.IME_ACTION_SEND
            "continue" -> EditorInfo.IME_ACTION_NEXT
            else -> EditorInfo.IME_ACTION_DONE
        }
    }

    fun resetReturnKeyText(editText: ValdiEditText, animator: ValdiAnimator?) {
        applyReturnKeyText(editText, "done", animator)
    }

    private fun applySelectTextOnFocus(editText: ValdiEditText, value: Boolean, animator: ValdiAnimator?) {
        editText.selectTextOnFocus = value
    }

    private fun resetSelectTextOnFocus(editText: ValdiEditText, animator: ValdiAnimator?) {
        applySelectTextOnFocus(editText, false, animator)
    }

    private fun applyAutocorrection(editText: ValdiEditText, value: String, animator: ValdiAnimator?) {
        val clearedInputType = editText.inputType and (
            InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS or
            InputType.TYPE_TEXT_FLAG_AUTO_CORRECT
        ).inv()
        when (value) {
            "none"-> {
                editText.inputType = clearedInputType or InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
            }
            else -> {
                editText.inputType = clearedInputType or InputType.TYPE_TEXT_FLAG_AUTO_CORRECT
            }
        }
    }

    private fun resetAutocorrection(editText: ValdiEditText, animator: ValdiAnimator?) {
        applyAutocorrection(editText, "default", animator)
    }

    private fun applyContentType(editText: ValdiEditText, value: String, animator: ValdiAnimator?) {
        val inputType = editText.inputType
        val clearedInputType = (inputType and InputType.TYPE_MASK_VARIATION.inv() and InputType.TYPE_MASK_CLASS.inv())

        when (value) {
            "phoneNumber"-> {
                editText.inputType = clearedInputType or InputType.TYPE_CLASS_PHONE
            }
            "password" -> {
                editText.inputType = clearedInputType or InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD
            }
            "email" -> {
                editText.inputType = clearedInputType or InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS
            }
            "url" -> {
                editText.inputType = clearedInputType or InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_URI
            }
            "number" -> {
                editText.inputType = clearedInputType or InputType.TYPE_CLASS_NUMBER
            }
            "numberDecimal" -> {
                editText.inputType = clearedInputType or InputType.TYPE_CLASS_NUMBER or InputType.TYPE_NUMBER_FLAG_DECIMAL
            }
            "numberDecimalSigned" -> {
                editText.inputType = clearedInputType or InputType.TYPE_CLASS_NUMBER or InputType.TYPE_NUMBER_FLAG_DECIMAL or InputType.TYPE_NUMBER_FLAG_SIGNED
            }
            "passwordNumber" -> {
                editText.inputType = clearedInputType or InputType.TYPE_CLASS_NUMBER or InputType.TYPE_NUMBER_VARIATION_PASSWORD
            }
            "passwordVisible" -> {
                editText.inputType = clearedInputType or InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
            }
            "noSuggestions" -> {
                editText.inputType = clearedInputType or
                        InputType.TYPE_CLASS_TEXT or
                        InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS or
                        InputType.TYPE_TEXT_VARIATION_FILTER or
                        InputType.TYPE_TEXT_FLAG_AUTO_CORRECT.inv()
            }
            else -> {
                editText.inputType = clearedInputType or InputType.TYPE_CLASS_TEXT
            }
        }
    }

    private fun resetContentType(editText: ValdiEditText, animator: ValdiAnimator?) {
        applyContentType(editText, "default", animator)
    }

    private fun applyTintColor(editText: ValdiEditText, value: Int, animator: ValdiAnimator?) {
        // TODO(simon): Implement
    }
    private fun resetTintColor(editText: ValdiEditText, animator: ValdiAnimator?) {
    }

    private fun applyKeyboardAppearance(editText: ValdiEditText, value: String, animator: ValdiAnimator?) {
        // TODO(vbrunet): Implement
    }
    private fun resetKeyboardAppearance(editText: ValdiEditText, animator: ValdiAnimator?) {
    }

    private fun applySelection(editText: ValdiEditText, selection: Any?, animator: ValdiAnimator?) {
        if (selection !is Array<*>) {
            resetSelection(editText, animator)
            return
        }

        if (selection.size != ValdiEditText.EXPECTED_SELECTION_DATA_SIZE) {
            throw AttributeError(
                "Selection should have two values in the given array: start + end"
            )
        }

        val start = (selection[0] as? Double)?.roundToInt() ?: 1
        val end = (selection[1] as? Double)?.roundToInt() ?: 1
       getTextViewHelper(editText).selection = Pair(start, end)
    }

    private fun resetSelection(editText: ValdiEditText, animator: ValdiAnimator?) {
        if (resetSelectionMatchesIos) {
            // Match iOS: clearing the `selection` attribute does not move the caret.
            // (iOS `valdi_setSelection` with an empty/invalid array returns early without changing `selectedRange`.)
            getTextViewHelper(editText).selection = null
        } else {
            editText.setSelection(0)
        }
    }

    private fun applyOnSelectionChange(editText: ValdiEditText, action: ValdiFunction) {
        editText.onSelectionChangeFunction = action
    }

    private fun resetOnSelectionChange(editText: ValdiEditText?) {
        editText?.onSelectionChangeFunction = null
    }

    private fun applyEnableInlinePredictionsNoop(view: ValdiEditText, value: Boolean, animator: ValdiAnimator?) {
        // No-op
    }

    private fun resetEnableInlinePredictionsNoop(view: ValdiEditText, animator: ValdiAnimator?) {
        // No-op
    }

    fun applyBackgroundEffectColor(view: ValdiEditText, value: Int, animator: ValdiAnimator?) {
        if (view.backgroundEffects == null) {
            view.backgroundEffects = ValdiTextViewBackgroundEffects()
        }
        view.backgroundEffects?.color = value
    }

    fun resetBackgroundEffectColor(view: ValdiEditText, animator: ValdiAnimator?) {
        view.backgroundEffects?.color = Color.TRANSPARENT
    }

    fun applyBackgroundEffectBorderRadius(view: ValdiEditText, value: Float, animator: ValdiAnimator?) {
        if (view.backgroundEffects == null) {
            view.backgroundEffects = ValdiTextViewBackgroundEffects()
        }
        view.backgroundEffects?.borderRadius = value * scaledDensity
    }

    fun resetBackgroundEffectBorderRadius(view: ValdiEditText, animator: ValdiAnimator?) {
        view.backgroundEffects?.borderRadius = 0f
    }

    fun applyBackgroundEffectPadding(view: ValdiEditText, value: Float, animator: ValdiAnimator?) {
        if (view.backgroundEffects == null) {
            view.backgroundEffects = ValdiTextViewBackgroundEffects()
        }
        view.backgroundEffects?.padding = value.toDouble() * scaledDensity
    }

    fun resetBackgroundEffectPadding(view: ValdiEditText, animator: ValdiAnimator?) {
        view.backgroundEffects?.padding = 0.0
    }
}
