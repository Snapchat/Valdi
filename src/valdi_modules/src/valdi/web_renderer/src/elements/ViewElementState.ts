import type { AttributeApplierContext } from '../core/ElementClass';

const VIEW_PRESENTATION_STATE = '__viewElementClassPresentationState';
const VIEW_PAINT_ELEMENT_STATE = '__viewElementClassPaintElement';

export interface ViewPresentationState {
  borderRadiusCss: string | undefined;
  boxShadow: string | undefined;
  boxShadowElement: HTMLElement | undefined;
  overflow: string | undefined;
  slowClipping: boolean;
}

export function getViewPresentationState(context: AttributeApplierContext): ViewPresentationState {
  const existing = context.getState<ViewPresentationState>(VIEW_PRESENTATION_STATE);
  if (existing) {
    return existing;
  }
  const state: ViewPresentationState = {
    borderRadiusCss: undefined,
    boxShadow: undefined,
    boxShadowElement: undefined,
    overflow: undefined,
    slowClipping: false,
  };
  context.setState(VIEW_PRESENTATION_STATE, state);
  return state;
}

export function getViewPaintElement(context: AttributeApplierContext): HTMLElement | undefined {
  return context.getState<HTMLElement>(VIEW_PAINT_ELEMENT_STATE);
}

export function setViewPaintElement(context: AttributeApplierContext, element: HTMLElement): void {
  context.setState(VIEW_PAINT_ELEMENT_STATE, element);
}
