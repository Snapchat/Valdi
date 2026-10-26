import { NativeNode } from 'valdi_tsx/src/NativeNode';
import { NativeView } from 'valdi_tsx/src/NativeView';
import { ViewFactory } from 'valdi_tsx/src/ViewFactory';
import { AnimationOptions } from './AnimationOptions';
import { CancelToken } from './CancellableAnimation';
import { ElementFrame } from 'valdi_tsx/src/Geometry';
import { Style } from './Style';

export type VisibilityObserver = (
  appearingElements: number[],
  disappearingElements: number[],
  viewportUpdates: number[],
  eventTime: number,
) => void;
export interface IRendererDelegate {
  setRenderCompleteScheduler(schedule: (callback: () => void) => void): void;

  onElementBecameRoot(id: number): void;
  onElementMoved(id: number, parentId: number, parentIndex: number): void;
  onElementCreated(id: number, viewClass: string): void;
  onCustomElementCreated(id: number, viewFactory: ViewFactory): void;
  onElementDestroyed(id: number): void;
  onElementDestroyedFromParent(id: number): void;

  onElementAttributeChangeAny(id: number, attributeName: string, attributeValue: any): void;
  onElementAttributeChangeNumber(id: number, attributeName: string, value: number): void;
  onElementAttributeChangeString(id: number, attributeName: string, value: string): void;
  onElementAttributeChangeTrue(id: number, attributeName: string): void;
  onElementAttributeChangeFalse(id: number, attributeName: string): void;
  onElementAttributeChangeUndefined(id: number, attributeName: string): void;
  onElementAttributeChangeStyle(id: number, attributeName: string, style: Style<any>): void;
  onElementAttributeChangeFunction(id: number, attributeName: string, fn: () => void): void;

  onNextLayoutComplete(callback: () => void): void;
  onNextDraw(callback: (hookTimeMs: number) => void): void;

  onRenderStart(): void;
  onRenderEnd(): void;

  onAnimationStart(options: AnimationOptions, token: CancelToken): void;
  onAnimationEnd(): void;
  onAnimationCancel(token: CancelToken): void;

  registerVisibilityObserver(observer: VisibilityObserver): void;

  getNativeView(id: number, callback: (instance: NativeView | undefined) => void): void;
  getNativeNode(id: number): NativeNode | undefined;
  getCachedElementFrame(id: number): ElementFrame | undefined;
  getElementFrame(id: number, callback: (instance: ElementFrame | undefined) => void): void;
  takeElementSnapshot(id: number, callback: (snapshotBase64: string | undefined) => void): void;

  onUncaughtError(message: string, error: Error): void;

  onDestroyed(): void;
}
