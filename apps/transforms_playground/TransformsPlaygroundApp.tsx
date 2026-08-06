import { StatefulComponent } from 'valdi_core/src/Component';
import { Style } from 'valdi_core/src/Style';
import { systemFont } from 'valdi_core/src/SystemFont';
import { Label, ScrollView, View } from 'valdi_tsx/src/NativeTemplateElements';

// Valdi PR #124 widens translationX/Y to accept strings and adds
// transformOrigin / transform. The type declarations for those props
// haven't reached checkpoint yet, so cast through this helper. Delete
// the helper (and its call sites) once the import PR lands.
function futureTransforms(props: {
  translationX?: number | string;
  translationY?: number | string;
  transformOrigin?: string;
  transform?: string;
}): any {
  return props;
}

interface State {
  percentIndex: number;
  transformIndex: number;
  isRtl: boolean;
}

const percentSteps: Array<{ x: number | string; y: number | string; label: string }> = [
  { x: 0, y: 0, label: 'translationX = 0, translationY = 0' },
  { x: '50%', y: 0, label: "translationX = '50%'" },
  { x: '50%', y: '50%', label: "translationX = '50%', translationY = '50%'" },
  { x: '-50%', y: '50%', label: "translationX = '-50%', translationY = '50%'" },
];

const transformStrings: string[] = [
  'rotate(20deg)',
  'translate(20px, -10px) scale(1.4)',
  'scale(1.5) rotate(-45deg)',
  'translateX(30%) rotate(90deg)',
];

/**
 * Standalone playground for the transform attributes introduced in
 * `TransformAttributes.hpp`: percent-based translations, `transformOrigin`, and
 * CSS-like composed `transform` strings, plus the RTL flip on horizontal
 * percent values. Every interactive box uses `setStateAnimated` so the runtime
 * exercises attribute updates, not just first-render resolution.
 */
export class App extends StatefulComponent<{}, State> {
  state: State = {
    percentIndex: 0,
    transformIndex: 0,
    isRtl: false,
  };

  onRender(): void {
    const percent = percentSteps[this.state.percentIndex]!;
    const transformString = transformStrings[this.state.transformIndex]!;

    <view backgroundColor='white' width='100%' height='100%'>
      <scroll style={styles.scroll} padding={16}>
        <label style={styles.title} value='Valdi Transforms Playground' font={systemFont(20)} />

        <label style={styles.section} value='Percent translation' font={systemFont(16)} />
        <label style={styles.caption} value='Tap the box to cycle. Percents resolve against the moving view.' />
        <label style={styles.value} value={percent.label} />
        <view style={styles.stage} onTap={this.cyclePercent}>
          <view style={styles.percentBox} {...futureTransforms({ translationX: percent.x, translationY: percent.y })} />
        </view>

        <label style={styles.section} value='transformOrigin' font={systemFont(16)} />
        <label style={styles.caption} value='Same rotation (0.5 rad) applied at four different origins.' />
        <label style={styles.rowLabel} value="transformOrigin = 'center'" />
        <view style={styles.originRow}>
          <view style={styles.originBoxPrimary} rotation={0.5} {...futureTransforms({ transformOrigin: 'center' })} />
        </view>
        <label style={styles.rowLabel} value="transformOrigin = 'top left'" />
        <view style={styles.originRow}>
          <view style={styles.originBoxSecondary} rotation={0.5} {...futureTransforms({ transformOrigin: 'top left' })} />
        </view>
        <label style={styles.rowLabel} value="transformOrigin = 'right bottom'" />
        <view style={styles.originRow}>
          <view style={styles.originBoxTertiary} rotation={0.5} {...futureTransforms({ transformOrigin: 'right bottom' })} />
        </view>
        <label style={styles.rowLabel} value="transformOrigin = '25% 75%'" />
        <view style={styles.originRow}>
          <view style={styles.originBoxQuaternary} rotation={0.5} {...futureTransforms({ transformOrigin: '25% 75%' })} />
        </view>

        <label style={styles.section} value='LTR vs RTL (horizontal percent flips)' font={systemFont(16)} />
        <label style={styles.caption} value="Both boxes below apply translationX='50%'. RTL flips the resolved value." />
        <label style={styles.rowLabel} value="direction = 'ltr'" />
        <view style={styles.rtlStage} direction='ltr'>
          <view style={styles.rtlBox} {...futureTransforms({ translationX: '50%' })} />
        </view>
        <label style={styles.rowLabel} value="direction = 'rtl'" />
        <view style={styles.rtlStage} direction='rtl'>
          <view style={styles.rtlBox} {...futureTransforms({ translationX: '50%' })} />
        </view>
        <label style={styles.value} value={`Tap box below to toggle. direction = '${this.state.isRtl ? 'rtl' : 'ltr'}'`} />
        <view
          style={styles.rtlToggleStage}
          direction={this.state.isRtl ? 'rtl' : 'ltr'}
          onTap={this.toggleDirection}
        >
          <view style={styles.rtlToggleBox} {...futureTransforms({ translationX: '50%' })} />
        </view>

        <label style={styles.section} value='CSS transform strings' font={systemFont(16)} />
        <label style={styles.caption} value='Tap the box to cycle through composed transforms.' />
        <label style={styles.value} value={`transform = '${transformString}'`} />
        <view style={styles.transformStage} onTap={this.cycleTransform}>
          <view style={styles.transformBox} {...futureTransforms({ transform: transformString })} />
        </view>
      </scroll>
    </view>;
  }

  private cyclePercent = (): void => {
    this.setStateAnimated({ percentIndex: (this.state.percentIndex + 1) % percentSteps.length }, { duration: 0.35 });
  };

  private cycleTransform = (): void => {
    this.setStateAnimated(
      { transformIndex: (this.state.transformIndex + 1) % transformStrings.length },
      { duration: 0.35 },
    );
  };

  private toggleDirection = (): void => {
    this.setStateAnimated({ isRtl: !this.state.isRtl }, { duration: 0.35 });
  };
}

const styles = {
  scroll: new Style<ScrollView>({
    height: '100%',
  }),
  title: new Style<Label>({
    color: 'black',
    marginTop: 40,
    marginBottom: 16,
    accessibilityCategory: 'header',
  }),
  section: new Style<Label>({ color: 'black', marginTop: 20, marginBottom: 4 }),
  caption: new Style<Label>({ color: '#606060' }),
  value: new Style<Label>({ color: 'black', marginTop: 6, marginBottom: 6 }),
  rowLabel: new Style<Label>({ color: '#404040', marginTop: 10, marginBottom: 4 }),

  stage: new Style<View>({
    height: 140,
    backgroundColor: '#f0f0f0',
    padding: 10,
    marginTop: 8,
    marginBottom: 8,
  }),
  percentBox: new Style<View>({
    width: 60,
    height: 60,
    borderRadius: 8,
    backgroundColor: '#4a90e2',
  }),

  originRow: new Style<View>({
    width: '100%',
    height: 90,
    backgroundColor: '#f0f0f0',
    padding: 20,
    alignItems: 'center',
    justifyContent: 'center',
  }),
  originBoxPrimary: new Style<View>({ width: 50, height: 50, backgroundColor: '#4a90e2' }),
  originBoxSecondary: new Style<View>({ width: 50, height: 50, backgroundColor: '#e2884a' }),
  originBoxTertiary: new Style<View>({ width: 50, height: 50, backgroundColor: '#5ac878' }),
  originBoxQuaternary: new Style<View>({ width: 50, height: 50, backgroundColor: '#c85ac8' }),

  rtlStage: new Style<View>({
    width: '100%',
    height: 80,
    backgroundColor: '#f0f0f0',
    padding: 10,
  }),
  rtlBox: new Style<View>({
    width: 40,
    height: 40,
    borderRadius: 6,
    backgroundColor: '#4a90e2',
  }),

  rtlToggleStage: new Style<View>({
    height: 100,
    backgroundColor: '#f0f0f0',
    padding: 10,
    marginBottom: 8,
  }),
  rtlToggleBox: new Style<View>({
    width: 60,
    height: 60,
    borderRadius: 8,
    backgroundColor: '#5ac878',
  }),

  transformStage: new Style<View>({
    height: 140,
    backgroundColor: '#f0f0f0',
    padding: 10,
    marginTop: 8,
    marginBottom: 8,
    alignItems: 'center',
    justifyContent: 'center',
  }),
  transformBox: new Style<View>({
    width: 60,
    height: 60,
    borderRadius: 8,
    backgroundColor: '#4a90e2',
  }),
};
