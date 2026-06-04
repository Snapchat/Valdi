import { WebValdiLayout } from './WebValdiLayout';

export class WebValdiScroll extends WebValdiLayout {
  public type = 'scroll';
  private _onScrollEndTimer: number | null = null;
  private _contentOffsetAnimated: boolean = false;
  private _fadingEdgeLength: number = 0;
  private _fadingEdgeStartEnabled: boolean = true;
  private _fadingEdgeEndEnabled: boolean = true;
  private _pagingEnabled: boolean = false;
  private _pagingObserver?: MutationObserver;
  private _scrollListeners: { type: string; handler: EventListener }[] = [];

  createHtmlElement(): HTMLElement {
    const element = super.createHtmlElement();

    Object.assign(element.style, {
      // Default to vertical scrolling
      overflowX: 'hidden',
      overflowY: 'auto',
      pointerEvents: 'auto',
    });

    return element;
  }

  destroy() {
    if (this._onScrollEndTimer !== null) {
      clearTimeout(this._onScrollEndTimer);
    }
    this._pagingObserver?.disconnect();
    for (const { type, handler } of this._scrollListeners) {
      this.htmlElement.removeEventListener(type, handler);
    }
    this._scrollListeners = [];
    super.destroy();
  }

  changeAttribute(attributeName: string, attributeValue: any): void {
    switch (attributeName) {
      case "onScroll":
        this._replaceScrollListener('onScroll', 'scroll', (() => {
          if (typeof attributeValue === 'function') {
            attributeValue({
              contentOffset: {
                x: this.htmlElement.scrollLeft,
                y: this.htmlElement.scrollTop,
              },
              contentSize: {
                width: this.htmlElement.scrollWidth,
                height: this.htmlElement.scrollHeight,
              },
              layoutMeasurement: {
                width: this.htmlElement.clientWidth,
                height: this.htmlElement.clientHeight,
              },
            });
          }
        }) as EventListener);
        return;
      case "onScrollEnd":
        this._replaceScrollListener('onScrollEnd', 'scroll', (() => {
          if (this._onScrollEndTimer !== null) {
            clearTimeout(this._onScrollEndTimer);
          }
          this._onScrollEndTimer = window.setTimeout(() => {
            if (typeof attributeValue === 'function') {
              attributeValue();
            }
          }, 100);
        }) as EventListener);
        return;
      case "onDragStart":
        this._replaceScrollListener('onDragStart:mousedown', 'mousedown', attributeValue);
        this._replaceScrollListener('onDragStart:touchstart', 'touchstart', attributeValue);
        return;
      case "onDragEnding":
        this._replaceScrollListener('onDragEnding', 'scroll', (() => {
          if (this._onScrollEndTimer !== null) {
            clearTimeout(this._onScrollEndTimer);
          }
          this._onScrollEndTimer = window.setTimeout(() => {
            if (typeof attributeValue === 'function') {
              attributeValue();
            }
          }, 150);
        }) as EventListener);
        return;
      case "onDragEnd":
        this._replaceScrollListener('onDragEnd:mouseup', 'mouseup', attributeValue);
        this._replaceScrollListener('onDragEnd:touchend', 'touchend', attributeValue);
        return;
      case "onContentSizeChange":
        const observer = new ResizeObserver(entries => {
            for (const entry of entries) {
                if (typeof attributeValue === 'function') {
                    attributeValue({
                        width: this.htmlElement.scrollWidth,
                        height: this.htmlElement.scrollHeight,
                    });
                }
            }
        });
        // This observes the scroll container itself. It will trigger when the
        // container's size changes, which is an approximation for content changes.
        // A more robust solution would observe children, but is more complex.
        observer.observe(this.htmlElement);
        return;
      case "bounces":
        // CSS `overscroll-behavior` can prevent scroll-chaining, but doesn't provide a bounce effect.
        // A true bounce effect requires a custom JS-based scrolling solution.
        this.htmlElement.style.overscrollBehavior = attributeValue ? 'auto' : 'contain';
        return;
      case "bouncesFromDragAtStart":
      case "bouncesFromDragAtEnd":
      case "bouncesVerticalWithSmallContent":
      case "bouncesHorizontalWithSmallContent":
        // No direct web equivalent for bounce physics.
        console.log("WebValdiScroll not implemented: ", attributeName, attributeValue);
        return;
      case "cancelsTouchesOnScroll":
        // This is default browser behavior. A no-op.
        return;
      case "dismissKeyboardOnDrag":
        this._replaceScrollListener('dismissKeyboardOnDrag', 'scroll', (() => {
          if (attributeValue && document.activeElement instanceof HTMLElement) {
            document.activeElement.blur();
          }
        }) as EventListener);
        return;
      case "pagingEnabled":
        this._pagingEnabled = !!attributeValue;
        if (attributeValue) {
          this.htmlElement.style.scrollSnapType = this.htmlElement.style.overflowX === 'hidden' ? 'y mandatory' : 'x mandatory';
          this._applySnapAlignToChildren();
          if (!this._pagingObserver) {
            this._pagingObserver = new MutationObserver(() => this._applySnapAlignToChildren());
            this._pagingObserver.observe(this.htmlElement, { childList: true });
          }
        } else {
          this.htmlElement.style.scrollSnapType = '';
          this._pagingObserver?.disconnect();
          this._pagingObserver = undefined;
          for (const child of Array.from(this.htmlElement.children) as HTMLElement[]) {
            child.style.scrollSnapAlign = '';
          }
        }
        return;
      case "horizontal":
       //Todo implement
        return;
      case "showsVerticalScrollIndicator":
        if (attributeValue) {
          this.htmlElement.classList.remove('hide-v-scrollbar');
          this.htmlElement.style.setProperty('scrollbar-width', 'auto'); // Firefox
        } else {
          this.htmlElement.classList.add('hide-v-scrollbar');
          this.htmlElement.style.setProperty('scrollbar-width', 'none'); // Firefox
        }
        return;
      case "showsHorizontalScrollIndicator":
        if (attributeValue) {
          this.htmlElement.classList.remove('hide-h-scrollbar');
          // Do NOT touch 'scrollbar-width' here; in Firefox it affects both axes.
        } else {
          this.htmlElement.classList.add('hide-h-scrollbar');
          // If you set 'scrollbar-width: none' you’ll also hide the vertical bar in Firefox.
          // So we leave it alone to avoid nuking the vertical scrollbar.
        }
        return;
      case "scrollEnabled":
        this.htmlElement.style.overflow = attributeValue ? 'auto' : 'hidden';
        return;
      case "ref":
        // This is likely for framework-level component references. No-op at this level.
        return;
      case "scrollPerfLoggerBridge":
      case "circularRatio":
        // Complex features requiring custom implementation.
        console.log("WebValdiScroll not implemented: ", attributeName, attributeValue);
        return;
      case 'fadingEdgeLength':
        this._fadingEdgeLength = attributeValue;
        this._updateFadingEdge();
        return;
      case 'fadingEdgeStart':
        this._fadingEdgeStartEnabled = attributeValue;
        this._updateFadingEdge();
        return;
      case 'fadingEdgeEnd':
        this._fadingEdgeEndEnabled = attributeValue;
        this._updateFadingEdge();
        return;
      case "decelerationRate":
        // Controls scroll momentum, not directly controllable via standard web APIs.
        console.log("WebValdiScroll not implemented: ", attributeName, attributeValue);
        return;
      case "viewportExtensionTop":
      case "viewportExtensionRight":
      case "viewportExtensionBottom":
      case "viewportExtensionLeft":
        // Related to virtualized lists; handled by the list implementation, not the scroll container.
        console.log("WebValdiScroll not implemented: ", attributeName, attributeValue);
        return;
      case "contentOffsetX":
        if (this._contentOffsetAnimated) {
          this.htmlElement.scrollTo({ left: attributeValue, behavior: 'smooth' });
        } else {
          this.htmlElement.scrollLeft = attributeValue;
        }
        return;
      case "contentOffsetY":
        if (this._contentOffsetAnimated) {
          this.htmlElement.scrollTo({ top: attributeValue, behavior: 'smooth' });
        } else {
          this.htmlElement.scrollTop = attributeValue;
        }
        return;
      case "contentOffsetAnimated":
        this._contentOffsetAnimated = !!attributeValue;
        return;
      case "staticContentWidth":
      case "staticContentHeight":
        // These are hints for native layout systems, often for virtualization.
        // No direct equivalent for a simple web scroll view.
        console.log("WebValdiScroll not implemented: ", attributeName, attributeValue);
        return;
    }
    super.changeAttribute(attributeName, attributeValue);
  }

  private _applySnapAlignToChildren(): void {
    if (!this._pagingEnabled) return;
    for (const child of Array.from(this.htmlElement.children) as HTMLElement[]) {
      child.style.scrollSnapAlign = 'start';
    }
  }

  private _replaceScrollListener(key: string, eventType: string, handler: EventListener) {
    const idx = this._scrollListeners.findIndex(l => l.type === key);
    if (idx >= 0) {
      this.htmlElement.removeEventListener(eventType, this._scrollListeners[idx].handler);
      this._scrollListeners.splice(idx, 1);
    }
    this.htmlElement.addEventListener(eventType, handler);
    this._scrollListeners.push({ type: key, handler });
  }

  private _updateFadingEdge(): void {
    if (this._fadingEdgeLength <= 0) {
      this.htmlElement.style.maskImage = '';
      this.htmlElement.style.webkitMaskImage = '';
      return;
    }

    const length = `${this._fadingEdgeLength}px`;
    const isHorizontal = this.htmlElement.style.overflowX !== 'hidden';
    const gradientDirection = isHorizontal ? 'to right' : 'to bottom';

    let gradientStops: string;
    if (this._fadingEdgeStartEnabled && this._fadingEdgeEndEnabled) {
      gradientStops = `transparent, black ${length}, black calc(100% - ${length}), transparent`;
    } else if (this._fadingEdgeStartEnabled) {
      gradientStops = `transparent, black ${length}, black`;
    } else if (this._fadingEdgeEndEnabled) {
      gradientStops = `black, black calc(100% - ${length}), transparent`;
    } else {
      this.htmlElement.style.maskImage = '';
      this.htmlElement.style.webkitMaskImage = '';
      return;
    }

    const gradient = `linear-gradient(${gradientDirection}, ${gradientStops})`;
    this.htmlElement.style.maskImage = gradient;
    this.htmlElement.style.webkitMaskImage = gradient;
  }
}
