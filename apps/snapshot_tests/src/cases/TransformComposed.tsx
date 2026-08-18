import { Component } from 'valdi_core/src/Component';

// Exercises the CSS-style composed `transform` string: chained translate/rotate/scale, unit-bearing and
// percent lengths, and degree angles. This is the attribute that overrides the individual transform props.
const anchor = {
  position: 'absolute' as const,
  width: 60,
  height: 60,
  borderWidth: 1,
  borderColor: '#CCCCCC',
};
const box = { position: 'absolute' as const, width: 60, height: 60 };

export class TransformComposed extends Component {
  onRender(): void {
    <view width="100%" height="100%" backgroundColor="#FFFFFF">
      {/* translate + rotate + uniform scale composed in one string */}
      <view {...anchor} top={20} left={20} />
      <view {...box} top={20} left={20} backgroundColor="#C2185B" transform="translate(30px, 10px) rotate(20deg) scale(1.2)" />

      {/* percent translateX + negative rotation */}
      <view {...anchor} top={120} left={20} />
      <view {...box} top={120} left={20} backgroundColor="#303F9F" transform="translateX(50%) rotate(-15deg)" />
    </view>;
  }
}
