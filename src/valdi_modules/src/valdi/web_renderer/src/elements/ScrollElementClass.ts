import { AttributesBinder } from '../attributes/AttributesBinder';
import { parseBoolean, parseNumber } from '../attributes/AttributeApplierHelpers';
import { AttributeApplierContext, CompositeAttribute, ElementClass, ElementLayoutObserver } from '../core/ElementClass';
import {
  assignStyles,
  AttributeApplierMap,
  createBaseElement,
  getActiveElement,
  replaceEventListener,
  setApplierCleanup,
} from './ElementClassSupport';
import { ViewElementClass } from './ViewElementClass';
import { injectScrollbarStyles } from '../styles/scrollbar';

interface ScrollState {
  canAlwaysScrollHorizontal: boolean;
  canAlwaysScrollVertical: boolean;
  contentOffsetAnimated: boolean;
  pendingContentOffsetX?: ScrollOffsetRequest;
  pendingContentOffsetY?: ScrollOffsetRequest;
  fadingEdgeLength: number;
  fadingEdgeStartEnabled: boolean;
  fadingEdgeEndEnabled: boolean;
  showsHorizontalScrollIndicator: boolean;
  showsVerticalScrollIndicator: boolean;
}

interface ScrollOffsetRequest {
  animated: boolean;
  value: number;
}

const SCROLL_STATE = '__scrollElementClassState';
const MOUSE_DRAG_SCROLL_THRESHOLD = 4;

function getScrollState(context: AttributeApplierContext): ScrollState {
  const existing = context.getState<ScrollState>(SCROLL_STATE);
  if (existing) {
    return existing;
  }
  const state: ScrollState = {
    canAlwaysScrollHorizontal: false,
    canAlwaysScrollVertical: false,
    contentOffsetAnimated: false,
    fadingEdgeLength: 0,
    fadingEdgeStartEnabled: true,
    fadingEdgeEndEnabled: true,
    showsHorizontalScrollIndicator: false,
    showsVerticalScrollIndicator: false,
  };
  context.setState(SCROLL_STATE, state);
  return state;
}

function resolveFadingEdgeMask(element: HTMLElement, state: ScrollState): string {
  if (state.fadingEdgeLength <= 0) {
    return '';
  }

  const length = `${state.fadingEdgeLength}px`;
  const isHorizontal = element.style.overflowX !== 'hidden';
  const offset = isHorizontal ? element.scrollLeft : element.scrollTop;
  const scrollSize = isHorizontal ? element.scrollWidth : element.scrollHeight;
  const clientSize = isHorizontal ? element.clientWidth : element.clientHeight;
  const maxOffset = Math.max(0, scrollSize - clientSize);
  const fadeStart = state.fadingEdgeStartEnabled && offset > 0;
  const fadeEnd = state.fadingEdgeEndEnabled && (maxOffset === 0 || offset < maxOffset - 0.5);
  const gradientDirection = isHorizontal ? 'to right' : 'to bottom';
  let gradientStops: string;
  if (fadeStart && fadeEnd) {
    gradientStops = `transparent, black ${length}, black calc(100% - ${length}), transparent`;
  } else if (fadeStart) {
    gradientStops = `transparent, black ${length}, black`;
  } else if (fadeEnd) {
    gradientStops = `black, black calc(100% - ${length}), transparent`;
  } else {
    return '';
  }

  return `linear-gradient(${gradientDirection}, ${gradientStops})`;
}

function updateFadingEdge(element: HTMLElement, context: AttributeApplierContext): void {
  const mask = resolveFadingEdgeMask(element, getScrollState(context));
  element.style.maskImage = mask;
  element.style.webkitMaskImage = mask;
}

class FadingEdgeLayoutObserver implements ElementLayoutObserver {
  private mask = '';

  constructor(
    private readonly element: HTMLElement,
    private readonly state: ScrollState,
  ) {}

  onSizeChanged(_width: number, _height: number): void {
    this.mask = resolveFadingEdgeMask(this.element, this.state);
  }

  onCommit(element: HTMLElement): void {
    element.style.maskImage = this.mask;
    element.style.webkitMaskImage = this.mask;
  }
}

function updateFadingEdgeScrollListener(element: HTMLElement, context: AttributeApplierContext): void {
  const state = getScrollState(context);
  if (state.fadingEdgeLength <= 0) {
    replaceEventListener(element, context, 'scroll:fadingEdge', 'scroll', undefined);
    return;
  }
  replaceEventListener(element, context, 'scroll:fadingEdge', 'scroll', () => updateFadingEdge(element, context));
}

function applyFadingEdgeConfiguration(
  element: HTMLElement,
  context: AttributeApplierContext,
  horizontal: boolean,
  fadingEdgeLength: number,
  fadingEdgeStartEnabled: boolean,
  fadingEdgeEndEnabled: boolean,
  attributeName: string,
): void {
  element.style.overflowX = horizontal ? 'auto' : 'hidden';
  element.style.overflowY = horizontal ? 'hidden' : 'auto';
  element.style.flexDirection = horizontal ? 'row' : 'column';
  const state = getScrollState(context);
  state.fadingEdgeLength = fadingEdgeLength;
  state.fadingEdgeStartEnabled = fadingEdgeStartEnabled;
  state.fadingEdgeEndEnabled = fadingEdgeEndEnabled;
  updateFadingEdgeScrollListener(element, context);
  if (fadingEdgeLength > 0) {
    context.setLayoutObserver(attributeName, new FadingEdgeLayoutObserver(element, state));
  } else {
    context.setLayoutObserver(attributeName, undefined);
    element.style.maskImage = '';
    element.style.webkitMaskImage = '';
  }
}

const fadingEdgeComposite: CompositeAttribute = {
  name: 'scrollFadingEdge',
  parts: [
    {
      name: 'horizontal',
      optional: true,
      layoutDependent: true,
      parse: (_element, value, name) => parseBoolean(value, name),
    },
    {
      name: 'fadingEdgeLength',
      optional: true,
      parse: (_element, value, name) => parseNumber(value, name),
    },
    {
      name: 'fadingEdgeStart',
      optional: true,
      parse: (_element, value, name) => parseBoolean(value, name),
    },
    {
      name: 'fadingEdgeEnd',
      optional: true,
      parse: (_element, value, name) => parseBoolean(value, name),
    },
  ],
  apply(element, values, attributeName, context) {
    applyFadingEdgeConfiguration(
      element,
      context,
      (values[0] as boolean | undefined) ?? false,
      (values[1] as number | undefined) ?? 0,
      (values[2] as boolean | undefined) ?? true,
      (values[3] as boolean | undefined) ?? true,
      attributeName,
    );
  },
  reset(element, attributeName, context) {
    applyFadingEdgeConfiguration(element, context, false, 0, true, true, attributeName);
  },
};

class ContentSizeLayoutObserver implements ElementLayoutObserver {
  onCommit: ((element: HTMLElement) => void) | undefined;
  private lastWidth: number | undefined;
  private lastHeight: number | undefined;
  private pendingWidth = 0;
  private pendingHeight = 0;

  constructor(
    private readonly element: HTMLElement,
    private readonly callback: Function,
  ) {}

  onSizeChanged(_width: number, _height: number): void {
    const width = this.element.scrollWidth;
    const height = this.element.scrollHeight;
    if (this.lastWidth === width && this.lastHeight === height) {
      this.onCommit = undefined;
      return;
    }
    this.pendingWidth = width;
    this.pendingHeight = height;
    this.onCommit = this.commit;
  }

  private readonly commit = (): void => {
    this.lastWidth = this.pendingWidth;
    this.lastHeight = this.pendingHeight;
    this.callback({ width: this.pendingWidth, height: this.pendingHeight });
  };
}

function getPendingScrollOffsetRequest(state: ScrollState, axis: 'x' | 'y'): ScrollOffsetRequest | undefined {
  return axis === 'x' ? state.pendingContentOffsetX : state.pendingContentOffsetY;
}

function setPendingScrollOffsetRequest(
  state: ScrollState,
  axis: 'x' | 'y',
  request: ScrollOffsetRequest | undefined,
): void {
  if (axis === 'x') {
    state.pendingContentOffsetX = request;
  } else {
    state.pendingContentOffsetY = request;
  }
}

function applyScrollOffsetNow(element: HTMLElement, axis: 'x' | 'y', value: number, animated: boolean): void {
  if (axis === 'x') {
    if (animated) {
      element.scrollTo({ left: value, behavior: 'smooth' });
    } else {
      element.scrollLeft = value;
    }
  } else if (animated) {
    element.scrollTo({ top: value, behavior: 'smooth' });
  } else {
    element.scrollTop = value;
  }
}

function applyScrollOffset(
  element: HTMLElement,
  context: AttributeApplierContext,
  axis: 'x' | 'y',
  value: number,
  animated: boolean,
): void {
  const state = getScrollState(context);
  const request: ScrollOffsetRequest = { animated, value };
  setPendingScrollOffsetRequest(state, axis, request);

  if (!animated) {
    applyScrollOffsetNow(element, axis, value, false);
  }

  context.enqueuePostLayoutCallback(() => {
    if (getPendingScrollOffsetRequest(state, axis) !== request) {
      return;
    }
    applyScrollOffsetNow(element, axis, request.value, request.animated);
  });
}

function resetScrollOffset(element: HTMLElement, context: AttributeApplierContext, axis: 'x' | 'y'): void {
  const state = getScrollState(context);
  setPendingScrollOffsetRequest(state, axis, undefined);
  if (axis === 'x') {
    element.scrollLeft = 0;
  } else {
    element.scrollTop = 0;
  }
}

function installScrollbarStyles(element: HTMLElement): void {
  const rootNode = element.getRootNode();
  if (typeof ShadowRoot !== 'undefined' && rootNode instanceof ShadowRoot) {
    injectScrollbarStyles(rootNode);
  } else if (typeof Document !== 'undefined' && rootNode instanceof Document) {
    injectScrollbarStyles(rootNode);
  } else if (typeof document !== 'undefined') {
    injectScrollbarStyles(document);
  }
}

function updateScrollIndicators(element: HTMLElement, context: AttributeApplierContext): void {
  const state = getScrollState(context);
  const hideHorizontal = !state.showsHorizontalScrollIndicator && !state.canAlwaysScrollHorizontal;
  const hideVertical = !state.showsVerticalScrollIndicator && !state.canAlwaysScrollVertical;
  if (hideHorizontal) {
    element.classList.add('hide-h-scrollbar');
  } else {
    element.classList.remove('hide-h-scrollbar');
  }
  if (hideVertical) {
    element.classList.add('hide-v-scrollbar');
  } else {
    element.classList.remove('hide-v-scrollbar');
  }
  element.style.setProperty('scrollbar-width', hideHorizontal && hideVertical ? 'none' : 'auto');
}

function isNonSelectableScrollTarget(container: HTMLElement, target: EventTarget | null): boolean {
  let current = target as HTMLElement | null;
  while (current !== null && current !== container) {
    if (current.style?.userSelect === 'none') {
      return true;
    }
    current = current.parentElement;
  }
  return false;
}

function installMouseDragScrolling(element: HTMLElement, context: AttributeApplierContext): void {
  let startX: number | undefined;
  let startY = 0;
  let startScrollLeft = 0;
  let startScrollTop = 0;
  let suppressClick = false;

  const handleMouseDown = (event: MouseEvent): void => {
    suppressClick = false;
    if (event.button !== 0 || !isNonSelectableScrollTarget(element, event.target)) {
      startX = undefined;
      return;
    }
    startX = event.clientX;
    startY = event.clientY;
    startScrollLeft = element.scrollLeft;
    startScrollTop = element.scrollTop;
  };

  const handleMouseMove = (event: MouseEvent): void => {
    if (startX === undefined || event.buttons !== 1) {
      return;
    }

    const deltaX = event.clientX - startX;
    const deltaY = event.clientY - startY;
    if (Math.abs(deltaX) < MOUSE_DRAG_SCROLL_THRESHOLD && Math.abs(deltaY) < MOUSE_DRAG_SCROLL_THRESHOLD) {
      return;
    }

    const maxScrollLeft = Math.max(0, element.scrollWidth - element.clientWidth);
    const maxScrollTop = Math.max(0, element.scrollHeight - element.clientHeight);
    if (maxScrollLeft === 0 && maxScrollTop === 0) {
      return;
    }

    element.scrollLeft = Math.max(0, Math.min(maxScrollLeft, startScrollLeft - deltaX));
    element.scrollTop = Math.max(0, Math.min(maxScrollTop, startScrollTop - deltaY));
    suppressClick = true;
    event.preventDefault();
  };

  const handleMouseEnd = (): void => {
    startX = undefined;
  };

  const handleClick = (event: MouseEvent): void => {
    if (!suppressClick) {
      return;
    }
    suppressClick = false;
    event.preventDefault();
    event.stopImmediatePropagation();
  };

  element.addEventListener('mousedown', handleMouseDown);
  element.addEventListener('mousemove', handleMouseMove);
  element.addEventListener('mouseup', handleMouseEnd);
  element.addEventListener('mouseleave', handleMouseEnd);
  element.addEventListener('click', handleClick, true);
  setApplierCleanup(context, 'scroll:mouseDragScrolling', () => {
    element.removeEventListener('mousedown', handleMouseDown);
    element.removeEventListener('mousemove', handleMouseMove);
    element.removeEventListener('mouseup', handleMouseEnd);
    element.removeEventListener('mouseleave', handleMouseEnd);
    element.removeEventListener('click', handleClick, true);
  });
}

function buildScrollAttributeAppliers(viewElementClass: ViewElementClass): AttributeApplierMap {
  const binder = new AttributesBinder<HTMLElement>();
  binder.bindFunctionAttribute(
    'onScroll',
    (element, callback, context) => {
      replaceEventListener(element, context, 'scroll:onScroll', 'scroll', () => {
        callback({
          contentOffset: {
            x: element.scrollLeft,
            y: element.scrollTop,
          },
          contentSize: {
            width: element.scrollWidth,
            height: element.scrollHeight,
          },
          layoutMeasurement: {
            width: element.clientWidth,
            height: element.clientHeight,
          },
        });
      });
    },
    (element, context) => {
      replaceEventListener(element, context, 'scroll:onScroll', 'scroll', undefined);
    },
  );
  binder.bindFunctionAttribute(
    'onScrollEnd',
    (element, callback, context) => {
      let timer: number | undefined;
      const listener = () => {
        if (timer !== undefined) {
          clearTimeout(timer);
        }
        timer = window.setTimeout(() => callback(), 100);
      };
      element.addEventListener('scroll', listener);
      setApplierCleanup(context, 'scroll:onScrollEnd', () => {
        element.removeEventListener('scroll', listener);
        if (timer !== undefined) {
          clearTimeout(timer);
        }
      });
    },
    (_element, context) => {
      setApplierCleanup(context, 'scroll:onScrollEnd', undefined);
    },
  );
  binder.bindFunctionAttribute(
    'onDragStart',
    (element, callback, context) => {
      replaceEventListener(element, context, 'scroll:onDragStartMouse', 'mousedown', event => callback(event));
      replaceEventListener(element, context, 'scroll:onDragStartTouch', 'touchstart', event => callback(event));
    },
    (element, context) => {
      replaceEventListener(element, context, 'scroll:onDragStartMouse', 'mousedown', undefined);
      replaceEventListener(element, context, 'scroll:onDragStartTouch', 'touchstart', undefined);
    },
  );
  binder.bindFunctionAttribute(
    'onDragEnd',
    (element, callback, context) => {
      replaceEventListener(element, context, 'scroll:onDragEndMouse', 'mouseup', event => callback(event));
      replaceEventListener(element, context, 'scroll:onDragEndTouch', 'touchend', event => callback(event));
    },
    (element, context) => {
      replaceEventListener(element, context, 'scroll:onDragEndMouse', 'mouseup', undefined);
      replaceEventListener(element, context, 'scroll:onDragEndTouch', 'touchend', undefined);
    },
  );
  binder.bindFunctionAttribute(
    'onDragEnding',
    (element, callback, context) => {
      let timer: number | undefined;
      const listener = () => {
        if (timer !== undefined) {
          clearTimeout(timer);
        }
        timer = window.setTimeout(() => callback(), 150);
      };
      element.addEventListener('scroll', listener);
      setApplierCleanup(context, 'scroll:onDragEnding', () => {
        element.removeEventListener('scroll', listener);
        if (timer !== undefined) {
          clearTimeout(timer);
        }
      });
    },
    (_element, context) => {
      setApplierCleanup(context, 'scroll:onDragEnding', undefined);
    },
  );
  binder.bindFunctionAttribute(
    'onContentSizeChange',
    (element, callback, context, attributeName) => {
      context.setLayoutObserver(attributeName, new ContentSizeLayoutObserver(element, callback));
    },
    (_element, context, attributeName) => {
      context.setLayoutObserver(attributeName, undefined);
    },
  );
  binder.bindBooleanAttribute(
    'bounces',
    (element, value) => {
      element.style.overscrollBehavior = value ? 'auto' : 'contain';
    },
    element => {
      element.style.overscrollBehavior = '';
    },
  );
  binder.bindNoOpAttribute('bouncesFromDragAtStart');
  binder.bindNoOpAttribute('bouncesFromDragAtEnd');
  binder.bindNoOpAttribute('bouncesVerticalWithSmallContent');
  binder.bindNoOpAttribute('bouncesHorizontalWithSmallContent');
  binder.bindBooleanAttribute(
    'cancelsTouchesOnScroll',
    (element, enabled, context) => {
      if (enabled) {
        installMouseDragScrolling(element, context);
      } else {
        setApplierCleanup(context, 'scroll:mouseDragScrolling', undefined);
      }
    },
    (_element, context) => {
      setApplierCleanup(context, 'scroll:mouseDragScrolling', undefined);
    },
  );
  binder.bindBooleanAttribute(
    'dismissKeyboardOnDrag',
    (element, value, context) => {
      if (!value) {
        setApplierCleanup(context, 'scroll:dismissKeyboardOnDrag', undefined);
        return;
      }
      const listener = () => {
        const activeElement = getActiveElement(element);
        if (activeElement instanceof HTMLElement) {
          activeElement.blur();
        }
      };
      element.addEventListener('scroll', listener);
      setApplierCleanup(context, 'scroll:dismissKeyboardOnDrag', () => {
        element.removeEventListener('scroll', listener);
      });
    },
    (_element, context) => {
      setApplierCleanup(context, 'scroll:dismissKeyboardOnDrag', undefined);
    },
  );
  binder.bindBooleanAttribute(
    'pagingEnabled',
    (element, value) => {
      if (value) {
        element.style.scrollSnapType = element.style.overflowX === 'hidden' ? 'y mandatory' : 'x mandatory';
      } else {
        element.style.scrollSnapType = '';
      }
    },
    element => {
      element.style.scrollSnapType = '';
    },
  );
  binder.bindBooleanAttribute(
    'showsVerticalScrollIndicator',
    (element, value, context) => {
      installScrollbarStyles(element);
      getScrollState(context).showsVerticalScrollIndicator = value;
      updateScrollIndicators(element, context);
    },
    (element, context) => {
      installScrollbarStyles(element);
      getScrollState(context).showsVerticalScrollIndicator = false;
      updateScrollIndicators(element, context);
    },
  );
  binder.bindBooleanAttribute(
    'showsHorizontalScrollIndicator',
    (element, value, context) => {
      installScrollbarStyles(element);
      getScrollState(context).showsHorizontalScrollIndicator = value;
      updateScrollIndicators(element, context);
    },
    (element, context) => {
      installScrollbarStyles(element);
      getScrollState(context).showsHorizontalScrollIndicator = false;
      updateScrollIndicators(element, context);
    },
  );
  binder.bindBooleanAttribute(
    'canAlwaysScrollHorizontal',
    (element, value, context) => {
      getScrollState(context).canAlwaysScrollHorizontal = value;
      element.style.overflowX = value ? 'scroll' : 'auto';
      updateScrollIndicators(element, context);
    },
    (element, context) => {
      getScrollState(context).canAlwaysScrollHorizontal = false;
      element.style.overflowX = '';
      updateScrollIndicators(element, context);
    },
  );
  binder.bindBooleanAttribute(
    'canAlwaysScrollVertical',
    (element, value, context) => {
      getScrollState(context).canAlwaysScrollVertical = value;
      element.style.overflowY = value ? 'scroll' : 'auto';
      updateScrollIndicators(element, context);
    },
    (element, context) => {
      getScrollState(context).canAlwaysScrollVertical = false;
      element.style.overflowY = '';
      updateScrollIndicators(element, context);
    },
  );
  binder.bindBooleanAttribute(
    'scrollEnabled',
    (element, enabled) => {
      element.style.overflow = enabled ? 'auto' : 'hidden';
    },
    element => {
      element.style.overflow = '';
    },
  );
  binder.bindNoOpAttribute('ref');
  binder.bindNoOpAttribute('scrollPerfLoggerBridge');
  binder.bindNoOpAttribute('circularRatio');
  binder.bindNoOpAttribute('decelerationRate');
  binder.bindNoOpAttribute('viewportExtensionTop');
  binder.bindNoOpAttribute('viewportExtensionRight');
  binder.bindNoOpAttribute('viewportExtensionBottom');
  binder.bindNoOpAttribute('viewportExtensionLeft');
  binder.bindNumberAttribute(
    'contentOffsetX',
    (element, value, context) => {
      applyScrollOffset(element, context, 'x', value, getScrollState(context).contentOffsetAnimated);
    },
    (element, context) => {
      resetScrollOffset(element, context, 'x');
    },
  );
  binder.bindNumberAttribute(
    'contentOffsetY',
    (element, value, context) => {
      applyScrollOffset(element, context, 'y', value, getScrollState(context).contentOffsetAnimated);
    },
    (element, context) => {
      resetScrollOffset(element, context, 'y');
    },
  );
  binder.bindBooleanAttribute(
    'contentOffsetAnimated',
    (_element, value, context) => {
      getScrollState(context).contentOffsetAnimated = value;
    },
    (_element, context) => {
      getScrollState(context).contentOffsetAnimated = false;
    },
  );
  binder.bindNoOpAttribute('staticContentWidth');
  binder.bindNoOpAttribute('staticContentHeight');
  return { ...viewElementClass.attributeAppliers, ...binder.attributeAppliers };
}

export class ScrollElementClass extends ElementClass {
  constructor(viewElementClass: ViewElementClass) {
    super('scroll', buildScrollAttributeAppliers(viewElementClass), {
      ...viewElementClass.compositeAttributes,
      [fadingEdgeComposite.name]: fadingEdgeComposite,
    });
  }

  protected onCreateElement(): HTMLElement {
    const element = createBaseElement('div');
    installScrollbarStyles(element);
    element.classList.add('hide-v-scrollbar', 'hide-h-scrollbar');
    assignStyles(element, {
      overflowX: 'hidden',
      overflowY: 'auto',
      pointerEvents: 'auto',
      scrollbarWidth: 'none',
    });
    return element;
  }
}
