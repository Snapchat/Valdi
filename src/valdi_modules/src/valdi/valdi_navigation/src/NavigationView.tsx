import { SerialTaskQueue } from 'coreutils/src/SerialTaskQueue';
import { StringSet } from 'coreutils/src/StringSet';
import { Lazy } from 'foundation/src/Lazy';
import { StatefulComponent } from 'valdi_core/src/Component';
import { parseComponentPath, resolveComponentConstructor } from 'valdi_core/src/ComponentPath';
import { ComponentRef } from 'valdi_core/src/ComponentRef';
import { Device } from 'valdi_core/src/Device';
import { ElementRef } from 'valdi_core/src/ElementRef';
import { IComponent } from 'valdi_core/src/IComponent';
import { getModuleLoader } from 'valdi_core/src/ModuleLoaderGlobal';
import { Style } from 'valdi_core/src/Style';
import { DefaultErrorBoundary } from 'valdi_core/src/debugging/DefaultErrorBoundary';
import { onIdleInterruptible } from 'valdi_core/src/utils/OnIdle';
import { when } from 'valdi_core/src/utils/When';
import { Layout, View } from 'valdi_tsx/src/NativeTemplateElements';
import { INavigator, INavigatorPageConfig, JSOnlyINavigator } from './INavigator';
import { NavigationPageContext } from './NavigationComponent';

export interface NavigationViewContext {
  hideBackButton: boolean;
}

interface ReloadableComponentData {
  componentPath: string;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any -- reachable through NavigationView's public state type; narrowing it is a public API change
  viewModel: any;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any -- reachable through NavigationView's public state type; narrowing it is a public API change
  context: any;
}

interface ComponentStackEntry {
  key: string;
  renderFn: () => void;
  reloadableData: ReloadableComponentData | undefined;
  title: string;
  ref: ComponentRef<IComponent>;
  viewContainerRef: ElementRef<View>;
  occlusionViewRef: ElementRef<View>;
  navigator: INavigator;
  hotReloadObserver: (() => void) | undefined;
  isModal: boolean;
  isTransparent: boolean;
}

interface State {
  stack: ComponentStackEntry[];
}

const ANIMATION_DURATION = 0.25;

const enum AnimationType {
  MODAL,
  PUSH_POP,
}

export interface NavigationViewModel {
  children?: (navigator: INavigator) => void;
}

export class NavigationView extends StatefulComponent<NavigationViewModel, State, NavigationViewContext> {
  state: State = { stack: [] };

  private keySequence = 0;
  private scheduledRebuild = false;
  private hotReloadedComponents: StringSet = {};
  private containerRef = new ElementRef<Layout>();
  private taskQueue = new SerialTaskQueue();

  onCreate(): void {
    const key = (++this.keySequence).toString();
    const navigator = new NavigationViewNavigator(this);
    const entry: ComponentStackEntry = {
      key,
      reloadableData: undefined,
      renderFn: () => {
        this.viewModel.children?.(navigator);
      },
      title: '',
      isModal: false,
      hotReloadObserver: undefined,
      navigator,
      ref: new ComponentRef(),
      viewContainerRef: new ElementRef(),
      occlusionViewRef: new ElementRef(),
      isTransparent: false,
    };

    this.appendEntry(entry, false);

    Device.setBackButtonObserver(this.onBackButtonPressed);
  }

  onDestroy(): void {
    Device.setBackButtonObserver(undefined);

    for (const item of this.state.stack) {
      this.teardownStackEntry(item);
    }
  }

  onRender(): void {
    <view backgroundColor="white" width={'100%'} height={'100%'}>
      <layout style={style.itemContainer} ref={this.containerRef}>
        {this.renderNavigationStack()}
      </layout>
    </view>;
  }

  pushComponent(
    componentPath: string,
    // eslint-disable-next-line @typescript-eslint/no-explicit-any -- public method; the values come from the any-typed INavigatorPageConfig
    viewModel: any,
    // eslint-disable-next-line @typescript-eslint/no-explicit-any -- public method; the values come from the any-typed INavigatorPageConfig
    context: any,
    title: string,
    isModal: boolean,
    isTransparent: boolean,
    animated: boolean,
  ): void {
    const newEntry = this.makeEntry(componentPath, viewModel, context, title, isModal, isTransparent);

    this.appendEntry(newEntry, animated);
  }

  popToNavigator(navigator: INavigator, animated: boolean): void {
    this.taskQueue.enqueueTask(done => {
      const index = this.indexOfNavigator(navigator);
      if (index > 0) {
        this.doPop(this.state.stack[index], AnimationType.PUSH_POP, animated, done);
      } else {
        done();
      }
    });
  }

  dismissNavigator(navigator: INavigator, animated: boolean): void {
    this.taskQueue.enqueueTask(done => {
      let index = this.indexOfNavigator(navigator);

      if (index < 0) {
        done();
        return;
      }

      while (index > 0 && !this.state.stack[index].isModal) {
        index--;
      }

      this.doPop(this.state.stack[index], AnimationType.MODAL, animated, done);
    });
  }

  pop(animated: boolean): void {
    this.taskQueue.enqueueTask(done => {
      if (this.state.stack.length > 1) {
        const itemToRemove = this.getCurrentItem();
        this.doPop(itemToRemove, AnimationType.PUSH_POP, animated, done);
      }
    });
  }

  private onBackButtonPressed = (): boolean => {
    this.pop(true);
    return true;
  };

  private renderNavigationStack(): void {
    const topItem = this.getCurrentItem();
    for (const item of this.state.stack) {
      <view key={item.key} style={style.item} ref={item.viewContainerRef}>
        <DefaultErrorBoundary>
          {item.renderFn()}
          {this.setupItem(item.ref)}
        </DefaultErrorBoundary>
        {when(item !== topItem, () => {
          <view style={style.occlusionOverlay} ref={item.occlusionViewRef} />;
        })}
      </view>;
    }
  }

  private setupItem(ref: ComponentRef<IComponent>): void {
    const component = ref.single();
    if (!component) {
      return;
    }

    const elements = this.renderer.getComponentRootElements(component, false);
    for (const element of elements) {
      element.setAttribute('$width', '100%');
      element.setAttribute('$height', '100%');
    }
  }

  private getCurrentItem(): ComponentStackEntry {
    return this.state.stack[this.state.stack.length - 1];
  }

  private appendEntry(newEntry: ComponentStackEntry, animated: boolean): void {
    this.taskQueue.enqueueTask(done => {
      if (animated && this.state.stack.length > 0) {
        const previousEntry = this.getCurrentItem();

        // Render with the item at the right
        this.renderer.batchUpdates(() => {
          this.setState({ stack: [...this.state.stack, newEntry] });

          this.applyTransitionFromState(previousEntry, AnimationType.PUSH_POP, [newEntry]);
        });

        // eslint-disable-next-line @typescript-eslint/no-floating-promises
        this.animatePromise({ duration: ANIMATION_DURATION }, () => {
          this.applyTransitionToState(previousEntry, [newEntry]);
        }).then(() => {
          this.updateStackNonAnimated(this.state.stack);
          done();
        });
      } else {
        this.updateStackNonAnimated([...this.state.stack, newEntry]);
        done();
      }
    });
  }

  private applyTransitionFromState(
    leftEntry: ComponentStackEntry,
    animationType: AnimationType,
    rightEntries: ComponentStackEntry[],
  ): void {
    const containerFrame = this.containerRef.single()!.frame;

    for (const rightEntry of rightEntries) {
      rightEntry.viewContainerRef.setAttribute('opacity', 1.0);

      switch (animationType) {
        case AnimationType.PUSH_POP:
          if (rightEntry.isModal) {
            rightEntry.viewContainerRef.setAttribute('translationY', containerFrame.height);
          } else {
            rightEntry.viewContainerRef.setAttribute('translationX', containerFrame.width);
          }
          break;
        case AnimationType.MODAL:
          rightEntry.viewContainerRef.setAttribute('translationY', containerFrame.height);
          break;
      }
    }

    leftEntry.viewContainerRef.setAttribute('opacity', 1.0);
    leftEntry.viewContainerRef.setAttribute('translationX', 0);
    leftEntry.occlusionViewRef.setAttribute('opacity', 0);
  }

  private applyTransitionToState(leftEntry: ComponentStackEntry, rightEntries: ComponentStackEntry[]): void {
    const containerFrame = this.containerRef.single()!.frame;

    let hasModal = false;
    for (const rightEntry of rightEntries) {
      rightEntry.viewContainerRef.setAttribute('opacity', 1.0);

      if (rightEntry.isModal) {
        hasModal = true;
        rightEntry.viewContainerRef.setAttribute('translationY', 0);
      } else {
        rightEntry.viewContainerRef.setAttribute('translationX', 0);
      }
    }

    leftEntry.viewContainerRef.setAttribute('opacity', 1.0);

    if (hasModal) {
      leftEntry.viewContainerRef.setAttribute('translationX', 0);
    } else {
      leftEntry.viewContainerRef.setAttribute('translationX', -containerFrame.width / 6);
    }

    leftEntry.occlusionViewRef.setAttribute('opacity', 1.0);
  }

  private makeEntry(
    componentPath: string,
    viewModel: object | undefined,
    context: object | undefined,
    title: string,
    isModal: boolean,
    isTransparent: boolean,
  ): ComponentStackEntry {
    const parsedComponentPath = parseComponentPath(componentPath);
    const moduleLoader = getModuleLoader();
    const requireFunc = moduleLoader.load.bind(moduleLoader);
    const ctor = new Lazy(() => resolveComponentConstructor(requireFunc, parsedComponentPath));

    const hotReloadObserver = getModuleLoader().onHotReload(module, parsedComponentPath.filePath, () => {
      this.hotReloadedComponents[componentPath] = true;

      if (!this.scheduledRebuild) {
        this.scheduledRebuild = true;

        onIdleInterruptible(() => {
          if (this.isDestroyed()) {
            return;
          }

          this.scheduledRebuild = false;
          this.rebuildStack();
        });
      }
    });

    const key = (++this.keySequence).toString();

    const navigator = new NavigationViewNavigator(this);
    const contextWithNavigator = { ...context, navigator };

    const ref = new ComponentRef();
    const renderFn = (): void => {
      <ctor.target context={contextWithNavigator} ref={ref} {...viewModel} />;
    };

    return {
      key,
      reloadableData: {
        componentPath,
        viewModel,
        context: contextWithNavigator,
      },
      renderFn,
      title,
      isModal,
      hotReloadObserver,
      navigator,
      ref,
      viewContainerRef: new ElementRef(),
      occlusionViewRef: new ElementRef(),
      isTransparent,
    };
  }

  private indexOfNavigator(navigator: INavigator): number {
    for (let i = 0; i < this.state.stack.length; i++) {
      const entry = this.state.stack[i];
      if (entry.navigator === navigator) {
        return i;
      }
    }

    return -1;
  }

  private doPop(atItem: ComponentStackEntry, animationType: AnimationType, animated: boolean, done: () => void): void {
    const itemsToRemove: ComponentStackEntry[] = [];
    const itemIndex = this.state.stack.indexOf(atItem);
    const newTopItem = this.state.stack[itemIndex - 1];

    for (let i = itemIndex; i < this.state.stack.length; i++) {
      itemsToRemove.push(this.state.stack[i]);
    }

    for (const itemToRemove of itemsToRemove) {
      this.teardownStackEntry(itemToRemove);
    }

    if (animated) {
      this.renderer.batchUpdates(() => {
        this.applyTransitionToState(newTopItem, itemsToRemove);
      });

      // eslint-disable-next-line @typescript-eslint/no-floating-promises
      this.animatePromise({ duration: ANIMATION_DURATION }, () => {
        this.applyTransitionFromState(newTopItem, animationType, itemsToRemove);
      }).then(() => {
        const index = this.state.stack.indexOf(atItem);
        if (index >= 0) {
          const newStack = this.state.stack.slice(0, itemIndex);
          this.updateStackNonAnimated(newStack);
        }
        done();
      });
    } else {
      this.updateStackNonAnimated(this.state.stack.slice(0, itemIndex));
      done();
    }
  }

  private teardownStackEntry(entry: ComponentStackEntry): void {
    entry.hotReloadObserver?.();
  }

  private rebuildStack(): void {
    const newStack: ComponentStackEntry[] = [];

    for (const entry of this.state.stack) {
      if (entry.reloadableData && this.hotReloadedComponents[entry.reloadableData.componentPath]) {
        this.teardownStackEntry(entry);
        newStack.push(
          this.makeEntry(
            entry.reloadableData.componentPath,
            entry.reloadableData.viewModel,
            entry.reloadableData.context,
            entry.title,
            entry.isModal,
            entry.isTransparent,
          ),
        );
      } else {
        newStack.push(entry);
      }
    }

    this.hotReloadedComponents = {};

    this.updateStackNonAnimated(newStack);
  }

  private updateStackNonAnimated(newStack: ComponentStackEntry[]): void {
    this.renderer.batchUpdates(() => {
      this.setState({ stack: newStack });

      const activeItem = this.getCurrentItem();

      const stack = this.state.stack;
      let index = stack.length;
      let previousIsTransparent = false;
      while (index > 0) {
        index--;

        const item = stack[index];
        if (item === activeItem || previousIsTransparent) {
          item.viewContainerRef.setAttribute('translationX', 0);
          item.viewContainerRef.setAttribute('opacity', 1.0);
          previousIsTransparent = item.isTransparent;
        } else {
          const containerWidth = this.containerRef.single()!.frame.width;
          // Put the container outside of bounds so its not inflated
          item.viewContainerRef.setAttribute('translationX', containerWidth * 10000);
          item.viewContainerRef.setAttribute('opacity', 0.0);
          previousIsTransparent = false;
        }
      }
    });
  }
}

const style = {
  itemContainer: new Style<Layout>({
    width: '100%',
    flexGrow: 1,
  }),
  item: new Style<View>({
    position: 'absolute',
    width: '100%',
    height: '100%',
  }),
  occlusionOverlay: new Style<View>({
    backgroundColor: 'rgba(0, 0, 0, 0.2)',
    position: 'absolute',
    width: '100%',
    height: '100%',
  }),
};

class NavigationViewNavigator implements INavigator, JSOnlyINavigator {
  // eslint-disable-next-line @typescript-eslint/naming-convention -- implements JSOnlyINavigator.__shouldDisableMakeOpaque
  __shouldDisableMakeOpaque = true;

  constructor(private navigationView: NavigationView) {}

  pushComponent(page: INavigatorPageConfig, animated: boolean): void {
    this.navigationView.pushComponent(
      page.componentPath,
      page.componentViewModel,
      page.componentContext,
      page.platformNavigationTitle ?? '',
      /* isModal */ false,
      this.hasTransparentBackground(page),
      animated,
    );
  }

  pop(animated: boolean): void {
    this.navigationView.popToNavigator(this, animated);
  }

  popToRoot(): void {
    // TODO: Actual implementation. Could pop until the next isModal (without including it)
    throw new Error('popToRoot is not currently implemented in the desktop app navigation');
  }

  popToSelf(): void {
    // TODO: Actual implementation.
    throw new Error('popToSelf is not currently implemented in the desktop app navigation');
  }

  presentComponent(page: INavigatorPageConfig, animated: boolean): void {
    this.navigationView.pushComponent(
      page.componentPath,
      page.componentViewModel,
      page.componentContext,
      page.platformNavigationTitle ?? '',
      /* isModal */ true,
      this.hasTransparentBackground(page),
      animated,
    );
  }

  dismiss(animated: boolean): void {
    this.navigationView.dismissNavigator(this, animated);
  }

  forceDisableDismissalGesture(): void {
    console.error('forceDisableDismissalGesture is not implemented in the desktop app navigation');
  }

  setBackButtonObserver(): void {
    // setBackButtonObserver is not implemented in the desktop app navigation
  }

  setOnPausePopAfterDelay(): void {
    // setOnPausePopAfterDelay is not implemented in the desktop app navigation
  }

  private hasTransparentBackground(page: INavigatorPageConfig): boolean {
    const componentContext = page.componentContext as NavigationPageContext;
    if (!componentContext) {
      return false;
    }
    return componentContext.pageNavigationOptions?.pageBackgroundColor === 'transparent';
  }
}
