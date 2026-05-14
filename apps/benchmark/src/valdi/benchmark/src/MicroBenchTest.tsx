import { NavigationPage } from 'valdi_navigation/src/NavigationPage';
import { NavigationPageStatefulComponent } from 'valdi_navigation/src/NavigationPageComponent';
import { Style } from 'valdi_core/src/Style';
import { Label, ScrollView } from 'valdi_tsx/src/NativeTemplateElements';
import { Device } from 'valdi_core/src/Device';
import { systemFont } from 'valdi_core/src/SystemFont';
import { setTimeoutInterruptible } from 'valdi_core/src/SetTimeout';

import { RequireFunc } from 'valdi_core/src/IModuleLoader';
declare const require: RequireFunc;
const microbench = require('./microbench');

export interface ViewModel {}
export interface ComponentContext {}

@NavigationPage(module)
export class MicroBenchTest extends NavigationPageStatefulComponent<ViewModel, ComponentContext> {
  state = {
    text: '',
  };

  onCreate() {
    setTimeoutInterruptible(() => {
      this.setState({ text: '' });
      const self = this;
      microbench.setConsole({
        log(s: string) {
          console.log(s);
          const text = self.state.text + '\n' + s;
          self.setState({ text: text });
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
