import { Component, StatefulComponent } from 'valdi_core/src/Component';
import { Device } from 'valdi_core/src/Device';
import { Style } from 'valdi_core/src/Style';
import { systemBoldFont, systemFont } from 'valdi_core/src/SystemFont';
import { AttributedTextBuilder } from 'valdi_core/src/utils/AttributedTextBuilder';
import { AttributedTextInlineViewVerticalAlignment } from 'valdi_tsx/src/AttributedTextInlineViewAttachment';
import { View } from 'valdi_tsx/src/NativeTemplateElements';

const alignmentText = new AttributedTextBuilder()
  .append('Top ')
  .appendInlineView(0, AttributedTextInlineViewVerticalAlignment.Top)
  .append('  Center ')
  .appendInlineView(1, AttributedTextInlineViewVerticalAlignment.Center)
  .append('  Bottom ')
  .appendInlineView(2, AttributedTextInlineViewVerticalAlignment.Bottom)
  .append('  Baseline ')
  .appendInlineView(3, AttributedTextInlineViewVerticalAlignment.Baseline)
  .append(' inside one wrapped text run.')
  .build();

const labelInteractiveText = new AttributedTextBuilder()
  .append('A label can host a stateful inline child: ')
  .appendInlineView(0, AttributedTextInlineViewVerticalAlignment.Center)
  .append(' and the surrounding text is not rebuilt when it changes size.')
  .build();

const textViewInteractiveText = new AttributedTextBuilder()
  .append('The same stateful child works in a textview: ')
  .appendInlineView(0, AttributedTextInlineViewVerticalAlignment.Center)
  .append(' so text layout can place a resized view again.')
  .build();

const ltrEndInlineText = new AttributedTextBuilder()
  .append('Text before ')
  .appendInlineView(0, AttributedTextInlineViewVerticalAlignment.Center)
  .build();

const rtlEndInlineText = new AttributedTextBuilder()
  .append('אבג ')
  .appendInlineView(0, AttributedTextInlineViewVerticalAlignment.Center)
  .build();

/**
 * @ViewModel
 * @ExportModel
 */
export interface ViewModel {}

interface MarkerViewModel {
  title: string;
  color: string;
  height: number;
  width: number;
}

class AlignmentMarker extends Component<MarkerViewModel> {
  onRender(): void {
    <view
      alignItems="center"
      backgroundColor={this.viewModel.color}
      borderRadius={5}
      height={this.viewModel.height}
      justifyContent="center"
      width={this.viewModel.width}
    >
      <label color="#FFFFFF" font={systemBoldFont(9)} textAlign="center" value={this.viewModel.title} width="100%" />
    </view>;
  }
}

interface ExpandableInlineButtonViewModel {
  compactTitle: string;
  expandedTitle: string;
}

interface ExpandableInlineButtonState {
  expanded: boolean;
}

class ExpandableInlineButton extends StatefulComponent<ExpandableInlineButtonViewModel, ExpandableInlineButtonState> {
  state: ExpandableInlineButtonState = {
    expanded: false,
  };

  onRender(): void {
    const expanded = this.state.expanded;
    <view
      accessibilityCategory="button"
      alignItems="center"
      backgroundColor={expanded ? '#0F766E' : '#2563EB'}
      border="1 solid #0F172A"
      borderRadius={7}
      height={26}
      justifyContent="center"
      onTap={this.toggle}
      paddingLeft={expanded ? 12 : 8}
      paddingRight={expanded ? 12 : 8}
      width={expanded ? 156 : 82}
    >
      <label
        color="#FFFFFF"
        font={systemBoldFont(expanded ? 13 : 12)}
        textAlign="center"
        value={expanded ? this.viewModel.expandedTitle : this.viewModel.compactTitle}
        width="100%"
      />
    </view>;
  }

  private toggle = () => {
    this.setState({ expanded: !this.state.expanded });
  };
}

/**
 * @Component
 * @ExportModel
 */
export class App extends Component<ViewModel> {
  onRender(): void {
    <view backgroundColor="#F8FAFC" height="100%" width="100%">
      <scroll height="100%" width="100%">
        <view padding={22} paddingTop={22 + Device.getDisplayTopInset()} width="100%">
          <label color="#0F172A" font={systemBoldFont(27)} marginBottom={8} value="Inline Text Children" width="100%" />
          <label
            color="#475569"
            font={systemFont(15)}
            lineHeight={1.25}
            marginBottom={18}
            numberOfLines={0}
            value="Labels and textviews can embed real Valdi child views. Yoga resolves child size; native text layout places each child inline."
            width="100%"
          />

          <view style={styles.card} marginBottom={14}>
            <label color="#334155" font={systemBoldFont(14)} marginBottom={10} value="Label alignment" width="100%" />
            <label
              color="#0F172A"
              font={systemFont(20)}
              lineHeight={1.45}
              numberOfLines={0}
              value={alignmentText}
              width="100%"
            >
              <AlignmentMarker color="#DC2626" height={12} title="TOP" width={48} />
              <AlignmentMarker color="#7C3AED" height={12} title="MID" width={48} />
              <AlignmentMarker color="#047857" height={12} title="BOT" width={48} />
              <AlignmentMarker color="#0F766E" height={12} title="BASE" width={48} />
            </label>
          </view>

          <view style={styles.card} marginBottom={14}>
            <label color="#334155" font={systemBoldFont(14)} marginBottom={10} value="Textview alignment" width="100%" />
            <textview
              backgroundColor="#FFFFFF"
              color="#0F172A"
              enabled={false}
              font={systemFont(19)}
              height={104}
              lineHeight={1.45}
              numberOfLines={0}
              value={alignmentText}
              width="100%"
            >
              <AlignmentMarker color="#EA580C" height={18} title="TOP" width={48} />
              <AlignmentMarker color="#2563EB" height={18} title="MID" width={48} />
              <AlignmentMarker color="#16A34A" height={18} title="BOT" width={48} />
              <AlignmentMarker color="#0F766E" height={18} title="BASE" width={48} />
            </textview>
          </view>

          <view style={styles.card} marginBottom={14}>
            <label color="#334155" font={systemBoldFont(14)} marginBottom={10} value="Label LTR vs RTL append position" width="100%" />
            <label
              color="#64748B"
              font={systemFont(13)}
              lineHeight={1.25}
              marginBottom={10}
              numberOfLines={0}
              value="Both labels append the inline child after the text. The END pill should sit on the right in LTR and on the left in RTL."
              width="100%"
            />
            <view alignItems="center" flexDirection="row" marginBottom={8} width="100%">
              <label color="#475569" font={systemBoldFont(12)} marginRight={10} textAlign="center" value="LTR" width={34} />
              <view
                backgroundColor="#F8FAFC"
                border="1 solid #CBD5E1"
                borderRadius={7}
                direction="ltr"
                flexGrow={1}
                padding={10}
              >
                <label
                  color="#0F172A"
                  font={systemFont(19)}
                  lineHeight={1.45}
                  numberOfLines={0}
                  value={ltrEndInlineText}
                  width="100%"
                >
                  <AlignmentMarker color="#2563EB" height={18} title="END" width={48} />
                </label>
              </view>
            </view>
            <view alignItems="center" flexDirection="row" width="100%">
              <label color="#475569" font={systemBoldFont(12)} marginRight={10} textAlign="center" value="RTL" width={34} />
              <view
                backgroundColor="#F8FAFC"
                border="1 solid #CBD5E1"
                borderRadius={7}
                direction="rtl"
                flexGrow={1}
                padding={10}
              >
                <label
                  color="#0F172A"
                  font={systemFont(19)}
                  lineHeight={1.45}
                  numberOfLines={0}
                  value={rtlEndInlineText}
                  width="100%"
                >
                  <AlignmentMarker color="#7C3AED" height={18} title="END" width={48} />
                </label>
              </view>
            </view>
          </view>

          <view style={styles.card} marginBottom={14}>
            <label color="#334155" font={systemBoldFont(14)} marginBottom={10} value="Stateful child in a label" width="100%" />
            <label
              color="#0F172A"
              font={systemFont(19)}
              lineHeight={1.45}
              numberOfLines={0}
              value={labelInteractiveText}
              width="100%"
            >
              <ExpandableInlineButton compactTitle="Expand" expandedTitle="Contract inline" />
            </label>
          </view>

          <view style={styles.card}>
            <label color="#334155" font={systemBoldFont(14)} marginBottom={10} value="Stateful child in a textview" width="100%" />
            <textview
              backgroundColor="#FFFFFF"
              color="#0F172A"
              enabled={false}
              font={systemFont(19)}
              height={124}
              lineHeight={1.45}
              numberOfLines={0}
              value={textViewInteractiveText}
              width="100%"
            >
              <ExpandableInlineButton compactTitle="Open" expandedTitle="Close inline view" />
            </textview>
          </view>
        </view>
      </scroll>
    </view>;
  }
}

const styles = {
  card: new Style<View>({
    backgroundColor: '#FFFFFF',
    border: '1 solid #CBD5E1',
    borderRadius: 8,
    padding: 14,
    width: '100%',
  }),
};
