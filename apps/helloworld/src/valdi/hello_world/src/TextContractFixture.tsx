import { Component } from 'valdi_core/src/Component';
import { systemFont } from 'valdi_core/src/SystemFont';

/**
 * @ViewModel
 * @ExportModel
 */
export interface TextContractFixtureViewModel {}

/**
 * @Context
 * @ExportModel
 */
export interface TextContractFixtureContext {}

/**
 * Text elements the Android contract tests in apps/helloworld/androidTest mount. Never part of the
 * app's own screens.
 *
 * @Component
 * @ExportModel
 */
export class TextContractFixture extends Component<TextContractFixtureViewModel, TextContractFixtureContext> {
  onRender(): void {
    <view backgroundColor="white" padding={16}>
      <textfield
        accessibilityId="contract-input"
        value="Initial"
        placeholder="Type here"
        font={systemFont(18)}
        width="100%"
        height={48}
        marginBottom={12}
      />
      <label accessibilityId="contract-label" value="Label text" font={systemFont(18)} marginBottom={12} />
      <custom-view
        androidClass="com.snap.valdi.helloworld.textbinding.HelloWorldEmojiTextView"
        accessibilityId="contract-emoji"
        value="Hello 👋"
        font={systemFont(18)}
        marginBottom={12}
      />
      <custom-view
        androidClass="com.snap.valdi.helloworld.textbinding.HelloWorldTextView"
        accessibilityId="contract-host-label"
        value="Host label"
        placeholder="Host placeholder"
        font={systemFont(18)}
      />
    </view>;
  }
}
