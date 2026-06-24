import { StatefulComponent } from 'valdi_core/src/Component';
import { Style } from 'valdi_core/src/Style';
import { systemBoldFont, systemFont } from 'valdi_core/src/SystemFont';
import { AttributedTextBuilder } from 'valdi_core/src/utils/AttributedTextBuilder';
import { Label, TextAnimationGroup, TextView, View } from 'valdi_tsx/src/NativeTemplateElements';

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
    <view style={styles.screen}>
      <label style={styles.title} value="Text Animation Group" />
      <label
        style={styles.subtitle}
        value="Tap start to restart a single progressive fade across several distant labels and one textview."
      />

      <textanimationgroup style={styles.group}>
        <view style={styles.groupLabelBlock}>{this.renderAnimatedLabel('One shared timeline starts here.')}</view>

        <view style={styles.groupNestedBlock}>
          {this.renderAnimatedLabel('Nested labels join the same sequence.')}
          <view style={styles.groupNestedChild}>{this.renderAnimatedLabel('This one is a distant descendant.')}</view>
        </view>

        <view style={styles.groupTextViewBlock}>
          <textview
            style={styles.groupTextView}
            value={this.animatedText('A disabled textview participates after the labels.')}
          />
        </view>
      </textanimationgroup>

      <view style={styles.button} onTap={this.startAnimation}>
        <label style={styles.buttonLabel} value="Start fade in" />
      </view>
    </view>;
  }

  private renderAnimatedLabel(text: string): void {
    <label style={styles.animatedLabel} value={this.animatedText(text)} />;
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
  animatedLabel: new Style<Label>({
    color: '#1E293B',
    font: systemFont(18),
    lineHeight: 1.2,
    numberOfLines: 0,
    width: '100%',
  }),

  button: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#2563EB',
    borderRadius: 12,
    padding: 16,
    width: '100%',
  }),

  buttonLabel: new Style<Label>({
    color: '#FFFFFF',
    font: systemBoldFont(17),
    textAlign: 'center',
    width: '100%',
  }),

  group: new Style<TextAnimationGroup>({
    backgroundColor: '#EFF6FF',
    borderRadius: 16,
    marginBottom: 20,
    padding: 16,
    width: '100%',
  }),

  groupLabelBlock: new Style<View>({
    marginBottom: 14,
  }),

  groupNestedBlock: new Style<View>({
    backgroundColor: '#E0F2FE',
    borderRadius: 12,
    marginBottom: 14,
    padding: 14,
  }),

  groupNestedChild: new Style<View>({
    marginTop: 10,
    paddingLeft: 16,
  }),

  groupTextView: new Style<TextView>({
    backgroundColor: '#FFFFFF',
    color: '#334155',
    enabled: false,
    font: systemFont(17),
    height: 72,
    numberOfLines: 0,
    width: '100%',
  }),

  groupTextViewBlock: new Style<View>({
    backgroundColor: '#FFFFFF',
    border: '1 solid #CBD5E1',
    borderRadius: 12,
    padding: 12,
  }),

  screen: new Style<View>({
    backgroundColor: '#F8FAFC',
    height: '100%',
    padding: 24,
    width: '100%',
  }),

  subtitle: new Style<Label>({
    color: '#475569',
    font: systemFont(15),
    lineHeight: 1.25,
    marginBottom: 20,
    numberOfLines: 0,
    width: '100%',
  }),

  title: new Style<Label>({
    color: '#0F172A',
    font: systemBoldFont(26),
    marginBottom: 8,
    width: '100%',
  }),
};
