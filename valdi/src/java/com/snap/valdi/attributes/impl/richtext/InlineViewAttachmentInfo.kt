package com.snap.valdi.attributes.impl.richtext

/**
 * Vertical placement modes for a Valdi child view embedded in a text line.
 */
enum class InlineViewVerticalAlignment(val rawValue: Int) {
    Center(0),
    Top(1),
    Bottom(2),
    Baseline(3);

    companion object {
        fun fromRawValue(rawValue: Int): InlineViewVerticalAlignment {
            return when (rawValue) {
                Top.rawValue -> Top
                Bottom.rawValue -> Bottom
                Baseline.rawValue -> Baseline
                else -> Center
            }
        }
    }
}

/**
 * Runtime metadata for one inline-view attributed-text part.
 *
 * The parser resolves the referenced Valdi child index and its Yoga-measured
 * size before Android text layout runs. [InlineViewAttachmentSpan] reserves
 * this space in TextView, and ValdiTextViewBase later places the real child
 * view at the frame produced by text layout.
 */
class InlineViewAttachmentInfo(
    val childIndex: Int,
    val verticalAlignment: InlineViewVerticalAlignment,
    val width: Float,
    val height: Float
)
