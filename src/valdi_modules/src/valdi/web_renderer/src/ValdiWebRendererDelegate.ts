import { AnimationOptions } from 'valdi_core/src/AnimationOptions';
import { IRendererDelegate, VisibilityObserver } from 'valdi_core/src/IRendererDelegate';
import { Style } from 'valdi_core/src/Style';
import { ElementFrame } from 'valdi_tsx/src/Geometry';
import { NativeNode } from 'valdi_tsx/src/NativeNode';
import { NativeView } from 'valdi_tsx/src/NativeView';
import { ViewFactory } from 'valdi_tsx/src/ViewFactory';
import type { AttributeUpdatedExternallyDelegate } from './core/ElementClass';
import { ViewNodeTree } from './core/ViewNodeTree';
import { WebViewFactory } from './ViewFactory';

export class ValdiWebRendererDelegate implements IRendererDelegate {
  private attributeUpdatedExternallyDelegate?: AttributeUpdatedExternallyDelegate;

  constructor(
    private htmlRoot: HTMLElement | ShadowRoot,
    private readonly viewNodeTree: ViewNodeTree,
  ) {}

  setRenderCompleteScheduler(schedule: (callback: () => void) => void): void {
    this.viewNodeTree.setRenderCompleteScheduler(schedule);
  }

  setAttributeUpdatedExternallyDelegate(delegate: AttributeUpdatedExternallyDelegate): void {
    this.attributeUpdatedExternallyDelegate = delegate;
  }

  onElementBecameRoot(id: number): void {
    this.viewNodeTree.makeElementRoot(id, this.htmlRoot);
    this.viewNodeTree.scheduleVisibilityRefresh(false);
  }
  onElementMoved(id: number, parentId: number, parentIndex: number): void {
    this.viewNodeTree.moveElement(id, parentId, parentIndex);
    this.viewNodeTree.scheduleVisibilityRefresh(false);
  }
  onElementCreated(id: number, viewClass: string): void {
    this.viewNodeTree.createElement(id, viewClass, this.attributeUpdatedExternallyDelegate);
  }
  onCustomElementCreated(id: number, viewFactory: ViewFactory): void {
    if (!(viewFactory instanceof WebViewFactory)) {
      throw new Error('Expected a web view factory when creating a custom element');
    }
    this.viewNodeTree.createElementWithClass(
      id,
      'custom-view',
      viewFactory.elementClass,
      this.attributeUpdatedExternallyDelegate,
    );
  }
  onElementDestroyed(id: number): void {
    this.viewNodeTree.destroyElement(id);
  }
  onElementDestroyedFromParent(id: number): void {}
  onElementAttributeChangeAny(id: number, attributeName: string, attributeValue: any): void {
    this.viewNodeTree.setAttributeOnElement(id, attributeName, attributeValue);
  }
  onElementAttributeChangeNumber(id: number, attributeName: string, attributeValue: number): void {
    this.viewNodeTree.setAttributeOnElement(id, attributeName, attributeValue);
  }
  onElementAttributeChangeString(id: number, attributeName: string, attributeValue: string): void {
    this.viewNodeTree.setAttributeOnElement(id, attributeName, attributeValue);
  }
  onElementAttributeChangeTrue(id: number, attributeName: string): void {
    this.viewNodeTree.setAttributeOnElement(id, attributeName, true);
  }
  onElementAttributeChangeFalse(id: number, attributeName: string): void {
    this.viewNodeTree.setAttributeOnElement(id, attributeName, false);
  }
  onElementAttributeChangeUndefined(id: number, attributeName: string): void {
    this.viewNodeTree.setAttributeOnElement(id, attributeName, undefined);
  }
  onElementAttributeChangeStyle(id: number, attributeName: string, style: Style<any>): void {
    this.viewNodeTree.setStyleAttributeOnElement(id, attributeName, style);
  }
  onElementAttributeChangeFunction(id: number, attributeName: string, fn: () => void): void {
    this.viewNodeTree.setAttributeOnElement(id, attributeName, fn);
  }
  onNextLayoutComplete(callback: () => void): void {
    if (typeof document !== 'undefined' && document.visibilityState === 'hidden') {
      Promise.resolve().then(() => {
        this.drainScheduledLayoutObservers(true);
        Promise.resolve().then(() => {
          this.drainScheduledLayoutObservers(true);
          callback();
        });
      });
      return;
    }
    requestAnimationFrame(() => {
      this.drainScheduledLayoutObservers(false);
      requestAnimationFrame(() => {
        this.drainScheduledLayoutObservers(false);
        callback();
      });
    });
  }
  onNextDraw(callback: (hookTimeMs: number) => void): void {
    requestAnimationFrame(hookTimeMs => callback(hookTimeMs));
  }
  onRenderStart(): void {
    this.viewNodeTree.beginRender();
  }
  onRenderEnd(): void {
    this.viewNodeTree.endRender();
    this.viewNodeTree.scheduleVisibilityRefresh(false);
  }
  onAnimationStart(options: AnimationOptions, token: number): void {
    this.viewNodeTree.beginAnimation(options, token);
  }
  onAnimationEnd(): void {
    this.viewNodeTree.endAnimation();
  }
  onAnimationCancel(token: number): void {
    this.viewNodeTree.cancelAnimation(token);
  }
  registerVisibilityObserver(observer: VisibilityObserver): void {
    this.viewNodeTree.registerVisibilityObserver(observer);
  }
  getNativeView(id: number, callback: (instance: NativeView | undefined) => void): void {}
  getNativeNode(id: number): NativeNode | undefined {
    return this.viewNodeTree.getNode(id)?.htmlElement as unknown as NativeNode | undefined;
  }
  getCachedElementFrame(id: number): ElementFrame | undefined {
    return this.viewNodeTree.getElementFrame(id);
  }
  getElementFrame(id: number, callback: (instance: ElementFrame | undefined) => void): void {
    callback(this.viewNodeTree.getElementFrame(id));
  }
  takeElementSnapshot(id: number, callback: (snapshotBase64: string | undefined) => void): void {
    const element = this.viewNodeTree.getNode(id)?.htmlElement;
    const takeSnapshot = (
      globalThis as unknown as {
        __valdiTakeElementSnapshot?: (element: HTMLElement) => Promise<string | undefined>;
      }
    ).__valdiTakeElementSnapshot;
    if (!element || !takeSnapshot) {
      callback(undefined);
      return;
    }

    takeSnapshot(element)
      .then(snapshot => callback(snapshot))
      .catch(error => {
        console.error('Failed to capture Valdi web element snapshot', error);
        callback(undefined);
      });
  }
  onUncaughtError(message: string, error: Error): void {
    console.error(message, error);
  }
  onDestroyed(): void {
    this.viewNodeTree.destroy();
  }

  private drainScheduledLayoutObservers(forceVisibility: boolean): void {
    this.viewNodeTree.drainScheduledLayoutObserverRefresh();
    this.viewNodeTree.drainScheduledVisibilityRefresh(forceVisibility);
  }
}
