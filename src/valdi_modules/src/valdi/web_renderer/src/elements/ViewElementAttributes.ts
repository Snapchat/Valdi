import {
  AttributesBinder,
  MIN_VISIBLE_CHANGE_ALPHA,
  MIN_VISIBLE_CHANGE_COLOR,
  MIN_VISIBLE_CHANGE_PIXEL,
} from '../attributes/AttributesBinder';
import { parseCssLength, parseNumber, resolveValdiGradientAngles } from '../attributes/AttributeApplierHelpers';
import { createBorderRadiusAttributeApplier } from '../attributes/BorderRadiusAttribute';
import type { AttributeApplier, AttributeApplierContext, CompositeAttribute } from '../core/ElementClass';
import { TouchEventState } from 'valdi_tsx/src/GestureEvents';
import type { DragEvent as ValdiDragEvent, TouchEvent as ValdiTouchEvent } from 'valdi_tsx/src/GestureEvents';
import { geometricPathToSvgPath, isGeometricPathValue, SvgGeometricPath } from '../utils/geometricPath';
import { isPlainCssNumber, readWhitespaceSeparatedToken, skipCssWhitespace } from '../utils/cssScanner';
import { injectTouchAreaStyles } from '../styles/touchAreaExtension';
import { AttributeApplierMap, borderAttributeApplier, replaceEventListener } from './ElementClassSupport';
import { getViewPaintElement, getViewPresentationState } from './ViewElementState';

const SVG_NS = 'http://www.w3.org/2000/svg';
const VIEW_INTERACTION_STATE = '__viewElementClassInteractionState';
const VIEW_MASK_STATE = '__viewElementClassMaskState';
const VIEW_BORDER_STYLE_STATE = '__viewElementClassBorderStyle';

interface ViewInteractionState {
  defaultPointerEvents?: string;
  documentDragCleanup?: () => void;
  dragSession?: BrowserDragSession;
  hitTest?: boolean;
  longPressDuration: number;
  onTapDisabled: boolean;
  onDoubleTapDisabled: boolean;
  onLongPressDisabled: boolean;
  onDragDisabled: boolean;
  onTouchActive?: boolean;
  onTouchStartActive?: boolean;
  onTouchEndActive?: boolean;
  onTap?: (event: unknown) => void;
  onTapPredicate?: (event: unknown) => boolean;
  onDoubleTap?: (event: unknown) => void;
  onDoubleTapPredicate?: (event: unknown) => boolean;
  onLongPress?: (event: unknown) => void;
  onLongPressPredicate?: (event: unknown) => boolean;
  onDrag?: (event: unknown) => void;
  onDragPredicate?: (event: unknown) => boolean;
  longPressTimer?: number;
  gestureListenersInstalled?: boolean;
  suppressNextTap?: boolean;
  touchEnabled?: boolean;
  touchAreaExtension?: { top: number; right: number; bottom: number; left: number };
}

interface BrowserDragSession {
  readonly originX: number;
  readonly originY: number;
  previousTime: number;
  previousX: number;
  previousY: number;
  lastEvent?: MouseEvent | TouchEvent;
}

interface BrowserPointerPosition {
  readonly clientX: number;
  readonly clientY: number;
}

interface ViewMaskState {
  maskOpacity: number;
  path?: SvgGeometricPath;
}

function getViewInteractionState(context: AttributeApplierContext): ViewInteractionState {
  const existing = context.getState<ViewInteractionState>(VIEW_INTERACTION_STATE);
  if (existing) {
    return existing;
  }
  const state: ViewInteractionState = {
    longPressDuration: 500,
    onTapDisabled: false,
    onDoubleTapDisabled: false,
    onLongPressDisabled: false,
    onDragDisabled: false,
  };
  context.setState(VIEW_INTERACTION_STATE, state);
  return state;
}

function getViewMaskState(context: AttributeApplierContext): ViewMaskState {
  const existing = context.getState<ViewMaskState>(VIEW_MASK_STATE);
  if (existing) {
    return existing;
  }
  const state: ViewMaskState = { maskOpacity: 1 };
  context.setState(VIEW_MASK_STATE, state);
  return state;
}

function updateViewPointerEvents(element: HTMLElement, state: ViewInteractionState): void {
  if (state.defaultPointerEvents === undefined) {
    state.defaultPointerEvents = element.style.pointerEvents || 'none';
  }
  if (state.touchEnabled === false || state.hitTest === false) {
    element.style.pointerEvents = 'none';
    return;
  }
  const hasInteraction =
    state.touchEnabled === true ||
    state.hitTest === true ||
    state.onTouchActive === true ||
    state.onTouchStartActive === true ||
    state.onTouchEndActive === true ||
    state.onTap !== undefined ||
    state.onDoubleTap !== undefined ||
    state.onLongPress !== undefined ||
    state.onDrag !== undefined;
  element.style.pointerEvents = hasInteraction ? 'auto' : state.defaultPointerEvents;
}

function escapeSvgAttribute(value: string): string {
  return value.split('&').join('&amp;').split('"').join('&quot;').split('<').join('&lt;');
}

function clampMaskOpacity(value: number): number {
  return Math.max(0, Math.min(1, value));
}

function clearMask(element: HTMLElement): void {
  element.style.removeProperty('mask-image');
  element.style.removeProperty('mask-mode');
  element.style.removeProperty('mask-position');
  element.style.removeProperty('mask-repeat');
  element.style.removeProperty('mask-size');
  element.style.removeProperty('-webkit-mask-image');
  element.style.removeProperty('-webkit-mask-position');
  element.style.removeProperty('-webkit-mask-repeat');
  element.style.removeProperty('-webkit-mask-size');
  element.style.removeProperty('-webkit-mask-source-type');
}

function maskDataUrl(path: SvgGeometricPath, maskOpacity: number): string {
  const svg = `<svg xmlns="${SVG_NS}" viewBox="${escapeSvgAttribute(path.viewBox)}" preserveAspectRatio="${escapeSvgAttribute(path.preserveAspectRatio)}"><rect width="100%" height="100%" fill="white"/><path d="${escapeSvgAttribute(path.d)}" fill="black" fill-opacity="${clampMaskOpacity(maskOpacity)}"/></svg>`;
  return `url("data:image/svg+xml,${encodeURIComponent(svg)}")`;
}

function updateMask(element: HTMLElement, context: AttributeApplierContext): void {
  const state = getViewMaskState(context);
  if (!state.path || !state.path.d) {
    clearMask(element);
    return;
  }
  const image = maskDataUrl(state.path, state.maskOpacity);
  element.style.setProperty('mask-image', image);
  element.style.setProperty('mask-mode', 'luminance');
  element.style.setProperty('mask-position', 'center');
  element.style.setProperty('mask-repeat', 'no-repeat');
  element.style.setProperty('mask-size', '100% 100%');
  element.style.setProperty('-webkit-mask-image', image);
  element.style.setProperty('-webkit-mask-position', 'center');
  element.style.setProperty('-webkit-mask-repeat', 'no-repeat');
  element.style.setProperty('-webkit-mask-size', '100% 100%');
  element.style.setProperty('-webkit-mask-source-type', 'luminance');
}

function createTouchEvent(
  element: HTMLElement,
  event: MouseEvent | TouchEvent,
  state: TouchEventState,
  activePointers?: ReadonlyMap<number, BrowserPointerPosition>,
): ValdiTouchEvent {
  const touch = 'touches' in event ? event.touches[0] || event.changedTouches[0] : event;
  const rect = element.getBoundingClientRect();
  const pointerLocations = activePointers
    ? Array.from(activePointers, ([pointerId, pointer]) => ({
        pointerId,
        x: pointer.clientX - rect.left,
        y: pointer.clientY - rect.top,
      }))
    : 'touches' in event
      ? Array.from(event.touches, pointer => ({
          pointerId: pointer.identifier,
          x: pointer.clientX - rect.left,
          y: pointer.clientY - rect.top,
        }))
      : [];
  return {
    state,
    x: touch.clientX - rect.left,
    y: touch.clientY - rect.top,
    absoluteX: touch.clientX,
    absoluteY: touch.clientY,
    pointerCount: activePointers ? activePointers.size : 'touches' in event ? event.touches.length : 1,
    pointerLocations,
  };
}

function runDragCallback(
  element: HTMLElement,
  callback: ((event: unknown) => void) | undefined,
  predicate: ((event: unknown) => boolean) | undefined,
  event: MouseEvent | TouchEvent,
  state: TouchEventState,
  session: BrowserDragSession,
): void {
  if (!callback) {
    return;
  }

  const touchEvent = createTouchEvent(element, event, state);
  const time = Number.isFinite(event.timeStamp) ? event.timeStamp : Date.now();
  const elapsedSeconds = Math.max((time - session.previousTime) / 1000, 0.001);
  const dragEvent: ValdiDragEvent = {
    ...touchEvent,
    deltaX: touchEvent.x - session.originX,
    deltaY: touchEvent.y - session.originY,
    velocityX: state === TouchEventState.Started ? 0 : (touchEvent.x - session.previousX) / elapsedSeconds,
    velocityY: state === TouchEventState.Started ? 0 : (touchEvent.y - session.previousY) / elapsedSeconds,
  };
  session.previousTime = time;
  session.previousX = touchEvent.x;
  session.previousY = touchEvent.y;
  if (!predicate || predicate(dragEvent)) {
    callback(dragEvent);
  }
}

function runTouchCallback(
  element: HTMLElement,
  callback: ((event: unknown) => void) | undefined,
  predicate: ((event: unknown) => boolean) | undefined,
  event: MouseEvent | TouchEvent,
  state: TouchEventState,
): void {
  if (!callback) {
    return;
  }
  const touchEvent = createTouchEvent(element, event, state);
  if (!predicate || predicate(touchEvent)) {
    callback(touchEvent);
  }
}

function ensureGestureListeners(element: HTMLElement, context: AttributeApplierContext): void {
  const state = getViewInteractionState(context);
  if (state.gestureListenersInstalled) {
    return;
  }
  state.gestureListenersInstalled = true;
  const clickListener = (event: MouseEvent) => {
    if (state.suppressNextTap === true) {
      state.suppressNextTap = false;
      event.preventDefault();
      return;
    }
    if (!state.onTapDisabled) {
      runTouchCallback(element, state.onTap, state.onTapPredicate, event, TouchEventState.Ended);
    }
  };
  const doubleClickListener = (event: MouseEvent) => {
    if (!state.onDoubleTapDisabled) {
      runTouchCallback(element, state.onDoubleTap, state.onDoubleTapPredicate, event, TouchEventState.Ended);
    }
  };
  const startLongPress = (event: MouseEvent | TouchEvent) => {
    if (state.onLongPressDisabled || !state.onLongPress) {
      return;
    }
    state.longPressTimer = window.setTimeout(() => {
      runTouchCallback(element, state.onLongPress, state.onLongPressPredicate, event, TouchEventState.Started);
    }, state.longPressDuration);
  };
  const cancelLongPress = () => {
    if (state.longPressTimer !== undefined) {
      clearTimeout(state.longPressTimer);
      state.longPressTimer = undefined;
    }
  };
  const startGesture = (event: MouseEvent | TouchEvent) => {
    startLongPress(event);
    if (state.onDragDisabled || !state.onDrag) {
      return;
    }
    event.preventDefault();
    state.documentDragCleanup?.();
    state.suppressNextTap = false;
    const touchEvent = createTouchEvent(element, event, TouchEventState.Started);
    state.dragSession = {
      originX: touchEvent.x,
      originY: touchEvent.y,
      previousTime: Number.isFinite(event.timeStamp) ? event.timeStamp : Date.now(),
      previousX: touchEvent.x,
      previousY: touchEvent.y,
    };
    runDragCallback(element, state.onDrag, state.onDragPredicate, event, TouchEventState.Started, state.dragSession);
    const document = element.ownerDocument;
    if (typeof document?.addEventListener === 'function' && typeof document.removeEventListener === 'function') {
      document.addEventListener('mousemove', dragListener);
      document.addEventListener('mouseup', endGesture);
      document.addEventListener('touchmove', dragListener);
      document.addEventListener('touchend', endGesture);
      document.addEventListener('touchcancel', endGesture);
      state.documentDragCleanup = () => {
        document.removeEventListener('mousemove', dragListener);
        document.removeEventListener('mouseup', endGesture);
        document.removeEventListener('touchmove', dragListener);
        document.removeEventListener('touchend', endGesture);
        document.removeEventListener('touchcancel', endGesture);
        state.documentDragCleanup = undefined;
      };
    }
  };
  const endGesture = (event: MouseEvent | TouchEvent) => {
    cancelLongPress();
    const session = state.dragSession;
    state.dragSession = undefined;
    state.documentDragCleanup?.();
    if (!state.onDragDisabled && session !== undefined) {
      runDragCallback(element, state.onDrag, state.onDragPredicate, event, TouchEventState.Ended, session);
    }
  };
  const dragListener = (event: MouseEvent | TouchEvent) => {
    if ('buttons' in event && event.buttons !== 1) {
      return;
    }
    const session = state.dragSession;
    if (!state.onDragDisabled && session !== undefined) {
      if (session.lastEvent === event) {
        return;
      }
      session.lastEvent = event;
      event.preventDefault();
      const current = createTouchEvent(element, event, TouchEventState.Changed);
      if (Math.abs(current.x - session.originX) > 3 || Math.abs(current.y - session.originY) > 3) {
        state.suppressNextTap = true;
      }
      runDragCallback(element, state.onDrag, state.onDragPredicate, event, TouchEventState.Changed, session);
    }
  };
  element.addEventListener('click', clickListener);
  element.addEventListener('dblclick', doubleClickListener);
  element.addEventListener('mousedown', startGesture);
  element.addEventListener('touchstart', startGesture);
  element.addEventListener('mouseup', endGesture);
  element.addEventListener('mouseleave', cancelLongPress);
  element.addEventListener('touchend', endGesture);
  element.addEventListener('touchcancel', endGesture);
  element.addEventListener('touchmove', cancelLongPress);
  element.addEventListener('mousemove', dragListener);
  element.addEventListener('touchmove', dragListener);
  context.addCleanup(() => {
    element.removeEventListener('click', clickListener);
    element.removeEventListener('dblclick', doubleClickListener);
    element.removeEventListener('mousedown', startGesture);
    element.removeEventListener('touchstart', startGesture);
    element.removeEventListener('mouseup', endGesture);
    element.removeEventListener('mouseleave', cancelLongPress);
    element.removeEventListener('touchend', endGesture);
    element.removeEventListener('touchcancel', endGesture);
    element.removeEventListener('touchmove', cancelLongPress);
    element.removeEventListener('mousemove', dragListener);
    element.removeEventListener('touchmove', dragListener);
    state.dragSession = undefined;
    state.documentDragCleanup?.();
    cancelLongPress();
  });
}

function installTouchAreaStyles(element: HTMLElement): void {
  const rootNode = element.getRootNode();
  if (typeof ShadowRoot !== 'undefined' && rootNode instanceof ShadowRoot) {
    injectTouchAreaStyles(rootNode);
  } else if (typeof Document !== 'undefined' && rootNode instanceof Document) {
    injectTouchAreaStyles(rootNode);
  }
}

function updateTouchAreaExtension(element: HTMLElement, context: AttributeApplierContext): void {
  const state = getViewInteractionState(context);
  const extension = state.touchAreaExtension ?? { top: 0, right: 0, bottom: 0, left: 0 };
  const hasExtension = extension.top > 0 || extension.right > 0 || extension.bottom > 0 || extension.left > 0;
  if (hasExtension) {
    installTouchAreaStyles(element);
    element.setAttribute('data-touch-ext', '');
    element.style.setProperty('--touch-ext-top', `${extension.top}px`);
    element.style.setProperty('--touch-ext-right', `${extension.right}px`);
    element.style.setProperty('--touch-ext-bottom', `${extension.bottom}px`);
    element.style.setProperty('--touch-ext-left', `${extension.left}px`);
  } else {
    element.removeAttribute('data-touch-ext');
    element.style.removeProperty('--touch-ext-top');
    element.style.removeProperty('--touch-ext-right');
    element.style.removeProperty('--touch-ext-bottom');
    element.style.removeProperty('--touch-ext-left');
  }
}

function setTouchAreaExtensionPart(
  element: HTMLElement,
  context: AttributeApplierContext,
  part: 'top' | 'right' | 'bottom' | 'left',
  value: number,
): void {
  const state = getViewInteractionState(context);
  state.touchAreaExtension ??= { top: 0, right: 0, bottom: 0, left: 0 };
  state.touchAreaExtension[part] = value;
  updateTouchAreaExtension(element, context);
}

function normalizeValdiBoxShadow(value: string): string {
  const source = value.trim();
  if (!source) {
    return '';
  }
  const firstToken = readWhitespaceSeparatedToken(source, 0);
  if (!firstToken) {
    return '';
  }
  const offsetXToken =
    firstToken.token === 'complex' ? readWhitespaceSeparatedToken(source, firstToken.nextIndex) : firstToken;
  if (!offsetXToken) {
    return source;
  }
  const offsetYToken = readWhitespaceSeparatedToken(source, offsetXToken.nextIndex);
  const blurToken = offsetYToken ? readWhitespaceSeparatedToken(source, offsetYToken.nextIndex) : undefined;
  if (!offsetYToken || !blurToken) {
    return source;
  }
  const colorStartIndex = skipCssWhitespace(source, blurToken.nextIndex);
  if (
    colorStartIndex >= source.length ||
    !isPlainCssNumber(offsetXToken.token) ||
    !isPlainCssNumber(offsetYToken.token) ||
    !isPlainCssNumber(blurToken.token)
  ) {
    return source;
  }
  return `${Number(offsetXToken.token)}px ${Number(offsetYToken.token)}px ${Number(blurToken.token)}px ${source.slice(colorStartIndex)}`;
}

function updateViewBoxShadow(context: AttributeApplierContext): void {
  const state = getViewPresentationState(context);
  if (state.boxShadowElement) {
    state.boxShadowElement.style.boxShadow =
      state.boxShadow && !state.slowClipping ? normalizeValdiBoxShadow(state.boxShadow) : '';
  }
}

function updateViewOverflow(element: HTMLElement, context: AttributeApplierContext): void {
  const state = getViewPresentationState(context);
  element.style.overflow = state.slowClipping ? 'hidden' : (state.overflow ?? 'visible');
  if (getViewPaintElement(context)) {
    element.style.borderRadius = state.slowClipping ? (state.borderRadiusCss ?? '') : '';
  }
}

function boxShadowAttributeApplier(): AttributeApplier {
  return {
    apply(_element, value, attributeName, context) {
      if (typeof value !== 'string') {
        throw new Error(`Expected '${attributeName}' to be a string`);
      }
      const state = getViewPresentationState(context);
      state.boxShadow = value;
      state.boxShadowElement = context.getViewAttributeElement();
      updateViewBoxShadow(context);
    },
    reset(_element, _attributeName, context) {
      const state = getViewPresentationState(context);
      state.boxShadow = undefined;
      updateViewBoxShadow(context);
    },
  };
}

type ResolvedTranslationUnit = 'px' | '%';

interface ResolvedTranslation {
  value: number;
  unit: ResolvedTranslationUnit;
}

interface ResolvedTransform {
  transformOrigin: string | undefined;
  transform: string | undefined;
  translationX: ResolvedTranslation;
  translationY: ResolvedTranslation;
  scaleX: number;
  scaleY: number;
  rotation: number;
}

function resolveTranslation(value: unknown): ResolvedTranslation {
  if (value === undefined || value === null) {
    return { value: 0, unit: 'px' };
  }
  if (typeof value === 'number') {
    if (Number.isFinite(value)) {
      return { value, unit: 'px' };
    }
  } else if (typeof value === 'string') {
    let number = value.trim();
    let unit: ResolvedTranslationUnit = 'px';
    if (number.endsWith('%')) {
      number = number.slice(0, -1);
      unit = '%';
    } else if (number.endsWith('px') || number.endsWith('pt')) {
      number = number.slice(0, -2);
    }
    const parsedValue = Number(number);
    if (number.length > 0 && Number.isFinite(parsedValue)) {
      return { value: parsedValue, unit };
    }
  }
  throw new Error('Expected translation to use a unitless, px, pt, or percent dimension');
}

function optionalNumber(value: unknown, fallback: number): number {
  return value === undefined || value === null ? fallback : parseNumber(value, 'transform');
}

function resolveTransform(values: ReadonlyArray<unknown>): ResolvedTransform {
  return {
    transformOrigin: typeof values[0] === 'string' ? values[0] : undefined,
    transform: typeof values[1] === 'string' && values[1].length > 0 ? values[1] : undefined,
    translationX: resolveTranslation(values[2]),
    translationY: resolveTranslation(values[3]),
    scaleX: optionalNumber(values[4], 1),
    scaleY: optionalNumber(values[5], 1),
    rotation: optionalNumber(values[6], 0),
  };
}

function translationToCss(translation: ResolvedTranslation): string {
  return `${translation.value}${translation.unit}`;
}

function resolvedTransformToValues(transform: ResolvedTransform): ReadonlyArray<unknown> {
  return [
    transform.transformOrigin,
    transform.transform,
    translationToCss(transform.translationX),
    translationToCss(transform.translationY),
    transform.scaleX,
    transform.scaleY,
    transform.rotation,
  ];
}

function applyResolvedTransform(element: HTMLElement, transform: ResolvedTransform): void {
  element.style.transformOrigin = transform.transformOrigin ?? '';
  if (transform.transform) {
    element.style.transform = transform.transform;
    return;
  }
  const parts: string[] = [];
  if (transform.translationX.value !== 0 || transform.translationY.value !== 0) {
    parts.push(`translate(${translationToCss(transform.translationX)}, ${translationToCss(transform.translationY)})`);
  }
  if (transform.scaleX !== 1 || transform.scaleY !== 1) {
    parts.push(`scale(${transform.scaleX}, ${transform.scaleY})`);
  }
  if (transform.rotation !== 0) {
    parts.push(`rotate(${transform.rotation}rad)`);
  }
  element.style.transform = parts.join(' ');
}

function makeTranslationInterpolator(
  from: ResolvedTranslation,
  to: ResolvedTranslation,
): ((progress: number) => ResolvedTranslation) | undefined {
  let unit = from.unit;
  if (from.unit !== to.unit) {
    if (from.value === 0) {
      unit = to.unit;
    } else if (to.value !== 0) {
      return undefined;
    }
  }
  return progress => ({ value: from.value + (to.value - from.value) * progress, unit });
}

function hasInterpolatedTransformChange(source: ResolvedTransform, target: ResolvedTransform): boolean {
  return (
    source.translationX.value !== target.translationX.value ||
    source.translationY.value !== target.translationY.value ||
    source.scaleX !== target.scaleX ||
    source.scaleY !== target.scaleY ||
    source.rotation !== target.rotation
  );
}

const transformComposite: CompositeAttribute = {
  name: 'transformComposite',
  parts: [
    { name: 'transformOrigin', optional: true },
    { name: 'transform', optional: true },
    { name: 'translationX', optional: true },
    { name: 'translationY', optional: true },
    { name: 'scaleX', optional: true, parse: (_element, value, name) => parseNumber(value, name) },
    { name: 'scaleY', optional: true, parse: (_element, value, name) => parseNumber(value, name) },
    { name: 'rotation', optional: true, parse: (_element, value, name) => parseNumber(value, name) },
  ],
  apply(element, values) {
    applyResolvedTransform(element, resolveTransform(values));
  },
  reset(element) {
    element.style.transform = '';
    element.style.transformOrigin = '';
  },
  animationMinimumVisibleChange: MIN_VISIBLE_CHANGE_PIXEL,
  makeAnimationInterpolator(_element, from, to) {
    if (to !== undefined && to !== null && !Array.isArray(to)) {
      return undefined;
    }
    const source = resolveTransform(Array.isArray(from) ? from : []);
    const target = resolveTransform(Array.isArray(to) ? to : []);
    if (source.transform !== target.transform || source.transform || target.transform) {
      return undefined;
    }
    if (!hasInterpolatedTransformChange(source, target)) {
      return undefined;
    }
    const translationX = makeTranslationInterpolator(source.translationX, target.translationX);
    const translationY = makeTranslationInterpolator(source.translationY, target.translationY);
    if (!translationX || !translationY) {
      return undefined;
    }
    return progress =>
      resolvedTransformToValues({
        transformOrigin: target.transformOrigin,
        transform: target.transform,
        translationX: translationX(progress),
        translationY: translationY(progress),
        scaleX: source.scaleX + (target.scaleX - source.scaleX) * progress,
        scaleY: source.scaleY + (target.scaleY - source.scaleY) * progress,
        rotation: source.rotation + (target.rotation - source.rotation) * progress,
      });
  },
};

export function buildViewAttributeAppliers(): AttributeApplierMap {
  const binder = new AttributesBinder<HTMLElement>();
  binder.bindNumberAttribute(
    'touchAreaExtension',
    (element, value, context) => {
      getViewInteractionState(context).touchAreaExtension = {
        top: value,
        right: value,
        bottom: value,
        left: value,
      };
      updateTouchAreaExtension(element, context);
    },
    (element, context) => {
      getViewInteractionState(context).touchAreaExtension = { top: 0, right: 0, bottom: 0, left: 0 };
      updateTouchAreaExtension(element, context);
    },
  );
  binder.bindNumberAttribute(
    'touchAreaExtensionTop',
    (element, value, context) => setTouchAreaExtensionPart(element, context, 'top', value),
    (element, context) => setTouchAreaExtensionPart(element, context, 'top', 0),
  );
  binder.bindNumberAttribute(
    'touchAreaExtensionRight',
    (element, value, context) => setTouchAreaExtensionPart(element, context, 'right', value),
    (element, context) => setTouchAreaExtensionPart(element, context, 'right', 0),
  );
  binder.bindNumberAttribute(
    'touchAreaExtensionBottom',
    (element, value, context) => setTouchAreaExtensionPart(element, context, 'bottom', value),
    (element, context) => setTouchAreaExtensionPart(element, context, 'bottom', 0),
  );
  binder.bindNumberAttribute(
    'touchAreaExtensionLeft',
    (element, value, context) => setTouchAreaExtensionPart(element, context, 'left', value),
    (element, context) => setTouchAreaExtensionPart(element, context, 'left', 0),
  );

  binder.bindColorAttribute(
    'background',
    (_element, value, context) => {
      context.getViewAttributeElement().style.background = resolveValdiGradientAngles(value);
    },
    (_element, context) => {
      context.getViewAttributeElement().style.background = '';
    },
  );
  binder.bindAnimatableColorAttribute(
    'backgroundColor',
    'transparent',
    MIN_VISIBLE_CHANGE_COLOR,
    (_element, value, context) => {
      context.getViewAttributeElement().style.backgroundColor = value;
    },
    (_element, context) => {
      context.getViewAttributeElement().style.backgroundColor = '';
    },
  );
  binder.bindAttribute('borderWidth', {
    apply(_element, value, attributeName, context) {
      const element = context.getViewAttributeElement();
      const borderWidth = parseCssLength(value, attributeName);
      element.style.borderWidth = borderWidth;
      if (!context.getState<string>(VIEW_BORDER_STYLE_STATE)) {
        element.style.borderStyle = borderWidth === '0px' ? '' : 'solid';
      }
    },
    reset(_element, _attributeName, context) {
      const element = context.getViewAttributeElement();
      element.style.borderWidth = '0px';
      if (!context.getState<string>(VIEW_BORDER_STYLE_STATE)) {
        element.style.borderStyle = '';
      }
    },
  });
  binder.bindAttribute('borderRadius', createBorderRadiusAttributeApplier(false));
  binder.bindAnimatableColorAttribute(
    'borderColor',
    'transparent',
    MIN_VISIBLE_CHANGE_COLOR,
    (_element, value, context) => {
      context.getViewAttributeElement().style.borderColor = value;
    },
    (_element, context) => {
      context.getViewAttributeElement().style.borderColor = '';
    },
  );
  binder.bindAttribute('border', borderAttributeApplier());
  binder.bindStringAttribute(
    'borderStyle',
    (_element, value, context) => {
      const element = context.getViewAttributeElement();
      context.setState(VIEW_BORDER_STYLE_STATE, value);
      if (!element.style.borderWidth) {
        element.style.borderWidth = '0px';
      }
      element.style.borderStyle = value;
    },
    (_element, context) => {
      context.setState(VIEW_BORDER_STYLE_STATE, undefined);
      const element = context.getViewAttributeElement();
      element.style.borderStyle = element.style.borderWidth === '0px' ? '' : 'solid';
    },
  );
  binder.bindAttribute('boxShadow', boxShadowAttributeApplier());

  binder.bindAnimatableColorAttribute(
    'color',
    'black',
    MIN_VISIBLE_CHANGE_COLOR,
    (element, value) => {
      element.style.color = value;
    },
    element => {
      element.style.color = 'black';
    },
  );
  binder.bindAnimatableNumberAttribute(
    'opacity',
    1,
    MIN_VISIBLE_CHANGE_ALPHA,
    (element, value) => {
      element.style.opacity = String(value);
    },
    element => {
      element.style.opacity = '';
    },
  );
  binder.bindStyleValueAttribute('cursor', 'cursor');
  binder.bindBooleanAttribute(
    'touchEnabled',
    (element, enabled, context) => {
      const state = getViewInteractionState(context);
      state.touchEnabled = enabled;
      updateViewPointerEvents(element, state);
    },
    (element, context) => {
      const state = getViewInteractionState(context);
      state.touchEnabled = undefined;
      updateViewPointerEvents(element, state);
    },
  );
  binder.bindBooleanAttribute(
    'slowClipping',
    (element, enabled, context) => {
      getViewPresentationState(context).slowClipping = enabled;
      updateViewBoxShadow(context);
      updateViewOverflow(element, context);
    },
    (element, context) => {
      getViewPresentationState(context).slowClipping = false;
      updateViewBoxShadow(context);
      updateViewOverflow(element, context);
    },
  );
  binder.bindBooleanAttribute(
    'hitTest',
    (element, enabled, context) => {
      const state = getViewInteractionState(context);
      state.hitTest = enabled;
      updateViewPointerEvents(element, state);
    },
    (element, context) => {
      const state = getViewInteractionState(context);
      state.hitTest = undefined;
      updateViewPointerEvents(element, state);
    },
  );
  binder.bindFunctionAttribute(
    'onTouch',
    (element, callback, context) => {
      const state = getViewInteractionState(context);
      const activePointers = new Map<number, BrowserPointerPosition>();
      const updatePointer = (event: PointerEvent) => {
        activePointers.set(event.pointerId, { clientX: event.clientX, clientY: event.clientY });
      };
      const endPointer = (event: PointerEvent) => {
        if (!activePointers.has(event.pointerId)) {
          return;
        }
        activePointers.delete(event.pointerId);
        callback(createTouchEvent(element, event, TouchEventState.Ended, activePointers));
      };
      replaceEventListener(element, context, 'view:onTouchStart', 'pointerdown', (event: PointerEvent) => {
        updatePointer(event);
        if (typeof element.setPointerCapture === 'function') {
          element.setPointerCapture(event.pointerId);
        }
        callback(createTouchEvent(element, event, TouchEventState.Started, activePointers));
      });
      replaceEventListener(element, context, 'view:onTouchMove', 'pointermove', (event: PointerEvent) => {
        if (!activePointers.has(event.pointerId)) {
          return;
        }
        updatePointer(event);
        callback(createTouchEvent(element, event, TouchEventState.Changed, activePointers));
      });
      replaceEventListener(element, context, 'view:onTouchEnd', 'pointerup', endPointer);
      replaceEventListener(element, context, 'view:onTouchCancel', 'pointercancel', endPointer);
      state.onTouchActive = true;
      updateViewPointerEvents(element, state);
    },
    (element, context) => {
      replaceEventListener(element, context, 'view:onTouchStart', 'pointerdown', undefined);
      replaceEventListener(element, context, 'view:onTouchMove', 'pointermove', undefined);
      replaceEventListener(element, context, 'view:onTouchEnd', 'pointerup', undefined);
      replaceEventListener(element, context, 'view:onTouchCancel', 'pointercancel', undefined);
      const state = getViewInteractionState(context);
      state.onTouchActive = false;
      updateViewPointerEvents(element, state);
    },
  );
  binder.bindFunctionAttribute(
    'onTouchStart',
    (element, callback, context) => {
      replaceEventListener(element, context, 'view:onTouchStartOnly', 'pointerdown', event =>
        callback(createTouchEvent(element, event, TouchEventState.Started)),
      );
      const state = getViewInteractionState(context);
      state.onTouchStartActive = true;
      updateViewPointerEvents(element, state);
    },
    (element, context) => {
      replaceEventListener(element, context, 'view:onTouchStartOnly', 'pointerdown', undefined);
      const state = getViewInteractionState(context);
      state.onTouchStartActive = false;
      updateViewPointerEvents(element, state);
    },
  );
  binder.bindFunctionAttribute(
    'onTouchEnd',
    (element, callback, context) => {
      replaceEventListener(element, context, 'view:onTouchEndOnly', 'pointerup', event =>
        callback(createTouchEvent(element, event, TouchEventState.Ended)),
      );
      const state = getViewInteractionState(context);
      state.onTouchEndActive = true;
      updateViewPointerEvents(element, state);
    },
    (element, context) => {
      replaceEventListener(element, context, 'view:onTouchEndOnly', 'pointerup', undefined);
      const state = getViewInteractionState(context);
      state.onTouchEndActive = false;
      updateViewPointerEvents(element, state);
    },
  );
  binder.bindNumberAttribute(
    'onTouchDelayDuration',
    (_element, value, context) => {
      getViewInteractionState(context).longPressDuration = value;
    },
    (_element, context) => {
      getViewInteractionState(context).longPressDuration = 500;
    },
  );
  binder.bindBooleanAttribute(
    'onTapDisabled',
    (element, value, context) => {
      getViewInteractionState(context).onTapDisabled = value;
      ensureGestureListeners(element, context);
    },
    (_element, context) => {
      getViewInteractionState(context).onTapDisabled = false;
    },
  );
  binder.bindFunctionAttribute(
    'onTap',
    (element, callback, context) => {
      const state = getViewInteractionState(context);
      state.onTap = callback as (event: unknown) => void;
      updateViewPointerEvents(element, state);
      element.style.cursor = 'pointer';
      ensureGestureListeners(element, context);
    },
    (element, context) => {
      const state = getViewInteractionState(context);
      state.onTap = undefined;
      updateViewPointerEvents(element, state);
      element.style.cursor = '';
    },
  );
  binder.bindFunctionAttribute(
    'onTapPredicate',
    (element, callback, context) => {
      getViewInteractionState(context).onTapPredicate = callback as (event: unknown) => boolean;
      ensureGestureListeners(element, context);
    },
    (_element, context) => {
      getViewInteractionState(context).onTapPredicate = undefined;
    },
  );
  binder.bindBooleanAttribute(
    'onDoubleTapDisabled',
    (element, value, context) => {
      getViewInteractionState(context).onDoubleTapDisabled = value;
      ensureGestureListeners(element, context);
    },
    (_element, context) => {
      getViewInteractionState(context).onDoubleTapDisabled = false;
    },
  );
  binder.bindFunctionAttribute(
    'onDoubleTap',
    (element, callback, context) => {
      const state = getViewInteractionState(context);
      state.onDoubleTap = callback as (event: unknown) => void;
      updateViewPointerEvents(element, state);
      ensureGestureListeners(element, context);
    },
    (element, context) => {
      const state = getViewInteractionState(context);
      state.onDoubleTap = undefined;
      updateViewPointerEvents(element, state);
    },
  );
  binder.bindFunctionAttribute(
    'onDoubleTapPredicate',
    (element, callback, context) => {
      getViewInteractionState(context).onDoubleTapPredicate = callback as (event: unknown) => boolean;
      ensureGestureListeners(element, context);
    },
    (_element, context) => {
      getViewInteractionState(context).onDoubleTapPredicate = undefined;
    },
  );
  binder.bindNumberAttribute(
    'longPressDuration',
    (element, value, context) => {
      getViewInteractionState(context).longPressDuration = value;
      ensureGestureListeners(element, context);
    },
    (_element, context) => {
      getViewInteractionState(context).longPressDuration = 500;
    },
  );
  binder.bindBooleanAttribute(
    'onLongPressDisabled',
    (element, value, context) => {
      getViewInteractionState(context).onLongPressDisabled = value;
      ensureGestureListeners(element, context);
    },
    (_element, context) => {
      getViewInteractionState(context).onLongPressDisabled = false;
    },
  );
  binder.bindFunctionAttribute(
    'onLongPress',
    (element, callback, context) => {
      const state = getViewInteractionState(context);
      state.onLongPress = callback as (event: unknown) => void;
      updateViewPointerEvents(element, state);
      ensureGestureListeners(element, context);
    },
    (element, context) => {
      const state = getViewInteractionState(context);
      state.onLongPress = undefined;
      updateViewPointerEvents(element, state);
    },
  );
  binder.bindFunctionAttribute(
    'onLongPressPredicate',
    (element, callback, context) => {
      getViewInteractionState(context).onLongPressPredicate = callback as (event: unknown) => boolean;
      ensureGestureListeners(element, context);
    },
    (_element, context) => {
      getViewInteractionState(context).onLongPressPredicate = undefined;
    },
  );
  binder.bindBooleanAttribute(
    'onDragDisabled',
    (element, value, context) => {
      getViewInteractionState(context).onDragDisabled = value;
      ensureGestureListeners(element, context);
    },
    (_element, context) => {
      getViewInteractionState(context).onDragDisabled = false;
    },
  );
  binder.bindFunctionAttribute(
    'onDrag',
    (element, callback, context) => {
      const state = getViewInteractionState(context);
      state.onDrag = callback as (event: unknown) => void;
      updateViewPointerEvents(element, state);
      ensureGestureListeners(element, context);
    },
    (element, context) => {
      const state = getViewInteractionState(context);
      state.onDrag = undefined;
      updateViewPointerEvents(element, state);
    },
  );
  binder.bindFunctionAttribute(
    'onDragPredicate',
    (element, callback, context) => {
      getViewInteractionState(context).onDragPredicate = callback as (event: unknown) => boolean;
      ensureGestureListeners(element, context);
    },
    (_element, context) => {
      getViewInteractionState(context).onDragPredicate = undefined;
    },
  );
  binder.bindNoOpAttribute('onPinchDisabled');
  binder.bindNoOpAttribute('onPinch');
  binder.bindNoOpAttribute('onPinchPredicate');
  binder.bindNoOpAttribute('onRotateDisabled');
  binder.bindNoOpAttribute('onRotate');
  binder.bindNoOpAttribute('onRotatePredicate');
  binder.bindBooleanAttribute(
    'canAlwaysScrollHorizontal',
    (element, value) => {
      element.style.overflowX = value ? 'scroll' : 'auto';
    },
    element => {
      element.style.overflowX = '';
    },
  );
  binder.bindBooleanAttribute(
    'canAlwaysScrollVertical',
    (element, value) => {
      element.style.overflowY = value ? 'scroll' : 'auto';
    },
    element => {
      element.style.overflowY = '';
    },
  );
  binder.bindNumberAttribute(
    'maskOpacity',
    (element, value, context) => {
      getViewMaskState(context).maskOpacity = value;
      updateMask(element, context);
    },
    (element, context) => {
      getViewMaskState(context).maskOpacity = 1;
      updateMask(element, context);
    },
  );
  binder.bindAttribute('maskPath', {
    apply(element, value, _attributeName, context) {
      const state = getViewMaskState(context);
      if (typeof value === 'string') {
        state.path = { d: value, viewBox: '0 0 1 1', preserveAspectRatio: 'none' };
      } else if (isGeometricPathValue(value)) {
        state.path = geometricPathToSvgPath(value);
      } else {
        state.path = undefined;
      }
      updateMask(element, context);
    },
    reset(element, _attributeName, context) {
      getViewMaskState(context).path = undefined;
      updateMask(element, context);
    },
  });
  binder.bindNoOpAttribute('filterTouchesWhenObscured');
  return binder.attributeAppliers;
}

export const viewCompositeAttributes: Readonly<Record<string, CompositeAttribute>> = { transformComposite };
