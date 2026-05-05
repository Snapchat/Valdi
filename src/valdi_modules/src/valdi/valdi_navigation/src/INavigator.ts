// @NativeClass({ marshallAsUntyped: false, ios: 'SCValdiINavigatorPageConfig', iosImportPrefix: 'valdi_core', android: 'com.snap.valdi.navigation.INavigatorPageConfig'})
export interface INavigatorPageConfig {
  componentPath: string;
  componentViewModel: any;
  componentContext: any;
  showPlatformNavigationBar?: boolean;
  wrapInPlatformNavigationController?: boolean;
  platformNavigationTitle?: string;
  // Keeps the source page partially visible.
  isPartiallyHiding?: boolean;
  // Android only: enable using the gesture touch adjustments
  enableGestureAdjustCoordinates?: boolean;
}

/**
 * The visible state of the page in the navigation stack.
 */
export const enum INavigatorPageVisibility {
  HIDDEN = 0,
  VISIBLE = 1,
}

/**
 * The bridge interface that provides the implementation underlying all the operations exposed via NavigationController.
 * @NativeInterface({ marshallAsUntyped: false, ios: 'SCValdiINavigator', iosImportPrefix: 'valdi_core', android: 'com.snap.valdi.navigation.INavigator'})
 */
export interface INavigator {
  pushComponent(page: INavigatorPageConfig, animated: boolean): void;
  pop(animated: boolean): void;
  popToRoot(animated: boolean): void;
  popToSelf(animated: boolean): void;

  presentComponent(page: INavigatorPageConfig, animated: boolean): void;
  dismiss(animated: boolean): void;

  forceDisableDismissalGesture(forceDisable: boolean): void;
  setBackButtonObserver?(observer: (() => void) | undefined): void;
  setOnPausePopAfterDelay?(delayMs: number | undefined): void;
  setPageVisibilityObserver?(observer: ((visibility: INavigatorPageVisibility) => void) | undefined): void;
}

// this is a bit of a hack for the purposes of a JS-only navigator implementation that has
// to prevent context and viewModel from being made opaque.
export interface JSOnlyINavigator {
  __shouldDisableMakeOpaque?: boolean;
}
