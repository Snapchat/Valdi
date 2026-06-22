import { Component } from 'valdi_core/src/Component';
import { Style } from 'valdi_core/src/Style';
import { systemFont } from 'valdi_core/src/SystemFont';
import { Label, ScrollView } from 'valdi_tsx/src/NativeTemplateElements';

import res from '../res';
import { onRootComponentCreated } from './CppModule';
import { APP_NAME } from './NativeModule';

/**
 * @ViewModel
 * @ExportModel
 */
export interface ViewModel {}

/**
 * @Context
 * @ExportModel
 */
export interface ComponentContext {}

/**
 * @Component
 * @ExportModel
 */
export class App extends Component<ViewModel, ComponentContext> {
  onCreate(): void {
    onRootComponentCreated(this.renderer.contextId);
    console.log('Hello World onCreate!');
  }

  onRender(): void {
    console.log('Hello World onRender!!!');
    <view backgroundColor="white">
      <scroll style={styles.scroll} padding={16}>
        <layout marginTop={100} flexDirection="row" width="100%" minHeight={10}>
          <image src={res.emoji} height="100%" tint="gray" marginRight={10} />
          <label style={styles.title} value={`Welcome to ${APP_NAME}!`} font={systemFont(20)} />
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
