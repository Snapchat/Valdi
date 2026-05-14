import { IRenderedElement } from 'valdi_core/src/IRenderedElement';

/**
 * Find elements with a specific key recursively
 */
export function elementKeyFind<T = any>(element: IRenderedElement | IRenderedElement[], key: string): IRenderedElement<T>[] {
  const results: IRenderedElement<T>[] = [];

  const recursor = (current: IRenderedElement) => {
    if (current.key === key) {
      results.push(current);
    }
    for (const child of current.children) {
      recursor(child);
    }
  };

  if (element instanceof Array) {
    for (const item of element) {
      recursor(item);
    }
  } else {
    recursor(element);
  }

  return results;
}
