/**
 * Represents an inline image attachment that can be embedded within attributed text.
 * Used for rendering images (like LaTeX equations) inline with text using native text layout.
 */
export interface AttributedTextInlineImageAttachment {
  /** Unique identifier for the image attachment */
  attachmentId: string;
  /** Width of the image in logical pixels */
  width: number;
  /** Height of the image in logical pixels */
  height: number;
  /** PNG image data. If undefined, a placeholder will be shown. */
  imageData?: Uint8Array;
}
