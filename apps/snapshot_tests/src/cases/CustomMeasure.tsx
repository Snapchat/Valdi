import { Component } from 'valdi_core/src/Component';
import { testFont, testBoldFont } from './TestFont';

const styles = {
  root: {
    width: '100%' as const,
    height: '100%' as const,
    backgroundColor: '#FFFFFF',
    flexDirection: 'column' as const,
    padding: 8,
  },
  section: {
    marginBottom: 8,
    backgroundColor: '#F5F5F5',
    borderRadius: 6,
    padding: 8,
  },
  row: {
    flexDirection: 'row' as const,
    alignItems: 'center' as const,
  },
  tag: {
    backgroundColor: '#E3F2FD',
    borderRadius: 12,
    paddingLeft: 8,
    paddingRight: 8,
    paddingTop: 4,
    paddingBottom: 4,
    marginRight: 4,
    marginBottom: 4,
  },
  badge: {
    backgroundColor: '#F44336',
    borderRadius: 10,
    width: 20,
    height: 20,
    alignItems: 'center' as const,
    justifyContent: 'center' as const,
  },
  growItem: {
    flexGrow: 1,
    backgroundColor: '#E8F5E9',
    borderRadius: 4,
    padding: 6,
    marginRight: 4,
  },
  fixedItem: {
    width: 60,
    backgroundColor: '#FFF3E0',
    borderRadius: 4,
    padding: 6,
  },
  wrapContainer: {
    flexDirection: 'row' as const,
    flexWrap: 'wrap' as const,
  },
  shrinkRow: {
    flexDirection: 'row' as const,
    height: 30,
  },
  shrinkLabel: {
    flexShrink: 1,
    backgroundColor: '#FCE4EC',
    borderRadius: 4,
    padding: 4,
  },
  shrinkSpacer: {
    width: 80,
    backgroundColor: '#E0E0E0',
    borderRadius: 4,
  },
};

export class CustomMeasure extends Component {
  onRender(): void {
    <view {...styles.root}>
      {/* Labels in flex-grow: measured text size + flex-grow interaction */}
      <view {...styles.section}>
        <layout {...styles.row}>
          <view {...styles.growItem}>
            <label value="Short" font={testFont(11)} color="#333333" />
          </view>
          <view {...styles.growItem}>
            <label value="Medium length" font={testFont(11)} color="#333333" />
          </view>
          <view {...styles.fixedItem}>
            <label value="Fixed" font={testBoldFont(11)} color="#333333" />
          </view>
        </layout>
      </view>

      {/* Wrapping tags: measured label width drives wrap behavior */}
      <view {...styles.section}>
        <layout {...styles.wrapContainer}>
          <view {...styles.tag}>
            <label value="Layout" font={testFont(10)} color="#1565C0" />
          </view>
          <view {...styles.tag}>
            <label value="Yoga" font={testFont(10)} color="#1565C0" />
          </view>
          <view {...styles.tag}>
            <label value="Custom Measure" font={testFont(10)} color="#1565C0" />
          </view>
          <view {...styles.tag}>
            <label value="FlexWrap" font={testFont(10)} color="#1565C0" />
          </view>
          <view {...styles.tag}>
            <label value="Snapshot" font={testFont(10)} color="#1565C0" />
          </view>
          <view {...styles.tag}>
            <label value="Test" font={testFont(10)} color="#1565C0" />
          </view>
        </layout>
      </view>

      {/* Flex-shrink with text: labels should shrink when container is constrained */}
      <view {...styles.section}>
        <layout {...styles.shrinkRow}>
          <view {...styles.shrinkLabel}>
            <label value="This label should shrink" font={testFont(10)} color="#C62828" />
          </view>
          <view {...styles.shrinkSpacer} />
        </layout>
      </view>

      {/* Nested measure: labels inside nested flex containers */}
      <view {...styles.section} flexGrow={1}>
        <layout flexDirection="row" flexGrow={1}>
          <layout flexDirection="column" flexGrow={1} marginRight={4}>
            <view backgroundColor="#4CAF50" borderRadius={4} padding={6} flexGrow={1}>
              <label value="Top" font={testBoldFont(12)} color="#FFFFFF" />
            </view>
            <layout height={4} />
            <view backgroundColor="#2196F3" borderRadius={4} padding={6} flexGrow={2}>
              <label value="Bottom (2x)" font={testBoldFont(12)} color="#FFFFFF" />
            </view>
          </layout>
          <view backgroundColor="#FF9800" borderRadius={4} padding={6} flexGrow={1}>
            <label value="Right" font={testBoldFont(14)} color="#FFFFFF" />
          </view>
        </layout>
      </view>
    </view>;
  }
}
