import { makePropertiesOpaque, WithOpaqueProperties } from 'foundation/src/makePropertiesOpaque';
import { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import { INavigator, INavigatorPageConfig, INavigatorPageVisibility, JSOnlyINavigator } from './INavigator';

type WithComponentPath<T> = T & { componentPath: string };

export interface NavigationOptions extends InternalNavigationOptions {
  /**
   * Whether to animate the transition.
   */
  animated?: boolean;

  /**
   * The page container's background color. Will be SemanticColor.Background.MAIN by default.
   */
  pageBackgroundColor?: string;

  /** Whether to keep the source page partially visible. */
  isPartiallyHiding?: boolean;
}

export interface NavigationPushOptions extends NavigationOptions {}

export interface NavigationPresentOptions extends NavigationOptions {
  /**
   * iOS-only: Pass true here to make the container view controller that gets created for the presented page be wrapped
   * in a UINavigationController instance before presenting. This is required if you'd like to use push/pop navigation operations
   * from this presented page.
   */
  wrapInPlatformNavigationController?: boolean;
}

function verifyPage<T extends IComponent<ViewModel, Context>, ViewModel, Context>(
  ctr: ComponentConstructor<T, ViewModel, Context>,
): asserts ctr is WithComponentPath<typeof ctr> {
  if (!(ctr as { componentPath?: string }).componentPath) {
    throw new Error('Attempting to navigate with a Component that has not been decorated with @NavigationPage');
  }
}

export type BackButtonObserver = () => void;
export type PageVisibilityObserver = (visibility: INavigatorPageVisibility) => void;

interface OnPausePopAfterDelay {
  delayMs: number | undefined;
}

/**
 * Components should use this class to perform various operations related to navigation, like easily presenting
 * some other Component as a new modal "page".
 */
export class NavigationController {
  private disableDismissalCount = 0;
  private backButtonObservers: BackButtonObserver[] = [];
  private registeredNavigatorBackButtonObserver = false;

  private addedVisibilityObserver = false;
  private latestVisibility: INavigatorPageVisibility | undefined;
  private onPausePopAfterDelay: OnPausePopAfterDelay[] = [];
  private currentOnPausePopAfterDelay: number | undefined;
  private pageVisibilityObservers = new Set<PageVisibilityObserver>();

  constructor(private navigator: INavigator) {
    if (!navigator) {
      throw new Error('Attempting to create a NavigationController with a missing navigator');
    }
  }

  /**
   * Pushes a Component as a new page onto the current navigation stack.
   *
   * Important: on iOS this requires the containing view controller to be within a UINavigationController.
   * In the future we'll have SIGPresentation/mochi manage this automatically.
   *
   * @param componentClass The Component class that describes the pushed page. Must be annotated with the NavigationPage annotation.
   * @param viewModel View model to pass into the component when it's created.
   * @param context Context to pass into the component when it's created. A `navigator: INavigator` will be injected automatically, you shouldn't pass it manually.
   * @param options Misc. options to configure the transition and/or page.
   */
  push<ViewModel, Context, ProvidedContext extends Omit<Context, 'navigator'>>(
    componentClass: ComponentConstructor<IComponent<ViewModel, Context>, ViewModel, Context>,
    viewModel: ViewModel,
    context: ProvidedContext,
    options: NavigationPushOptions = { animated: true },
  ): void {
    const page = this.makePage(componentClass, viewModel, context, options);
    this.navigator.pushComponent(page, options.animated ?? true);
  }

  /**
   * Goes back one step in the current navigation stack.
   *
   * Important: on iOS this requires the containing view controller to be within a UINavigationController.
   * In the future we'll have the default navigator manage this automatically using SIGPresentation/mochi.
   */
  pop(animated: boolean = true): void {
    this.navigator.pop(animated);
  }

  /**
   * Unwinds the navigation stack to the page that is managed by this INavigator instance.
   *
   * Important: on iOS this requires the containing view controller to be within a UINavigationController.
   * In the future we'll have the default navigator manage this automatically using SIGPresentation/mochi.
   */
  popToSelf(animated: boolean = true): void {
    this.navigator.popToSelf(animated);
  }

  /**
   * Unwinds the navigation stack to the root page.
   *
   * Important: on iOS this requires the containing view controller to be within a UINavigationController.
   * In the future we'll have the default navigator manage this automatically using SIGPresentation/mochi.
   */
  popToRoot(animated: boolean = true): void {
    this.navigator.popToRoot(animated);
  }

  /**
   * Presents a Component using a modal presentation.
   *
   * @param componentClass The Component class that describes the presented page. Must be annotated with the NavigationPage annotation.
   * @param viewModel View model to pass into the component when it's created.
   * @param context Context to pass into the component when it's created. A `navigator: INavigator` will be injected automatically, you shouldn't pass it manually.
   * @param options Misc. options to configure the transition and/or page.
   */
  present<ViewModel, Context, ProvidedContext extends Omit<Context, 'navigator'>>(
    componentClass: ComponentConstructor<IComponent<ViewModel, Context>, ViewModel, Context>,
    viewModel: ViewModel,
    context: ProvidedContext,
    options: NavigationPresentOptions = { animated: true },
  ): void {
    const page = this.makePage(componentClass, viewModel, context, options);
    page.wrapInPlatformNavigationController = options.wrapInPlatformNavigationController ?? true;
    this.navigator.presentComponent(page, options.animated ?? true);
  }

  /**
   * Dismisses the Component that was presenting using a modal presentation (and any Components that it might've presented too).
   */
  dismiss(animated: boolean): void {
    this.navigator.dismiss(animated);
  }

  /**
   * Disable the interactive dismissal gesture.
   *
   * Returns back a function that should be called to reenable the gesture back again.
   *
   * Note: This method maintains an internal count of how many "disable" requests are currently
   * "active", since different parts of your UI logic might want to disable the gesture for
   * different reasons. The interactive dismissal gesture will be reenabled once the count goes
   * back down to zero.
   */
  disableDismissalGesture(): () => void {
    this.disableDismissalCount += 1;
    this.navigator.forceDisableDismissalGesture(true);
    return () => {
      this.disableDismissalCount -= 1;
      const shouldReenable = this.disableDismissalCount === 0;
      this.navigator.forceDisableDismissalGesture(!shouldReenable);
    };
  }

  /**
   * Android-only: Register a callback that can handle the back button press. Registering
   * a callback will prevent the default handling of the back button press.
   */
  registerBackButtonObserver(observer: BackButtonObserver): () => void {
    this.backButtonObservers.push(observer);
    this.updateNavigatorBackButtonObserverIfNeeded();

    return () => {
      const index = this.backButtonObservers.indexOf(observer);
      if (index >= 0) {
        this.backButtonObservers.splice(index, 1);
        this.updateNavigatorBackButtonObserverIfNeeded();
      }
    };
  }

  addPageVisibilityObserver(observer: PageVisibilityObserver): void {
    this.pageVisibilityObservers.add(observer);
    if (!this.addedVisibilityObserver) {
      // native will notify of the latest visibility upon setting the observer
      this.navigator.setPageVisibilityObserver?.(this.onPageVisibilityChange);
    } else if (this.latestVisibility !== undefined) {
      // otherwise notify using the locally cached value
      observer(this.latestVisibility);
    }
  }

  removePageVisibilityObserver(observer: PageVisibilityObserver): void {
    this.pageVisibilityObservers.delete(observer);
  }

  /**
   * Android-only: Sets the number of millis that the screen will wait before being dismissed after the app has been backgrounded.
   */
  registerOnPausePopAfterDelay(delayMs: number | undefined): () => void {
    const entry: OnPausePopAfterDelay = {
      delayMs: delayMs,
    };

    this.onPausePopAfterDelay.push(entry);

    this.updateOnPausePopAfterDelay();

    return () => {
      const index = this.onPausePopAfterDelay.indexOf(entry);
      if (index >= 0) {
        this.onPausePopAfterDelay.splice(index, 1);
        this.updateOnPausePopAfterDelay();
      }
    };
  }

  private onBackButtonPressed = (): void => {
    for (const observer of this.backButtonObservers) {
      observer();
    }
  };

  private onPageVisibilityChange = (visibility: INavigatorPageVisibility): void => {
    this.latestVisibility = visibility;
    for (const observer of this.pageVisibilityObservers) {
      observer(visibility);
    }
  };

  private makePage<
    T extends IComponent<ViewModel, Context>,
    ViewModel,
    Context,
    ProvidedContext extends Omit<Context, 'navigator'>,
  >(
    componentClass: ComponentConstructor<T, ViewModel, Context>,
    viewModel: ViewModel,
    context: ProvidedContext,
    options: NavigationOptions,
  ): INavigatorPageConfig {
    const componentCtr = componentClass;
    let processedViewModel: ViewModel | WithOpaqueProperties<ViewModel>;
    let processedContext: Context | WithOpaqueProperties<Context>;
    if ((this.navigator as JSOnlyINavigator)?.__shouldDisableMakeOpaque) {
      processedViewModel = viewModel;
      const enhancedContext = { ...(context ?? {}), pageNavigationOptions: options };
      processedContext = enhancedContext;
    } else {
      processedViewModel = makePropertiesOpaque(viewModel);
      const enhancedContext = { ...context, pageNavigationOptions: options };
      processedContext = makePropertiesOpaque(enhancedContext);
    }

    verifyPage(componentCtr);
    return {
      componentPath: componentCtr.componentPath,
      componentViewModel: processedViewModel,
      componentContext: processedContext,
      showPlatformNavigationBar: options.showPlatformNavigationBar,
      platformNavigationTitle: options.platformNavigationTitle,
      isPartiallyHiding: options.isPartiallyHiding,
    };
  }

  private updateNavigatorBackButtonObserverIfNeeded(): void {
    if (this.backButtonObservers.length) {
      if (!this.registeredNavigatorBackButtonObserver) {
        this.registeredNavigatorBackButtonObserver = true;
        if (this.navigator.setBackButtonObserver) {
          this.navigator.setBackButtonObserver(this.onBackButtonPressed);
        }
      }
    } else {
      if (this.registeredNavigatorBackButtonObserver) {
        this.registeredNavigatorBackButtonObserver = false;
        if (this.navigator.setBackButtonObserver) {
          this.navigator.setBackButtonObserver(undefined);
        }
      }
    }
  }

  private updateOnPausePopAfterDelay(): void {
    let newOnPausePopAfterDelay: number | undefined;
    if (this.onPausePopAfterDelay.length) {
      newOnPausePopAfterDelay = this.onPausePopAfterDelay[this.onPausePopAfterDelay.length - 1].delayMs;
    }

    if (newOnPausePopAfterDelay !== this.currentOnPausePopAfterDelay) {
      this.currentOnPausePopAfterDelay = newOnPausePopAfterDelay;
      if (this.navigator.setOnPausePopAfterDelay) {
        this.navigator.setOnPausePopAfterDelay(newOnPausePopAfterDelay);
      }
    }
  }
}

////////////////////////////////////////////////////////////////

interface InternalNavigationOptions {
  /**
   * iOS and, really, internal-only: whether to show the standard iOS navigation bar
   */
  showPlatformNavigationBar?: boolean;
  /**
   * iOS and, really, internal-only: what title to display in the standard iOS navigation bar
   */
  platformNavigationTitle?: string;
}
