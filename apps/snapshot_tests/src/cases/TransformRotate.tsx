import { Component } from 'valdi_core/src/Component';

// Exercises the rotation attribute (radians) and how transformOrigin moves the pivot. Both boxes rotate
// by the same 30 degrees; the second pivots about its top-left corner rather than its center.
const HALF_PI = Math.PI / 2;
const THIRTY_DEG = HALF_PI / 3;

const anchor = {
  position: 'absolute' as const,
  width: 70,
  height: 70,
  borderWidth: 1,
  borderColor: '#CCCCCC',
};
const box = { position: 'absolute' as const, width: 70, height: 70 };

export class TransformRotate extends Component {
  onRender(): void {
    <view width="100%" height="100%" backgroundColor="#FFFFFF">
      {/* 30deg about center */}
      <view {...anchor} top={20} left={30} />
      <view {...box} top={20} left={30} backgroundColor="#00897B" rotation={THIRTY_DEG} />

      {/* 30deg about top-left corner */}
      <view {...anchor} top={110} left={30} />
      <view {...box} top={110} left={30} backgroundColor="#D81B60" rotation={THIRTY_DEG} transformOrigin="top left" />
    </view>;
  }
}
