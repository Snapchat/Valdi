import { Component, StatefulComponent } from 'valdi_core/src/Component';
import { Device } from 'valdi_core/src/Device';
import { Style } from 'valdi_core/src/Style';
import { systemBoldFont, systemFont } from 'valdi_core/src/SystemFont';
import { AttributedTextBuilder } from 'valdi_core/src/utils/AttributedTextBuilder';
import { AttributedTextInlineViewVerticalAlignment } from 'valdi_tsx/src/AttributedTextInlineViewAttachment';
import { Label, ScrollView, TextField, TextView, View } from 'valdi_tsx/src/NativeTemplateElements';

const richTextValue = new AttributedTextBuilder()
  .append('Attributed ', { color: '#1D4ED8', font: 'system-bold 18' })
  .append('background ', {
    backgroundBorderRadius: 5,
    backgroundColor: '#DBEAFE',
    backgroundPadding: { bottom: 2, left: 5, right: 5, top: 2 },
    color: '#1E3A8A',
    font: 'system 18',
  })
  .append('underline', { color: '#BE123C', font: 'system 18', textDecoration: 'dotted-underline' })
  .append(' and outline.', {
    color: '#FFFFFF',
    font: 'system-bold 18',
    outlineColor: '#0F172A',
    outlineWidth: 1,
  })
  .build();

const inlineTextValue = new AttributedTextBuilder()
  .append('Inline child ')
  .appendInlineView(0, AttributedTextInlineViewVerticalAlignment.Baseline)
  .append(' baseline aligned in label and textview.')
  .build();

/**
 * @ViewModel
 * @ExportModel
 */
export interface ViewModel {}

interface State {
  enabled: boolean;
  fieldValue: string;
  multilineValue: string;
  eventLog: string;
}

interface PillViewModel {
  title: string;
  color: string;
}

class InlinePill extends Component<PillViewModel> {
  onRender(): void {
    <view style={styles.inlinePill.extend({ backgroundColor: this.viewModel.color })}>
      <label style={styles.inlinePillLabel} value={this.viewModel.title} />
    </view>;
  }
}

/**
 * @Component
 * @ExportModel
 */
export class App extends StatefulComponent<ViewModel, State> {
  state: State = {
    enabled: true,
    eventLog: 'Interact with fields to update this log.',
    fieldValue: 'Editable value',
    multilineValue: 'Editable multiline text\nwith line returns.',
  };

  onRender(): void {
    <view style={styles.screen}>
      <scroll style={styles.screenScroll}>
        <view style={styles.screenContent.extend({ paddingTop: 18 + Device.getDisplayTopInset() })}>
          <label style={styles.appTitle} value="Text Attributes" />
          <label
            style={styles.appSubtitle}
            value="A focused smoke app for label, textview, and textfield attributes. Toggle enabled state and edit fields to exercise callbacks."
          />

          <view style={styles.buttonRow}>
            <view style={styles.toggleButton} onTap={this.toggleEnabled}>
              <label style={styles.toggleButtonLabel} value={this.state.enabled ? 'Disable inputs' : 'Enable inputs'} />
            </view>
            <view style={styles.resetButton} onTap={this.resetValues}>
              <label style={styles.resetButtonLabel} value="Reset values" />
            </view>
          </view>

          {this.renderTextFieldSection()}
          {this.renderTextViewSection()}
          {this.renderLabelSection()}

          <view style={styles.eventCard}>
            {this.renderSectionTitle('Event log')}
            <label style={styles.eventLog} value={this.state.eventLog} />
          </view>
        </view>
      </scroll>
    </view>;
  }

  private renderLabelSection(): void {
    <view style={styles.sectionCard}>
      {this.renderSectionTitle('Label')}
      <label style={styles.fontColorLabel} value="font + color" />
      <label style={styles.letterSpacingLabel} value="letterSpacing expands this label" />
      <label style={styles.dashedUnderlineLabel} value="dashed custom underline" />
      <label style={styles.gradientLabel} value="gradient text" />
      <label style={styles.shadowLabel} value="text shadow" />
      <label
        style={styles.justifiedLabel}
        value="Justified multiline label uses explicit lineHeightAbsolute. This sentence wraps to show paragraph alignment and line spacing."
      />
      <label
        style={styles.autoshrinkLabel}
        value="autoshrink plus ellipsis for a deliberately long single line label"
      />
      <label
        style={styles.twoLineEllipsisLabel}
        value="Two-line label ellipsis should render the truncation glyph at the end of the second line instead of clipping this deliberately long sentence."
      />
      <label
        style={styles.richTextLabel}
        value={richTextValue}
        onSelectionChange={event => this.record(`label selection ${event.selectionStart}-${event.selectionEnd}`)}
      />
      <label style={styles.inlineTextLabel} value={inlineTextValue}>
        <InlinePill color="#0F766E" title="BASE" />
      </label>
    </view>;
  }

  private renderTextViewSection(): void {
    <view style={styles.sectionCard}>
      {this.renderSectionTitle('Textview')}
      <textview
        style={styles.primaryTextView.extend({ enabled: this.state.enabled })}
        value={this.state.multilineValue}
        onChange={event => this.updateMultiline(event.text)}
        onEditBegin={event => this.record(`textview begin ${event.text.length}`)}
        onEditEnd={event => this.record(`textview end reason ${event.reason}`)}
        onReturn={event => this.record(`textview return ${event.text.length}`)}
        onSelectionChange={event => this.record(`textview selection ${event.selectionStart}-${event.selectionEnd}`)}
      />
      <textview style={styles.inlineTextView} value={inlineTextValue}>
        <InlinePill color="#2563EB" title="BASE" />
      </textview>
    </view>;
  }

  private renderTextFieldSection(): void {
    <view style={styles.sectionCard}>
      {this.renderSectionTitle('Textfield')}
      <textfield
        style={styles.primaryTextField.extend({ enabled: this.state.enabled })}
        value={this.state.fieldValue}
        onChange={event => this.updateField(event.text)}
        onEditBegin={event => this.record(`textfield begin ${event.text.length}`)}
        onEditEnd={event => this.record(`textfield end reason ${event.reason}`)}
        onReturn={event => this.record(`textfield return ${event.text}`)}
        onSelectionChange={event => this.record(`textfield selection ${event.selectionStart}-${event.selectionEnd}`)}
        onWillChange={event => {
          this.record(`textfield willChange ${event.text.length}`);
          return event;
        }}
        onWillDelete={event => this.record(`textfield willDelete ${event.text.length}`)}
      />
      <view style={styles.textFieldRow}>
        <textfield style={styles.phoneTextField.extend({ enabled: this.state.enabled })} value="+1 555 0100" />
        <textfield style={styles.passwordTextField.extend({ enabled: this.state.enabled })} value="visible-secret" />
      </view>
      <textfield style={styles.disabledTextField} value="-123.45 disabled selectable" />
    </view>;
  }

  private renderSectionTitle(title: string): void {
    <label style={styles.sectionTitle} value={title} />;
  }

  private toggleEnabled = () => {
    this.setState({
      enabled: !this.state.enabled,
      eventLog: this.state.enabled ? 'Inputs disabled.' : 'Inputs enabled.',
    });
  };

  private resetValues = () => {
    this.setState({
      eventLog: 'Values reset.',
      fieldValue: 'Editable value',
      multilineValue: 'Editable multiline text\nwith line returns.',
    });
  };

  private updateField(text: string): void {
    this.setState({
      eventLog: `textfield change "${text}"`,
      fieldValue: text,
    });
  }

  private updateMultiline(text: string): void {
    this.setState({
      eventLog: `textview change length ${text.length}`,
      multilineValue: text,
    });
  }

  private record(message: string): void {
    this.setState({ eventLog: message });
  }
}

const styles = {
  appSubtitle: new Style<Label>({
    color: '#475569',
    font: systemFont(15),
    lineHeight: 1.25,
    marginBottom: 14,
    numberOfLines: 0,
    width: '100%',
  }),

  appTitle: new Style<Label>({
    color: '#0F172A',
    font: systemBoldFont(27),
    marginBottom: 6,
    width: '100%',
  }),

  autoshrinkLabel: new Style<Label>({
    adjustsFontSizeToFitWidth: true,
    color: '#BE123C',
    font: systemBoldFont(22),
    marginTop: 8,
    minimumScaleFactor: 0.55,
    numberOfLines: 1,
    textOverflow: 'ellipsis',
    width: 260,
  }),

  buttonRow: new Style<View>({
    flexDirection: 'row',
    marginBottom: 12,
    width: '100%',
  }),

  dashedUnderlineLabel: new Style<Label>({
    color: '#0F172A',
    customUnderlineStyle: '1 3 2 -2',
    font: systemFont(17),
    marginTop: 8,
    textDecoration: 'dashed-underline',
    width: '100%',
  }),

  disabledTextField: new Style<TextField>({
    color: '#64748B',
    contentType: 'numberDecimalSigned',
    enabled: false,
    font: systemFont(16),
    height: 42,
    marginTop: 10,
    selectable: true,
    textAlign: 'right',
    width: '100%',
  }),

  eventCard: new Style<View>({
    backgroundColor: '#FFFFFF',
    border: '1 solid #CBD5E1',
    borderRadius: 8,
    marginBottom: 24,
    padding: 14,
    width: '100%',
  }),

  eventLog: new Style<Label>({
    color: '#334155',
    font: systemFont(14),
    lineHeight: 1.25,
    numberOfLines: 0,
    width: '100%',
  }),

  fontColorLabel: new Style<Label>({
    color: '#1D4ED8',
    font: 'system-bold 24 unscaled 24',
    width: '100%',
  }),

  gradientLabel: new Style<Label>({
    font: systemBoldFont(22),
    marginTop: 8,
    textGradient: 'linear-gradient(#DC2626, #7C3AED, #2563EB)',
    width: '100%',
  }),

  inlinePill: new Style<View>({
    alignItems: 'center',
    borderRadius: 5,
    height: 18,
    justifyContent: 'center',
    width: 58,
  }),

  inlinePillLabel: new Style<Label>({
    color: '#FFFFFF',
    font: systemBoldFont(9),
    textAlign: 'center',
    width: '100%',
  }),

  inlineTextLabel: new Style<Label>({
    color: '#0F172A',
    font: systemFont(18),
    lineHeight: 1.35,
    marginTop: 8,
    numberOfLines: 0,
    width: '100%',
  }),

  inlineTextView: new Style<TextView>({
    backgroundColor: '#FFFFFF',
    color: '#334155',
    enabled: false,
    font: systemFont(17),
    height: 74,
    lineHeight: 1.3,
    marginTop: 10,
    numberOfLines: 0,
    textGravity: 'center',
    width: '100%',
  }),

  justifiedLabel: new Style<Label>({
    color: '#0F172A',
    font: systemFont(17),
    lineHeightAbsolute: 26,
    marginTop: 8,
    numberOfLines: 0,
    textAlign: 'justified',
    width: '100%',
  }),

  letterSpacingLabel: new Style<Label>({
    color: '#334155',
    font: systemFont(17),
    letterSpacing: 1.8,
    marginTop: 8,
    width: '100%',
  }),

  passwordTextField: new Style<TextField>({
    color: '#0F172A',
    contentType: 'passwordVisible',
    font: systemFont(16),
    height: 42,
    placeholder: 'Password',
    returnKeyText: 'go',
    width: '48%',
  }),

  phoneTextField: new Style<TextField>({
    color: '#0F172A',
    contentType: 'phoneNumber',
    font: systemFont(16),
    height: 42,
    marginRight: 8,
    placeholder: 'Phone',
    returnKeyText: 'next',
    textAlign: 'center',
    width: '48%',
  }),

  primaryTextField: new Style<TextField>({
    autocapitalization: 'words',
    autocorrection: 'none',
    characterLimit: 24,
    closesWhenReturnKeyPressed: true,
    color: '#0F172A',
    contentType: 'email',
    enableInlinePredictions: true,
    font: systemFont(17),
    height: 44,
    keyboardAppearance: 'dark',
    placeholder: 'Email',
    placeholderColor: '#94A3B8',
    returnKeyText: 'send',
    selectTextOnFocus: true,
    selectable: true,
    selection: [0, 4],
    textAlign: 'left',
    tintColor: '#2563EB',
    width: '100%',
  }),

  primaryTextView: new Style<TextView>({
    backgroundColor: '#FFFFFF',
    backgroundEffectBorderRadius: 8,
    backgroundEffectColor: '#DBEAFE',
    backgroundEffectPadding: 4,
    color: '#0F172A',
    customUnderlineStyle: '1 0 0 -2',
    font: systemFont(17),
    height: 112,
    lineHeightAbsolute: 25,
    numberOfLines: 0,
    placeholder: 'Type multiline text',
    placeholderColor: '#94A3B8',
    returnType: 'linereturn',
    selectable: true,
    selection: [0, 4],
    textAlign: 'left',
    textDecoration: 'underline',
    textGravity: 'top',
    textOverflow: 'ellipsis',
    width: '100%',
  }),

  resetButton: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#E2E8F0',
    borderRadius: 8,
    justifyContent: 'center',
    marginLeft: 8,
    padding: 12,
    width: '48%',
  }),

  resetButtonLabel: new Style<Label>({
    color: '#0F172A',
    font: systemBoldFont(14),
    textAlign: 'center',
    width: '100%',
  }),

  richTextLabel: new Style<Label>({
    color: '#0F172A',
    font: systemFont(18),
    lineHeight: 1.35,
    marginTop: 8,
    numberOfLines: 0,
    selectable: true,
    selection: [0, 6],
    width: '100%',
  }),

  screen: new Style<View>({
    backgroundColor: '#F8FAFC',
    height: '100%',
    width: '100%',
  }),

  screenContent: new Style<View>({
    padding: 18,
    width: '100%',
  }),

  screenScroll: new Style<ScrollView>({
    height: '100%',
    width: '100%',
  }),

  sectionCard: new Style<View>({
    backgroundColor: '#FFFFFF',
    border: '1 solid #CBD5E1',
    borderRadius: 8,
    marginBottom: 14,
    padding: 14,
    width: '100%',
  }),

  sectionTitle: new Style<Label>({
    color: '#334155',
    font: systemBoldFont(14),
    marginBottom: 10,
    width: '100%',
  }),

  shadowLabel: new Style<Label>({
    color: '#78350F',
    font: systemBoldFont(19),
    marginTop: 8,
    textShadow: 'rgba(120, 53, 15, 0.45) 2 0.75 0 2',
    width: '100%',
  }),

  textFieldRow: new Style<View>({
    flexDirection: 'row',
    marginTop: 10,
    width: '100%',
  }),

  toggleButton: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#2563EB',
    borderRadius: 8,
    justifyContent: 'center',
    padding: 12,
    width: '48%',
  }),

  toggleButtonLabel: new Style<Label>({
    color: '#FFFFFF',
    font: systemBoldFont(14),
    textAlign: 'center',
    width: '100%',
  }),

  twoLineEllipsisLabel: new Style<Label>({
    color: '#0F172A',
    font: systemBoldFont(18),
    lineHeightAbsolute: 24,
    marginTop: 10,
    numberOfLines: 2,
    textOverflow: 'ellipsis',
    width: 260,
  }),
};
