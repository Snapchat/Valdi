import { AnyRenderFunction } from 'valdi_core/src/AnyRenderFunction';
import { $slot } from 'valdi_core/src/CompilerIntrinsics';
import { Component } from 'valdi_core/src/Component';
import { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import { systemBoldFont } from 'valdi_core/src/SystemFont';
import { createReusableCallback } from 'valdi_core/src/utils/Callback';
import { NavigationController } from 'valdi_navigation/src/NavigationController';
import { NavigationPage } from 'valdi_navigation/src/NavigationPage';
import { NavigationPageComponent } from 'valdi_navigation/src/NavigationPageComponent';
import { NavigationRoot } from 'valdi_navigation/src/NavigationRoot';

interface PageButtonViewModel {
  title: string;
  onTap: () => void;
}

class PageButton extends Component<PageButtonViewModel> {
  onRender(): void {
    <view
      backgroundColor="lightgray"
      padding={16}
      alignSelf="stretch"
      margin={8}
      marginLeft={24}
      marginRight={24}
      borderRadius={'50%'}
      boxShadow={'0 0 3 rgba(0, 0, 0, 0.15)'}
      alignItems="center"
      onTap={this.viewModel.onTap}
    >
      <label color="black" value={this.viewModel.title} font={systemBoldFont(17)} />
    </view>;
  }
}

// JSX function components need a capitalized name.
// eslint-disable-next-line @typescript-eslint/naming-convention
function PageBackground(viewModel: { children?: AnyRenderFunction | void }): void {
  <view backgroundColor="white" width="100%" height="100%" alignItems="center" justifyContent="center">
    {viewModel.children?.()}
  </view>;
}

@NavigationPage(module)
export class Page1 extends NavigationPageComponent<{}> {
  onRender(): void {
    <PageBackground>
      <PageButton
        title={`Go To Page #2`}
        onTap={createReusableCallback(() => {
          this.navigationController.push(Page2, {}, {});
        })}
      />
    </PageBackground>;
  }
}

@NavigationPage(module)
export class Page2 extends NavigationPageComponent<{}> {
  onRender(): void {
    <PageBackground>
      <PageButton
        title={`Go To Page #3`}
        onTap={createReusableCallback(() => {
          this.navigationController.push(Page3, {}, {});
        })}
      />
    </PageBackground>;
  }
}

@NavigationPage(module)
export class Page3 extends NavigationPageComponent<{}> {
  onRender(): void {
    <PageBackground>{this.renderChildren()}</PageBackground>;
  }

  private renderChildren(): void {
    <PageButton
      title={`Present nested`}
      onTap={createReusableCallback(() => {
        this.navigationController.present(Page1, {}, {});
      })}
    />;
    <PageButton
      title={`Dismiss`}
      onTap={createReusableCallback(() => {
        this.navigationController.dismiss(true);
      })}
    />;
  }
}

export class App extends Component {
  onRender(): void {
    <NavigationRoot>
      {$slot(navigationController => {
        <view backgroundColor="white" width="100%" height="100%" alignItems="center" justifyContent="center">
          {this.renderButtons(navigationController)}
        </view>;
      })}
    </NavigationRoot>;
  }

  private renderButtons(navigationController: NavigationController): void {
    this.renderButton('Page #1', Page1, navigationController);
    this.renderButton('Page #2', Page2, navigationController);
    this.renderButton('Page #3', Page3, navigationController);
  }

  private presentPage(
    title: string,
    componentCtor: ComponentConstructor<IComponent>,
    navigationController: NavigationController,
  ): void {
    navigationController.present(componentCtor, {}, {});
  }

  private renderButton(
    name: string,
    componentCtor: ComponentConstructor<IComponent>,
    navigationController: NavigationController,
  ): void {
    <PageButton
      title={`Present ${name}`}
      onTap={createReusableCallback(() => {
        this.presentPage(name, componentCtor, navigationController);
      })}
    />;
  }
}
