import { AttributesBinder } from '../attributes/AttributesBinder';
import { parseCssTrackList } from '../attributes/AttributeApplierHelpers';
import type { ElementFrame } from 'valdi_tsx/src/Geometry';
import {
  AttributeApplier,
  AttributeApplierContext,
  CompositeAttribute,
  ElementClass,
  ElementLayoutObserver,
} from '../core/ElementClass';
import { isPlainCssNumber, parseCssFunctionCall, readCssNumber, skipCssWhitespace } from '../utils/cssScanner';
import { AttributeApplierMap, createBaseElement, replaceEventListener } from './ElementClassSupport';
import { getViewPresentationState } from './ViewElementState';

const MEASURE_MODE_UNSPECIFIED = 0;
const MEASURE_MODE_EXACTLY = 1;
const MEASURE_MODE_AT_MOST = 2;

function mapFlexWrap(value: string): string {
  return value === 'no-wrap' ? 'nowrap' : value;
}

function mapViewOverflow(value: string): string {
  return value === 'scroll' ? 'visible' : value;
}

const VIEW_LAZY_SIZE_STATE = '__viewElementClassLazySizeState';
const VIEW_GRID_COMPAT_STATE = '__viewElementClassGridCompatState';

interface ViewLazySizeState {
  estimatedWidthStyle?: string;
  estimatedHeightStyle?: string;
}

interface ViewGridCompatState {
  gridTemplateColumnsCss?: string;
  gridTemplateColumnsSource?: string;
  updateScheduled: boolean;
}

function getViewLazySizeState(context: AttributeApplierContext): ViewLazySizeState {
  const existing = context.getState<ViewLazySizeState>(VIEW_LAZY_SIZE_STATE);
  if (existing) {
    return existing;
  }
  const state: ViewLazySizeState = {};
  context.setState(VIEW_LAZY_SIZE_STATE, state);
  return state;
}

function getViewGridCompatState(context: AttributeApplierContext): ViewGridCompatState {
  const existing = context.getState<ViewGridCompatState>(VIEW_GRID_COMPAT_STATE);
  if (existing) {
    return existing;
  }
  const state: ViewGridCompatState = { updateScheduled: false };
  context.setState(VIEW_GRID_COMPAT_STATE, state);
  return state;
}

function skipOptionalLengthUnit(value: string, index: number): number {
  if (value.startsWith('px', index) || value.startsWith('pt', index)) {
    return index + 2;
  }
  return index;
}

function readCssNumberWithOptionalLengthUnit(
  value: string,
  index: number,
): { value: number; nextIndex: number } | undefined {
  const number = readCssNumber(value, index);
  if (!number) {
    return undefined;
  }
  return { value: number.value, nextIndex: skipOptionalLengthUnit(value, number.nextIndex) };
}

function parseMinmaxOneFlexibleTrack(
  value: string,
  index: number,
): { minTrack: number; nextIndex: number } | undefined {
  const parsed = parseCssFunctionCall(value, index);
  if (!parsed || parsed.name !== 'minmax' || parsed.parameters.length !== 2) {
    return undefined;
  }

  const minTrack = readCssNumberWithOptionalLengthUnit(
    parsed.parameters[0],
    skipCssWhitespace(parsed.parameters[0], 0),
  );
  if (!minTrack || skipCssWhitespace(parsed.parameters[0], minTrack.nextIndex) !== parsed.parameters[0].length) {
    return undefined;
  }
  if (parsed.parameters[1].trim() !== '1fr') {
    return undefined;
  }
  return { minTrack: minTrack.value, nextIndex: parsed.nextIndex };
}

function parseRepeatTwoMinmaxFrFixed(value: string): { minTrack: number; fixedTrack: number } | undefined {
  const repeat = parseCssFunctionCall(value, 0);
  if (
    !repeat ||
    repeat.name !== 'repeat' ||
    repeat.parameters.length !== 2 ||
    repeat.parameters[0].trim() !== '2' ||
    skipCssWhitespace(value, repeat.nextIndex) !== value.length
  ) {
    return undefined;
  }

  const repeatedTrack = repeat.parameters[1];
  const minmax = parseMinmaxOneFlexibleTrack(repeatedTrack, 0);
  if (!minmax) {
    return undefined;
  }
  const fixedTrackStartIndex = skipCssWhitespace(repeatedTrack, minmax.nextIndex);
  if (fixedTrackStartIndex === minmax.nextIndex) {
    return undefined;
  }
  const fixedTrack = readCssNumberWithOptionalLengthUnit(repeatedTrack, fixedTrackStartIndex);
  if (!fixedTrack) {
    return undefined;
  }
  if (skipCssWhitespace(repeatedTrack, fixedTrack.nextIndex) !== repeatedTrack.length) {
    return undefined;
  }

  return { minTrack: minmax.minTrack, fixedTrack: fixedTrack.value };
}

function applyMeasuredSize(element: HTMLElement, result: unknown, widthMode: number, heightMode: number): boolean {
  const width = Array.isArray(result)
    ? Number(result[0])
    : typeof result === 'object' && result !== null && 'width' in result
      ? Number((result as { width: unknown }).width)
      : NaN;
  const height = Array.isArray(result)
    ? Number(result[1])
    : typeof result === 'object' && result !== null && 'height' in result
      ? Number((result as { height: unknown }).height)
      : NaN;

  let changed = false;
  if (widthMode !== MEASURE_MODE_EXACTLY && Number.isFinite(width) && width >= 0) {
    const value = `${width}px`;
    if (element.style.width !== value) {
      element.style.width = value;
      changed = true;
    }
  }
  if (heightMode !== MEASURE_MODE_EXACTLY && Number.isFinite(height) && height >= 0) {
    const value = `${height}px`;
    if (element.style.height !== value) {
      element.style.height = value;
      changed = true;
    }
  }
  return changed;
}

function getChildElement(element: HTMLElement, index: number): HTMLElement | undefined {
  const child = element.children?.item(index) ?? element.childNodes.item(index);
  return child && typeof child === 'object' && 'style' in child ? (child as HTMLElement) : undefined;
}

function childSpansRepeatedFlexibleTrackEnd(element: HTMLElement): boolean {
  for (let index = 0; ; index++) {
    const child = getChildElement(element, index);
    if (!child) {
      return false;
    }
    if (
      child.style.gridColumnStart === '3' &&
      (child.style.gridColumnEnd === '5' || child.style.gridColumnEnd === 'span 2')
    ) {
      return true;
    }
  }
}

function yogaCompatibleGridTemplateColumns(element: HTMLElement, state: ViewGridCompatState): string | undefined {
  const source = state.gridTemplateColumnsSource;
  const css = state.gridTemplateColumnsCss;
  if (!source || !css || !childSpansRepeatedFlexibleTrackEnd(element)) {
    return css;
  }
  const repeatTracks = parseRepeatTwoMinmaxFrFixed(source);
  if (!repeatTracks) {
    return css;
  }
  const { minTrack, fixedTrack } = repeatTracks;
  return `${minTrack + fixedTrack / 2}px ${fixedTrack}px minmax(0, 1fr) ${fixedTrack}px`;
}

function updateGridTemplateColumnsForYogaCompatibility(element: HTMLElement, state: ViewGridCompatState): void {
  const value = yogaCompatibleGridTemplateColumns(element, state);
  if (value !== undefined && element.style.gridTemplateColumns !== value) {
    element.style.gridTemplateColumns = value;
  }
}

function scheduleGridTemplateColumnsCompatibilityUpdate(element: HTMLElement, state: ViewGridCompatState): void {
  if (state.updateScheduled) {
    return;
  }
  state.updateScheduled = true;
  Promise.resolve().then(() => {
    state.updateScheduled = false;
    updateGridTemplateColumnsForYogaCompatibility(element, state);
  });
}

function gridTemplateColumnsAttributeApplier(): AttributeApplier {
  return {
    layoutDependent: true,
    apply(element, value, attributeName, context) {
      const state = getViewGridCompatState(context);
      state.gridTemplateColumnsSource = parseCssTrackList(value, attributeName);
      state.gridTemplateColumnsCss = state.gridTemplateColumnsSource;
      element.style.gridTemplateColumns = state.gridTemplateColumnsCss;
      scheduleGridTemplateColumnsCompatibilityUpdate(element, state);
    },
    reset(element, _attributeName, context) {
      const state = getViewGridCompatState(context);
      state.gridTemplateColumnsSource = undefined;
      state.gridTemplateColumnsCss = undefined;
      element.style.gridTemplateColumns = '';
    },
  };
}

class OnMeasureLayoutObserver implements ElementLayoutObserver {
  private width = 0;
  private widthMode = MEASURE_MODE_UNSPECIFIED;
  private height = 0;
  private heightMode = MEASURE_MODE_UNSPECIFIED;

  constructor(
    private readonly callback: Function,
    private readonly requestLayoutPass: () => void,
  ) {}

  onMeasure(element: HTMLElement): void {
    const rect = element.getBoundingClientRect();
    const parentRect = rect.width <= 0 || rect.height <= 0 ? element.parentElement?.getBoundingClientRect() : undefined;
    const parentWidth = parentRect?.width ?? 0;
    const parentHeight = parentRect?.height ?? 0;
    this.width = rect.width > 0 ? rect.width : parentWidth;
    this.widthMode = rect.width > 0 || parentWidth > 0 ? MEASURE_MODE_EXACTLY : MEASURE_MODE_UNSPECIFIED;
    this.height = rect.height > 0 ? rect.height : parentHeight;
    this.heightMode =
      rect.height > 0 ? MEASURE_MODE_EXACTLY : parentHeight > 0 ? MEASURE_MODE_AT_MOST : MEASURE_MODE_UNSPECIFIED;
  }

  onCommit(element: HTMLElement): void {
    const result = this.callback(this.width, this.widthMode, this.height, this.heightMode);
    if (applyMeasuredSize(element, result, this.widthMode, this.heightMode)) {
      this.requestLayoutPass();
    }
  }
}

function applyEstimatedSize(
  element: HTMLElement,
  context: AttributeApplierContext,
  axis: 'width' | 'height',
  value: number,
): void {
  const state = getViewLazySizeState(context);
  const styleValue = `${value}px`;
  if (axis === 'width') {
    element.style.containIntrinsicWidth = styleValue;
    state.estimatedWidthStyle = styleValue;
  } else {
    element.style.containIntrinsicHeight = styleValue;
    if (!element.style.height) {
      element.style.height = styleValue;
      state.estimatedHeightStyle = styleValue;
    }
  }
}

function resetEstimatedSize(element: HTMLElement, context: AttributeApplierContext, axis: 'width' | 'height'): void {
  const state = getViewLazySizeState(context);
  if (axis === 'width') {
    element.style.containIntrinsicWidth = '';
    if (state.estimatedWidthStyle !== undefined && element.style.width === state.estimatedWidthStyle) {
      element.style.width = '';
    }
    state.estimatedWidthStyle = undefined;
  } else {
    element.style.containIntrinsicHeight = '';
    if (state.estimatedHeightStyle !== undefined && element.style.height === state.estimatedHeightStyle) {
      element.style.height = '';
    }
    state.estimatedHeightStyle = undefined;
  }
}

const layoutCompositeAttributes: Readonly<Record<string, CompositeAttribute>> = {};

const LAYOUT_DEPENDENT = true;

function buildLayoutAttributeAppliers(): AttributeApplierMap {
  const binder = new AttributesBinder<HTMLElement>();
  binder.bindNoOpAttribute('allowReuse');
  bindLifecycleFunctionAttribute(binder, 'onViewCreate', (context, callback) => {
    context.emitCurrentViewCreate(callback);
  });
  bindLifecycleFunctionAttribute(binder, 'onViewDestroy', undefined);
  bindLifecycleFunctionAttribute(binder, 'onViewChange', context => {
    context.emitCurrentViewChange();
  });
  binder.bindCssLengthStyleAttribute('width', 'width', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('height', 'height', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('minWidth', 'minWidth', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('minHeight', 'minHeight', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('maxWidth', 'maxWidth', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('maxHeight', 'maxHeight', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('top', 'top', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('right', 'right', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('bottom', 'bottom', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('left', 'left', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('margin', 'margin', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('marginTop', 'marginTop', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('marginRight', 'marginRight', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('marginBottom', 'marginBottom', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('marginLeft', 'marginLeft', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('padding', 'padding', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('paddingTop', 'paddingTop', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('paddingRight', 'paddingRight', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('paddingBottom', 'paddingBottom', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('paddingLeft', 'paddingLeft', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('gap', 'gap', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('rowGap', 'rowGap', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('columnGap', 'columnGap', LAYOUT_DEPENDENT);
  binder.bindCssLengthStyleAttribute('flexBasis', 'flexBasis', LAYOUT_DEPENDENT);
  binder.bindAttribute('gridTemplateColumns', gridTemplateColumnsAttributeApplier());
  binder.bindCssTrackListStyleAttribute('gridTemplateRows', 'gridTemplateRows', LAYOUT_DEPENDENT);
  binder.bindCssTrackListStyleAttribute('gridAutoColumns', 'gridAutoColumns', LAYOUT_DEPENDENT);
  binder.bindCssTrackListStyleAttribute('gridAutoRows', 'gridAutoRows', LAYOUT_DEPENDENT);
  binder.bindStyleValueAttribute('gridColumnStart', 'gridColumnStart', LAYOUT_DEPENDENT);
  binder.bindStyleValueAttribute('gridColumnEnd', 'gridColumnEnd', LAYOUT_DEPENDENT);
  binder.bindStyleValueAttribute('gridRowStart', 'gridRowStart', LAYOUT_DEPENDENT);
  binder.bindStyleValueAttribute('gridRowEnd', 'gridRowEnd', LAYOUT_DEPENDENT);
  binder.bindNumberAttribute(
    'aspectRatio',
    (element, value) => {
      element.style.aspectRatio = String(value);
    },
    element => {
      element.style.aspectRatio = '';
    },
    LAYOUT_DEPENDENT,
  );
  binder.bindEnumAttribute(
    'position',
    ['relative', 'absolute'] as const,
    (element, value) => {
      element.style.position = value;
    },
    element => {
      element.style.position = 'relative';
    },
    LAYOUT_DEPENDENT,
  );
  binder.bindEnumAttribute(
    'display',
    ['flex', 'grid', 'none'] as const,
    (element, value) => {
      element.style.display = value;
    },
    element => {
      element.style.display = 'flex';
    },
    LAYOUT_DEPENDENT,
  );
  binder.bindStyleValueAttribute('flexDirection', 'flexDirection', LAYOUT_DEPENDENT);
  binder.bindStringAttribute(
    'flexWrap',
    (element, value) => {
      element.style.flexWrap = mapFlexWrap(value);
    },
    element => {
      element.style.flexWrap = '';
    },
    LAYOUT_DEPENDENT,
  );
  binder.bindNumberAttribute(
    'flexGrow',
    (element, value) => {
      element.style.flexGrow = String(value);
    },
    element => {
      element.style.flexGrow = '';
    },
    LAYOUT_DEPENDENT,
  );
  binder.bindNumberAttribute(
    'flexShrink',
    (element, value) => {
      element.style.flexShrink = String(value);
    },
    element => {
      element.style.flexShrink = '0';
    },
    LAYOUT_DEPENDENT,
  );
  binder.bindStyleValueAttribute('justifyContent', 'justifyContent', LAYOUT_DEPENDENT);
  binder.bindStyleValueAttribute('justifyItems', 'justifyItems', LAYOUT_DEPENDENT);
  binder.bindStyleValueAttribute('justifySelf', 'justifySelf', LAYOUT_DEPENDENT);
  binder.bindStyleValueAttribute('alignContent', 'alignContent', LAYOUT_DEPENDENT);
  binder.bindStyleValueAttribute('alignItems', 'alignItems', LAYOUT_DEPENDENT);
  binder.bindStyleValueAttribute('alignSelf', 'alignSelf', LAYOUT_DEPENDENT);
  binder.bindStyleValueAttribute('direction', 'direction', LAYOUT_DEPENDENT);
  binder.bindStringAttribute(
    'overflow',
    (element, value, context) => {
      const state = getViewPresentationState(context);
      state.overflow = mapViewOverflow(value);
      element.style.overflow = state.slowClipping ? 'hidden' : state.overflow;
    },
    (element, context) => {
      const state = getViewPresentationState(context);
      state.overflow = undefined;
      element.style.overflow = state.slowClipping ? 'hidden' : 'visible';
    },
    LAYOUT_DEPENDENT,
  );
  binder.bindStringAttribute(
    'colorPaletteName',
    (_element, value, context) => {
      context.setColorPalette(value);
    },
    (_element, context) => {
      context.setColorPalette(undefined);
    },
  );
  binder.bindNumberAttribute(
    'zIndex',
    (element, value) => {
      element.style.zIndex = String(value);
    },
    element => {
      element.style.zIndex = '0';
    },
  );

  binder.bindDirectAttribute('id', 'id');
  binder.bindDirectAttribute('key', 'data-key');
  binder.bindDirectAttribute('accessibilityId', 'id');
  binder.bindStringAttribute(
    'class',
    (element, value) => {
      element.className = value;
    },
    element => {
      element.className = '';
    },
    LAYOUT_DEPENDENT,
  );
  binder.bindDirectAttribute('accessibilityLabel', 'aria-label');
  binder.bindDirectAttribute('accessibilityCategory', 'aria-roledescription');
  binder.bindDirectAttribute('accessibilityRole', 'role');
  binder.bindDirectAttribute('accessibilityHint', 'title');
  binder.bindDirectAttribute('accessibilityValue', 'aria-valuetext');
  binder.bindDirectAttribute('accessibilityStateSelected', 'aria-selected');
  binder.bindDirectAttribute('accessibilityStateChecked', 'aria-checked');
  binder.bindAriaBooleanAttribute('accessibilityStateDisabled', 'aria-disabled');
  binder.bindAriaBooleanAttribute('accessibilityStateExpanded', 'aria-expanded');
  binder.bindAttribute('accessibilityStateLiveRegion', {
    apply(element, value) {
      element.setAttribute('aria-live', value ? 'polite' : 'off');
    },
    reset(element) {
      element.removeAttribute('aria-live');
    },
  });
  binder.bindAriaBooleanAttribute('accessibilityHidden', 'aria-hidden');
  binder.bindAriaBooleanAttribute('accessibilityElementsHidden', 'aria-hidden');
  binder.bindAriaBooleanAttribute('accessibilityViewIsModal', 'aria-modal');
  binder.bindNoOpAttribute('accessibilityIgnoresInvertColors');
  binder.bindNoOpAttribute('accessibilityTraits');
  binder.bindNoOpAttribute('accessibilityNavigation');
  binder.bindNoOpAttribute('accessibilityPriority');
  binder.bindNoOpAttribute('onAccessibilityMagicTap');
  binder.bindNoOpAttribute('onAccessibilityIncrement');
  binder.bindNoOpAttribute('onAccessibilityDecrement');
  binder.bindFunctionAttribute(
    'onAccessibilityTap',
    (element, callback, context) => {
      replaceEventListener(element, context, 'view:onAccessibilityTap', 'click', event => callback(event));
    },
    (element, context) => {
      replaceEventListener(element, context, 'view:onAccessibilityTap', 'click', undefined);
    },
  );
  binder.bindFunctionAttribute(
    'onAccessibilityActivate',
    (element, callback, context) => {
      replaceEventListener(element, context, 'view:onAccessibilityActivate', 'click', event => callback(event));
    },
    (element, context) => {
      replaceEventListener(element, context, 'view:onAccessibilityActivate', 'click', undefined);
    },
  );
  binder.bindFunctionAttribute(
    'onAccessibilityEscape',
    (element, callback, context) => {
      replaceEventListener(element, context, 'view:onAccessibilityEscape', 'keydown', event => {
        if (event.key === 'Escape') {
          callback(event);
        }
      });
    },
    (element, context) => {
      replaceEventListener(element, context, 'view:onAccessibilityEscape', 'keydown', undefined);
    },
  );
  binder.bindFunctionAttribute(
    'onLayout',
    (_element, callback, context) => {
      context.setOnLayoutCallback(callback as (frame: ElementFrame) => void);
    },
    (_element, context) => {
      context.setOnLayoutCallback(undefined);
    },
  );
  // Renderer owns these callbacks and registers the element with VisibilityObserverController.
  binder.bindNoOpAttribute('onVisibilityChanged');
  binder.bindNoOpAttribute('onViewportChanged');
  binder.bindFunctionAttribute(
    'onLayoutComplete',
    (_element, callback) => {
      requestAnimationFrame(() => callback());
    },
    () => {},
  );
  binder.bindBooleanAttribute(
    'lazyLayout',
    element => {
      element.style.removeProperty('content-visibility');
    },
    element => {
      element.style.removeProperty('content-visibility');
    },
    LAYOUT_DEPENDENT,
  );
  binder.bindBooleanAttribute(
    'lazy',
    element => {
      element.style.removeProperty('content-visibility');
    },
    element => {
      element.style.removeProperty('content-visibility');
    },
    LAYOUT_DEPENDENT,
  );
  binder.bindFunctionAttribute(
    'onMeasure',
    (_element, callback, context, attributeName) => {
      context.setLayoutObserver(
        attributeName,
        new OnMeasureLayoutObserver(callback, () => {
          context.requestLayoutPass();
        }),
      );
    },
    (_element, context, attributeName) => {
      context.setLayoutObserver(attributeName, undefined);
    },
    LAYOUT_DEPENDENT,
  );
  binder.bindNumberAttribute(
    'estimatedWidth',
    (element, value, context) => {
      applyEstimatedSize(element, context, 'width', value);
    },
    (element, context) => {
      resetEstimatedSize(element, context, 'width');
    },
    LAYOUT_DEPENDENT,
  );
  binder.bindNumberAttribute(
    'estimatedHeight',
    (element, value, context) => {
      applyEstimatedSize(element, context, 'height', value);
    },
    (element, context) => {
      resetEstimatedSize(element, context, 'height');
    },
    LAYOUT_DEPENDENT,
  );
  binder.bindBooleanAttribute(
    'limitToViewport',
    (element, value) => {
      element.style.overflow = value ? 'hidden' : 'visible';
    },
    element => {
      element.style.overflow = '';
    },
  );
  binder.bindBooleanAttribute(
    'animationsEnabled',
    (_element, value, context) => {
      context.setAnimationsEnabled(value);
    },
    (_element, context) => {
      context.setAnimationsEnabled(true);
    },
  );
  binder.bindNoOpAttribute('ignoreParentViewport');
  binder.bindStyleValueAttribute('flex', 'flex', LAYOUT_DEPENDENT);

  return binder.attributeAppliers;
}

function bindLifecycleFunctionAttribute(
  binder: AttributesBinder<HTMLElement>,
  name: string,
  onApply: ((context: AttributeApplierContext, callback: Function) => void) | undefined,
): void {
  binder.bindFunctionAttribute(
    name,
    (_element, callback, context) => {
      onApply?.(context, callback);
    },
    () => {},
  );
}

export class LayoutElementClass extends ElementClass {
  constructor(
    className: string,
    additionalAttributeAppliers: AttributeApplierMap,
    additionalCompositeAttributes: Readonly<Record<string, CompositeAttribute>>,
  ) {
    super(
      className,
      { ...buildLayoutAttributeAppliers(), ...additionalAttributeAppliers },
      { ...layoutCompositeAttributes, ...additionalCompositeAttributes },
    );
  }

  protected onCreateElement(): HTMLElement {
    return createBaseElement('div');
  }
}
