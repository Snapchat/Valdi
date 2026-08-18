import { Component } from 'valdi_core/src/Component';

// Exercises scaleX / scaleY, including a negative scale (flip) and the interaction with transformOrigin.
// Faint outlines mark the unscaled layout rect so the scale direction and pivot are visible.
const anchor = {
  position: 'absolute' as const,
  width: 60,
  height: 60,
  borderWidth: 1,
  borderColor: '#CCCCCC',
};
const box = { position: 'absolute' as const, width: 60, height: 60 };

export class TransformScale extends Component {
  onRender(): void {
    <view width="100%" height="100%" backgroundColor="#FFFFFF">
      {/* uniform 1.5x about the default center origin */}
      <view {...anchor} top={30} left={30} />
      <view {...box} top={30} left={30} backgroundColor="#43A047" scaleX={1.5} scaleY={1.5} />

      {/* 1.5x anchored at top-left: grows down and to the right only */}
      <view {...anchor} top={30} left={120} />
      <view {...box} top={30} left={120} backgroundColor="#FB8C00" scaleX={1.5} scaleY={1.5} transformOrigin="top left" />

      {/* vertical flip about center */}
      <view {...anchor} top={130} left={30} />
      <view {...box} top={130} left={30} backgroundColor="#8E24AA" scaleY={-1} />
    </view>;
  }
}
