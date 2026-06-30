package com.snap.valdi.views

import android.content.Context
import android.content.res.ColorStateList
import android.graphics.Color
import android.graphics.PorterDuff
import android.os.Build
import android.widget.ProgressBar
import androidx.annotation.Keep

@Keep
class ValdiSpinnerView(context: Context) :
        ProgressBar(context, null, android.R.attr.progressBarStyleSmall),
        ValdiRecyclableView {

    init {
        isIndeterminate = true
        resetColor()
    }

    fun setColor(color: Int) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            indeterminateTintList = ColorStateList.valueOf(color)
        } else {
            indeterminateDrawable?.setColorFilter(color, PorterDuff.Mode.SRC_IN)
        }
    }

    fun resetColor() {
        setColor(Color.WHITE)
    }
}
