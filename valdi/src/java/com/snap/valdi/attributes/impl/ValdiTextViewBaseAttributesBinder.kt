package com.snap.valdi.attributes.impl

import android.content.Context
import com.snap.valdi.attributes.AttributesBindingContext
import com.snap.valdi.attributes.impl.animations.ValdiAnimator
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.exceptions.AttributeError
import com.snap.valdi.logger.Logger
import com.snap.valdi.views.ValdiTextSelection
import com.snap.valdi.views.ValdiTextViewBase
import kotlin.math.roundToInt

/**
 * Attribute binder for properties shared by all Android text controls backed by
 * ValdiTextViewBase.
 *
 * The text rendering attributes (value, fontAttributes, textShadow, textGradient, textOverflow)
 * live in [AbstractTextViewAttributesBinder] so host-app text views can reuse them. This subclass
 * adds the selection/editing attributes that only ValdiTextViewBase-backed controls expose.
 */
class ValdiTextViewBaseAttributesBinder(
    context: Context,
    fontManager: FontManager,
    defaultAttributes: FontAttributes,
    logger: Logger,
) : AbstractTextViewAttributesBinder<ValdiTextViewBase>(context, fontManager, defaultAttributes, logger) {

    override val viewClass: Class<ValdiTextViewBase>
        get() = ValdiTextViewBase::class.java

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
        val helper = getTextViewHelper(view)
        if (helper.matchIosTextSetCaret) {
            // Match iOS: clearing the `selection` attribute does not move the caret. iOS
            // `valdi_setSelection` returns early on an empty/invalid array without touching
            // `selectedRange`. master did this in EditTextAttributesBinder.resetSelection; moving the
            // reset here dropped it.
            //
            // The flag lives on the shared per-view TextViewHelper and is only set by
            // EditTextAttributesBinder.getTextViewHelper, not by the base binder's. In practice the
            // EditText binder touches the helper while binding `value`/`hint`, so it is set by the
            // time this runs; if it somehow is not, this falls back to the pre-tweak behaviour rather
            // than misbehaving. Labels never set it, which keeps them on master's behaviour.
            helper.selection = null
        } else {
            view.setValdiSelection(0, 0)
        }
    }

    fun applyOnSelectionChange(view: ValdiTextViewBase, action: ValdiFunction) {
        view.onSelectionChangeFunction = action
    }

    fun resetOnSelectionChange(view: ValdiTextViewBase) {
        view.onSelectionChangeFunction = null
    }

    override fun bindAttributes(attributesBindingContext: AttributesBindingContext<ValdiTextViewBase>) {
        bindTextAttributes(attributesBindingContext)
        attributesBindingContext.bindBooleanAttribute("selectable", false, this::applySelectable, this::resetSelectable)
        attributesBindingContext.bindUntypedAttribute("selection", false, this::applySelection, this::resetSelection)
        attributesBindingContext.bindFunctionAttribute("onSelectionChange", this::applyOnSelectionChange, this::resetOnSelectionChange)
    }
}
