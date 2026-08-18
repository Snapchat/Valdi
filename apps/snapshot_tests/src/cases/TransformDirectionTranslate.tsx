import { Component } from 'valdi_core/src/Component';

// Exercises how translationX mirrors under rtl. The same translationX (absolute and percent) is applied to a
// box in an ltr band and in rtl bands; under rtl the resolved translationX is negated, so the box shifts the
// opposite way. Each band sets its own `direction` and sizes itself so the absolute boxes have a container.
// Faint outlines mark the untranslated position.
const band = { width: '100%' as const, height: 80 };
const anchor = {
  position: 'absolute' as const,
  top: 10,
  left: 70,
  width: 60,
  height: 60,
  borderWidth: 1,
  borderColor: '#CCCCCC',
};
const box = {
  position: 'absolute' as const,
  top: 10,
  left: 70,
  width: 60,
  height: 60,
  borderRadius: 6,
};

export class TransformDirectionTranslate extends Component {
  onRender(): void {
    <view width="100%" height="100%" backgroundColor="#FFFFFF" flexDirection="column">
      {/* ltr: translationX=40 shifts right */}
      <view {...band} direction="ltr">
        <view {...anchor} />
        <view {...box} backgroundColor="#E53935" translationX={40} />
      </view>

      {/* rtl: same translationX=40 is mirrored, shifts left */}
      <view {...band} direction="rtl">
        <view {...anchor} />
        <view {...box} backgroundColor="#1E88E5" translationX={40} />
      </view>

      {/* rtl percent: translationX="50%" of 60 = 30, mirrored to shift left */}
      <view {...band} direction="rtl">
        <view {...anchor} />
        <view {...box} backgroundColor="#8E24AA" translationX="50%" />
      </view>
    </view>;
  }
}
