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
  registerElements,
  setAllElementsAttributeDelegate,
} from './HTMLRenderer';

export interface UpdateAttributeDelegate {
  updateAttribute(elementId: number, attributeName: string, attributeValue: any): void;
}

export class ValdiWebRendererDelegate implements IRendererDelegate {
  private attributeDelegate?: UpdateAttributeDelegate;

  constructor(private htmlRoot: HTMLElement) {
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
  }
  onElementDestroyed(id: number): void {
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
    // TOOD(mgharmalkar)
    // console.log('registerFrameObserver');
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
