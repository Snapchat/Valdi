import { Component } from 'valdi_core/src/Component';
import { Style } from 'valdi_core/src/Style';
import { systemFont } from 'valdi_core/src/SystemFont';
import { Label, Layout, ScrollView, View } from 'valdi_tsx/src/NativeTemplateElements';

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
        <layout marginTop={80} style={styles.content}>
          <layout flexDirection="row" width="100%" minHeight={10} marginBottom={24}>
            <image src={res.emoji} height="100%" tint="gray" marginRight={10} />
            <label style={styles.title} value={`Welcome to ${APP_NAME}!`} font={systemFont(20)} />
          </layout>

          <view style={styles.previewCard}>
            <label
              style={styles.previewTitle}
              value="Text decoration preview"
              font={systemFont(20)}
            />
            <label
              style={styles.previewBody}
              value="Single line dashed underline"
              font={systemFont(18)}
              textDecoration="dashed-underline"
            />
            <label
              style={styles.previewBody}
              value="This longer dashed underline sample should wrap across multiple lines so we can verify the decoration stays visible after the line break."
              font={systemFont(18)}
              textDecoration="dashed-underline"
              numberOfLines={0}
              lineHeightMultiple={1.2}
            />
            <label
              style={styles.previewBody}
              value="Single line dotted underline"
              font={systemFont(18)}
              textDecoration="dotted-underline"
            />
            <label
              style={styles.previewBody}
              value="This longer dotted underline sample should wrap across multiple lines so we can verify the decoration stays visible after the line break."
              font={systemFont(18)}
              textDecoration="dotted-underline"
              numberOfLines={0}
              lineHeightMultiple={1.2}
            />
          </view>
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

  content: new Style<Layout>({
    alignItems: 'center',
    width: '100%',
  }),

  title: new Style<Label>({
    color: 'black',
    accessibilityCategory: 'header',
    width: '100%',
  }),

  previewCard: new Style<View>({
    backgroundColor: '#F2F4F7',
    borderRadius: 20,
    padding: 20,
    width: 320,
  }),

  previewTitle: new Style<Label>({
    color: '#111827',
    marginBottom: 14,
    width: '100%',
  }),

  previewBody: new Style<Label>({
    color: '#1F2937',
    marginBottom: 14,
    width: '100%',
  }),
};
