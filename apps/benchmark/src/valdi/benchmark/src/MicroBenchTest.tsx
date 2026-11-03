import { Device } from 'valdi_core/src/Device';
import { RequireFunc } from 'valdi_core/src/IModuleLoader';
import { setTimeoutInterruptible } from 'valdi_core/src/SetTimeout';
import { Style } from 'valdi_core/src/Style';
import { systemFont } from 'valdi_core/src/SystemFont';
import { NavigationPage } from 'valdi_navigation/src/NavigationPage';
import { NavigationPageStatefulComponent } from 'valdi_navigation/src/NavigationPageComponent';
import { Label, ScrollView } from 'valdi_tsx/src/NativeTemplateElements';

declare const require: RequireFunc;
interface MicroBenchConsole {
  log(s: string): void;
}

interface MicroBench {
  setConsole(console: MicroBenchConsole): void;
  main(argc: number, argv: string[], global: typeof globalThis): void;
}

const microbench = require('./microbench') as MicroBench;

export interface ViewModel {}
export interface ComponentContext {}

@NavigationPage(module)
export class MicroBenchTest extends NavigationPageStatefulComponent<ViewModel, ComponentContext> {
  state = {
    text: '',
  };

  onCreate(): void {
    setTimeoutInterruptible(() => {
      this.setState({ text: '' });
      microbench.setConsole({
        log: (s: string): void => {
          console.log(s);
          const text = this.state.text + '\n' + s;
          this.setState({ text: text });
        },
      });
      const args = ['microbench'];
      microbench.main(args.length, args, globalThis);
    });
  }

  onRender(): void {
    <view backgroundColor="white">
      <scroll style={styles.scroll} padding={16}>
        <layout flexDirection="column">
          <label style={styles.title} value={`QuickJS MicroBench!`} font={systemFont(20)} />
          <label marginTop={20} value={this.state.text} font={monoSpaceFont(10)} numberOfLines={0} />
        </layout>
      </scroll>
    </view>;
  }
}

const styles = {
  scroll: new Style<ScrollView>({
    alignItems: 'center',
    height: '100%',
  }),

  title: new Style<Label>({
    color: 'black',
    accessibilityCategory: 'header',
    width: '100%',
  }),
};

function monoSpaceFont(size: number): string {
  if (Device.isIOS()) {
    return `RobotoMono ${size}`;
    // } else if (Device.isAndroid()) {
    //   return `Roboto Mono ${size}`;
  } else {
    return `monospace ${size}`;
  }
}
