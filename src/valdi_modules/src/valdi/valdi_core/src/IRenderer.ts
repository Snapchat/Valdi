import { AnimationOptions } from './AnimationOptions';
import { CancelToken } from './CancellableAnimation';
import { IComponent } from './IComponent';
import { IComponentRenderObserver } from './IComponentRenderObserver';
import { IRenderedElement } from './IRenderedElement';
import { IRenderedVirtualNode } from './IRenderedVirtualNode';
import { IRendererEventListener } from './IRendererEventListener';
import { IRootElementObserver } from './IRootElementObserver';
import { IRuntimeIssueObserver } from './IRuntimeIssueObserver';

export type RendererObserver = Partial<IRuntimeIssueObserver> &
  Partial<IRootElementObserver> &
  Partial<IComponentRenderObserver>;

interface Unsubscribable {
  unsubscribe(): void;
}

export type ComponentDisposable = (() => void) | Unsubscribable;

export interface IRenderer {
  contextId: string;
  renderComponent(component: IComponent, properties: any | undefined): void;
  isComponentAlive(component: IComponent): boolean;
  getRootComponent(): IComponent | undefined;
  getComponentChildren(component: IComponent): IComponent[];
  getComponentParent(component: IComponent): IComponent | undefined;
  getCurrentComponentInstance(): IComponent | undefined;
  getComponentKey(component: IComponent): string;
  getComponentRootElements(component: IComponent, collectElementsOfChildComponents: boolean): IRenderedElement[];
  getComponentVirtualNode(component: IComponent): IRenderedVirtualNode;
  getElementForId(elementId: number): IRenderedElement | undefined;
  getRootVirtualNode(): IRenderedVirtualNode | undefined;

  /**
   * Registers a function which will be called right after the component is destroyed.
   */
  registerComponentDisposable(component: IComponent, disposable: ComponentDisposable): void;

  /**
   * Allows to make multiple changes into a single render.
   */
  batchUpdates(block: () => void): void;

  /**
   * Associate a set of changes with an animation
   * @param block a block which will be executed in a render. All element mutations
   * belong to this renderer will be animated.
   */
  animate(animationOptions: AnimationOptions, block: () => void): CancelToken;

  cancelAnimation(token: CancelToken): void;

  /**
   * Enqueue a callback to be called when the current render completed.
   * If the renderer is not currently rendering, the callback will be
   * called immediately.
   */
  onRenderComplete(callback: () => void): void;

  /**
   * Enqueue a callback to be called whenever the next layout pass completes,
   * which will be after all the frames of the elements are resolved.
   */
  onLayoutComplete(callback: () => void): void;

  /**
   * Enqueue a callback to be called after the next root draw/commit pass reaches
   * Valdi's platform rendering callback.
   *
   * On Android, this runs after the traversal that reached `OnPreDraw` returns
   * to the main queue. On iOS, it runs from the `CATransaction` completion for
   * the root view update.
   *
   * This is later than `onLayoutComplete`, but it is not a guaranteed
   * screen-presentation callback. `hookTimeMs` is a monotonic platform-thread
   * timestamp in milliseconds captured when Valdi begins executing that native
   * next-draw callback. On Android, this can be slightly later than
   * `OnPreDraw` itself because the callback is posted back onto the main queue
   * after the traversal continues.
   */
  onNextDraw(callback: (hookTimeMs: number) => void): void;

  /**
   * Enqueue a callback to be called when the next render completes.
   */
  onNextRenderComplete(callback: () => void): void;

  /**
   * Set an event listener, which will observe render events.
   * Used for debugging.
   */
  setEventListener(eventListener: IRendererEventListener): void;

  /**
   * Return the event listener that is currently set on the renderer
   */
  getEventListener(): IRendererEventListener | undefined;

  addObserver(observer: RendererObserver): void;

  removeObserver(observer: RendererObserver): void;

  onUncaughtError(message: string, error: Error, sourceComponent?: IComponent): void;
}
