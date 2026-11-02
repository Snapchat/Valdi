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
  sectionLabel: { color: '#666666' },
  section: { marginBottom: 6 },
  row: {
    flexDirection: 'row' as const,
    backgroundColor: '#F5F5F5',
    borderRadius: 4,
    padding: 4,
  },
  box: { width: 30, borderRadius: 4 },
  tallBox: { width: 30, height: 50, borderRadius: 4 },
};

export class AlignSelfOverrides extends Component {
  onRender(): void {
    <view {...styles.root}>
      <label value="parent: stretch (default)" font={testFont(9)} {...styles.sectionLabel} />
      <layout {...styles.row} {...styles.section} height={50} alignItems="stretch">
        <view {...styles.box} backgroundColor="#2196F3" marginRight={4} />
        <view {...styles.box} backgroundColor="#4CAF50" marginRight={4} alignSelf="flex-start" />
        <view {...styles.box} backgroundColor="#FF9800" marginRight={4} alignSelf="center" />
        <view {...styles.box} backgroundColor="#E91E63" alignSelf="flex-end" />
      </layout>

      <label value="parent: center, child: stretch" font={testFont(9)} {...styles.sectionLabel} />
      <layout {...styles.row} {...styles.section} height={50} alignItems="center">
        <view {...styles.box} height={20} backgroundColor="#9C27B0" marginRight={4} />
        <view {...styles.box} backgroundColor="#00BCD4" marginRight={4} alignSelf="stretch" />
        <view {...styles.box} height={20} backgroundColor="#FF5722" marginRight={4} />
        <view {...styles.box} height={20} backgroundColor="#795548" alignSelf="flex-end" />
      </layout>

      <label value="column + alignSelf" font={testFont(9)} {...styles.sectionLabel} />
      <view
        flexDirection="column"
        backgroundColor="#F5F5F5"
        borderRadius={4}
        padding={4}
        {...styles.section}
        height={70}
      >
        <view
          height={15}
          backgroundColor="#3F51B5"
          borderRadius={4}
          marginBottom={4}
          alignSelf="flex-start"
          width={60}
        />
        <view height={15} backgroundColor="#009688" borderRadius={4} marginBottom={4} alignSelf="center" width={80} />
        <view height={15} backgroundColor="#FFC107" borderRadius={4} alignSelf="flex-end" width={50} />
      </view>

      <label value="mixed sizes + align" font={testFont(9)} {...styles.sectionLabel} />
      <layout {...styles.row} flexGrow={1} alignItems="center">
        <view
          width={20}
          height={20}
          backgroundColor="#F44336"
          borderRadius={10}
          marginRight={4}
          alignSelf="flex-start"
        />
        <view flexGrow={1} height={30} backgroundColor="#E8F5E9" borderRadius={4} marginRight={4}>
          <label value="grows" font={testFont(8)} color="#333333" />
        </view>
        <view {...styles.tallBox} backgroundColor="#8BC34A" marginRight={4} alignSelf="stretch" />
        <view width={20} height={20} backgroundColor="#673AB7" borderRadius={10} alignSelf="flex-end" />
      </layout>
    </view>;
  }
}
