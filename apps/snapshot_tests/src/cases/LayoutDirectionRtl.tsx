import { Component } from 'valdi_core/src/Component';

// Exercises how the `direction` attribute changes layout: the same row markup laid out ltr vs rtl. Under rtl
// the flex row packs from the right and `marginStart` applies on the right, so the child order visually
// mirrors. The two rows are otherwise identical.
const row = {
  flexDirection: 'row' as const,
  height: 40,
  marginBottom: 16,
};
const cell = { width: 40, height: 40, borderRadius: 4 };
const spaced = { ...cell, marginStart: 8 };

export class LayoutDirectionRtl extends Component {
  onRender(): void {
    <view width="100%" height="100%" backgroundColor="#FFFFFF" padding={12} flexDirection="column">
      <layout {...row} direction="ltr">
        <view {...cell} backgroundColor="#E53935" />
        <view {...spaced} backgroundColor="#43A047" />
        <view {...spaced} backgroundColor="#1E88E5" />
      </layout>

      <layout {...row} direction="rtl">
        <view {...cell} backgroundColor="#E53935" />
        <view {...spaced} backgroundColor="#43A047" />
        <view {...spaced} backgroundColor="#1E88E5" />
      </layout>
    </view>;
  }
}
