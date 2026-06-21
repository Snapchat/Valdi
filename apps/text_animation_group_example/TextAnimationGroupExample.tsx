import { StatefulComponent } from 'valdi_core/src/Component';
import { Style } from 'valdi_core/src/Style';
import { systemBoldFont, systemFont } from 'valdi_core/src/SystemFont';
import { AttributedTextBuilder } from 'valdi_core/src/utils/AttributedTextBuilder';
import { TextAnimationGroup, View } from 'valdi_tsx/src/NativeTemplateElements';

interface State {
  runId: number;
}

/**
 * @ViewModel
 * @ExportModel
 */
export interface ViewModel {}

/**
 * @Component
 * @ExportModel
 */
export class App extends StatefulComponent<ViewModel, State> {
  state: State = {
    runId: 0,
  };

  onRender(): void {
    <view backgroundColor="#F8FAFC" height="100%" padding={24} width="100%">
      <label
        color="#0F172A"
        font={systemBoldFont(26)}
        marginBottom={8}
        value="Text Animation Group"
        width="100%"
      />
      <label
        color="#475569"
        font={systemFont(15)}
        lineHeightMultiple={1.25}
        marginBottom={20}
        numberOfLines={0}
        value="Tap start to restart a single progressive fade across several distant labels and one textview."
        width="100%"
      />

      <textanimationgroup style={styles.group}>
        <view marginBottom={14}>
          {this.renderAnimatedLabel('One shared timeline starts here.')}
        </view>

        <view backgroundColor="#E0F2FE" borderRadius={12} marginBottom={14} padding={14}>
          {this.renderAnimatedLabel('Nested labels join the same sequence.')}
          <view marginTop={10} paddingLeft={16}>
            {this.renderAnimatedLabel('This one is a distant descendant.')}
          </view>
        </view>

        <view backgroundColor="#FFFFFF" border="1 solid #CBD5E1" borderRadius={12} padding={12}>
          <textview
            backgroundColor="#FFFFFF"
            color="#334155"
            enabled={false}
            font={systemFont(17)}
            height={72}
            numberOfLines={0}
            value={this.animatedText('A disabled textview participates after the labels.')}
            width="100%"
          />
        </view>
      </textanimationgroup>

      <view style={styles.button} onTap={this.startAnimation}>
        <label color="#FFFFFF" font={systemBoldFont(17)} textAlign="center" value="Start fade in" width="100%" />
      </view>
    </view>;
  }

  private renderAnimatedLabel(text: string): void {
    <label
      color="#1E293B"
      font={systemFont(18)}
      lineHeightMultiple={1.2}
      numberOfLines={0}
      value={this.animatedText(text)}
      width="100%"
    />;
  }

  private animatedText(text: string) {
    return new AttributedTextBuilder()
      .append(text, {
        animationTransform: {
          duration: 0.45,
          key: `group-demo-${this.state.runId}`,
          opacity: 0,
          timeOffsetBetweenParts: 0.08,
          translationY: 8,
        },
      })
      .build();
  }

  private startAnimation = () => {
    this.setState({ runId: this.state.runId + 1 });
  };
}

const styles = {
  group: new Style<TextAnimationGroup>({
    backgroundColor: '#EFF6FF',
    borderRadius: 16,
    marginBottom: 20,
    padding: 16,
    width: '100%',
  }),

  button: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#2563EB',
    borderRadius: 12,
    padding: 16,
    width: '100%',
  }),
};
