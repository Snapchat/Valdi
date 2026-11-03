import { Component, StatefulComponent } from 'valdi_core/src/Component';
import { INavigator } from './INavigator';
import { INavigationComponent, NavigationPageContext } from './NavigationComponent';
import { NavigationController } from './NavigationController';

/**
 * Subclass this component and implement `onRenderPageContent` instead of `onRender` to render the page content.
 */
export abstract class NavigationPageComponent<
    ViewModel,
    ComponentContext extends NavigationPageContext = {
      navigator: INavigator;
    },
  >
  extends Component<ViewModel, ComponentContext>
  implements INavigationComponent
{
  static componentPath: string;
  navigationController: NavigationController = new NavigationController(this.context.navigator);
}

/**
 * Subclass this component and implement `onRenderPageContent` instead of `onRender` to render the page content.
 */
export abstract class NavigationPageStatefulComponent<
    ViewModel,
    State,
    ComponentContext extends NavigationPageContext = {
      navigator: INavigator;
    },
  >
  extends StatefulComponent<ViewModel, State, ComponentContext>
  implements INavigationComponent
{
  static componentPath: string;
  navigationController: NavigationController = new NavigationController(this.context.navigator);
}
