import { $slot } from 'valdi_core/src/CompilerIntrinsics';
import { Component } from 'valdi_core/src/Component';
import { NavigationPageContext } from 'valdi_navigation/src/NavigationComponent';
import { NavigationController } from 'valdi_navigation/src/NavigationController';
import { INavigator } from './INavigator';
import { NavigationView } from './NavigationView';

export interface NavigationRootViewModel {
  children?: (navigationController: NavigationController) => void;
}

export class NavigationRoot extends Component<NavigationRootViewModel, NavigationPageContext> {
  private _navigationController?: NavigationController;

  onRender(): void {
    if (this.context?.navigator) {
      this.renderChildrenWithNavigator(this.context.navigator);
    } else {
      <NavigationView>
        {$slot(navigator => {
          this.renderChildrenWithNavigator(navigator);
        })}
      </NavigationView>;
    }
  }

  private renderChildrenWithNavigator(navigator: INavigator): void {
    if (!this._navigationController) {
      this._navigationController = new NavigationController(navigator);
    }
    this.viewModel?.children?.(this._navigationController);
  }
}
