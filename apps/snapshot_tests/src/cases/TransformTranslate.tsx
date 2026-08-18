import { Component } from 'valdi_core/src/Component';

// Exercises the translationX / translationY transform attributes, both as absolute points and as
// percent-of-own-size strings. Each colored box is drawn over a faint outline marking its
// untranslated layout position so the translation is visible in the snapshot.
const anchor = {
  position: 'absolute' as const,
  width: 60,
  height: 60,
  borderWidth: 1,
  borderColor: '#CCCCCC',
};
const box = { position: 'absolute' as const, width: 60, height: 60, borderRadius: 6 };

export class TransformTranslate extends Component {
  onRender(): void {
    <view width="100%" height="100%" backgroundColor="#FFFFFF">
      {/* absolute points: shift right 40, down 20 */}
      <view {...anchor} top={20} left={20} />
      <view {...box} top={20} left={20} backgroundColor="#E53935" translationX={40} translationY={20} />

      {/* percent of own 60x60: 50% -> 30 right, -25% -> 15 up */}
      <view {...anchor} top={120} left={30} />
      <view {...box} top={120} left={30} backgroundColor="#1E88E5" translationX="50%" translationY="-25%" />
    </view>;
  }
}
