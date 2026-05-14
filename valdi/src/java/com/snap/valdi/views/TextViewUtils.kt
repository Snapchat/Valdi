package com.snap.valdi.views

import android.text.TextDirectionHeuristic
import android.text.TextDirectionHeuristics
import android.text.TextUtils
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import com.snap.valdi.utils.ValdiTextDirectionHeuristic

object TextViewUtils {

    /**
     * Configure the text view with default parameters so that it can measure correctly
     */
    fun configure(textView: TextView) {
        textView.apply {
            maxLines = 1
            ellipsize = TextUtils.TruncateAt.END
            includeFontPadding = false
            textDirection = View.TEXT_DIRECTION_LOCALE
            textAlignment = View.TEXT_ALIGNMENT_VIEW_START
            gravity = Gravity.CENTER_VERTICAL
            layoutParams = ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT
            )
        }
    }

    fun resolveTextDirectionHeuristic(textDirectionHeuristic: TextDirectionHeuristic): TextDirectionHeuristic {
        return if (textDirectionHeuristic === TextDirectionHeuristics.LOCALE) {
            ValdiTextDirectionHeuristic
        } else {
            return textDirectionHeuristic
        }
    }

    fun resolveTextDirectionHeuristic(textView: TextView): TextDirectionHeuristic {
        return when (textView.textDirection) {
            View.TEXT_DIRECTION_ANY_RTL -> TextDirectionHeuristics.ANYRTL_LTR
            View.TEXT_DIRECTION_LTR -> TextDirectionHeuristics.LTR
            View.TEXT_DIRECTION_RTL -> TextDirectionHeuristics.RTL
            View.TEXT_DIRECTION_FIRST_STRONG_LTR -> TextDirectionHeuristics.FIRSTSTRONG_LTR
            View.TEXT_DIRECTION_FIRST_STRONG_RTL -> TextDirectionHeuristics.FIRSTSTRONG_RTL
            TextView.TEXT_DIRECTION_LOCALE -> ValdiTextDirectionHeuristic
            else -> ValdiTextDirectionHeuristic
        }
    }

    /**
     * Resolves the height measure spec for the given text view.
     * This will make sure the TextView measures like on iOS, with a height of 0 if no text is set.
     */
    fun resolveHeightMeasureSpec(textView: TextView, heightMeasureSpec: Int): Int {
        return if (textView.text.isNullOrEmpty() && View.MeasureSpec.getMode(heightMeasureSpec) != View.MeasureSpec.EXACTLY) {
            // iOS like behavior: When text is empty, label height should be zero.
            View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.EXACTLY)
        } else {
            heightMeasureSpec
        }
    }

}
