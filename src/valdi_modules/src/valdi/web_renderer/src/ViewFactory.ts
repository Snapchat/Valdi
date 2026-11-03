import type { ViewFactory } from 'valdi_tsx/src/ViewFactory';
import type { AnyElementClass } from './core/ElementClass';

export class WebViewFactory implements ViewFactory {
  readonly __tag = 'ViewFactory';

  constructor(readonly elementClass: AnyElementClass) {}
}

export function createViewFactory(elementClass: AnyElementClass): ViewFactory {
  return new WebViewFactory(elementClass);
}
