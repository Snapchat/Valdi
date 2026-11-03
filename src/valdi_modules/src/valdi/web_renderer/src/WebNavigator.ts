import { INavigator, INavigatorPageConfig } from 'valdi_navigation/src/INavigator';
import { WebNavStack } from './WebNavStack';

export class WebNavigator implements INavigator {
  // Name is defined by JSOnlyINavigator in valdi_navigation, which NavigationController checks.
  // eslint-disable-next-line @typescript-eslint/naming-convention
  __shouldDisableMakeOpaque = true;

  constructor(private navStack: WebNavStack, private pageIndex: number) {}

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

  forceDisableDismissalGesture(): void {}

  setBackButtonObserver(): void {}

  setOnPausePopAfterDelay(): void {}

  setPageVisibilityObserver(): void {}
}
