import { Component } from 'valdi_core/src/Component';
import { testFont } from './TestFont';

const styles = {
  root: {
    width: '100%' as const,
    height: '100%' as const,
    backgroundColor: '#FFFFFF',
    flexDirection: 'column' as const,
    padding: 8,
  },
  bar: {
    flexDirection: 'row' as const,
    backgroundColor: '#F0F0F0',
    marginBottom: 8,
  },
  tab: {
    paddingTop: 6,
    paddingBottom: 6,
    paddingLeft: 8,
    paddingRight: 8,
    alignItems: 'center' as const,
  },
};

const labels = ['Chat', 'Stories', 'Map'];

function renderTabs(selected: number, equalWidth: boolean): void {
  labels.forEach((text, index) => {
    const isSelected = index === selected;
    <view
      {...styles.tab}
      flexGrow={equalWidth ? 1 : 0}
      flexShrink={equalWidth ? 1 : 0}
      flexBasis={equalWidth ? 0 : undefined}
    >
      <label value={text} font={testFont(11)} color={isSelected ? '#0000FF' : '#444444'} />
      <view height={2} width="100%" marginTop={4} backgroundColor={isSelected ? '#0000FF' : '#F0F0F0'} />
    </view>;
  });
}

export class TabRow extends Component {
  onRender(): void {
    <view {...styles.root}>
      <label value="tab row tests" font={testFont(10)} color="#999999" />
      <layout height={6} />

      <label value="Equal-width tabs fill the row:" font={testFont(9)} color="#666666" />
      <view {...styles.bar}>{renderTabs(0, true)}</view>

      <label value="Content-width tabs:" font={testFont(9)} color="#666666" />
      <view {...styles.bar}>{renderTabs(1, false)}</view>

      <label value="Tabs in a horizontal scroll:" font={testFont(9)} color="#666666" />
      <scroll horizontal={true} height={30} backgroundColor="#F0F0F0">
        {renderTabs(2, false)}
        {renderTabs(-1, false)}
      </scroll>
    </view>;
  }
}
