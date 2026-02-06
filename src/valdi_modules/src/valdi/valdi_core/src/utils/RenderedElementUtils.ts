import { ElementFrame } from 'valdi_tsx/src/Geometry';
import { Point, Size } from '../Geometry';
import { IRenderedElement } from '../IRenderedElement';

export namespace RenderedElementUtils {
  /**
   * Compute a relative position within the element tree by recursively looking through the parents
   */
  export function relativePositionTo(parent: IRenderedElement, child: IRenderedElement): Point | undefined {
    let current: IRenderedElement | undefined = child;
    const position = { x: 0, y: 0 };
    while (current) {
      if (current == parent) {
        return position;
      }
      const contentOffsetX = current.getAttribute('contentOffsetX') ?? 0;
      const contentOffsetY = current.getAttribute('contentOffsetY') ?? 0;
      const translationX = current.getAttribute('translationX') ?? 0;
      const translationY = current.getAttribute('translationY') ?? 0;
      position.x += current.frame.x - contentOffsetX + translationX;
      position.y += current.frame.y - contentOffsetY + translationY;
      current = current.parent;
    }
    return undefined;
  }
  /**
   * Check if a relative position is within the bounds of a frame
   */
  export function frameContainsPosition(frame: ElementFrame, position: Point): boolean {
    if (position.x > frame.width || position.y > frame.height) {
      return false;
    }
    if (position.x < 0 || position.y < 0) {
      return false;
    }
    return true;
  }
  /**
   * Extract the origin of an element frame
   */
  export function framePosition(frame: ElementFrame): Point {
    return frame;
  }
  /**
   * Extract the size of an element frame
   */
  export function frameSize(frame: ElementFrame): Size {
    return frame;
  }

  /**
   * Get the root element of the tree by walking up the parent chain.
   * Returns undefined if the element is undefined.
   */
  export function rootElement(element: IRenderedElement | undefined): IRenderedElement | undefined {
    let current = element;
    while (current?.parent) {
      current = current.parent;
    }
    return current;
  }

  /**
   * Get the nearest ancestor with a valid (non-zero) frame width by walking up the parent chain.
   * This is useful when the actual root element may have a zero-width frame.
   * Returns undefined if no element with a valid frame width is found.
   */
  export function rootElementWithFrame(element: IRenderedElement | undefined): IRenderedElement | undefined {
    let current = element;
    let lastWithValidWidth: IRenderedElement | undefined;

    while (current) {
      if (current.frame?.width) {
        lastWithValidWidth = current;
      }
      if (!current.parent) {
        break;
      }
      current = current.parent;
    }

    return lastWithValidWidth;
  }

  /**
   * Compute the absolute position of an element by walking up to the root of the tree.
   * This accounts for contentOffset and translation attributes at each level.
   * Returns {x: 0, y: 0} if the element has no frame.
   */
  export function absolutePosition(element: IRenderedElement | undefined): Point {
    const position = { x: 0, y: 0 };
    let current = element;

    while (current) {
      if (!current.frame) {
        break;
      }
      // Only subtract contentOffset if it belongs to a parent (affecting the current element's position)
      // The element's own contentOffset does not affect its own absolute frame position
      const isTargetElement = current === element;
      const contentOffsetX = isTargetElement ? 0 : (current.getAttribute('contentOffsetX') ?? 0);
      const contentOffsetY = isTargetElement ? 0 : (current.getAttribute('contentOffsetY') ?? 0);
      const translationX = current.getAttribute('translationX') ?? 0;
      const translationY = current.getAttribute('translationY') ?? 0;
      position.x += current.frame.x - contentOffsetX + translationX;
      position.y += current.frame.y - contentOffsetY + translationY;
      current = current.parent;
    }

    return position;
  }
}
