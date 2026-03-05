import { AnimationOptions } from 'valdi_core/src/AnimationOptions';
import { FrameObserver, IRendererDelegate, VisibilityObserver } from 'valdi_core/src/IRendererDelegate';
import { Style } from 'valdi_core/src/Style';
import { NativeNode } from 'valdi_tsx/src/NativeNode';
import { NativeView } from 'valdi_tsx/src/NativeView';
import {
  changeAttributeOnElement,
  createElement,
  destroyElement,
  makeElementRoot,
  moveElement,
  nodesRef,
  registerElements,
  setAllElementsAttributeDelegate,
} from './HTMLRenderer';

export interface UpdateAttributeDelegate {
  updateAttribute(elementId: number, attributeName: string, attributeValue: any): void;
}

export class ValdiWebRendererDelegate implements IRendererDelegate {
  private attributeDelegate?: UpdateAttributeDelegate;
  private frameObserver?: FrameObserver;
  private resizeObserver?: ResizeObserver;
  private elementIdByHtmlElement = new WeakMap<Element, number>();

  constructor(private htmlRoot: HTMLElement | ShadowRoot) {
    registerElements();
  }
  setAttributeDelegate(delegate: UpdateAttributeDelegate) {
    this.attributeDelegate = delegate;

    setAllElementsAttributeDelegate(this.attributeDelegate);
  }

  onElementBecameRoot(id: number): void {
    makeElementRoot(id, this.htmlRoot);
  }
  onElementMoved(id: number, parentId: number, parentIndex: number): void {
    moveElement(id, parentId, parentIndex);
  }
  onElementCreated(id: number, viewClass: string): void {
    createElement(id, viewClass, this.attributeDelegate);
    const element = nodesRef.get(id);
    if (element?.htmlElement) {
      this.elementIdByHtmlElement.set(element.htmlElement, id);
      this.resizeObserver?.observe(element.htmlElement);
    }
  }
  onElementDestroyed(id: number): void {
    const element = nodesRef.get(id);
    if (element?.htmlElement) {
      this.resizeObserver?.unobserve(element.htmlElement);
    }
    destroyElement(id);
  }
  onElementAttributeChangeAny(id: number, attributeName: string, attributeValue: any): void {
    changeAttributeOnElement(id, attributeName, attributeValue);
  }
  onElementAttributeChangeNumber(id: number, attributeName: string, attributeValue: number): void {
    changeAttributeOnElement(id, attributeName, attributeValue);
  }
  onElementAttributeChangeString(id: number, attributeName: string, attributeValue: string): void {
    changeAttributeOnElement(id, attributeName, attributeValue);
  }
  onElementAttributeChangeTrue(id: number, attributeName: string): void {
    changeAttributeOnElement(id, attributeName, undefined);
  }
  onElementAttributeChangeFalse(id: number, attributeName: string): void {
    changeAttributeOnElement(id, attributeName, undefined);
  }
  onElementAttributeChangeUndefined(id: number, attributeName: string): void {
    changeAttributeOnElement(id, attributeName, undefined);
  }
  onElementAttributeChangeStyle(id: number, attributeName: string, style: Style<any>): void {
    const attributes = style.attributes ?? {};
    Object.keys(attributes).forEach(key => {
      changeAttributeOnElement(id, key, attributes[key]);
    });
  }
  onElementAttributeChangeFunction(id: number, attributeName: string, fn: () => void): void {
    changeAttributeOnElement(id, attributeName, fn);
  }
  onNextLayoutComplete(callback: () => void): void {}
  onRenderStart(): void {
    // TODO(mgharmalkar)
    // console.log('onRenderStart');
  }
  onRenderEnd(): void {
    // TODO(mgharmalkar)
    // console.log('onRenderEnd');
  }
  onAnimationStart(options: AnimationOptions, token: number): void {
    // TODO: no animation support on web yet, so just call completion with cancelled = false.
    options.completion?.(false);
  }
  onAnimationEnd(): void {}
  onAnimationCancel(token: number): void {}
  registerVisibilityObserver(observer: VisibilityObserver): void {
    // TODO(mgharmalkar)
    // console.log('registerVisibilityObserver');
  }
  registerFrameObserver(observer: FrameObserver): void {
    this.frameObserver = observer;

    this.resizeObserver = new ResizeObserver((entries) => {
      if (!this.frameObserver) return;

      const updates: number[] = [];
      for (const entry of entries) {
        const elementId = this.elementIdByHtmlElement.get(entry.target);
        if (elementId === undefined) continue;

        const htmlElement = entry.target as HTMLElement;
        const rect = htmlElement.getBoundingClientRect();
        const offsetParent = htmlElement.offsetParent as HTMLElement | null;

        let x: number;
        let y: number;
        if (offsetParent) {
          const parentRect = offsetParent.getBoundingClientRect();
          const cs = getComputedStyle(offsetParent);
          x = rect.left - parentRect.left + offsetParent.scrollLeft - (parseFloat(cs.borderLeftWidth) || 0);
          y = rect.top - parentRect.top + offsetParent.scrollTop - (parseFloat(cs.borderTopWidth) || 0);
        } else {
          x = rect.left;
          y = rect.top;
        }

        updates.push(elementId, x, y, rect.width, rect.height);
      }

      if (updates.length > 0) {
        this.frameObserver(new Float64Array(updates));
      }
    });
  }
  getNativeView(id: number, callback: (instance: NativeView | undefined) => void): void {}
  getNativeNode(id: number): NativeNode | undefined {
    throw new Error('Method not implemented.');
  }
  getElementFrame(id: number, callback: (instance: any) => void): void {}
  takeElementSnapshot(id: number, callback: (snapshotBase64: string | undefined) => void): void {}
  onUncaughtError(message: string, error: Error): void {
    console.error(message, error);
  }
  onDestroyed(): void {}
}
