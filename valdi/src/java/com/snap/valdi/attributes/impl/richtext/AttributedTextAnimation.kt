package com.snap.valdi.attributes.impl.richtext

import android.text.style.UpdateAppearance

class AttributedTextAnimation(
    val key: String,
    val timelineKey: String,
    val startTransform: TextAnimationTransform
) : UpdateAppearance {
    var translationY: Float = startTransform.translationY
        internal set
    var scale: Float = startTransform.scale
        internal set
    var opacity: Float = startTransform.opacity
        internal set
    var progress: Float = 0f
        internal set

    internal var startTimeMillis: Long = 0L
    internal var scheduledStartTimeMillis: Long = 0L
    internal var active: Boolean = true
    internal var rangeStart: Int = 0
    internal var rangeEnd: Int = 0
}
