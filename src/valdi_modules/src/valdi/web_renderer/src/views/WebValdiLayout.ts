import { generateStyles, isAttributeValidStyle } from '../styles/ValdiWebStyles';
import { UpdateAttributeDelegate } from '../ValdiWebRendererDelegate';
import { TouchEventState } from 'valdi_tsx/src/GestureEvents';
import { WebAnimationManager } from '../WebAnimationManager';

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

  makeRoot(root: HTMLElement) {
    this.parent?.removeChild(this);
    this.parent = null;
    root.replaceChildren(this.htmlElement);
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
      parts.push(`rotate(${rotation}deg)`);
    }
    this.htmlElement.style.transform = parts.join(' ');
  }

  private updateTouchAreaExtension() {
    const { top, right, bottom, left } = this._touchAreaExtension;
    // Only use negative margin to extend touch area, don't add padding
    // This matches native behavior where touchAreaExtension doesn't affect visual layout
    if (top === 0 && right === 0 && bottom === 0 && left === 0) {
      this.htmlElement.style.margin = '';
    } else {
      this.htmlElement.style.margin = `-${top}px -${right}px -${bottom}px -${left}px`;
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
    if (attributeName === 'boxShadow' || attributeName === 'borderRadius') {
      console.log('[WebValdiLayout.changeAttribute] Entry:', { attributeName, attributeValue, elementId: this.id, elementType: this.type });
    }
    if (isAttributeValidStyle(attributeName)) {
      const generatedStyles = generateStyles(attributeName, attributeValue);
      if (attributeName === 'boxShadow' || attributeName === 'borderRadius') {
        console.log('[WebValdiLayout.changeAttribute]', {
          attributeName,
          attributeValue,
          generatedStyles,
          hasGeneratedStyles: Object.keys(generatedStyles).length > 0,
        });
      }
      
      // Get animation manager if available
      const animationManager = (window as any).__valdiAnimationManager as WebAnimationManager | undefined;
      const animationContext = animationManager?.getAnimationContext(this.id);
      
      // Apply all styles
      Object.keys(generatedStyles).forEach(key => {
        const value = generatedStyles[key as keyof CSSStyleDeclaration];
        if (attributeName === 'boxShadow' || attributeName === 'borderRadius') {
          console.log('[WebValdiLayout] Applying style:', { key, value, elementId: this.id });
        }
        if (value === '' || value === null || value === undefined) {
          this.htmlElement.style.removeProperty(key);
        } else {
          (this.htmlElement.style as any)[key] = value;
          if (attributeName === 'boxShadow' || attributeName === 'borderRadius') {
            const computedStyle = window.getComputedStyle(this.htmlElement);
            console.log('[WebValdiLayout] Style applied, element:', {
              elementId: this.id,
              styleKey: key,
              styleValue: this.htmlElement.style[key as any],
              computedValue: computedStyle[key as any],
              width: computedStyle.width,
              height: computedStyle.height,
              element: this.htmlElement,
            });
          }
        }
      });
      
      // If justifyContent is set (either now or already on element), ensure flexbox is properly configured
      // Check position after styles are applied (it might be set in this same update)
      const hasJustifyContent = 'justifyContent' in generatedStyles || 
                                (this.htmlElement.style.justifyContent !== '' && this.htmlElement.style.justifyContent !== 'normal');
      const isAbsolute = this.htmlElement.style.position === 'absolute' || generatedStyles.position === 'absolute';
      
      if (hasJustifyContent && isAbsolute) {
        // Ensure display: flex is set for flexbox to work
        this.htmlElement.style.display = 'flex';
        // Ensure flexDirection is column for vertical centering (justifyContent centers along main axis)
        // Only set if not already explicitly set
        if (!generatedStyles.flexDirection) {
          this.htmlElement.style.flexDirection = 'column';
        }
        // With position: absolute, top: 0, bottom: 0, the element should stretch to parent height
        // This is needed for justify-content to work properly
        const hasTop = this.htmlElement.style.top !== '' && this.htmlElement.style.top !== 'auto';
        const hasBottom = this.htmlElement.style.bottom !== '' && this.htmlElement.style.bottom !== 'auto';
        if (hasTop && hasBottom) {
          // Element should automatically have height from top/bottom
          // Ensure no conflicting height is set
          if (this.htmlElement.style.height && this.htmlElement.style.height !== 'auto' && this.htmlElement.style.height !== '100%') {
            // If height is explicitly set to a fixed value, remove it to let top/bottom control the height
            this.htmlElement.style.height = '';
          }
        }
      }
      
      // For child View elements inside a flex container with justifyContent, ensure they don't interfere
      // Only apply to View elements (not Layout) that are relatively positioned with explicit dimensions
      // This targets elements like thumbInner that should be simple block elements, not flex containers
      if (this.type === 'view' && this.parent && 
          (!this.htmlElement.style.position || this.htmlElement.style.position === 'relative')) {
        const hasExplicitWidth = this.htmlElement.style.width !== '' && this.htmlElement.style.width !== 'auto';
        const hasExplicitHeight = this.htmlElement.style.height !== '' && this.htmlElement.style.height !== 'auto';
        const parentComputedStyle = window.getComputedStyle(this.parent.htmlElement);
        const parentHasJustifyContent = parentComputedStyle.justifyContent !== 'normal' && 
                                        parentComputedStyle.justifyContent !== '';
        
        // Only override if: View element, explicit dimensions, parent has justifyContent, and not already block
        if (hasExplicitWidth && hasExplicitHeight && parentHasJustifyContent && 
            this.htmlElement.style.display === 'flex') {
          // Simple View element with explicit dimensions inside a flex container with justifyContent
          // Use block display to prevent flex properties from interfering with centering
          this.htmlElement.style.display = 'block';
        }
      }
      
      // Apply animation transitions if element is being animated
      if (animationContext && animationManager) {
        const transition = animationManager.getAllPropertiesTransition(animationContext.options);
        this.htmlElement.style.transition = transition;
        animationManager.markElementAnimated(this.id, animationContext.token);
      }
      
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
        // slowClipping enables proper clipping with border-radius by using overflow: hidden
        this.htmlElement.style.overflow = attributeValue ? 'hidden' : 'visible';
        return;
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
        // Ensure width values are converted to pixels if they're numbers
        if (typeof attributeValue === 'number') {
          this.htmlElement.style.width = `${attributeValue}px`;
        } else {
          this.htmlElement.style.width = attributeValue;
        }
        return;
      case 'height':
        // Ensure height values are converted to pixels if they're numbers
        if (typeof attributeValue === 'number') {
          this.htmlElement.style.height = `${attributeValue}px`;
        } else {
          this.htmlElement.style.height = attributeValue;
        }
        return;
    }

    throw new Error(`Attribute ${attributeName} is not valid`);
  }
}
