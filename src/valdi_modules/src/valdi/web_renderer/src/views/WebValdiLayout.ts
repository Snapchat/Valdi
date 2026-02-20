import { generateStyles, isAttributeValidStyle } from '../styles/ValdiWebStyles';
import { injectTouchAreaStyles } from '../styles/touchAreaExtension';
import { UpdateAttributeDelegate } from '../ValdiWebRendererDelegate';
import { TouchEventState } from 'valdi_tsx/src/GestureEvents';

export class WebValdiLayout {
  public type = 'layout';

  public id: number;
  public parent: WebValdiLayout | null = null;
  public children: WebValdiLayout[] = [];
  public htmlElement: HTMLElement;
  public attributeDelegate?: UpdateAttributeDelegate;
  private _transforms: { [key: string]: any } = {};
  private _onLayoutObserver?: ResizeObserver;
  private _visibilityAndViewportObserver?: IntersectionObserver;
  private _longPressTimer?: number;

  // Gesture properties
  private _longPressDuration: number = 500;
  private _onTapDisabled: boolean = false;
  private _onDoubleTapDisabled: boolean = false;
  private _onLongPressDisabled: boolean = false;
  private _onDragDisabled: boolean = false;

  // Gesture handlers
  private _onTap?: (e: any) => void;
  private _onTapPredicate?: (e: any) => boolean;
  private _onDoubleTap?: (e: any) => void;
  private _onDoubleTapPredicate?: (e: any) => boolean;
  private _onLongPress?: (e: any) => void;
  private _onLongPressPredicate?: (e: any) => boolean;
  private _onDrag?: (e: any) => void;
  private _onDragPredicate?: (e: any) => boolean;

  // Lifecycle, visibility, and attachment handlers
  private _onViewCreate?: () => void;
  private _onViewDestroy?: () => void;
  private _onViewChange?: (event: { type: 'Attached' | 'Detached' }) => void;
  private _onVisibilityChanged?: (isVisible: boolean) => void;
  private _onViewportChanged?: (viewport: any, frame: any, eventTime: any) => void;
  private _isAttached: boolean = false;

  // Touch area extension
  private _touchAreaExtension = { top: 0, right: 0, bottom: 0, left: 0 };

  constructor(id: number, attributeDelegate?: UpdateAttributeDelegate) {
    this.id = id;
    this.attributeDelegate = attributeDelegate;
    this.htmlElement = this.createHtmlElement();
    this.setupEventListeners();
    // Defer call to allow attributes to be set
    setTimeout(() => this._onViewCreate?.(), 0);
  }

  setAttributeDelegate(attributeDelegate?: UpdateAttributeDelegate) {
    this.attributeDelegate = attributeDelegate;
  }

  createHtmlElement(): HTMLElement {
    const element = document.createElement('div');

    Object.assign(element.style, {
      alignItems: 'stretch',
      backgroundColor: 'transparent',
      border: '0 solid black',
      boxSizing: 'border-box',
      display: 'flex',
      flexBasis: 'auto',
      flexDirection: 'column',
      flexShrink: 0,
      listStyle: 'none',
      margin: 0,
      minHeight: 0,
      minWidth: 0,
      padding: 0,
      position: 'relative',
      textDecoration: 'none',
      zIndex: 0,
      pointerEvents: 'auto',
      overflow: 'visible',
    });

    element.style.pointerEvents = 'none';

    return element;
  }

  makeRoot(root: HTMLElement | ShadowRoot) {
    this.parent?.removeChild(this);
    this.parent = null;
    root.replaceChildren(this.htmlElement);
    if (root instanceof ShadowRoot) {
      injectTouchAreaStyles(root);
    } else {
      injectTouchAreaStyles(root.ownerDocument);
    }
    if (this._onViewChange && !this._isAttached) {
      this._isAttached = true;
      this._onViewChange({ type: 'Attached' });
    }
  }

  removeChild(child: WebValdiLayout) {
    this.children = this.children.filter(c => c.id !== child.id);
  }

  move(parent: WebValdiLayout, index: number) {
    this.parent?.removeChild(this);

    this.parent = parent;
    this.parent.children.splice(index, 0, this);
    this.parent.htmlElement.insertBefore(this.htmlElement, parent.htmlElement.childNodes.item(index));
    if (this._onViewChange && !this._isAttached) {
      this._isAttached = true;
      this._onViewChange({ type: 'Attached' });
    }
  }

  destroy() {
    if (this._onViewChange && this._isAttached) {
      this._isAttached = false;
      this._onViewChange({ type: 'Detached' });
    }
    this._onViewDestroy?.();
    this._onLayoutObserver?.disconnect();
    this._visibilityAndViewportObserver?.disconnect();
    this.htmlElement.remove();
  }

  private updateTransform() {
    const { scaleX = 1, scaleY = 1, rotation = 0, translationX = 0, translationY = 0 } = this._transforms;
    const parts: string[] = [];
    if (translationX !== 0 || translationY !== 0) {
      parts.push(`translate(${translationX}px, ${translationY}px)`);
    }
    if (scaleX !== 1 || scaleY !== 1) {
      parts.push(`scale(${scaleX}, ${scaleY})`);
    }
    if (rotation !== 0) {
      parts.push(`rotate(${rotation}rad)`);
    }
    this.htmlElement.style.transform = parts.join(' ');
  }

  private updateTouchAreaExtension() {
    const { top, right, bottom, left } = this._touchAreaExtension;
    const hasExtension = top > 0 || right > 0 || bottom > 0 || left > 0;

    if (hasExtension) {
      const rootNode = this.htmlElement.getRootNode();
      if (rootNode instanceof ShadowRoot || rootNode instanceof Document) {
        injectTouchAreaStyles(rootNode);
      }
      this.htmlElement.setAttribute('data-touch-ext', '');
      this.htmlElement.style.setProperty('--touch-ext-top', `${top}px`);
      this.htmlElement.style.setProperty('--touch-ext-right', `${right}px`);
      this.htmlElement.style.setProperty('--touch-ext-bottom', `${bottom}px`);
      this.htmlElement.style.setProperty('--touch-ext-left', `${left}px`);
    } else {
      this.htmlElement.removeAttribute('data-touch-ext');
      this.htmlElement.style.removeProperty('--touch-ext-top');
      this.htmlElement.style.removeProperty('--touch-ext-right');
      this.htmlElement.style.removeProperty('--touch-ext-bottom');
      this.htmlElement.style.removeProperty('--touch-ext-left');
    }
  }

  private setupEventListeners() {
    this.htmlElement.addEventListener('click', (e: MouseEvent) => {
      if (this._onTapDisabled || !this._onTap) return;
      this.createTouchEventHandler(this._onTap, TouchEventState.Ended, this._onTapPredicate)(e);
    });

    this.htmlElement.addEventListener('dblclick', (e: MouseEvent) => {
      if (this._onDoubleTapDisabled || !this._onDoubleTap) return;
      this.createTouchEventHandler(this._onDoubleTap, TouchEventState.Ended, this._onDoubleTapPredicate)(e);
    });

    const startLongPress = (e: MouseEvent | TouchEvent) => {
      if (this._onLongPressDisabled || !this._onLongPress) return;
      const handler = this.createTouchEventHandler(
        this._onLongPress,
        TouchEventState.Started,
        this._onLongPressPredicate,
      );
      this._longPressTimer = window.setTimeout(() => handler(e), this._longPressDuration);
    };
    const endLongPress = () => {
      if (this._longPressTimer) clearTimeout(this._longPressTimer);
    };
    this.htmlElement.addEventListener('mousedown', startLongPress);
    this.htmlElement.addEventListener('touchstart', startLongPress);
    ['mouseup', 'mouseleave', 'touchend', 'touchcancel', 'touchmove'].forEach(evt =>
      this.htmlElement.addEventListener(evt, endLongPress),
    );

    const onDrag = (e: MouseEvent | TouchEvent) => {
      if (e instanceof MouseEvent && e.buttons !== 1) return;
      if (this._onDragDisabled || !this._onDrag) return;
      this.createTouchEventHandler(this._onDrag, TouchEventState.Changed, this._onDragPredicate)(e);
    };
    this.htmlElement.addEventListener('touchmove', onDrag);
    this.htmlElement.addEventListener('mousemove', onDrag);

    this._visibilityAndViewportObserver = new IntersectionObserver(
      entries => {
        entries.forEach(entry => {
          this._onVisibilityChanged?.(entry.isIntersecting);
          this._onViewportChanged?.(entry.rootBounds, entry.boundingClientRect, performance.now());
        });
      },
      { threshold: [0, 1] },
    );
    this._visibilityAndViewportObserver.observe(this.htmlElement);
  }

  private createTouchEventHandler(value: any, state: TouchEventState, predicate?: (e: any) => boolean) {
    return (event: MouseEvent | TouchEvent) => {
      if (typeof value !== 'function') return;
      const touch = 'touches' in event ? event.touches[0] || event.changedTouches[0] : event;
      const touchEvent = {
        state,
        x: touch.clientX,
        y: touch.clientY,
        absoluteX: touch.clientX,
        absoluteY: touch.clientY,
        pointerCount: 'touches' in event ? event.touches.length : 1,
        pointerLocations: [],
      };
      if (predicate && !predicate(touchEvent)) {
        return;
      }
      value(touchEvent);
    };
  }

  changeAttribute(attributeName: string, attributeValue: any) {
    if (isAttributeValidStyle(attributeName)) {
      const generatedStyles = generateStyles(attributeName, attributeValue);
      Object.assign(this.htmlElement.style, generatedStyles);
      return;
    }

    switch (attributeName) {
      case 'accessibilityId':
        this.htmlElement.setAttribute('id', attributeValue);
        return;
      case 'accessibilityLabel':
        this.htmlElement.setAttribute('aria-label', attributeValue);
        return;
      case 'accessibilityRole':
      case 'accessibilityCategory':
        this.htmlElement.setAttribute('aria-roledescription', attributeValue);
        return;
      case 'accessibilityStateSelected':
        this.htmlElement.setAttribute('aria-selected', attributeValue);
        return;
      case 'touchAreaExtension': {
        const extension = Number(attributeValue) || 0;
        this._touchAreaExtension = { top: extension, right: extension, bottom: extension, left: extension };
        this.updateTouchAreaExtension();
        return;
      }
      case 'touchAreaExtensionTop':
        this._touchAreaExtension.top = Number(attributeValue) || 0;
        this.updateTouchAreaExtension();
        return;
      case 'touchAreaExtensionRight':
        this._touchAreaExtension.right = Number(attributeValue) || 0;
        this.updateTouchAreaExtension();
        return;
      case 'touchAreaExtensionBottom':
        this._touchAreaExtension.bottom = Number(attributeValue) || 0;
        this.updateTouchAreaExtension();
        return;
      case 'touchAreaExtensionLeft':
        this._touchAreaExtension.left = Number(attributeValue) || 0;
        this.updateTouchAreaExtension();
        return;
      case 'allowReuse':
        // This is a rendering hint for native platforms, like React's `key` prop,
        // to help with view recycling. It's a no-op in this direct DOM manipulation model.
        return;
      /*
        From NativeTemplateElements ViewAttributes
      */
      case 'onViewCreate':
        this._onViewCreate = attributeValue;
        return;
      case 'onViewDestroy':
        this._onViewDestroy = attributeValue;
        return;
      case 'onViewChange':
        this._onViewChange = attributeValue;
        // Check initial state in case the element is already in the DOM when this is set.
        const isCurrentlyAttached = document.body.contains(this.htmlElement);
        if (this._isAttached !== isCurrentlyAttached) {
          this._isAttached = isCurrentlyAttached;
          this._onViewChange?.({ type: isCurrentlyAttached ? 'Attached' : 'Detached' });
        }
        return;
      case 'slowClipping':
      case 'touchEnabled':
      case 'hitTest':
        this.htmlElement.style.pointerEvents = attributeValue ? 'auto' : 'none';
        return;
      case 'onTouch':
        this.htmlElement.addEventListener(
          'touchstart',
          this.createTouchEventHandler(attributeValue, TouchEventState.Started),
        );
        this.htmlElement.addEventListener(
          'touchmove',
          this.createTouchEventHandler(attributeValue, TouchEventState.Changed),
        );
        this.htmlElement.addEventListener(
          'touchend',
          this.createTouchEventHandler(attributeValue, TouchEventState.Ended),
        );
        this.htmlElement.addEventListener(
          'touchcancel',
          this.createTouchEventHandler(attributeValue, TouchEventState.Ended),
        );
        return;
      case 'onTouchStart':
        this.htmlElement.addEventListener(
          'touchstart',
          this.createTouchEventHandler(attributeValue, TouchEventState.Started),
        );
        return;
      case 'onTouchEnd':
        this.htmlElement.addEventListener(
          'touchend',
          this.createTouchEventHandler(attributeValue, TouchEventState.Ended),
        );
        return;
      case 'onTouchDelayDuration':
        // This affects gesture recognition timing. Mapping to long press duration.
        this._longPressDuration = attributeValue;
        return;
      case 'onTapDisabled':
        this._onTapDisabled = !!attributeValue;
        return;
      case 'onTap':
        this._onTap = attributeValue;
        if (attributeValue) {
          this.htmlElement.style.pointerEvents = 'auto';
          this.htmlElement.style.cursor = 'pointer';
        } else {
          this.htmlElement.style.pointerEvents = 'none';
          this.htmlElement.style.cursor = '';
        }
        return;
      case 'onTapPredicate':
        this._onTapPredicate = attributeValue;
        return;
      case 'onDoubleTapDisabled':
        this._onDoubleTapDisabled = !!attributeValue;
        return;
      case 'onDoubleTap':
        this._onDoubleTap = attributeValue;
        return;
      case 'onDoubleTapPredicate':
        this._onDoubleTapPredicate = attributeValue;
        return;
      case 'longPressDuration':
        this._longPressDuration = attributeValue;
        return;
      case 'onLongPressDisabled':
        this._onLongPressDisabled = !!attributeValue;
        return;
      case 'onLongPress':
        this._onLongPress = attributeValue;
        return;
      case 'onLongPressPredicate':
        this._onLongPressPredicate = attributeValue;
        return;
      case 'onDragDisabled':
        this._onDragDisabled = !!attributeValue;
        return;
      case 'onDrag':
        this._onDrag = attributeValue;
        return;
      case 'onDragPredicate':
        this._onDragPredicate = attributeValue;
        return;
      case 'onPinchDisabled':
      case 'onPinch':
      case 'onPinchPredicate':
      case 'onRotateDisabled':
      case 'onRotate':
      case 'onRotatePredicate':
        // These require multi-touch gesture implementation by tracking `event.touches`.
        // For example, pinch would track the distance between two touch points,
        // and rotate would track the angle formed by them.
        console.log('WebValdiLayout not implemented: ', attributeName, attributeValue);
        return;
      case 'scaleX':
        this._transforms.scaleX = attributeValue;
        this.updateTransform();
        return;
      case 'scaleY':
        this._transforms.scaleY = attributeValue;
        this.updateTransform();
        return;
      case 'rotation':
        this._transforms.rotation = attributeValue;
        this.updateTransform();
        return;
      case 'translationX':
        this._transforms.translationX = attributeValue;
        this.updateTransform();
        return;
      case 'translationY':
        this._transforms.translationY = attributeValue;
        this.updateTransform();
        return;
      case 'canAlwaysScrollHorizontal':
        this.htmlElement.style.overflowX = attributeValue ? 'scroll' : 'auto';
        return;
      case 'canAlwaysScrollVertical':
        this.htmlElement.style.overflowY = attributeValue ? 'scroll' : 'auto';
        return;
      case 'maskOpacity':
        // This could map to CSS opacity, but it's often used with a mask.
        this.htmlElement.style.opacity = String(attributeValue);
        return;
      case 'maskPath':
        // This can be implemented using CSS `clip-path` or `mask-image`.
        // The format of `attributeValue` would determine the exact implementation.
        // e.g., `url(...)` for an SVG mask or `polygon(...)` for a shape.
        this.htmlElement.style.clipPath = attributeValue;
        return;
      case 'filterTouchesWhenObscured':
        // This is a mobile-specific security feature. Browsers handle this behavior.
        console.log('WebValdiLayout not implemented: ', attributeName, attributeValue);
        return;
      /*
        From NativeTemplateElements LayoutAttributes
      */
      case 'onLayout':
        if (this._onLayoutObserver) {
          this._onLayoutObserver.disconnect();
        }
        if (attributeValue) {
          this._onLayoutObserver = new ResizeObserver(entries => {
            for (const entry of entries) {
              const { width, height, x, y } = entry.contentRect;
              attributeValue({ layout: { x, y, width, height } });
            }
          });
          this._onLayoutObserver.observe(this.htmlElement);
        }
        return;
      case 'onVisibilityChanged':
        this._onVisibilityChanged = attributeValue;
        return;
      case 'onViewportChanged':
        this._onViewportChanged = attributeValue;
        return;
      case 'onLayoutComplete':
        if (typeof attributeValue === 'function') {
          // Schedules the callback to run after the next browser repaint.
          requestAnimationFrame(attributeValue);
        }
        return;
      case 'lazyLayout':
        // Uses `content-visibility` for performance optimization.
        this.htmlElement.style.setProperty('content-visibility', attributeValue ? 'auto' : 'visible');
        return;
      case 'onMeasure':
        if (typeof attributeValue === 'function') {
          const rect = this.htmlElement.getBoundingClientRect();
          attributeValue({ x: rect.x, y: rect.y, width: rect.width, height: rect.height });
        }
        return;
      case 'estimatedWidth':
        // Used with `content-visibility: auto` to reserve space.
        this.htmlElement.style.containIntrinsicWidth = `${attributeValue}px`;
        return;
      case 'estimatedHeight':
        // Used with `content-visibility: auto` to reserve space.
        this.htmlElement.style.containIntrinsicHeight = `${attributeValue}px`;
        return;
      case 'limitToViewport':
        this.htmlElement.style.overflow = attributeValue ? 'hidden' : 'visible';
        return;
      case 'lazy':
        // Assuming 'lazy' is equivalent to 'lazyLayout' for a layout component.
        this.htmlElement.style.setProperty('content-visibility', attributeValue ? 'auto' : 'visible');
        return;
      case 'id':
        this.htmlElement.setAttribute('id', attributeValue);
        return;
      case 'key':
        // Use a data-attribute for keys, useful for debugging and testing.
        this.htmlElement.setAttribute('data-key', attributeValue);
        return;
      case 'animationsEnabled':
        if (attributeValue === false) {
          this.htmlElement.style.transition = 'none';
        } else {
          this.htmlElement.style.transition = ''; // Re-enable transitions
        }
        return;
      case 'class':
        this.htmlElement.className = attributeValue;
        return;
      case 'ignoreParentViewport':
        // No direct web equivalent for ignoring a parent's clipping.
        // `position: fixed` is relative to the viewport, which is different.
        console.log('WebValdiLayout not implemented: ', attributeName, attributeValue);
        return;
      case 'aspectRatio':
        this.htmlElement.style.aspectRatio = String(attributeValue);
        return;
      case 'display':
        this.htmlElement.style.display = attributeValue;
        return;
      case 'accessibilityRole':
        this.htmlElement.setAttribute('role', attributeValue);
        return;
      case 'accessibilityNavigation':
        // Not a thing on the web. Navigation is based on DOM structure and ARIA roles.
        console.log('WebValdiLayout not implemented: ', attributeName, attributeValue);
        return;
      case 'accessibilityPriority':
        // Not a thing on the web. Focus order can be influenced by `tabindex`.
        console.log('WebValdiLayout not implemented: ', attributeName, attributeValue);
        return;
      case 'accessibilityLabel':
        this.htmlElement.setAttribute('aria-label', attributeValue);
        return;
      case 'accessibilityHint':
        this.htmlElement.setAttribute('title', attributeValue);
        return;
      case 'accessibilityValue':
        this.htmlElement.setAttribute('aria-valuetext', attributeValue);
        return;
      case 'accessibilityStateDisabled':
        this.htmlElement.setAttribute('aria-disabled', String(!!attributeValue));
        return;
      case 'accessibilityStateLiveRegion':
        this.htmlElement.setAttribute('aria-live', attributeValue ? 'polite' : 'off');
        return;
      case 'accessibilityHidden': // Alias for accessibilityElementsHidden
      case 'accessibilityElementsHidden':
        this.htmlElement.setAttribute('aria-hidden', String(!!attributeValue));
        return;
      case 'accessibilityStateChecked':
        // Can be 'true', 'false', or 'mixed' for tri-state checkboxes.
        this.htmlElement.setAttribute('aria-checked', String(attributeValue));
        return;
      case 'accessibilityStateExpanded':
        this.htmlElement.setAttribute('aria-expanded', String(!!attributeValue));
        return;
      case 'accessibilityViewIsModal':
        this.htmlElement.setAttribute('aria-modal', String(!!attributeValue));
        return;
      case 'onAccessibilityTap':
      case 'onAccessibilityActivate': // Usually the same as a tap/click.
        this.htmlElement.addEventListener('click', e => {
          if (typeof attributeValue === 'function') {
            attributeValue(e);
          }
        });
        return;
      case 'onAccessibilityEscape':
        this.htmlElement.addEventListener('keydown', (e: KeyboardEvent) => {
          if (e.key === 'Escape' && typeof attributeValue === 'function') {
            attributeValue(e);
          }
        });
        return;
      case 'accessibilityIgnoresInvertColors': // iOS-specific.
      case 'accessibilityTraits': // Complex mapping to ARIA roles/states.
      case 'onAccessibilityMagicTap': // iOS-specific gesture.
      case 'onAccessibilityIncrement': // For adjustable controls.
      case 'onAccessibilityDecrement': // For adjustable controls.
        // These properties do not have a direct, simple web equivalent
        // and would require a custom implementation or library.
        console.log('WebValdiLayout not implemented: ', attributeName, attributeValue);
        return;
      case 'width':
        this.htmlElement.style.width = attributeValue;
        return;
      case 'height':
        this.htmlElement.style.height = attributeValue;
        return;
    }

    throw new Error(`Attribute ${attributeName} is not valid`);
  }
}
