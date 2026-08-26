import { INavigator, INavigatorPageConfig, INavigatorPageVisibility } from 'valdi_navigation/src/INavigator';
import { WebNavStack } from './WebNavStack';

export class WebNavigator implements INavigator {
  __shouldDisableMakeOpaque = true;

  constructor(
    private navStack: WebNavStack,
    private pageIndex: number,
  ) {}

  pushComponent(page: INavigatorPageConfig, animated: boolean): void {
    this.navStack.push(page, animated, false);
  }

  presentComponent(page: INavigatorPageConfig, animated: boolean): void {
    this.navStack.push(page, animated, true);
  }

  pop(animated: boolean): void {
    // Remove this page and everything above it (go back to previous page).
    this.navStack.popTo(this.pageIndex - 1, animated);
  }

  popToRoot(animated: boolean): void {
    this.navStack.popTo(0, animated);
  }

  popToSelf(animated: boolean): void {
    // Keep this page, remove everything above it.
    this.navStack.popTo(this.pageIndex, animated);
  }

  dismiss(animated: boolean): void {
    this.navStack.dismissFrom(this.pageIndex, animated);
  }

  forceDisableDismissalGesture(_forceDisable: boolean): void {}

  setBackButtonObserver(_observer: (() => void) | undefined): void {}

  setOnPausePopAfterDelay(_delayMs: number | undefined): void {}

  setPageVisibilityObserver(_observer: ((visibility: INavigatorPageVisibility) => void) | undefined): void {}
}
