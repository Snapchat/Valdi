import { Component } from 'valdi_core/src/Component';
import { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import { systemBoldFont } from 'valdi_core/src/SystemFont';
import { NavigationRoot } from 'valdi_navigation/src/NavigationRoot';
import { NavigationController } from 'valdi_navigation/src/NavigationController';
import { $slot } from 'valdi_core/src/CompilerIntrinsics';
import { createReusableCallback } from 'valdi_core/src/utils/Callback';
import { MicroBenchTest } from './MicroBenchTest';
import { MarshallingTest } from './MarshallingTest';
import { RenderTest } from './RenderTest';
import { DrawFrameBenchmark } from './DrawFrameBenchmark';
import { ProtoImportTest } from './ProtoImportTest';

// Bechmarks
// - Quickjs microbench
// - Marshalling various data
// - Rendering many shapes

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
    this.renderButton('MicroBench', MicroBenchTest, navigationController);
    this.renderButton('Marshalling', MarshallingTest, navigationController);
    this.renderButton('Rendering', RenderTest, navigationController);
    this.renderButton('DrawFrame', DrawFrameBenchmark, navigationController);
    this.renderButton('Protobuf Import', ProtoImportTest, navigationController);
  }

  private renderButton(
    name: string,
    componentCtor: ComponentConstructor<IComponent>,
    navigationController: NavigationController,
  ): void {
    <PageButton
      title={`${name}`}
      onTap={createReusableCallback(() => {
        this.presentPage(name, componentCtor, navigationController);
      })}
    />;
  }
  private presentPage(
    title: string,
    componentCtor: ComponentConstructor<IComponent>,
    navigationController: NavigationController,
  ): void {
    navigationController.present(componentCtor, {}, {});
  }
}

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
