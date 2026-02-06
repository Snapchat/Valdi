import { AnimationOptions } from 'valdi_core/src/AnimationOptions';
import { FrameObserver, IRendererDelegate, VisibilityObserver } from 'valdi_core/src/IRendererDelegate';
import { Style } from 'valdi_core/src/Style';
import { NativeNode } from 'valdi_tsx/src/NativeNode';
import { NativeView } from 'valdi_tsx/src/NativeView';
import { CancelToken } from 'valdi_core/src/CancellableAnimation';
import {
  changeAttributeOnElement,
  createElement,
  destroyElement,
  makeElementRoot,
  moveElement,
  registerElements,
  setAllElementsAttributeDelegate,
} from './HTMLRenderer';
import { WebAnimationManager } from './WebAnimationManager';

export interface UpdateAttributeDelegate {
  updateAttribute(elementId: number, attributeName: string, attributeValue: any): void;
}

export class ValdiWebRendererDelegate implements IRendererDelegate {
  private attributeDelegate?: UpdateAttributeDelegate;
  private animationManager: WebAnimationManager;

  constructor(private htmlRoot: HTMLElement | ShadowRoot) {
    registerElements();
    this.animationManager = new WebAnimationManager();
    // Make animation manager globally accessible for elements
    (window as any).__valdiAnimationManager = this.animationManager;
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
    
    // Process left/right together to avoid conflicts
    // Note: On web, we swap left/right values for compatibility
    const hasLeft = 'left' in attributes;
    const hasRight = 'right' in attributes;
    const leftValue = attributes.left;
    const rightValue = attributes.right;
    
    if (hasLeft && hasRight) {
      // Both are present - process removals first, then set the new value
      // Swap: original left becomes web right, original right becomes web left
      const leftIsDefined = leftValue !== undefined && leftValue !== null && leftValue !== '';
      const rightIsDefined = rightValue !== undefined && rightValue !== null && rightValue !== '';
      
      if (rightIsDefined && !leftIsDefined) {
        // Original right becomes web left - remove web right first, then set web left
        changeAttributeOnElement(id, 'right', undefined);
        changeAttributeOnElement(id, 'left', rightValue);
      } else if (leftIsDefined && !rightIsDefined) {
        // Original left becomes web right - remove web left first, then set web right
        changeAttributeOnElement(id, 'left', undefined);
        changeAttributeOnElement(id, 'right', leftValue);
      } else if (leftIsDefined && rightIsDefined) {
        // Both have values - this shouldn't happen, but prioritize original left (web right)
        changeAttributeOnElement(id, 'left', undefined);
        changeAttributeOnElement(id, 'right', leftValue);
      } else {
        // Both are undefined/null - remove both
        changeAttributeOnElement(id, 'left', undefined);
        changeAttributeOnElement(id, 'right', undefined);
      }
    } else if (hasLeft) {
      // Only left is present - original left becomes web right
      const leftIsDefined = leftValue !== undefined && leftValue !== null && leftValue !== '';
      if (leftIsDefined) {
        // When setting original left (web right), also clear web left
        changeAttributeOnElement(id, 'left', undefined);
        changeAttributeOnElement(id, 'right', leftValue);
      } else {
        changeAttributeOnElement(id, 'right', undefined);
      }
    } else if (hasRight) {
      // Only right is present - original right becomes web left
      const rightIsDefined = rightValue !== undefined && rightValue !== null && rightValue !== '';
      if (rightIsDefined) {
        // When setting original right (web left), also clear web right
        changeAttributeOnElement(id, 'right', undefined);
        changeAttributeOnElement(id, 'left', rightValue);
      } else {
        changeAttributeOnElement(id, 'left', undefined);
      }
    }
    
    // Process all other attributes
    Object.keys(attributes).forEach(key => {
      if (key === 'left' || key === 'right') {
        // Already handled above
        return;
      }
      const value = attributes[key];
      changeAttributeOnElement(id, key, value);
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
  private tokenMap: Map<CancelToken, CancelToken> = new Map();
  
  onAnimationStart(options: AnimationOptions, token: CancelToken): void {
    const animationToken = this.animationManager.startAnimation(options);
    this.tokenMap.set(token, animationToken);
  }
  
  onAnimationEnd(): void {
    // End the most recently started animation
    // In practice, animations are nested, so we end the last one
    if (this.tokenMap.size > 0) {
      const tokens = Array.from(this.tokenMap.values());
      const lastToken = tokens[tokens.length - 1];
      this.animationManager.endAnimation(lastToken);
      // Remove from map
      for (const [rendererToken, animationToken] of this.tokenMap.entries()) {
        if (animationToken === lastToken) {
          this.tokenMap.delete(rendererToken);
          break;
        }
      }
    }
  }
  
  onAnimationCancel(token: CancelToken): void {
    const animationToken = this.tokenMap.get(token);
    if (animationToken !== undefined) {
      this.animationManager.cancelAnimation(animationToken);
      this.tokenMap.delete(token);
    }
  }
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
