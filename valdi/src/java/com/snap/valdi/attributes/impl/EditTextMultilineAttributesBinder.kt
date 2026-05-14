package com.snap.valdi.attributes.impl

import android.content.Context
import android.text.InputType
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import com.snap.valdi.actions.ValdiAction
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.attributes.AttributesBindingContext
import com.snap.valdi.attributes.impl.animations.ValdiAnimator
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.views.ValdiEditText
import com.snap.valdi.views.ValdiEditTextMultiline

/**
 * Binds attributes for the EditTextMultiline's view class
 */
class EditTextMultilineAttributesBinder(
    private val context: Context,
    private val editTextAttributesBinder: EditTextAttributesBinder
) : AttributesBinder<ValdiEditTextMultiline> {

    override val viewClass: Class<ValdiEditTextMultiline>
        get() = ValdiEditTextMultiline::class.java

    override fun bindAttributes(attributesBindingContext: AttributesBindingContext<ValdiEditTextMultiline>) {
        attributesBindingContext.bindStringAttribute(
            "returnType",
            false,
            this::applyReturnType,
            this::resetReturnType
        )

        attributesBindingContext.bindStringAttribute(
            "textGravity",
            false,
            this::applyTextGravity,
            this::resetTextGravity
        )

        attributesBindingContext.bindStringAttribute(
            "contentType",
            false,
            this::applyContentType,
            this::resetContentType,
        )

        attributesBindingContext.setPlaceholderViewMeasureDelegate(lazy {
            ValdiEditTextMultiline(context).apply {
                layoutParams = ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT
                )
            }
        })
    }

    private fun applyReturnType(editText: ValdiEditTextMultiline, value: String, animator: ValdiAnimator?) {
        if (value == "linereturn") {
            editText.allowLineReturns(true)
            editTextAttributesBinder.applyReturnKeyText(editText, "done", animator)
        } else {
            editText.allowLineReturns(false)
            editTextAttributesBinder.applyReturnKeyText(editText, value, animator)
        }
    }

    private fun resetReturnType(editText: ValdiEditTextMultiline, animator: ValdiAnimator?) {
        applyReturnType(editText, "linereturn", animator)
    }

    private fun applyTextGravity(editText: ValdiEditTextMultiline, value: String, animator: ValdiAnimator?) {
        editText.gravity = when (value) {
            "top" -> Gravity.TOP
            "center" -> Gravity.CENTER_VERTICAL
            "bottom" -> Gravity.BOTTOM
            else -> Gravity.CENTER_VERTICAL
        }
    }

    private fun resetTextGravity(editText: ValdiEditTextMultiline, animator: ValdiAnimator?) {
        applyTextGravity(editText, "center", animator)
    }

    private fun applyContentType(editText: ValdiEditTextMultiline, value: String, animator: ValdiAnimator?) {
        val cleared = editText.inputType and InputType.TYPE_MASK_VARIATION.inv() and InputType.TYPE_MASK_CLASS.inv()
        editText.privateImeOptions = null
        editText.disableMediaContent = false
        when (value) {
            "noSuggestions" -> {
                editText.inputType = (cleared or
                        InputType.TYPE_CLASS_TEXT or
                        InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS or
                        InputType.TYPE_TEXT_VARIATION_FILTER) and
                        InputType.TYPE_TEXT_FLAG_AUTO_CORRECT.inv()
                editText.privateImeOptions = "disableSticker=true;disableGifKeyboard=true"
                editText.disableMediaContent = true
            }
            else -> editText.inputType = cleared or InputType.TYPE_CLASS_TEXT
        }
    }

    private fun resetContentType(editText: ValdiEditTextMultiline, animator: ValdiAnimator?) {
        applyContentType(editText, "default", animator)
    }
}
