import { IComponent } from 'valdi_core/src/IComponent';
import { IRenderedElement } from 'valdi_core/src/IRenderedElement';
import { IRenderedElementViewClass } from 'valdi_test/test/IRenderedElementViewClass';
import { componentGetElements } from './componentGetElements';
import { isRenderedElement } from './isRenderedElement';
import { ElementForViewClass } from 'valdi_test/test/ElementForViewClass';

type Node = IComponent | IRenderedElement;

/**
 * Find elements with an type recursively
 */
export function elementTypeFind<T extends IRenderedElementViewClass>(element: Node | Node[], viewClass: T): IRenderedElement<ElementForViewClass<T>>[] {
  const results: IRenderedElement<ElementForViewClass<T>>[] = [];

  const recursor = (current: Node) => {
    const isElement = isRenderedElement(current);

    if (isElement && current.viewClass === viewClass) {
      results.push(current);
    }
    const children = isElement ? current.children : componentGetElements(current);
    children.forEach(child => recursor(child));
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