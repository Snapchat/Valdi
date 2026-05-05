import { AnimationOptions } from 'valdi_core/src/AnimationOptions';
import { FrameObserver, IRendererDelegate, VisibilityObserver } from 'valdi_core/src/IRendererDelegate';
import { Style } from 'valdi_core/src/Style';
import { ElementFrame } from 'valdi_tsx/src/Geometry';
import { NativeNode } from 'valdi_tsx/src/NativeNode';
import { NativeView } from 'valdi_tsx/src/NativeView';

export enum RawRenderRequestEntryType {
  createElement = 'CreateElement',
  destroyElement = 'DestroyElement',
  moveElementToParent = 'MoveElementToParent',
  removeElementFromParent = 'RemoveElementFromParent',
  setElementAttribute = 'SetElementAttribute',
  setRootElement = 'SetRootElement',
  beginAnimations = 'BeginAnimations',
  endAnimations = 'EndAnimations',
  cancelAnimation = 'CancelAnimation',
}

export interface RawRenderRequestEntry {
  type: RawRenderRequestEntryType;
  id?: number;
  parentId?: number;
  parentIndex?: number;
  className?: string;
  name?: string;
  value?: any;
  options?: AnimationOptions;
}

export interface RenderRequest {
  entries: RawRenderRequestEntry[];
}

export class RendererTestDelegate implements IRendererDelegate {
  requests: RenderRequest[] = [];
  nextLayoutCompleteCallbacks: Array<() => void> = [];
  nextDrawCallbacks: Array<(hookTimeMs: number) => void> = [];
  private entries: RawRenderRequestEntry[];
  private observer: VisibilityObserver | undefined;
  private error: Error | undefined;

  constructor() {
    this.requests = [];
    this.entries = [];
  }

  clear() {
    this.requests = [];
    this.nextLayoutCompleteCallbacks = [];
    this.nextDrawCallbacks = [];
  }

  onElementBecameRoot(id: number): void {
    this.entries.push({
      type: RawRenderRequestEntryType.setRootElement,
      id,
    });
  }

  onElementMoved(id: number, parentId: number, parentIndex: number): void {
    this.entries.push({
      type: RawRenderRequestEntryType.moveElementToParent,
      id,
      parentId,
      parentIndex,
    });
  }

  onElementCreated(id: number, viewClass: string): void {
    this.entries.push({
      type: RawRenderRequestEntryType.createElement,
      id,
      className: viewClass,
    });
  }

  onElementDestroyed(id: number): void {
    this.entries.push({
      type: RawRenderRequestEntryType.destroyElement,
      id,
    });
  }

  onElementAttributeChangeAny(id: number, attributeName: string, attributeValue: any): void {
    this.entries.push({
      type: RawRenderRequestEntryType.setElementAttribute,
      id,
      name: attributeName,
      value: attributeValue,
    });
  }

  onElementAttributeChangeNumber(id: number, attributeName: string, value: number): void {
    this.onElementAttributeChangeAny(id, attributeName, value);
  }

  onElementAttributeChangeString(id: number, attributeName: string, value: string): void {
    this.onElementAttributeChangeAny(id, attributeName, value);
  }

  onElementAttributeChangeTrue(id: number, attributeName: string): void {
    this.onElementAttributeChangeAny(id, attributeName, true);
  }

  onElementAttributeChangeFalse(id: number, attributeName: string): void {
    this.onElementAttributeChangeAny(id, attributeName, false);
  }

  onElementAttributeChangeUndefined(id: number, attributeName: string): void {
    this.onElementAttributeChangeAny(id, attributeName, undefined);
  }

  onElementAttributeChangeStyle(id: number, attributeName: string, style: Style<any>): void {
    this.onElementAttributeChangeAny(id, attributeName, style);
  }

  onElementAttributeChangeFunction(id: number, attributeName: string, fn: () => void): void {
    this.onElementAttributeChangeAny(id, attributeName, fn);
  }

  onNextLayoutComplete(cb: () => void) {
    this.nextLayoutCompleteCallbacks.push(cb);
  }

  onNextDraw(cb: (hookTimeMs: number) => void) {
    this.nextDrawCallbacks.push(cb);
  }

  onAnimationStart(options: AnimationOptions) {
    this.entries.push({
      type: RawRenderRequestEntryType.beginAnimations,
      options,
    });
  }

  onAnimationEnd() {
    this.entries.push({
      type: RawRenderRequestEntryType.endAnimations,
    });
  }

  onAnimationCancel(token: number): void {
    this.entries.push({
      type: RawRenderRequestEntryType.cancelAnimation,
    });
  }

  onRenderStart(): void {
    this.error = undefined;
  }

  onRenderEnd(): void {
    if (this.entries.length) {
      this.requests.push({
        entries: this.entries,
      });
      this.entries = [];
    }
    if (this.error) {
      throw this.error;
    }
  }

  notifyElementVisibilityChanged(elementId: number, visible: boolean) {
    if (this.observer) {
      if (visible) {
        this.observer([elementId], [], [], 0);
      } else {
        this.observer([], [elementId], [], 0);
      }
    }
  }

  registerVisibilityObserver(observer: VisibilityObserver) {
    this.observer = observer;
  }

  registerFrameObserver(observer: FrameObserver) {}

  getNativeView(id: number, callback: (view: NativeView | undefined) => void) {
    callback(undefined);
  }

  getNativeNode(id: number): NativeNode | undefined {
    return undefined;
  }

  getElementFrame(id: number, callback: (frame: ElementFrame | undefined) => void) {
    callback({ x: 0, y: 0, width: 0, height: 0 });
  }

  takeElementSnapshot(id: number, callback: (base64String: string | undefined) => void) {
    callback(undefined);
  }

  onDestroyed() {}

  onUncaughtError(message: string, error: Error) {
    if (!this.error) {
      this.error = error;
    }
  }
}
