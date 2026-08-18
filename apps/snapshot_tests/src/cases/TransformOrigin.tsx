import { Component } from 'valdi_core/src/Component';

// Isolates transformOrigin: three identical 45-degree rotations that differ only in their pivot
// (center vs. a corner vs. a percent/length point), so the snapshot captures where each pivots.
const FORTY_FIVE_DEG = Math.PI / 4;

const anchor = {
  position: 'absolute' as const,
  width: 70,
  height: 70,
  borderWidth: 1,
  borderColor: '#CCCCCC',
};
const box = { position: 'absolute' as const, width: 70, height: 70 };

export class TransformOrigin extends Component {
  onRender(): void {
    <view width="100%" height="100%" backgroundColor="#FFFFFF">
      {/* pivot: center (default) */}
      <view {...anchor} top={20} left={20} />
      <view {...box} top={20} left={20} backgroundColor="#F9A825" rotation={FORTY_FIVE_DEG} transformOrigin="center" />

      {/* pivot: top-left corner */}
      <view {...anchor} top={20} left={110} />
      <view {...box} top={20} left={110} backgroundColor="#2E7D32" rotation={FORTY_FIVE_DEG} transformOrigin="top left" />

      {/* pivot: 25% 75% of the box */}
      <view {...anchor} top={120} left={65} />
      <view {...box} top={120} left={65} backgroundColor="#00838F" rotation={FORTY_FIVE_DEG} transformOrigin="25% 75%" />
    </view>;
  }
}
