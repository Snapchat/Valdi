import { Component } from 'valdi_core/src/Component';
import { testFont } from './TestFont';

const longText =
  'Scroll content with no explicit height must grow to fit its text instead of being cut to the viewport. ' +
  'This last sentence only shows when the view is scrolled down. THE END.';

const styles = {
  root: {
    width: '100%' as const,
    height: '100%' as const,
    backgroundColor: '#FFFFFF',
    flexDirection: 'column' as const,
    padding: 8,
  },
  viewport: {
    width: 150,
    height: 70,
    backgroundColor: '#F0F0F0',
    borderWidth: 1,
    borderColor: '#000000',
    marginBottom: 6,
  },
  // The wrapper's own background shows how tall the content measured.
  content: {
    backgroundColor: '#FFD9A0',
    padding: 2,
  },
};

// contentOffsetY is programmatic-only, so it is not part of the typed scroll attributes.
const scrolledDown: object = { contentOffsetY: 120 };

function renderText(): void {
  <view {...styles.content}>
    <label value={longText} font={testFont(11)} color="#000000" numberOfLines={0} />
  </view>;
}

/**
 * With overflow scroll the children are measured unclamped along the main axis, so content taller
 * than the viewport keeps its full height. Offsetting past the viewport reveals whether it did.
 */
export class ScrollContentGrowth extends Component {
  onRender(): void {
    <view {...styles.root}>
      <label value="scroll content growth tests" font={testFont(10)} color="#999999" />
      <layout height={6} />

      <label value="Offset 0:" font={testFont(9)} color="#666666" />
      <scroll {...styles.viewport}>{renderText()}</scroll>

      <label value="Scrolled down 120pt:" font={testFont(9)} color="#666666" />
      <scroll {...styles.viewport} {...scrolledDown}>
        {renderText()}
      </scroll>
    </view>;
  }
}
