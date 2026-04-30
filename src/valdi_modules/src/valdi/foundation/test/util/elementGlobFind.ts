import { IRenderedElement } from 'valdi_core/src/IRenderedElement';
import { globToRegex } from './globToRegex';

/**
 * Find elements by globbing their relative path
 * Allow search by glob wildcard (like in file system)
 */
export function elementGlobFind<T = any>(element: IRenderedElement | IRenderedElement[], pattern: string): IRenderedElement<T>[] {
  const possibilities: { path: string; element: IRenderedElement }[] = [];

  const recursor = (current: IRenderedElement, path: string) => {
    const currentName = current.viewClass + ':' + current.key;
    const newPath = path + currentName;
    possibilities.push({ path: newPath, element: current });
    for (const child of current.children) {
      recursor(child, newPath + '/');
    }
  };

  if (element instanceof Array) {
    for (const item of element) {
      recursor(item, '');
    }
  } else {
    recursor(element, '');
  }

  const regex = globToRegex(pattern);
  const results: IRenderedElement<T>[] = [];
  for (const possibility of possibilities) {
    if (regex.test(possibility.path)) {
      results.push(possibility.element);
    }
  }
  return results;
}
