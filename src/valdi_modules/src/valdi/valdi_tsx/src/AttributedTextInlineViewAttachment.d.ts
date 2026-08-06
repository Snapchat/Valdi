/**
 * Represents how an inline Valdi child view is vertically aligned within
 * the surrounding text run.
 */
export const enum AttributedTextInlineViewVerticalAlignment {
  Center = 0,
  Top = 1,
  Bottom = 2,
  Baseline = 3,
}

/**
 * Represents an inline Valdi child view embedded within attributed text.
 * The childIndex points at the text element's rendered child order.
 */
export interface AttributedTextInlineViewAttachment {
  /** Index of the child view to place at this attributed text position. */
  childIndex: number;
  /** Vertical alignment of the child view within the surrounding text run. */
  verticalAlignment: AttributedTextInlineViewVerticalAlignment;
}
