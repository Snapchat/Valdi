import { Conversation } from 'conversation/src/Conversation';
import { Component } from 'valdi_core/src/Component';
import { Style } from 'valdi_core/src/Style';
import { View } from 'valdi_tsx/src/NativeTemplateElements';

/**
 * @ViewModel
 * @ExportModel({ ios: 'ValdiStartViewComponentViewModel', android: 'com.snap.valdi.helloworld.StartViewComponentViewModel'})
 */
export interface ValdiGptAppViewModel {}

/**
 * @Context
 * @ExportModel({ios: 'ValdiStartViewComponentContext', android: 'com.snap.valdi.helloworld.StartViewComponentContext'})
 */
export interface ValdiGptAppContext {}

/**
 * @Component
 * @ExportModel({ios: 'ValdiStartView', android: 'com.snap.valdi.helloworld.StartView'})
 */
export class ValdiGptApp extends Component<ValdiGptAppViewModel, ValdiGptAppContext> {
  onRender(): void {
    <view style={styles.page}>
      <Conversation />
    </view>;
  }
}

const styles = {
  page: new Style<View>({
    backgroundColor: 'white',
    width: '100%',
    height: '100%',
  }),
};
