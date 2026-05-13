import {
  AnimationOptions,
  CustomCurveAnimationOptions,
  PresetCurveAnimationOptions,
  SpringAnimationOptions,
} from './AnimationOptions';
import { CancellableAnimation } from './CancellableAnimation';
import { CancellableAnimationPromise } from './CancellableAnimationPromise';
import { ConsoleRepresentable } from './ConsoleRepresentable';
import { IComponent } from './IComponent';
import { ComponentDisposable, IRenderer } from './IRenderer';
import { setTimeoutInterruptible } from './SetTimeout';
import { mergePartial } from './utils/PartialUtils';

import 'valdi_tsx/src/JSX';

export class Component<ViewModel = object, ComponentContext = object>
  implements IComponent<ViewModel, ComponentContext>, ConsoleRepresentable
{
  readonly viewModel: Readonly<ViewModel>;
  readonly context: Readonly<ComponentContext>;

  readonly renderer: IRenderer;

  constructor(renderer: IRenderer, viewModel: ViewModel, componentContext: any) {
    this.renderer = renderer;
    this.viewModel = viewModel;
    this.context = componentContext;
  }

  /**
   * Called whenever the Component is created for the first time, before
   * it completed its first render.
   */
  onCreate(): void {}

  /**
   * Called during a render pass. You can override this method to make any JSX
   * calls which will represent the UI. You need to be in an onRender() callback
   * in order to make JSX calls.
   */
  onRender(): void {}

  /**
   * Called whenever the component has been destroyed, e.g. it is not part of the
   * hierarchy anymore.
   */
  onDestroy(): void {}

  /**
   * Called whenever the view model has changed. This will be called right after onCreate()
   * if the component is given an initial view model, or any time a new view model is provided.
   */
  onViewModelUpdate(previousViewModel?: Readonly<ViewModel>): void {}

  /**
   * @deprecated (use a StatefulComponent instead)
   * Schedule a render for the Component, will trigger an onRender() in the future.
   * This will typically be called automatically on your behalf, you shouldn't need this.
   */
  scheduleRender(): void {
    this.renderer.renderComponent(this, undefined);
  }

  /**
   * Registers a function or unsubscribable which will be called right after the component is destroyed.
   * @param disposable the callback to call after the component is destroyed
   * @returns {boolean} true if a disposable was registered to be disposed
   */
  registerDisposable(disposable: ComponentDisposable): true
  registerDisposable(disposable: undefined): false
  registerDisposable(disposable: ComponentDisposable | undefined): boolean
  registerDisposable(disposable: ComponentDisposable | undefined): boolean {
    if (!disposable) {
      return false;
    }
    this.renderer.registerComponentDisposable(this, disposable);
    return true;
  }

  /**
   * Registers each function or unsubscribable in the array which will be called right after the component is destroyed.
   * @param disposables array of callbacks to call after the component is destroyed
   */
  registerDisposables(disposables: ComponentDisposable[]): void {
    disposables.forEach(this.registerDisposable.bind(this));
  }

  /**
   * @deprecated @see registerDisposable
   */
  maybeRegisterDisposable(disposable: ComponentDisposable | undefined): void {
    if (!!disposable) {
      this.registerDisposable(disposable);
    }
  }

  // TODO: move IDisposable into valdi_core and modify ^^^this^^^ to be able to work with IDisposables?
  // (foundation depends on valdi_core, so can't keep it in foundation. seems like we need
  // some utility module that valdi_core can depend on?)

  // TODO: once IDisposable is accessible from valdi_core, setTimeoutDisposable could return an IDisposable
  /** Creates a timer that will get invalidated when the component is destroyed. */
  setTimeoutDisposable(handler: () => void, timeout?: number): number {
    const timerId = setTimeoutInterruptible(handler, timeout);
    this.registerDisposable(() => {
      clearTimeout(timerId);
    });
    return timerId;
  }

  animate(options: AnimationOptions, animations: () => void): void;

  animate(options: PresetCurveAnimationOptions, animations: () => void): void;

  animate(options: CustomCurveAnimationOptions, animations: () => void): void;

  animate(options: SpringAnimationOptions, animations: () => void): void;

  /**
   * Associate a set of changes with an animation
   * @param animations a block which will be executed in a render. All element mutations
   * belong to this renderer will be animated.
   * @return A promise which will be resolved when the animation finishes.
   *
   *
   * To get notified on animation complete, use `animatePromise` instead.
   */
  animate(options: AnimationOptions, animations: () => void): void {
    void this.animatePromise(options, animations);
  }

  animatePromise(options: AnimationOptions, animations: () => void): Promise<void>;

  animatePromise(options: PresetCurveAnimationOptions, animations: () => void): Promise<void>;

  animatePromise(options: CustomCurveAnimationOptions, animations: () => void): Promise<void>;

  animatePromise(options: SpringAnimationOptions, animations: () => void): Promise<void>;

  /**
   * Associate a set of changes with an animation
   * @param animations a block which will be executed in a render. All element mutations
   * belong to this renderer will be animated.
   * @return A promise which will be resolved when the animation finishes.
   */
  animatePromise(options: AnimationOptions, animations: () => void): Promise<void> {
    return new Promise(resolve => {
      this.renderer.animate(
        {
          ...options,
          completion: cancelled => {
            options.completion?.(cancelled);
            resolve();
            return;
          },
        },
        animations,
      );
    });
  }

  createAnimation(options: AnimationOptions, animations: () => void): CancellableAnimation {
    return new CancellableAnimationPromise(this.renderer, options, animations);
  }

  /**
   * Returns whether the Component was destroyed.
   */
  isDestroyed(): boolean {
    return !this.renderer.isComponentAlive(this);
  }

  /**
   * Returns an object to be serialized as a string representation for the component, used for debugging
   */
  toConsoleRepresentation() {
    return this.viewModel;
  }

  static disallowNullViewModel = true;
}

export class StatefulComponent<ViewModel = object, State = object, ComponentContext = object> extends Component<
  ViewModel,
  ComponentContext
> {
  state?: Readonly<State>;

  /**
   * Changes the state by merging the given partial state.
   * Will synchronously re-render the component if the state changes.
   */
  setState(state: Readonly<Partial<State>>): void {
    if (this.isDestroyed()) {
      console.error(
        new Error(
          `Can't call setState on a destroyed component '${
            (<any>this).constructor?.name
          }'. This operation does nothing, but it signals a potential memory leak in your application. To resolve this issue, ensure you cancel all subscriptions and asynchronous tasks in the onDestroy method.`,
        ),
      );
      return;
    }

    const newState = mergePartial(state, this.state);

    if (newState) {
      this.state = newState;
      this.scheduleRender();
    }
  }

  /**
   * Changes the state by merging the given partial state.
   * Will synchronously re-render the component if the state changes.
   * Any elements mutation that happens within that setState call will be animated.
   *
   * To get notified on animation completion, use `setStateAnimatedPromise` instead.
   */
  setStateAnimated(state: Readonly<Partial<State>>, animationOptions: AnimationOptions): void {
    void this.setStateAnimatedPromise(state, animationOptions);
  }

  /**
   * Changes the state by merging the given partial state.
   * Will synchronously re-render the component if the state changes.
   * Any elements mutation that happens within that setState call will be animated.

   * @return A promise which will be resolved when the animation finishes.
   */
  setStateAnimatedPromise(state: Readonly<Partial<State>>, animationOptions: AnimationOptions): Promise<void> {
    if (this.isDestroyed()) {
      console.error('Trying to call setStateAnimated on a destroyed component', (<any>this).constructor?.name);
      return Promise.resolve();
    }
    // We pass animationOptions as any because the animate(...) overload that accepts AnimationOptions
    // is hidden from us.
    return this.animatePromise(animationOptions as any, () => {
      this.setState(state);
    });
  }
}
