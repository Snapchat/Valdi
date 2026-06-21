import { Component, StatefulComponent } from 'valdi_core/src/Component';
import { Device } from 'valdi_core/src/Device';
import { Style } from 'valdi_core/src/Style';
import { systemBoldFont, systemFont } from 'valdi_core/src/SystemFont';
import { AttributedTextBuilder } from 'valdi_core/src/utils/AttributedTextBuilder';
import { AttributedTextInlineViewVerticalAlignment } from 'valdi_tsx/src/AttributedTextInlineViewAttachment';
import { View } from 'valdi_tsx/src/NativeTemplateElements';

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
    <view
      alignItems="center"
      backgroundColor={this.viewModel.color}
      borderRadius={5}
      height={18}
      justifyContent="center"
      width={58}
    >
      <label color="#FFFFFF" font={systemBoldFont(9)} textAlign="center" value={this.viewModel.title} width="100%" />
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
    <view backgroundColor="#F8FAFC" height="100%" width="100%">
      <scroll height="100%" width="100%">
        <view padding={18} paddingTop={18 + Device.getDisplayTopInset()} width="100%">
          <label color="#0F172A" font={systemBoldFont(27)} marginBottom={6} value="Text Attributes" width="100%" />
          <label
            color="#475569"
            font={systemFont(15)}
            lineHeightMultiple={1.25}
            marginBottom={14}
            numberOfLines={0}
            value="A focused smoke app for label, textview, and textfield attributes. Toggle enabled state and edit fields to exercise callbacks."
            width="100%"
          />

          <view style={styles.buttonRow} marginBottom={12}>
            <view style={styles.toggleButton} onTap={this.toggleEnabled}>
              <label
                color="#FFFFFF"
                font={systemBoldFont(14)}
                textAlign="center"
                value={this.state.enabled ? 'Disable inputs' : 'Enable inputs'}
                width="100%"
              />
            </view>
            <view style={styles.resetButton} onTap={this.resetValues}>
              <label color="#0F172A" font={systemBoldFont(14)} textAlign="center" value="Reset values" width="100%" />
            </view>
          </view>

          {this.renderTextFieldSection()}
          {this.renderTextViewSection()}
          {this.renderLabelSection()}

          <view style={styles.card} marginBottom={24}>
            {this.renderSectionTitle('Event log')}
            <label
              color="#334155"
              font={systemFont(14)}
              lineHeightMultiple={1.25}
              numberOfLines={0}
              value={this.state.eventLog}
              width="100%"
            />
          </view>
        </view>
      </scroll>
    </view>;
  }

  private renderLabelSection(): void {
    <view style={styles.card} marginBottom={14}>
      {this.renderSectionTitle('Label')}
      <label color="#1D4ED8" font="system-bold 24 unscaled 24" value="font + color" width="100%" />
      <label
        color="#334155"
        font={systemFont(17)}
        letterSpacing={1.8}
        marginTop={8}
        value="letterSpacing expands this label"
        width="100%"
      />
      <label
        color="#0F172A"
        customUnderlineStyle="1 3 2 -2"
        font={systemFont(17)}
        marginTop={8}
        textDecoration="dashed-underline"
        value="dashed custom underline"
        width="100%"
      />
      <label
        font={systemBoldFont(22)}
        marginTop={8}
        textGradient="linear-gradient(#DC2626, #7C3AED, #2563EB)"
        value="gradient text"
        width="100%"
      />
      <label
        color="#78350F"
        font={systemBoldFont(19)}
        marginTop={8}
        textShadow="rgba(120, 53, 15, 0.45) 2 0.75 0 2"
        value="text shadow"
        width="100%"
      />
      <label
        color="#0F172A"
        font={systemFont(17)}
        lineHeight={26}
        marginTop={8}
        numberOfLines={0}
        textAlign="justified"
        value="Justified multiline label uses explicit lineHeight. This sentence wraps to show paragraph alignment and line spacing."
        width="100%"
      />
      <label
        adjustsFontSizeToFitWidth={true}
        color="#BE123C"
        font={systemBoldFont(22)}
        marginTop={8}
        minimumScaleFactor={0.55}
        numberOfLines={1}
        textOverflow="ellipsis"
        value="autoshrink plus ellipsis for a deliberately long single line label"
        width={260}
      />
      <label
        color="#0F172A"
        font={systemBoldFont(18)}
        lineHeight={24}
        marginTop={10}
        numberOfLines={2}
        textOverflow="ellipsis"
        value="Two-line label ellipsis should render the truncation glyph at the end of the second line instead of clipping this deliberately long sentence."
        width={260}
      />
      <label
        color="#0F172A"
        font={systemFont(18)}
        lineHeightMultiple={1.35}
        marginTop={8}
        numberOfLines={0}
        selectable={true}
        selection={[0, 6]}
        value={richTextValue}
        width="100%"
        onSelectionChange={event => this.record(`label selection ${event.selectionStart}-${event.selectionEnd}`)}
      />
      <label
        color="#0F172A"
        font={systemFont(18)}
        lineHeightMultiple={1.35}
        marginTop={8}
        numberOfLines={0}
        value={inlineTextValue}
        width="100%"
      >
        <InlinePill color="#0F766E" title="BASE" />
      </label>
    </view>;
  }

  private renderTextViewSection(): void {
    <view style={styles.card} marginBottom={14}>
      {this.renderSectionTitle('Textview')}
      <textview
        backgroundColor="#FFFFFF"
        backgroundEffectBorderRadius={8}
        backgroundEffectColor="#DBEAFE"
        backgroundEffectPadding={4}
        color="#0F172A"
        customUnderlineStyle="1 0 0 -2"
        enabled={this.state.enabled}
        font={systemFont(17)}
        height={112}
        lineHeight={25}
        numberOfLines={0}
        placeholder="Type multiline text"
        placeholderColor="#94A3B8"
        returnType="linereturn"
        selectable={true}
        selection={[0, 4]}
        textAlign="left"
        textDecoration="underline"
        textGravity="top"
        textOverflow="ellipsis"
        value={this.state.multilineValue}
        width="100%"
        onChange={event => this.updateMultiline(event.text)}
        onEditBegin={event => this.record(`textview begin ${event.text.length}`)}
        onEditEnd={event => this.record(`textview end reason ${event.reason}`)}
        onReturn={event => this.record(`textview return ${event.text.length}`)}
        onSelectionChange={event => this.record(`textview selection ${event.selectionStart}-${event.selectionEnd}`)}
      />
      <textview
        backgroundColor="#FFFFFF"
        color="#334155"
        enabled={false}
        font={systemFont(17)}
        height={74}
        lineHeightMultiple={1.3}
        marginTop={10}
        numberOfLines={0}
        textGravity="center"
        value={inlineTextValue}
        width="100%"
      >
        <InlinePill color="#2563EB" title="BASE" />
      </textview>
    </view>;
  }

  private renderTextFieldSection(): void {
    <view style={styles.card} marginBottom={14}>
      {this.renderSectionTitle('Textfield')}
      <textfield
        autocapitalization="words"
        autocorrection="none"
        characterLimit={24}
        closesWhenReturnKeyPressed={true}
        color="#0F172A"
        contentType="email"
        enabled={this.state.enabled}
        enableInlinePredictions={true}
        font={systemFont(17)}
        height={44}
        keyboardAppearance="dark"
        placeholder="Email"
        placeholderColor="#94A3B8"
        returnKeyText="send"
        selectTextOnFocus={true}
        selectable={true}
        selection={[0, 4]}
        textAlign="left"
        tintColor="#2563EB"
        value={this.state.fieldValue}
        width="100%"
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
      <view flexDirection="row" marginTop={10} width="100%">
        <textfield
          color="#0F172A"
          contentType="phoneNumber"
          enabled={this.state.enabled}
          font={systemFont(16)}
          height={42}
          marginRight={8}
          placeholder="Phone"
          returnKeyText="next"
          textAlign="center"
          value="+1 555 0100"
          width="48%"
        />
        <textfield
          color="#0F172A"
          contentType="passwordVisible"
          enabled={this.state.enabled}
          font={systemFont(16)}
          height={42}
          placeholder="Password"
          returnKeyText="go"
          value="visible-secret"
          width="48%"
        />
      </view>
      <textfield
        color="#64748B"
        contentType="numberDecimalSigned"
        enabled={false}
        font={systemFont(16)}
        height={42}
        marginTop={10}
        selectable={true}
        textAlign="right"
        value="-123.45 disabled selectable"
        width="100%"
      />
    </view>;
  }

  private renderSectionTitle(title: string): void {
    <label color="#334155" font={systemBoldFont(14)} marginBottom={10} value={title} width="100%" />;
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
  buttonRow: new Style<View>({
    flexDirection: 'row',
    width: '100%',
  }),

  card: new Style<View>({
    backgroundColor: '#FFFFFF',
    border: '1 solid #CBD5E1',
    borderRadius: 8,
    padding: 14,
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

  toggleButton: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#2563EB',
    borderRadius: 8,
    justifyContent: 'center',
    padding: 12,
    width: '48%',
  }),
};
