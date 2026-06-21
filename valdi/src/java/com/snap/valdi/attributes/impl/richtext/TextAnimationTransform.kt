package com.snap.valdi.attributes.impl.richtext

data class TextAnimationTransform(
    val key: String?,
    val translationY: Float,
    val scale: Float,
    val opacity: Float,
    val duration: Double,
    val timeOffsetBetweenParts: Double,
    val groupIndex: Int,
    val partIndexInGroup: Int,
    val partPattern: String?
)
