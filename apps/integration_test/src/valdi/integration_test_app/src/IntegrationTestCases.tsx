import { FontManager } from 'drawing/src/FontManager';
import { Base64 } from 'coreutils/src/Base64';
import { GeometricPathBuilder, GeometricPathScaleType } from 'valdi_core/src/GeometricPath';
import { Component, StatefulComponent } from 'valdi_core/src/Component';
import { Style } from 'valdi_core/src/Style';
import type { Asset } from 'valdi_core/src/Asset';
import { ValdiRuntime } from 'valdi_core/src/ValdiRuntime';
import { AttributedTextBuilder } from 'valdi_core/src/utils/AttributedTextBuilder';
import { ImageFilters } from 'valdi_core/src/utils/ImageFilter';
import { AttributedTextInlineViewVerticalAlignment } from 'valdi_tsx/src/AttributedTextInlineViewAttachment';
import type { ViewFactory } from 'valdi_tsx/src/ViewFactory';
import { Worker } from 'worker/src/Worker';
import type {
  AnimatedImage,
  BlurView,
  ImageView,
  IWebViewNativeController,
  Label,
  Layout,
  ShapeView,
  SpinnerView,
  TextField,
  TextView,
  View,
  WebViewElement,
} from 'valdi_tsx/src/NativeTemplateElements';

import res from '../res';
import { createIntegrationViewFactory } from './FactoryIntegrationHost';
import {
  getPlatform,
  submitTouchSequence,
  focusTextInput,
  pressReturn,
  replaceText,
  pressBackspace,
} from './IntegrationTestHost';
import {
  IntegrationTestAttributeCoverage,
  IntegrationTestCase,
  IntegrationTestCoverageKind,
  IntegrationTestInteractionContext,
  IntegrationTestRenderContext,
  NativeTemplateElementName,
} from './IntegrationTestTypes';

declare const runtime: ValdiRuntime;

const WIDTH = 360;
const HEIGHT = 560;
const CARD = '#F8FAFC';
const ANIMATED_IMAGE_PNG_DATA_URL =
  'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAHgAAABQCAYAAADSm7GJAAABiUlEQVR4nO3TsU0FMRQF0W2BjA5ICciohFZokISelgqQ/n7be98bz5UmXa985OP8fD936uPna6uO9IULLLDAAgsssMB7lL5wgQUWWGCBBRZ4jwSGJzA8geEJDE9geALDExiewPDQwC+vbw+VRhB4Aeou2BjgUVgqdHvg2bA06NbAq3EJyC2B74IlQLcDTuF2RW4FnMbtiCywwDVKo3ZFbgE8ivHfdkDGAl+dwGBcMjIOeHQCg3GJyBjg2RO4EPCqCVwAtwJwZWSBBa4NvHoCCyywwEHg4/s8K9YN+Pw9SiawwAILHAReiXzpHwpgCixwrboApxHxwCuQCa8XBTwT+fK5BSBbAieQSbhI4BHkp84qgNga+C5kIi4a+BHs4e8WAEQAz0CeXRoOB1wJOY0msMA9gSsgp8HwwEnkNNY2wHdDp5G2Bb4DOQ20PfAq6DSMwIug0yACL8BOIwg8sfSFCyywwAILLLDAeyQwPIHhCQxPYHgCwxMYnsDwBIYnMDyB4QkMT2B4AsP7AzsXR9oA2ei4AAAAAElFTkSuQmCC';

const INLINE_IMAGE_BYTES = pngDataUrlBytes(ANIMATED_IMAGE_PNG_DATA_URL);

let integrationViewFactory: ViewFactory | undefined;

function pngDataUrlBytes(dataUrl: string): Uint8Array {
  return Base64.toByteArray(dataUrl.substring(dataUrl.indexOf(',') + 1));
}

function coverage(
  kind: IntegrationTestCoverageKind,
  attributes: readonly string[],
): readonly IntegrationTestAttributeCoverage[] {
  return [{ kind, attributes }];
}

function configureIntegrationColorPalettes(): void {
  // Exercise the deprecated single-palette API before configuring the named palettes used by this case.
  runtime.setColorPalette({
    background: '#000000',
    foreground: '#FFFFFF',
    accent: '#808080',
  });
  runtime.configureColorPalette('integration-light', {
    background: '#DBEAFE',
    foreground: '#1D4ED8',
    accent: '#047857',
  });
  runtime.configureColorPalette('integration-dark', {
    background: '#111827',
    foreground: '#FBBF24',
    accent: '#F97316',
  });
  runtime.setActiveColorPalette('integration-light');
}

function lottieResourceSource(ctx: IntegrationTestRenderContext): Asset | string {
  try {
    const bytes = runtime.getModuleEntry('integration_test_app', 'res/animation.json', false) as Uint8Array;
    ctx.record(`lottie resource bytes:${bytes.length}`);
    return runtime.makeAssetFromBytes(bytes);
  } catch (error: any) {
    ctx.record(`lottie resource unavailable:${error?.message ?? String(error)}`);
    return 'integration-test-missing-lottie.json';
  }
}

const styleLayoutBase = new Style<Layout>({
  padding: 12,
  width: '100%',
});

const styleViewCard = new Style<View>({
  backgroundColor: '#DBEAFE',
  border: '2 solid #2563EB',
  borderRadius: 12,
  height: 92,
  width: '100%',
});

const styleLabelSample = new Style<Label>({
  color: '#1D4ED8',
  font: 'system-bold 22',
  width: '100%',
});

const styleTextFieldSample = new Style<TextField>({
  backgroundColor: '#FFFFFF',
  border: '2 solid #0EA5E9',
  borderRadius: 10,
  color: '#0F172A',
  font: 'system 17',
  height: 48,
  width: '100%',
});

const styleTextViewSample = new Style<TextView>({
  backgroundColor: '#FFFFFF',
  border: '2 solid #7C3AED',
  borderRadius: 12,
  color: '#312E81',
  font: 'system 16',
  height: 110,
  width: '100%',
});

const styleImageSample = new Style<ImageView>({
  border: '2 solid #0284C7',
  height: 120,
  objectFit: 'contain',
  width: 160,
});

const styleWebViewSample = new Style<WebViewElement>({
  backgroundColor: '#E0F2FE',
  border: '2 solid #0284C7',
  borderRadius: 12,
  height: 150,
  width: '100%',
});

const styleBlurSample = new Style<BlurView>({
  borderRadius: 10,
  height: 76,
  width: 132,
});

const styleSpinnerSample = new Style<SpinnerView>({
  color: '#2563EB',
  height: 72,
  width: 72,
});

const styleShapeSample = new Style<ShapeView>({
  fillColor: '#BFDBFE',
  strokeColor: '#1D4ED8',
  strokeWidth: 8,
  height: 130,
  width: 130,
});

const styleAnimatedImageSample = new Style<AnimatedImage>({
  backgroundColor: '#F5F3FF',
  border: '2 solid #7C3AED',
  borderRadius: 12,
  height: 130,
  objectFit: 'contain',
  width: 130,
});

function attributedLabelFontColorValue() {
  return new AttributedTextBuilder()
    .append('plain ', { color: '#111827', font: 'system 18' })
    .append('bold red ', { color: '#DC2626', font: 'system-bold 22' })
    .append('blue italic', { color: '#1D4ED8', font: 'system-italic 20' })
    .build();
}

function attributedLabelBackgroundDecorationValue() {
  return new AttributedTextBuilder()
    .append('badge ', {
      backgroundBorderRadius: 7,
      backgroundColor: '#DBEAFE',
      backgroundPadding: { left: 6, top: 3, right: 6, bottom: 3 },
      color: '#1D4ED8',
      font: 'system-bold 18',
    })
    .append('under ', { color: '#047857', font: 'system 18', textDecoration: 'underline' })
    .append('dots', { color: '#7C2D12', font: 'system 18', textDecoration: 'dotted-underline' })
    .build();
}

function attributedLabelOutlineLayoutValue(ctx: IntegrationTestRenderContext) {
  return new AttributedTextBuilder()
    .append('outline', {
      color: '#7C3AED',
      font: 'system-bold 18',
      onLayout: (x, y, width, height) =>
        ctx.record(
          `attributed label layout:${Math.round(x)},${Math.round(y)},${Math.round(width)}x${Math.round(height)}`,
        ),
      outlineColor: '#FBBF24',
      outlineWidth: 1,
    })
    .build();
}

function attributedInlineImageValue() {
  return new AttributedTextBuilder()
    .append('before ', { color: '#111827', font: 'system 19' })
    .appendInlineImage({
      attachmentId: 'red-dot',
      width: 22,
      height: 22,
      imageData: INLINE_IMAGE_BYTES,
    })
    .append(' after inline image', { color: '#1D4ED8', font: 'system-bold 19' })
    .build();
}

function attributedInlineViewAlignmentValue() {
  return new AttributedTextBuilder()
    .append('Top ', { color: '#0F172A', font: 'system 23' })
    .appendInlineView(0, AttributedTextInlineViewVerticalAlignment.Top)
    .append('  Center ', { color: '#0F172A', font: 'system 23' })
    .appendInlineView(1, AttributedTextInlineViewVerticalAlignment.Center)
    .append('  Bottom ', { color: '#0F172A', font: 'system 23' })
    .appendInlineView(2, AttributedTextInlineViewVerticalAlignment.Bottom)
    .append('  Baseline ', { color: '#0F172A', font: 'system 23' })
    .appendInlineView(3, AttributedTextInlineViewVerticalAlignment.Baseline)
    .append(' inside one wrapped text run.', { color: '#0F172A', font: 'system 23' })
    .build();
}

function attributedInlineViewLtrValue() {
  return new AttributedTextBuilder()
    .append('Text before ', { color: '#0F172A', font: 'system 22' })
    .appendInlineView(0, AttributedTextInlineViewVerticalAlignment.Center)
    .build();
}

function attributedInlineViewRtlValue() {
  return new AttributedTextBuilder()
    .append('אבג ', { color: '#0F172A', font: 'system 22' })
    .appendInlineView(0, AttributedTextInlineViewVerticalAlignment.Center)
    .build();
}

function attributedCitationPillValue() {
  return new AttributedTextBuilder()
    .append('Source ', { color: '#0F172A', font: 'system 17' })
    .appendInlineView(0, AttributedTextInlineViewVerticalAlignment.Center)
    .append(' aligned.', { color: '#0F172A', font: 'system 17' })
    .build();
}

function attributedInlineViewExpandingButtonValue(kind: 'label' | 'textview') {
  return new AttributedTextBuilder()
    .append(`${kind === 'label' ? 'Label' : 'Textview'} inline child: `, {
      color: '#0F172A',
      font: 'system 20',
    })
    .appendInlineView(0, AttributedTextInlineViewVerticalAlignment.Center)
    .append(' after child text wraps when the child resizes.', { color: '#0F172A', font: 'system 20' })
    .build();
}

function attributedTapValue(ctx: IntegrationTestRenderContext) {
  return new AttributedTextBuilder()
    .append('tap me', {
      color: '#7C3AED',
      font: 'system-bold 28',
      onTap: () => ctx.record('attributed span tap'),
      textDecoration: 'underline',
    })
    .build();
}

function attributedMultilineValue() {
  return new AttributedTextBuilder()
    .append('Large red words wrap ', { color: '#DC2626', font: 'system-bold 22' })
    .append('into smaller blue words with a highlighted background ', {
      backgroundBorderRadius: 6,
      backgroundColor: '#DBEAFE',
      backgroundPadding: 3,
      color: '#1D4ED8',
      font: 'system 16',
    })
    .append('and then truncate.', { color: '#047857', font: 'system-bold 18', textDecoration: 'dotted-underline' })
    .build();
}

function defaultLottieFontProvider(ctx: IntegrationTestRenderContext) {
  try {
    ctx.record('animatedimage lottie fontProvider created from drawing FontManager');
    return FontManager.getDefault().fontProvider;
  } catch (error: any) {
    ctx.record(`animatedimage lottie fontProvider unavailable:${error?.message ?? String(error)}`);
    return undefined;
  }
}

function attributedTextViewOutlineBackgroundValue() {
  return new AttributedTextBuilder()
    .append('Outer outline ', {
      color: '#111827',
      font: 'system-bold 22',
      outerOutlineColor: '#F97316',
      outerOutlineWidth: 3,
    })
    .append('background span ', {
      backgroundBorderRadius: 8,
      backgroundColor: '#FDE68A',
      backgroundPadding: 4,
      color: '#7C2D12',
      font: 'system 18',
    })
    .build();
}

function attributedTextViewDecorationAnimationValue(ctx: IntegrationTestRenderContext) {
  return new AttributedTextBuilder()
    .append('animated part', {
      animationTransform: {
        duration: 0.25,
        key: 'textview-attributed',
        opacity: 0.4,
        scale: 0.92,
        timeOffsetBetweenParts: 0.02,
        translationY: 6,
      },
      color: '#1D4ED8',
      font: 'system-bold 18',
      onLayout: (x, y, width, height) =>
        ctx.record(
          `attributed textview layout:${Math.round(x)},${Math.round(y)},${Math.round(width)}x${Math.round(height)}`,
        ),
      textDecoration: 'dotted-underline',
    })
    .build();
}

function summarizeEvent(event: any): string {
  if (!event) {
    return 'none';
  }

  const parts: string[] = [];
  for (const key of [
    'state',
    'x',
    'y',
    'absoluteX',
    'absoluteY',
    'translationX',
    'translationY',
    'scale',
    'rotation',
  ]) {
    const value = event[key];
    if (value !== undefined) {
      parts.push(`${key}=${typeof value === 'number' ? Math.round(value * 100) / 100 : value}`);
    }
  }
  return parts.join(',') || 'event';
}

function tapTargetSequence(kind: string = 'tap'): string {
  return JSON.stringify({
    kind,
    events: [
      { action: 'down', x: 0.5, y: 0.5, delayMs: 0 },
      { action: 'up', x: 0.5, y: 0.5, delayMs: 40 },
    ],
  });
}

function dragTargetSequence(): string {
  return JSON.stringify({
    kind: 'drag',
    events: [
      { action: 'down', x: 0.25, y: 0.5, delayMs: 0 },
      { action: 'move', x: 0.75, y: 0.55, delayMs: 80 },
      { action: 'up', x: 0.75, y: 0.55, delayMs: 40 },
    ],
  });
}

function doubleTapTargetSequence(): string {
  return JSON.stringify({
    kind: 'doubleTap',
    events: [
      { action: 'down', x: 0.5, y: 0.5, delayMs: 0 },
      { action: 'up', x: 0.5, y: 0.5, delayMs: 40 },
      { action: 'down', x: 0.5, y: 0.5, delayMs: 90 },
      { action: 'up', x: 0.5, y: 0.5, delayMs: 40 },
    ],
  });
}

function longPressTargetSequence(): string {
  return JSON.stringify({
    kind: 'longPress',
    events: [
      { action: 'down', x: 0.5, y: 0.5, delayMs: 0 },
      { action: 'up', x: 0.5, y: 0.5, delayMs: 420 },
    ],
  });
}

function recordTouchDispatch(context: IntegrationTestInteractionContext, sequenceJson: string): void {
  const node = context.targetRef.single()?.getNativeNode();
  if (!node) {
    context.record('native touch dispatch skipped: target node unavailable');
    return;
  }
  context.record(`native touch dispatch: ${submitTouchSequence(node, sequenceJson)}`);
}

async function interactTap(context: IntegrationTestInteractionContext): Promise<void> {
  recordTouchDispatch(context, tapTargetSequence());
  await context.waitForIdle();
}

async function interactDrag(context: IntegrationTestInteractionContext): Promise<void> {
  recordTouchDispatch(context, dragTargetSequence());
  await context.waitForIdle();
}

async function interactDoubleTap(context: IntegrationTestInteractionContext): Promise<void> {
  recordTouchDispatch(context, doubleTapTargetSequence());
  await context.waitForIdle();
}

async function interactLongPress(context: IntegrationTestInteractionContext): Promise<void> {
  recordTouchDispatch(context, longPressTargetSequence());
  await context.waitForIdle();
}

async function interactTextInput(context: IntegrationTestInteractionContext): Promise<void> {
  if (getPlatform() === 'ios') {
    context.record('native text input dispatch skipped: iOS simulator keyboard focus stalls this snapshot harness');
    await context.waitForIdle();
    return;
  }
  if (getPlatform() === 'android') {
    context.record('native text input dispatch skipped: Android text focus/selection can wedge this snapshot harness');
    await context.waitForIdle();
    return;
  }

  const node = context.targetRef.single()?.getNativeNode();
  if (!node) {
    context.record('native text input skipped: target node unavailable');
    return;
  }

  context.record(`native focus: ${focusTextInput(node)}`);
  context.record(`native replaceText: ${replaceText(node, 'typed by host')}`);
  context.record(`native return: ${pressReturn(node)}`);
  await context.waitForIdle();
}

async function interactTextInputWithBackspace(context: IntegrationTestInteractionContext): Promise<void> {
  if (getPlatform() === 'ios') {
    context.record('native text input dispatch skipped: iOS simulator keyboard focus stalls this snapshot harness');
    await context.waitForIdle();
    return;
  }
  if (getPlatform() === 'android') {
    context.record('native text input dispatch skipped: Android text focus/selection can wedge this snapshot harness');
    await context.waitForIdle();
    return;
  }

  const node = context.targetRef.single()?.getNativeNode();
  if (!node) {
    context.record('native text input skipped: target node unavailable');
    return;
  }

  context.record(`native focus: ${focusTextInput(node)}`);
  context.record(`native replaceText: ${replaceText(node, 'delete me')}`);
  context.record(`native backspace: ${pressBackspace(node)}`);
  await context.waitForIdle();
}

interface LifecycleRecreateFixtureViewModel {
  renderContext: IntegrationTestRenderContext;
}

interface LifecycleRecreateFixtureState {
  phase: number;
  show: boolean;
}

class LifecycleRecreateFixture extends StatefulComponent<
  LifecycleRecreateFixtureViewModel,
  LifecycleRecreateFixtureState
> {
  state: LifecycleRecreateFixtureState = { phase: 0, show: true };
  private scheduled = false;

  private scheduleRecreate(): void {
    if (this.scheduled) {
      return;
    }
    this.scheduled = true;
    const renderContext = this.viewModel.renderContext;
    this.setTimeoutDisposable(() => {
      renderContext.record('lifecycle fixture hiding child');
      this.setState({ show: false });
      this.setTimeoutDisposable(() => {
        renderContext.record('lifecycle fixture showing replacement child');
        this.setState({ phase: 1, show: true });
      }, 0);
    }, 0);
  }

  onRender(): void {
    this.scheduleRecreate();
    const phase = this.state?.phase ?? 0;
    const show = this.state?.show ?? true;
    <view height={170} width="100%">
      {show ? (
        <view
          key={`lifecycle-child-${phase}`}
          allowReuse={false}
          backgroundColor={phase === 0 ? '#0EA5E9' : '#22C55E'}
          borderRadius={18}
          height={150}
          onViewChange={nativeView =>
            this.viewModel.renderContext.record(`lifecycle:onViewChange:${nativeView ? 'created' : 'destroyed'}:${phase}`)
          }
          onViewCreate={() => this.viewModel.renderContext.record(`lifecycle:onViewCreate:${phase}`)}
          onViewDestroy={() => this.viewModel.renderContext.record(`lifecycle:onViewDestroy:${phase}`)}
          width="100%"
        >
          <label value={`lifecycle phase ${phase}`} color="#FFFFFF" font="system-bold 22" margin={24} />
        </view>
      ) : (
        <view backgroundColor="#E5E7EB" borderRadius={18} height={150} width="100%">
          <label value="child hidden" color="#334155" font="system-bold 22" margin={24} />
        </view>
      )}
    </view>;
  }
}

interface InlineViewMarkerViewModel {
  title: string;
  color: string;
}

class InlineViewMarker extends StatefulComponent<InlineViewMarkerViewModel, {}> {
  onRender(): void {
    <view
      alignItems="center"
      backgroundColor={this.viewModel.color}
      borderRadius={5}
      height={18}
      justifyContent="center"
      width={56}
    >
      <label color="#FFFFFF" font="system-bold 10" textAlign="center" value={this.viewModel.title} width="100%" />
    </view>;
  }
}

interface InlineViewEndMarkerViewModel {
  color: string;
}

class InlineViewEndMarker extends StatefulComponent<InlineViewEndMarkerViewModel, {}> {
  onRender(): void {
    <view
      alignItems="center"
      backgroundColor={this.viewModel.color}
      borderRadius={6}
      height={22}
      justifyContent="center"
      width={58}
    >
      <label color="#FFFFFF" font="system-bold 11" textAlign="center" value="END" width="100%" />
    </view>;
  }
}

interface CitationPillFixtureViewModel {
  domain: string;
  sourceCount?: number;
}

class CitationPillFixture extends Component<CitationPillFixtureViewModel> {
  onRender(): void {
    <view
      alignItems="center"
      backgroundColor="#E5E7EB"
      border="1 solid #CBD5E1"
      borderRadius="100%"
      flexDirection="row"
      gap={4}
      height={18}
      justifyContent="center"
      paddingLeft={6}
      paddingRight={6}
    >
      <view backgroundColor="#7C3AED" borderRadius="100%" height={12} width={12} />
      <label color="#4B5563" font="system 12 caption1" lineHeight={16 / 12} value={this.viewModel.domain} />
      {this.viewModel.sourceCount === undefined ? null : (
        <label
          color="#4B5563"
          font="system 12 caption1"
          lineHeight={16 / 12}
          value={`+${this.viewModel.sourceCount}`}
        />
      )}
    </view>;
  }
}

interface InlineViewExpandingRowViewModel {
  renderContext: IntegrationTestRenderContext;
  kind: 'label' | 'textview';
  interactive: boolean;
  title: string;
}

interface InlineViewExpandingRowState {
  expanded: boolean;
}

class InlineViewExpandingRow extends StatefulComponent<
  InlineViewExpandingRowViewModel,
  InlineViewExpandingRowState
> {
  state: InlineViewExpandingRowState = { expanded: false };
  private value: any = undefined;
  private scheduledExpansion = false;

  onCreate(): void {
    this.value = attributedInlineViewExpandingButtonValue(this.viewModel.kind);
  }

  onRender(): void {
    const expanded = this.state.expanded;
    if (this.viewModel.interactive && !this.scheduledExpansion) {
      this.scheduledExpansion = true;
      this.renderer.onLayoutComplete(() => {
        if (this.state.expanded) {
          return;
        }
        this.viewModel.renderContext.record('inline expanding button expanded after initial render');
        this.setState({ expanded: true });
      });
    }

    <view backgroundColor="#FFFFFF" border="1 solid #CBD5E1" borderRadius={10} padding={10} width="100%">
      <label color="#475569" font="system-bold 12" marginBottom={6} value={this.viewModel.title} width="100%" />
      {this.viewModel.kind === 'label' ? (
        <label
          color="#0F172A"
          font="system 20"
          lineHeight={1.35}
          numberOfLines={0}
          value={this.value}
          width="100%"
        >
          <view
            accessibilityCategory="button"
            alignItems="center"
            backgroundColor={expanded ? '#0F766E' : '#2563EB'}
            borderRadius={8}
            height={32}
            justifyContent="center"
            onTap={this.viewModel.interactive ? this.toggle : undefined}
            width={expanded ? 168 : 76}
          >
            <label
              color="#FFFFFF"
              font="system-bold 12"
              textAlign="center"
              value={expanded ? 'Expanded inline button' : 'Expand'}
              width="100%"
            />
          </view>
        </label>
      ) : (
        <textview
          backgroundColor="#FFFFFF"
          color="#0F172A"
          enabled={false}
          font="system 20"
          height={110}
          lineHeight={1.35}
          numberOfLines={0}
          value={this.value}
          width="100%"
        >
          <view
            accessibilityCategory="button"
            alignItems="center"
            backgroundColor={expanded ? '#0F766E' : '#2563EB'}
            borderRadius={8}
            height={32}
            justifyContent="center"
            onTap={this.viewModel.interactive ? this.toggle : undefined}
            width={expanded ? 168 : 76}
          >
            <label
              color="#FFFFFF"
              font="system-bold 12"
              textAlign="center"
              value={expanded ? 'Expanded inline button' : 'Expand'}
              width="100%"
            />
          </view>
        </textview>
      )}
    </view>;
  }

  private toggle = () => {
    this.viewModel.renderContext.record('inline expanding button tapped');
    this.setState({ expanded: !this.state.expanded });
  };
}

const roundedMask = new GeometricPathBuilder(1, 1, GeometricPathScaleType.Fill)
  .roundRectTo(0.12, 0.12, 0.76, 0.76, 0.2, 0.2)
  .build();

const trianglePath = new GeometricPathBuilder(1, 1, GeometricPathScaleType.Contain)
  .moveTo(0.5, 0.05)
  .lineTo(0.95, 0.9)
  .lineTo(0.08, 0.72)
  .close()
  .build();

const curvePath = new GeometricPathBuilder(1, 1, GeometricPathScaleType.Contain)
  .moveTo(0.05, 0.75)
  .cubicTo(0.2, 0.05, 0.8, 0.95, 0.95, 0.25)
  .build();

const linePath = new GeometricPathBuilder(1, 1, GeometricPathScaleType.Fill)
  .moveTo(0.08, 0.5)
  .lineTo(0.92, 0.5)
  .build();

const cornerPath = new GeometricPathBuilder(1, 1, GeometricPathScaleType.Contain)
  .moveTo(0.12, 0.82)
  .lineTo(0.5, 0.18)
  .lineTo(0.88, 0.82)
  .build();

export const INTEGRATION_TEST_CASES: readonly IntegrationTestCase[] = [
  {
    id: 'custom-view-host-view-factory',
    name: 'Host-provided custom view factory',
    description: 'Creates a custom view directly from a platform host factory and applies host plus standard view attributes.',
    element: 'custom-view',
    coverage: coverage('visual', ['viewFactory', 'factoryText', 'backgroundColor', 'borderRadius']),
    render: (ctx: IntegrationTestRenderContext) => {
      const platform = getPlatform();
      const viewFactory = platform !== 'macos' ? (integrationViewFactory ??= createIntegrationViewFactory()) : undefined;

      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <label value="Host-provided view factory" font="system-bold 20" color="#0F172A" marginBottom={18} />
        {viewFactory ? (
          <custom-view
            ref={ctx.targetRef}
            viewFactory={viewFactory}
            factoryText="Factory-backed custom view"
            width="100%"
            height={92}
            backgroundColor="#DBEAFE"
            borderRadius={12}
            onLayout={frame => ctx.record(`factory layout:${Math.round(frame.width)}x${Math.round(frame.height)}`)}
          />
        ) : (
          <view width="100%" height={92} backgroundColor="#DBEAFE" borderRadius={12}>
            <label value="View factory unavailable on this platform" font="system 16" />
          </view>
        )}
      </view>;

      ctx.record(`factory host:${getPlatform()}`);
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      if (getPlatform() === 'macos') {
        return;
      }

      const node = ctx.targetRef.single()?.getVirtualNode();
      if (!node) {
        throw new Error('The host factory did not create a custom-view node');
      }

      const element = node.element;
      if (!element || element.getAttribute('factoryText') !== 'Factory-backed custom view') {
        throw new Error('The host factory did not receive its custom attribute');
      }

      ctx.record(`factory custom attribute:${element.getAttribute('factoryText')}`);
    },
  },
  {
    id: 'layout-size-constraints',
    name: 'Layout size constraints',
    description: 'Renders constrained layout boxes using width/height, percentages, min/max size, and aspectRatio.',
    element: 'layout',
    coverage: coverage('visual', ['width', 'height', 'minWidth', 'minHeight', 'maxWidth', 'maxHeight', 'aspectRatio']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout
          width="100%"
          height={132}
          onLayout={frame => ctx.record(`layout constraints:${Math.round(frame.width)}x${Math.round(frame.height)}`)}
        >
          <view
            backgroundColor="#DBEAFE"
            border="2 solid #2563EB"
            borderRadius={10}
            minWidth={160}
            maxWidth={220}
            width="80%"
            height={58}
          />
          <view
            backgroundColor="#DCFCE7"
            border="2 solid #047857"
            borderRadius={10}
            marginTop={12}
            minHeight={46}
            maxHeight={64}
            width={180}
            height="50%"
          />
        </layout>
        <layout width={240} aspectRatio={2.4} marginTop={18}>
          <view backgroundColor="#FDE68A" border="2 solid #92400E" borderRadius={10} width="100%" height="100%" />
        </layout>
      </view>;
    },
  },
  {
    id: 'layout-position-offsets',
    name: 'Layout relative and absolute offsets',
    description: 'Compares relative offsets with absolute top/right/bottom/left placement inside one fixed parent.',
    element: 'layout',
    coverage: coverage('visual', ['position', 'top', 'right', 'bottom', 'left']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#E5E7EB" borderRadius={12} height={260} width="100%">
          <view
            backgroundColor="#2563EB"
            borderRadius={10}
            height={70}
            left={22}
            position="relative"
            top={18}
            width={120}
          />
          <view
            backgroundColor="#F97316"
            borderRadius={10}
            bottom={20}
            height={80}
            position="absolute"
            right={24}
            width={130}
          />
          <view
            backgroundColor="#22C55E"
            borderRadius={10}
            height={54}
            left={34}
            position="absolute"
            top={144}
            width={90}
          />
        </view>
      </view>;
    },
  },
  {
    id: 'layout-spacing-shorthands',
    name: 'Layout margin and padding shorthands',
    description: 'Renders shorthand and side-specific margin/padding values with visible child boxes.',
    element: 'layout',
    coverage: coverage('visual', [
      'margin',
      'marginTop',
      'marginRight',
      'marginBottom',
      'marginLeft',
      'padding',
      'paddingTop',
      'paddingRight',
      'paddingBottom',
      'paddingLeft',
    ]),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#E0F2FE" border="2 solid #0284C7" borderRadius={12} height={220} width="100%">
          <layout padding="8 18 14 28" width="100%">
            <view backgroundColor="#2563EB" borderRadius={8} height={52} margin="10 4 6 18" width={130} />
            <view
              backgroundColor="#F97316"
              borderRadius={8}
              height={52}
              marginTop={14}
              marginRight={22}
              marginBottom={8}
              marginLeft={48}
              width={170}
            />
          </layout>
        </view>
        <view
          backgroundColor="#DCFCE7"
          border="2 solid #047857"
          borderRadius={12}
          height={120}
          marginTop={18}
          paddingTop={16}
          paddingRight={30}
          paddingBottom={10}
          paddingLeft={44}
          width="100%"
        >
          <view backgroundColor="#047857" borderRadius={8} height={60} width="100%" />
        </view>
      </view>;
    },
  },
  {
    id: 'layout-flex-direction',
    name: 'Layout flex direction variants',
    description: 'Renders row, row-reverse, column, and column-reverse direction ordering with fixed children.',
    element: 'layout',
    coverage: coverage('visual', ['flexDirection']),
    render: (ctx: IntegrationTestRenderContext) => {
      const swatch = (color: string) => (
        <view backgroundColor={color} borderRadius={8} height={42} margin={4} width={58} />
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout flexDirection="row" width="100%">
          {swatch('#EF4444')}
          {swatch('#F59E0B')}
          {swatch('#10B981')}
        </layout>
        <layout flexDirection="row-reverse" marginTop={14} width="100%">
          {swatch('#EF4444')}
          {swatch('#F59E0B')}
          {swatch('#10B981')}
        </layout>
        <layout flexDirection="column" marginTop={14} width="100%">
          {swatch('#2563EB')}
          {swatch('#7C3AED')}
        </layout>
        <layout flexDirection="column-reverse" height={110} marginTop={14} width="100%">
          {swatch('#2563EB')}
          {swatch('#7C3AED')}
        </layout>
      </view>;
    },
  },
  {
    id: 'layout-justify-content',
    name: 'Layout justifyContent variants',
    description: 'Renders representative justifyContent values along the row main axis.',
    element: 'layout',
    coverage: coverage('visual', ['justifyContent']),
    render: (ctx: IntegrationTestRenderContext) => {
      const row = (
        justifyContent: 'flex-start' | 'flex-end' | 'center' | 'space-between' | 'space-around' | 'space-evenly',
        color: string,
      ) => (
        <layout flexDirection="row" justifyContent={justifyContent} height={48} marginBottom={10} width="100%">
          <view backgroundColor={color} borderRadius={7} height={36} width={44} />
          <view backgroundColor={color} borderRadius={7} height={36} width={44} />
          <view backgroundColor={color} borderRadius={7} height={36} width={44} />
        </layout>
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        {row('flex-start', '#2563EB')}
        {row('flex-end', '#F97316')}
        {row('center', '#047857')}
        {row('space-between', '#7C3AED')}
        {row('space-around', '#BE123C')}
        {row('space-evenly', '#0F766E')}
      </view>;
    },
  },
  {
    id: 'layout-align-items-baseline',
    name: 'Layout alignItems and baseline',
    description: 'Renders alignItems stretch, flex-start, center, flex-end, and baseline with labels.',
    element: 'layout',
    coverage: coverage('visual', ['alignItems']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout flexDirection="row" alignItems="stretch" height={60} marginBottom={12} width="100%">
          <view backgroundColor="#DBEAFE" width={64} />
          <view backgroundColor="#60A5FA" width={64} />
        </layout>
        <layout flexDirection="row" alignItems="flex-start" height={70} marginBottom={12} width="100%">
          <view backgroundColor="#F97316" height={34} width={64} />
          <view backgroundColor="#FDBA74" height={54} width={64} />
        </layout>
        <layout flexDirection="row" alignItems="center" height={70} marginBottom={12} width="100%">
          <view backgroundColor="#047857" height={34} width={64} />
          <view backgroundColor="#86EFAC" height={54} width={64} />
        </layout>
        <layout flexDirection="row" alignItems="flex-end" height={70} marginBottom={12} width="100%">
          <view backgroundColor="#7C3AED" height={34} width={64} />
          <view backgroundColor="#C4B5FD" height={54} width={64} />
        </layout>
        <layout flexDirection="row" alignItems="baseline" height={80} width="100%">
          <label value="Big" color="#111827" font="system-bold 32" />
          <label value="baseline" color="#1D4ED8" font="system 18" marginLeft={8} />
        </layout>
      </view>;
    },
  },
  {
    id: 'layout-align-self',
    name: 'Layout alignSelf override',
    description: 'Renders children overriding parent alignItems with alignSelf values.',
    element: 'layout',
    coverage: coverage('visual', ['alignSelf']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout alignItems="flex-start" height={230} width="100%">
          <view alignSelf="flex-start" backgroundColor="#EF4444" borderRadius={8} height={42} width={90} />
          <view alignSelf="center" backgroundColor="#F59E0B" borderRadius={8} height={42} marginTop={12} width={90} />
          <view alignSelf="flex-end" backgroundColor="#10B981" borderRadius={8} height={42} marginTop={12} width={90} />
          <view alignSelf="stretch" backgroundColor="#3B82F6" borderRadius={8} height={42} marginTop={12} />
        </layout>
      </view>;
    },
  },
  {
    id: 'layout-flex-grow-shrink-basis',
    name: 'Layout grow shrink and basis',
    description: 'Renders flexGrow, flexShrink, and flexBasis values in constrained rows.',
    element: 'layout',
    coverage: coverage('visual', ['flexGrow', 'flexShrink', 'flexBasis']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout flexDirection="row" width="100%" height={70}>
          <view backgroundColor="#2563EB" borderRadius={8} flexGrow={1} flexBasis={70} height={54} margin={4} />
          <view backgroundColor="#F97316" borderRadius={8} flexGrow={2} flexBasis="25%" height={54} margin={4} />
        </layout>
        <layout flexDirection="row" width={240} height={70} marginTop={20}>
          <view backgroundColor="#047857" borderRadius={8} flexShrink={1} width={160} height={54} margin={4} />
          <view backgroundColor="#7C3AED" borderRadius={8} flexShrink={2} width={160} height={54} margin={4} />
        </layout>
      </view>;
    },
  },
  {
    id: 'layout-flex-wrap-gaps',
    name: 'Layout wrap and gaps',
    description: 'Renders flexWrap, wrap-reverse, gap, rowGap, and columnGap with multiple children.',
    element: 'layout',
    coverage: coverage('visual', ['flexWrap', 'gap', 'rowGap', 'columnGap']),
    render: (ctx: IntegrationTestRenderContext) => {
      const child = (color: string) => <view backgroundColor={color} borderRadius={8} height={46} width={86} />;
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout flexDirection="row" flexWrap="wrap" gap="10 14" width={250}>
          {child('#EF4444')}
          {child('#F59E0B')}
          {child('#10B981')}
          {child('#3B82F6')}
          {child('#7C3AED')}
        </layout>
        <layout
          flexDirection="row"
          flexWrap="wrap-reverse"
          rowGap={8}
          columnGap={18}
          width={250}
          height={128}
          marginTop={24}
        >
          {child('#EF4444')}
          {child('#F59E0B')}
          {child('#10B981')}
          {child('#3B82F6')}
          {child('#7C3AED')}
        </layout>
      </view>;
    },
  },
  {
    id: 'layout-align-content',
    name: 'Layout alignContent variants',
    description:
      'Renders wrapped lines with alignContent flex-start, center, flex-end, space-between, and space-around.',
    element: 'layout',
    coverage: coverage('visual', ['alignContent']),
    render: (ctx: IntegrationTestRenderContext) => {
      const block = (
        alignContent: 'flex-start' | 'center' | 'flex-end' | 'space-between' | 'space-around',
        color: string,
      ) => (
        <layout alignContent={alignContent} flexDirection="row" flexWrap="wrap" width={140} height={150} margin={6}>
          <view backgroundColor={color} height={34} width={58} margin={3} />
          <view backgroundColor={color} height={34} width={58} margin={3} />
          <view backgroundColor={color} height={34} width={58} margin={3} />
          <view backgroundColor={color} height={34} width={58} margin={3} />
        </layout>
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout flexDirection="row" flexWrap="wrap">
          {block('flex-start', '#EF4444')}
          {block('center', '#F59E0B')}
          {block('flex-end', '#10B981')}
          {block('space-between', '#3B82F6')}
          {block('space-around', '#7C3AED')}
        </layout>
      </view>;
    },
  },
  {
    id: 'layout-display-overflow',
    name: 'Layout display and overflow',
    description: 'Renders display flex/none and overflow visible/scroll layout effects.',
    element: 'layout',
    coverage: coverage('visual', ['display', 'overflow']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout flexDirection="row" width="100%" height={80}>
          <view backgroundColor="#EF4444" borderRadius={8} height={54} width={80} />
          <view display="none" backgroundColor="#111827" height={54} width={80} />
          <view backgroundColor="#10B981" borderRadius={8} height={54} marginLeft={12} width={80} />
        </layout>
        <layout overflow="visible" width={120} height={70} marginTop={18}>
          <view backgroundColor="#2563EB" borderRadius={8} height={120} width={180} />
        </layout>
        <layout overflow="scroll" width={120} height={70} marginTop={70}>
          <view backgroundColor="#F97316" borderRadius={8} height={120} width={180} />
        </layout>
      </view>;
    },
  },
  {
    id: 'layout-zindex-overlap',
    name: 'Layout zIndex overlap',
    description: 'Renders overlapping siblings with explicit zIndex ordering.',
    element: 'layout',
    coverage: coverage('visual', ['zIndex']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view height={220} width="100%">
          <view
            backgroundColor="#EF4444"
            borderRadius={18}
            height={150}
            left={18}
            position="absolute"
            top={18}
            width={170}
            zIndex={1}
          />
          <view
            backgroundColor="#2563EB"
            borderRadius={18}
            height={150}
            left={96}
            position="absolute"
            top={72}
            width={170}
            zIndex={3}
          />
          <view
            backgroundColor="#F59E0B"
            borderRadius={18}
            height={150}
            left={158}
            position="absolute"
            top={38}
            width={130}
            zIndex={2}
          />
        </view>
      </view>;
    },
  },
  {
    id: 'layout-direction-rtl',
    name: 'Layout direction RTL',
    description: 'Renders LTR and RTL rows and a flipped image under an RTL parent.',
    element: 'layout',
    coverage: coverage('visual', ['direction', 'flipOnRtl']),
    render: (ctx: IntegrationTestRenderContext) => {
      const swatch = (color: string) => (
        <view backgroundColor={color} borderRadius={8} height={50} margin={4} width={62} />
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout direction="ltr" flexDirection="row" width="100%">
          {swatch('#EF4444')}
          {swatch('#F59E0B')}
          {swatch('#10B981')}
        </layout>
        <layout direction="rtl" flexDirection="row" marginTop={16} width="100%">
          {swatch('#EF4444')}
          {swatch('#F59E0B')}
          {swatch('#10B981')}
        </layout>
        <layout direction="rtl" marginTop={20} width="100%">
          <image src={res.image} objectFit="cover" width={220} height={140} flipOnRtl />
        </layout>
      </view>;
    },
  },
  {
    id: 'layout-measure-estimates',
    name: 'Layout measure callback and estimates',
    description: 'Uses onMeasure and estimatedWidth/estimatedHeight to size lazy layout placeholders.',
    element: 'layout',
    coverage: coverage('interaction', ['onMeasure', 'estimatedWidth', 'estimatedHeight', 'lazyLayout']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout
          lazyLayout
          onMeasure={(width, widthMode, height, heightMode) => {
            ctx.record(`onMeasure:${Math.round(width)}:${widthMode}:${Math.round(height)}:${heightMode}`);
            return [180, 76];
          }}
        >
          <view backgroundColor="#2563EB" borderRadius={10} height="100%" width="100%" />
        </layout>
        <layout lazyLayout estimatedWidth={150} estimatedHeight={64} marginTop={24}>
          <view backgroundColor="#F97316" borderRadius={10} height="100%" width="100%" />
        </layout>
      </view>;
    },
  },
  {
    id: 'layout-lifecycle-callbacks',
    name: 'Layout callbacks and viewport flags',
    description:
      'Records layout, viewport, visibility, and layout-complete callbacks while binding lazy/viewport flags.',
    element: 'layout',
    coverage: [
      { kind: 'interaction', attributes: ['onLayout', 'onVisibilityChanged', 'onViewportChanged', 'onLayoutComplete'] },
      {
        kind: 'node-output',
        attributes: ['lazy', 'limitToViewport', 'ignoreParentViewport', 'extendViewportWithChildren'],
      },
    ],
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout
          lazy
          limitToViewport={false}
          ignoreParentViewport
          extendViewportWithChildren
          width={250}
          height={140}
          onLayout={frame => ctx.record(`layout:onLayout:${Math.round(frame.width)}x${Math.round(frame.height)}`)}
          onLayoutComplete={() => ctx.record('layout:onLayoutComplete')}
          onVisibilityChanged={visible => ctx.record(`layout:onVisibilityChanged:${visible}`)}
          onViewportChanged={(viewport, frame) =>
            ctx.record(
              `layout:onViewportChanged:${Math.round(viewport.width)}x${Math.round(viewport.height)} of ${Math.round(frame.width)}x${Math.round(frame.height)}`,
            )
          }
        >
          <view backgroundColor="#0EA5E9" borderRadius={12} height={120} width={280} />
        </layout>
      </view>;
    },
  },
  {
    id: 'layout-accessibility-metadata',
    name: 'Layout accessibility and metadata attributes',
    description: 'Binds accessibility metadata, id/key/class, and animationsEnabled for node-output coverage.',
    element: 'layout',
    coverage: coverage('node-output', [
      'accessibilityCategory',
      'accessibilityNavigation',
      'accessibilityPriority',
      'accessibilityLabel',
      'accessibilityHint',
      'accessibilityValue',
      'accessibilityStateDisabled',
      'accessibilityStateSelected',
      'accessibilityStateLiveRegion',
      'id',
      'key',
      'class',
      'animationsEnabled',
    ]),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout
          id="integration-layout-metadata"
          key="integration-layout-key"
          class="integration-layout-class"
          animationsEnabled={false}
          accessibilityCategory="button"
          accessibilityNavigation="cover"
          accessibilityPriority={12}
          accessibilityLabel="Palette card"
          accessibilityHint="Metadata only fixture"
          accessibilityValue="Selected"
          accessibilityStateDisabled={false}
          accessibilityStateSelected
          accessibilityStateLiveRegion
          width="100%"
        >
          <view backgroundColor="#DBEAFE" border="2 solid #2563EB" borderRadius={12} height={120} width="100%" />
        </layout>
      </view>;
    },
  },
  {
    id: 'layout-scroll-anchor-bottom',
    name: 'Layout bottom scroll anchor',
    description: 'Complements the existing top-anchor scroll test with a descendant using scrollAnchorPosition=bottom.',
    element: 'layout',
    coverage: coverage('node-output', ['scrollAnchorPosition']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <scroll
          ref={ctx.targetRef}
          backgroundColor="#FFFFFF"
          border="2 solid #CBD5E1"
          borderRadius={12}
          height={260}
          width="100%"
        >
          <view backgroundColor="#EF4444" height={100} margin={8} />
          <view backgroundColor="#F59E0B" height={100} margin={8} />
          <view backgroundColor="#10B981" height={100} margin={8} scrollAnchorPosition="bottom" />
          <view backgroundColor="#3B82F6" height={100} margin={8} />
        </scroll>
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      ctx.targetRef.setAttribute('staticContentHeight', 460);
      ctx.targetRef.setAttribute('maintainScrollAnchor', true);
      await ctx.waitForIdle();
    },
  },
  {
    id: 'layout-color-palette-subtree-override',
    name: 'Layout color palette subtree override',
    description:
      'Configures light and dark palettes, sets light active globally, and overrides one subtree with colorPaletteName=integration-dark.',
    element: 'layout',
    coverage: coverage('visual', ['colorPaletteName']),
    render: (ctx: IntegrationTestRenderContext) => {
      configureIntegrationColorPalettes();
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} background="background" padding={18}>
        <view background="foreground" borderRadius={12} height={72} width="100%">
          <label value="active light foreground" color="background" font="system-bold 20" margin={14} />
        </view>
        <view
          colorPaletteName="integration-dark"
          background="background"
          border="4 solid foreground"
          borderRadius={16}
          height={160}
          marginTop={18}
          padding={18}
          width="100%"
        >
          <view background="foreground" borderRadius={12} height={78} width="100%">
            <label value="dark override subtree" color="background" font="system-bold 20" margin={14} />
          </view>
        </view>
        <view background="accent" borderRadius={12} height={72} marginTop={18} width="100%">
          <label value="sibling remains light" color="background" font="system-bold 20" margin={14} />
        </view>
      </view>;
    },
  },
  {
    id: 'view-background-color',
    name: 'View solid background color',
    description: 'Renders one view with a uniform backgroundColor and no other visual effects.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view
          accessibilityId="integration-view-color"
          backgroundColor="#0EA5E9"
          borderRadius={16}
          height={160}
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'view-background-gradient',
    name: 'View gradient background',
    description: 'Renders one view whose background attribute is a multi-stop linear gradient.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view background="linear-gradient(90deg, #0EA5E9, #A855F7, #F97316)" height={170} width="100%" />
      </view>;
    },
  },
  {
    id: 'view-border-styles',
    name: 'View border syntaxes and styles',
    description:
      'Renders dashed and dotted border shorthands plus an asymmetric borderRadius.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#DBEAFE" border="4 dashed #1D4ED8" height={100} marginBottom={18} width="100%" />
        <view backgroundColor="#DCFCE7" border="5 dotted #047857" height={100} marginBottom={18} width="100%" />
        <view backgroundColor="#FDE68A" border="3 solid #92400E" borderRadius="28 4 28 4" height={100} width="100%" />
      </view>;
    },
  },
  {
    id: 'view-opacity',
    name: 'View opacity',
    description: 'Renders one semi-transparent child view over colored stripes so opacity blending is easy to inspect.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view height={180} width="100%">
          <view backgroundColor="#EF4444" height={60} width="100%" />
          <view backgroundColor="#F59E0B" height={60} width="100%" />
          <view backgroundColor="#2563EB" height={60} width="100%" />
          <view
            backgroundColor="#111827"
            height={150}
            left={42}
            opacity={0.52}
            position="absolute"
            top={15}
            width={200}
          />
        </view>
      </view>;
    },
  },
  {
    id: 'view-shadow-radius-clipping',
    name: 'View shadow, radius, and slow clipping',
    description:
      'Renders a rounded view with boxShadow and slowClipping, with an inner child clipped by the rounded corners.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view
          backgroundColor="#22C55E"
          borderRadius={26}
          boxShadow="0 10 20 rgba(17, 24, 39, 0.35)"
          height={160}
          slowClipping
          width={250}
        >
          <view backgroundColor="#FFFFFF" height={70} margin={34} />
        </view>
      </view>;
    },
  },
  {
    id: 'view-transform-scale',
    name: 'View scale transform attributes',
    description: 'Renders scaleX and scaleY on one colored view.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#E5E7EB" borderRadius={12} height={220} width="100%">
          <view backgroundColor="#2563EB" borderRadius={14} height={120} scaleX={0.72} scaleY={1.22} width={170} />
        </view>
      </view>;
    },
  },
  {
    id: 'view-transform-rotation',
    name: 'View rotation transform attribute',
    description: 'Renders rotation on one colored view without scale or translation.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#E5E7EB" borderRadius={12} height={220} width="100%">
          <view backgroundColor="#F97316" borderRadius={14} height={120} rotation={0.28} width={170} />
        </view>
      </view>;
    },
  },
  {
    id: 'view-transform-translation',
    name: 'View translation transform attributes',
    description: 'Renders translationX as a percent string and translationY as points on one colored view.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#E5E7EB" borderRadius={12} height={220} width="100%">
          <view
            backgroundColor="#7C3AED"
            borderRadius={14}
            height={120}
            translationX="18%"
            translationY={30}
            width={170}
          />
        </view>
      </view>;
    },
  },
  {
    id: 'view-transform-string-origin',
    name: 'View transform string and origin',
    description: 'Renders a CSS-like transform string with a non-center transformOrigin.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#E5E7EB" borderRadius={12} height={230} width="100%">
          <view
            backgroundColor="#F97316"
            borderRadius={14}
            height={120}
            transform="translateX(42) translateY(34) scale(0.92) rotate(0.22)"
            transformOrigin="25% 75%"
            width={170}
          />
        </view>
      </view>;
    },
  },
  {
    id: 'view-mask-path-opacity',
    name: 'View mask path and mask opacity',
    description: 'Renders a green view with a rounded geometric mask cut into it using maskPath and maskOpacity.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#22C55E" height={180} maskOpacity={0.38} maskPath={roundedMask} width={230} />
      </view>;
    },
  },
  {
    id: 'view-touch-hit-testing',
    name: 'View touch area and hit testing',
    description:
      'Dispatches a native tap into a target view with extended touch area, hitTest, onTouchStart, onTouch, and onTouchEnd observations.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      const platform = getPlatform();
      if (platform !== 'android') {
        ctx.record('filterTouchesWhenObscured skipped: Android-only and asserts in SnapDrawing');
      }
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view
          ref={ctx.targetRef}
          backgroundColor="#7C3AED"
          borderRadius={18}
          filterTouchesWhenObscured={platform === 'android' ? true : undefined}
          height={150}
          hitTest={event => {
            ctx.record(`hitTest:${summarizeEvent(event)}`);
            return true;
          }}
          onTouch={event => ctx.record(`onTouch:${summarizeEvent(event)}`)}
          onTouchDelayDuration={0}
          onTouchEnd={event => ctx.record(`onTouchEnd:${summarizeEvent(event)}`)}
          onTouchStart={event => ctx.record(`onTouchStart:${summarizeEvent(event)}`)}
          touchAreaExtension={10}
          touchAreaExtensionBottom={12}
          touchAreaExtensionLeft={12}
          touchAreaExtensionRight={12}
          touchAreaExtensionTop={12}
          touchEnabled
          width="100%"
        >
          <label value="tap target" color="#FFFFFF" font="system-bold 22" margin={24} />
        </view>
      </view>;
    },
    interact: interactTap,
  },
  {
    id: 'view-scroll-intercept-flags',
    name: 'View platform canScroll override flags',
    description:
      'Renders a target view with canAlwaysScrollHorizontal and canAlwaysScrollVertical enabled so parent/platform gesture arbitration can be snapshotted and traced.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      ctx.record(
        'canAlwaysScrollHorizontal and canAlwaysScrollVertical are platform gesture-arbitration flags; visual output confirms the configured target exists.',
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view
          ref={ctx.targetRef}
          backgroundColor="#0F766E"
          borderRadius={18}
          canAlwaysScrollHorizontal
          canAlwaysScrollVertical
          height={150}
          width="100%"
        >
          <label value="canAlwaysScroll H+V" color="#FFFFFF" font="system-bold 21" margin={24} />
        </view>
      </view>;
    },
  },
  {
    id: 'view-gesture-tap',
    name: 'View tap gesture',
    description: 'Dispatches a native tap to a view with onTap, onTapPredicate, and onTapDisabled=false.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view
          ref={ctx.targetRef}
          backgroundColor="#2563EB"
          borderRadius={18}
          height={170}
          onTap={event => ctx.record(`onTap:${summarizeEvent(event)}`)}
          onTapDisabled={false}
          onTapPredicate={event => {
            ctx.record(`onTapPredicate:${summarizeEvent(event)}`);
            return true;
          }}
          width="100%"
        >
          <label value="tap target" color="white" font="system-bold 22" margin={24} />
        </view>
      </view>;
    },
    interact: interactTap,
  },
  {
    id: 'view-gesture-drag',
    name: 'View drag gesture',
    description: 'Dispatches a native drag sequence to a view with onDrag, onDragPredicate, and onDragDisabled=false.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view
          ref={ctx.targetRef}
          backgroundColor="#7C3AED"
          borderRadius={18}
          height={170}
          onDrag={event => ctx.record(`onDrag:${summarizeEvent(event)}`)}
          onDragDisabled={false}
          onDragPredicate={event => {
            ctx.record(`onDragPredicate:${summarizeEvent(event)}`);
            return true;
          }}
          width="100%"
        >
          <label value="drag target" color="white" font="system-bold 22" margin={24} />
        </view>
      </view>;
    },
    interact: interactDrag,
  },
  {
    id: 'view-gesture-double-tap',
    name: 'View double tap gesture',
    description:
      'Dispatches a native double tap to a view with onDoubleTap, onDoubleTapPredicate, and onDoubleTapDisabled=false.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view
          ref={ctx.targetRef}
          backgroundColor="#0EA5E9"
          borderRadius={18}
          height={170}
          onDoubleTap={event => ctx.record(`onDoubleTap:${summarizeEvent(event)}`)}
          onDoubleTapDisabled={false}
          onDoubleTapPredicate={event => {
            ctx.record(`onDoubleTapPredicate:${summarizeEvent(event)}`);
            return true;
          }}
          width="100%"
        >
          <label value="double tap target" color="white" font="system-bold 22" margin={24} />
        </view>
      </view>;
    },
    interact: interactDoubleTap,
  },
  {
    id: 'view-gesture-long-press',
    name: 'View long press gesture',
    description:
      'Dispatches a native long press to a view with longPressDuration, onLongPress, onLongPressPredicate, and onLongPressDisabled=false.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view
          ref={ctx.targetRef}
          backgroundColor="#EA580C"
          borderRadius={18}
          height={170}
          longPressDuration={0.25}
          onLongPress={event => ctx.record(`onLongPress:${summarizeEvent(event)}`)}
          onLongPressDisabled={false}
          onLongPressPredicate={event => {
            ctx.record(`onLongPressPredicate:${summarizeEvent(event)}`);
            return true;
          }}
          width="100%"
        >
          <label value="long press target" color="white" font="system-bold 22" margin={24} />
        </view>
      </view>;
    },
    interact: interactLongPress,
  },
  {
    id: 'view-gesture-pinch-rotate-wiring',
    name: 'View pinch and rotate gesture wiring',
    description:
      'Registers pinch and rotate callbacks, predicates, and disabled flags; current host interaction records that synthetic multi-touch is not available yet.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view
          ref={ctx.targetRef}
          backgroundColor="#0891B2"
          borderRadius={18}
          height={170}
          onPinch={event => ctx.record(`onPinch:${summarizeEvent(event)}`)}
          onPinchDisabled={false}
          onPinchPredicate={event => {
            ctx.record(`onPinchPredicate:${summarizeEvent(event)}`);
            return true;
          }}
          onRotate={event => ctx.record(`onRotate:${summarizeEvent(event)}`)}
          onRotateDisabled={false}
          onRotatePredicate={event => {
            ctx.record(`onRotatePredicate:${summarizeEvent(event)}`);
            return true;
          }}
          width="100%"
        >
          <label value="pinch / rotate target" color="white" font="system-bold 22" margin={24} />
        </view>
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      ctx.record(
        'synthetic multi-touch not implemented in IntegrationTestHost; this case validates callback registration and snapshot construction only.',
      );
      await ctx.waitForIdle();
    },
  },
  {
    id: 'view-lifecycle-reuse',
    name: 'View lifecycle and reuse callbacks',
    description: 'Renders allowReuse=false and records onViewCreate, onViewChange, and onViewDestroy wiring.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view
          allowReuse={false}
          backgroundColor="#14B8A6"
          height={160}
          onViewChange={nativeView => ctx.record(`onViewChange:${nativeView ? 'created' : 'destroyed'}`)}
          onViewCreate={() => ctx.record('onViewCreate')}
          onViewDestroy={() => ctx.record('onViewDestroy')}
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'label-flex-shrink-defaults',
    name: 'Label flex shrink and line defaults',
    description:
      'Constrains labels beside fixed-width siblings so the default label truncates to one line and numberOfLines=0 wraps.',
    element: 'label',
    coverage: coverage('visual', ['numberOfLines', 'flexShrink']),
    render: (ctx: IntegrationTestRenderContext) => {
      const value = 'This long label must stay inside the row instead of widening the page.';
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#FFFFFF" border="2 solid #CBD5E1" borderRadius={10} padding={10} width="100%">
          <label color="#475569" font="system-bold 13" marginBottom={8} value="Default: one line" width="100%" />
          <view alignItems="center" flexDirection="row" width="100%">
            <view backgroundColor="#2563EB" borderRadius={6} flexShrink={0} height={32} marginRight={8} width={42} />
            <label color="#1E3A8A" font="system 17" value={value} />
          </view>
        </view>
        <view
          backgroundColor="#FFFFFF"
          border="2 solid #CBD5E1"
          borderRadius={10}
          marginTop={18}
          padding={10}
          width="100%"
        >
          <label color="#475569" font="system-bold 13" marginBottom={8} value="numberOfLines=0: wraps" width="100%" />
          <view alignItems="flex-start" flexDirection="row" width="100%">
            <view backgroundColor="#059669" borderRadius={6} flexShrink={0} height={32} marginRight={8} width={42} />
            <label color="#065F46" font="system 17" numberOfLines={0} value={value} />
          </view>
        </view>
      </view>;
    },
  },
  {
    id: 'label-uniform-color',
    name: 'Label uniform text color',
    description: 'Renders a single label using value, font, and a uniform color, without gradient or shadow.',
    element: 'label',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <label color="#1D4ED8" font="system-bold 30 unscaled 30" value="Uniform blue text" width="100%" />
      </view>;
    },
  },
  {
    id: 'label-text-gradient',
    name: 'Label text gradient',
    description: 'Renders a label whose glyph fill comes from textGradient rather than color.',
    element: 'label',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <label
          font="system-bold 30 unscaled 30"
          textGradient="linear-gradient(#EF4444, #3B82F6)"
          value="Gradient text"
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'label-text-shadow',
    name: 'Label text shadow',
    description: 'Renders a label with textShadow and no gradient so shadow rendering is isolated.',
    element: 'label',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <label
          color="#7C2D12"
          font="system-bold 30 unscaled 30"
          textShadow="rgba(0, 0, 0, 0.38) 3 0.8 3 3"
          value="Shadow text"
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'label-letter-spacing',
    name: 'Label letter spacing',
    description: 'Renders three one-line labels with default, moderate, and wide positive letterSpacing.',
    element: 'label',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <label
          value="Normal spacing"
          color="#111827"
          font="system 20"
          letterSpacing={0}
          numberOfLines={1}
          width="100%"
        />
        <label
          value="Moderate spacing"
          color="#1D4ED8"
          font="system 20"
          letterSpacing={1.5}
          marginTop={16}
          numberOfLines={1}
          width="100%"
        />
        <label
          value="Wide spacing"
          color="#047857"
          font="system-bold 20"
          letterSpacing={3}
          marginTop={16}
          numberOfLines={1}
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'label-single-line-alignments',
    name: 'Label single-line alignments',
    description: 'Renders left, center, right, and justified textAlign values in separate one-line labels.',
    element: 'label',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <label
          value="Left aligned text"
          color="#1D4ED8"
          font="system 18"
          numberOfLines={1}
          textAlign="left"
          width="100%"
        />
        <label
          value="Centered text"
          color="#7C2D12"
          font="system-bold 20"
          marginTop={16}
          numberOfLines={1}
          textAlign="center"
          width="100%"
        />
        <label
          value="Right aligned text"
          color="#047857"
          font="system-bold 18"
          marginTop={16}
          numberOfLines={1}
          textAlign="right"
          width="100%"
        />
        <label
          value="Justified text stretches"
          color="#6D28D9"
          font="system 18"
          marginTop={16}
          numberOfLines={1}
          textAlign="justified"
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'label-multiline-lineheight',
    name: 'Label multiline with fixed line height',
    description: 'Renders one multiline label with numberOfLines=0 and explicit lineHeightAbsolute.',
    element: 'label',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#DBEAFE" borderRadius={10} padding={10} width="100%">
          <label
            color="#1E3A8A"
            font="system 16"
            lineHeightAbsolute={23}
            numberOfLines={0}
            textAlign="left"
            value="Left aligned multiline label wraps across several lines with fixed lineHeightAbsolute."
            width="100%"
          />
        </view>
      </view>;
    },
  },
  {
    id: 'label-lineheight-multiple',
    name: 'Label relative line height',
    description: 'Renders one multiline label with lineHeight and numberOfLines=3.',
    element: 'label',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#DCFCE7" borderRadius={10} marginTop={12} padding={10} width="100%">
          <label
            color="#14532D"
            font="system 16"
            lineHeight={1.45}
            numberOfLines={3}
            textAlign="center"
            value="Centered multiline label uses lineHeight for visibly taller spacing."
            width="100%"
          />
        </view>
      </view>;
    },
  },
  {
    id: 'label-overflow-ellipsis',
    name: 'Label overflow ellipsis',
    description: 'Renders a narrow single-line label with textOverflow=ellipsis.',
    element: 'label',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#FEE2E2" borderRadius={8} height={42} padding={6} width={250}>
          <label
            color="#991B1B"
            font="system-bold 22"
            numberOfLines={1}
            textOverflow="ellipsis"
            value="Ellipsis: this line should show a truncation marker"
            width="100%"
          />
        </view>
      </view>;
    },
  },
  {
    id: 'label-overflow-clip',
    name: 'Label overflow clip',
    description: 'Renders a narrow single-line label with textOverflow=clip.',
    element: 'label',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#E0E7FF" borderRadius={8} height={42} padding={6} width={250}>
          <label
            color="#3730A3"
            font="system-bold 22"
            numberOfLines={1}
            textOverflow="clip"
            value="Clip: this line should be hard clipped with no marker"
            width="100%"
          />
        </view>
      </view>;
    },
  },
  {
    id: 'label-autoshrink',
    name: 'Label autoshrink',
    description: 'Compares a clipped single-line label with an autoshrinking label using the same text.',
    element: 'label',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#FEE2E2" borderRadius={8} height={54} marginBottom={14} padding={7} width={250}>
          <label
            color="#991B1B"
            font="system-bold 34"
            numberOfLines={1}
            textOverflow="clip"
            value="Shrink to fit label"
            width="100%"
          />
        </view>
        <view backgroundColor="#DCFCE7" borderRadius={8} height={54} padding={7} width={250}>
          <label
            adjustsFontSizeToFitWidth
            color="#166534"
            font="system-bold 34"
            minimumScaleFactor={0.45}
            numberOfLines={1}
            textOverflow="clip"
            value="Shrink to fit label"
            width="100%"
          />
        </view>
      </view>;
    },
  },
  {
    id: 'label-decoration-styles',
    name: 'Label underline and strikethrough styles',
    description:
      'Renders underline, dashed underline, dotted underline, and strikethrough with customUnderlineStyle variants.',
    element: 'label',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <label
          value="Plain underline"
          color="#1D4ED8"
          font="system 22"
          marginBottom={14}
          textDecoration="underline"
          width="100%"
        />
        <label
          value="Dashed underline custom dash"
          color="#A16207"
          customUnderlineStyle="2 8 4 -3"
          font="system-bold 21"
          marginBottom={14}
          textDecoration="dashed-underline"
          width="100%"
        />
        <label
          value="Dotted underline custom dots"
          color="#047857"
          customUnderlineStyle="2 2 5 -2"
          font="system-bold 21"
          marginBottom={14}
          textDecoration="dotted-underline"
          width="100%"
        />
        <label
          value="Strikethrough sample"
          color="#BE123C"
          font="system-bold 22"
          textDecoration="strikethrough"
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'label-attributed-font-color',
    name: 'Label attributed font and color spans',
    description: 'Renders attributed text with per-span font and color only.',
    element: 'label',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#FFFFFF" border="2 solid #CBD5E1" borderRadius={12} padding={12} width="100%">
          <label
            font="system 18"
            numberOfLines={1}
            textAlign="left"
            value={attributedLabelFontColorValue()}
            width="100%"
          />
        </view>
      </view>;
    },
  },
  {
    id: 'label-attributed-background-decoration',
    name: 'Label attributed background and decoration',
    description: 'Renders attributed text with span background, background padding, underline, and dotted underline.',
    element: 'label',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#FFFFFF" border="2 solid #CBD5E1" borderRadius={12} padding={12} width="100%">
          <label
            font="system 18"
            numberOfLines={1}
            textAlign="left"
            value={attributedLabelBackgroundDecorationValue()}
            width="100%"
          />
        </view>
      </view>;
    },
  },
  {
    id: 'label-attributed-outline-layout',
    name: 'Label attributed outline and layout callback',
    description: 'Renders one attributed span with outlineColor, outlineWidth, and onLayout observation.',
    element: 'label',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#FFFFFF" border="2 solid #CBD5E1" borderRadius={12} padding={12} width="100%">
          <label
            font="system 18"
            numberOfLines={1}
            textAlign="left"
            value={attributedLabelOutlineLayoutValue(ctx)}
            width="100%"
          />
        </view>
      </view>;
    },
  },
  {
    id: 'label-attributed-inline-image',
    name: 'Label attributed inline image',
    description: 'Renders attributed text with an inline image attachment between two styled text spans.',
    element: 'label',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#FFFFFF" border="2 solid #CBD5E1" borderRadius={12} padding={12} width="100%">
          <label font="system 19" numberOfLines={1} value={attributedInlineImageValue()} width="100%" />
        </view>
      </view>;
    },
  },
  {
    id: 'label-attributed-tap-callback',
    name: 'Label attributed span tap callback',
    description:
      'Renders one tappable attributed span and dispatches a native tap into it to record the span callback when supported.',
    element: 'label',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <label
          ref={ctx.targetRef}
          backgroundColor="#FFFFFF"
          border="2 solid #CBD5E1"
          borderRadius={12}
          font="system 28"
          height={80}
          textAlign="center"
          value={attributedTapValue(ctx)}
          width="100%"
        />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      await ctx.waitForIdle();
    },
  },
  {
    id: 'label-attributed-multiline-truncation',
    name: 'Label attributed multiline truncation',
    description:
      'Renders attributed text with mixed font sizes and backgrounds constrained to two lines with ellipsis.',
    element: 'label',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#FFFFFF" border="2 solid #CBD5E1" borderRadius={12} padding={12} width={250}>
          <label
            font="system 18"
            numberOfLines={2}
            textOverflow="ellipsis"
            value={attributedMultilineValue()}
            width="100%"
          />
        </view>
      </view>;
    },
  },
  {
    id: 'textfield-placeholder-empty-states',
    name: 'TextField empty placeholders',
    description: 'Renders empty textfields with left, center, and right placeholder alignment and placeholderColor.',
    element: 'textfield',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textfield
          backgroundColor="#FFFFFF"
          border="2 solid #2563EB"
          borderRadius={10}
          font="system 18"
          height={48}
          placeholder="left placeholder"
          placeholderColor="#60A5FA"
          textAlign="left"
          tintColor="#2563EB"
          width="100%"
        />
        <textfield
          backgroundColor="#FFFFFF"
          border="2 solid #9333EA"
          borderRadius={10}
          font="system 18"
          height={48}
          marginTop={14}
          placeholder="center placeholder"
          placeholderColor="#A78BFA"
          textAlign="center"
          width="100%"
        />
        <textfield
          backgroundColor="#FFFFFF"
          border="2 solid #16A34A"
          borderRadius={10}
          font="system 18"
          height={48}
          marginTop={14}
          placeholder="right placeholder"
          placeholderColor="#86EFAC"
          textAlign="right"
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'textfield-value-disabled-state',
    name: 'TextField value and disabled visual state',
    description:
      'Renders editable and disabled textfields with visible values to compare color, enabled=false, and filled text.',
    element: 'textfield',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textfield
          backgroundColor="#FFFFFF"
          border="2 solid #2563EB"
          borderRadius={10}
          color="#111827"
          font="system 18"
          height={50}
          textAlign="left"
          value="editable value"
          width="100%"
        />
        <textfield
          backgroundColor="#F1F5F9"
          border="2 solid #94A3B8"
          borderRadius={10}
          color="#64748B"
          enabled={false}
          font="system 18"
          height={50}
          marginTop={14}
          textAlign="center"
          value="disabled center value"
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'textfield-secure-content-types',
    name: 'TextField secure and numeric content types',
    description:
      'Renders password, visible password, and signed decimal contentType variants without focusing the keyboard.',
    element: 'textfield',
    render: (ctx: IntegrationTestRenderContext) => {
      ctx.record(
        'contentType controls native keyboard/security behavior; visual fixture renders representative values without focusing.',
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textfield
          backgroundColor="#FFFFFF"
          border="2 solid #0EA5E9"
          borderRadius={10}
          contentType="password"
          font="system 17"
          height={48}
          placeholder="password"
          value="secret123"
          width="100%"
        />
        <textfield
          backgroundColor="#FFFFFF"
          border="2 solid #9333EA"
          borderRadius={10}
          contentType="passwordVisible"
          font="system 17"
          height={48}
          marginTop={14}
          placeholder="visible password"
          value="visible123"
          width="100%"
        />
        <textfield
          backgroundColor="#FFFFFF"
          border="2 solid #F97316"
          borderRadius={10}
          contentType="numberDecimalSigned"
          font="system 17"
          height={48}
          marginTop={14}
          placeholder="signed decimal"
          value="-42.50"
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'textfield-autocapitalization-correction',
    name: 'TextField capitalization and correction',
    description:
      'Renders textfields configured with autocapitalization=none, words, and characters plus autocorrection default/none.',
    element: 'textfield',
    render: (ctx: IntegrationTestRenderContext) => {
      ctx.record(
        'autocapitalization and autocorrection are native keyboard configuration; fields stay unfocused for deterministic screenshots.',
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textfield
          autocapitalization="none"
          autocorrection="none"
          backgroundColor="#FFFFFF"
          border="2 solid #0EA5E9"
          borderRadius={10}
          font="system 17"
          height={48}
          value="none@example.com"
          width="100%"
        />
        <textfield
          autocapitalization="words"
          autocorrection="default"
          backgroundColor="#FFFFFF"
          border="2 solid #9333EA"
          borderRadius={10}
          font="system 17"
          height={48}
          marginTop={14}
          value="word capitalization"
          width="100%"
        />
        <textfield
          autocapitalization="characters"
          autocorrection="none"
          backgroundColor="#FFFFFF"
          border="2 solid #F97316"
          borderRadius={10}
          font="system 17"
          height={48}
          marginTop={14}
          value="abc123"
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'textfield-keyboard-appearance-return',
    name: 'TextField keyboard appearance and return labels',
    description:
      'Renders textfields configured with keyboardAppearance dark/light/default and returnKeyText search/go/continue.',
    element: 'textfield',
    render: (ctx: IntegrationTestRenderContext) => {
      ctx.record(
        'keyboardAppearance and returnKeyText affect native keyboard chrome; the screenshot captures the configured fields before focus.',
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textfield
          backgroundColor="#FFFFFF"
          border="2 solid #0EA5E9"
          borderRadius={10}
          contentType="email"
          font="system 17"
          height={48}
          keyboardAppearance="dark"
          returnKeyText="search"
          value="person@example.com"
          width="100%"
        />
        <textfield
          backgroundColor="#FFFFFF"
          border="2 solid #9333EA"
          borderRadius={10}
          contentType="url"
          font="system 17"
          height={48}
          keyboardAppearance="light"
          marginTop={14}
          returnKeyText="go"
          value="https://valdi.test"
          width="100%"
        />
        <textfield
          backgroundColor="#FFFFFF"
          border="2 solid #F97316"
          borderRadius={10}
          contentType="number"
          font="system 17"
          height={48}
          keyboardAppearance="default"
          marginTop={14}
          returnKeyText="continue"
          value="123456"
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'textfield-inline-predictions',
    name: 'TextField inline predictions toggle',
    description: 'Renders two otherwise similar fields with enableInlinePredictions=false and true.',
    element: 'textfield',
    render: (ctx: IntegrationTestRenderContext) => {
      ctx.record(
        'enableInlinePredictions is iOS-only native input configuration; fields stay unfocused for deterministic screenshots.',
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textfield
          backgroundColor="#FFFFFF"
          border="2 solid #0EA5E9"
          borderRadius={10}
          enableInlinePredictions={false}
          font="system 17"
          height={48}
          value="predictions disabled"
          width="100%"
        />
        <textfield
          backgroundColor="#FFFFFF"
          border="2 solid #9333EA"
          borderRadius={10}
          enableInlinePredictions
          font="system 17"
          height={48}
          marginTop={14}
          value="predictions enabled"
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'textfield-selection-character-limit',
    name: 'TextField selection and character limit',
    description:
      'Sets selection, characterLimit, selectTextOnFocus, and edit callbacks; host interaction records focus, replacement, return, and callback order where native dispatch works.',
    element: 'textfield',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textfield
          ref={ctx.targetRef}
          backgroundColor="#FFFFFF"
          border="2 solid #4F46E5"
          borderRadius={12}
          characterLimit={12}
          closesWhenReturnKeyPressed
          color="#111827"
          font="system 19"
          height={56}
          onChange={event =>
            ctx.record(`textfield:onChange:${event.text}:${event.selectionStart}-${event.selectionEnd}`)
          }
          onEditBegin={event => ctx.record(`textfield:onEditBegin:${event.text}`)}
          onEditEnd={event => ctx.record(`textfield:onEditEnd:${event.text}:${event.reason}`)}
          onReturn={event => ctx.record(`textfield:onReturn:${event.text}`)}
          onSelectionChange={event =>
            ctx.record(`textfield:onSelectionChange:${event.selectionStart}-${event.selectionEnd}`)
          }
          onWillChange={event => {
            ctx.record(`textfield:onWillChange:${event.text}`);
            return undefined;
          }}
          onWillDelete={event => ctx.record(`textfield:onWillDelete:${event.text}`)}
          placeholder="selection"
          selectTextOnFocus
          selection={[2, 8]}
          value="selection value"
          width="100%"
        />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      if (getPlatform() === 'android') {
        ctx.record('programmatic selection skipped: Android selection mutation can wedge this snapshot harness');
        await ctx.waitForIdle();
        return;
      }
      ctx.targetRef.setAttribute('selection', [0, 9]);
      ctx.record('programmatic selection set to 0-9');
      await interactTextInput(ctx);
    },
  },
  {
    id: 'textfield-focused-programmatic',
    name: 'TextField programmatic focus field',
    description:
      'Sets the focused interactive attribute programmatically to validate the focus path separately from text replacement.',
    element: 'textfield',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textfield
          ref={ctx.targetRef}
          backgroundColor="#FFFFFF"
          border="2 solid #DC2626"
          borderRadius={12}
          font="system 19"
          height={56}
          onEditBegin={event => ctx.record(`focused:onEditBegin:${event.text}`)}
          onEditEnd={event => ctx.record(`focused:onEditEnd:${event.text}:${event.reason}`)}
          placeholder="programmatic focus"
          value="focus me"
          width="100%"
        />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      if (getPlatform() === 'android') {
        ctx.record('programmatic focus skipped: Android text focus can wedge this snapshot harness');
        await ctx.waitForIdle();
        return;
      }
      ctx.targetRef.setAttribute('focused', true);
      ctx.record('programmatic focused=true');
      await ctx.waitForIdle();
      ctx.targetRef.setAttribute('focused', false);
      ctx.record('programmatic focused=false');
      await ctx.waitForIdle();
    },
  },
  {
    id: 'textview-multiline-gravity',
    name: 'TextView multiline gravity variants',
    description:
      'Renders top, center, and bottom textGravity with distinct lineHeight, alignment, and overflow behavior.',
    element: 'textview',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textview
          backgroundColor="#DBEAFE"
          border="2 solid #60A5FA"
          borderRadius={10}
          color="#1E3A8A"
          enabled={false}
          font="system 15"
          height={112}
          lineHeightAbsolute={21}
          numberOfLines={3}
          textAlign="left"
          textGravity="top"
          textOverflow="ellipsis"
          value="Top gravity textview.\nStarts near top and truncates if too long.\nExtra line hidden."
          width="100%"
        />
        <textview
          backgroundColor="#DCFCE7"
          border="2 solid #86EFAC"
          borderRadius={10}
          color="#14532D"
          enabled={false}
          font="system 15"
          height={112}
          lineHeight={1.35}
          marginTop={10}
          numberOfLines={3}
          textAlign="center"
          textGravity="center"
          value="Center gravity textview.\nLine spacing uses a multiple.\nVertically centered."
          width="100%"
        />
        <textview
          backgroundColor="#FEF3C7"
          border="2 solid #FBBF24"
          borderRadius={10}
          color="#7C2D12"
          enabled={false}
          font="system 15"
          height={112}
          lineHeightAbsolute={21}
          marginTop={10}
          numberOfLines={3}
          textAlign="right"
          textGravity="bottom"
          textOverflow="clip"
          value="Bottom gravity textview.\nRight aligned text.\nClip overflow."
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'textview-background-effect',
    name: 'TextView background effect',
    description:
      'Renders backgroundEffectColor, backgroundEffectBorderRadius, and backgroundEffectPadding behind multiline text.',
    element: 'textview',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textview
          backgroundColor="#FFFFFF"
          backgroundEffectBorderRadius={12}
          backgroundEffectColor="rgba(251, 191, 36, 0.45)"
          backgroundEffectPadding={8}
          border="2 solid #D97706"
          borderRadius={12}
          color="#78350F"
          enabled={false}
          font="system-bold 18"
          height={160}
          numberOfLines={0}
          value="The yellow background effect should follow this multiline text block and exterior padding."
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'textview-decoration-selectable',
    name: 'TextView decoration and selectable states',
    description:
      'Renders selectable dashed underline and non-selectable dotted underline textviews with custom underline geometry.',
    element: 'textview',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textview
          backgroundColor="#FFFFFF"
          border="2 solid #D97706"
          borderRadius={12}
          color="#78350F"
          customUnderlineStyle="2 7 4 -3"
          enabled={false}
          font="system-bold 18"
          height={118}
          numberOfLines={0}
          selectable
          textDecoration="dashed-underline"
          value="Selectable textview with dashed underline."
          width="100%"
        />
        <textview
          backgroundColor="#FFFFFF"
          border="2 solid #7C3AED"
          borderRadius={12}
          color="#4C1D95"
          customUnderlineStyle="2 2 5 -2"
          enabled={false}
          font="system 18"
          height={118}
          marginTop={16}
          numberOfLines={0}
          selectable={false}
          textDecoration="dotted-underline"
          value="Non-selectable dotted underline textview."
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'textview-attributed-outline-background',
    name: 'TextView attributed outline and background',
    description: 'Renders attributed textview content with outer outline and span background only.',
    element: 'textview',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textview
          backgroundColor="#FFFFFF"
          border="2 solid #0EA5E9"
          borderRadius={12}
          color="#111827"
          enabled={false}
          font="system 18"
          height={150}
          numberOfLines={0}
          selectable
          value={attributedTextViewOutlineBackgroundValue()}
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'textview-attributed-decoration-animation',
    name: 'TextView attributed decoration and animation metadata',
    description:
      'Renders one attributed textview span with dotted underline, animationTransform metadata, and onLayout observation.',
    element: 'textview',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textview
          backgroundColor="#FFFFFF"
          backgroundEffectBorderRadius={10}
          backgroundEffectColor="rgba(14, 165, 233, 0.18)"
          backgroundEffectPadding={8}
          border="2 solid #0EA5E9"
          borderRadius={12}
          color="#111827"
          enabled={false}
          font="system 18"
          height={150}
          numberOfLines={0}
          selectable
          value={attributedTextViewDecorationAnimationValue(ctx)}
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'textview-edit-callbacks-return',
    name: 'TextView edit callbacks and return behavior',
    description:
      'Sets editable multiline textview callbacks, returnType=linereturn, closesWhenReturnKeyPressed=false, characterLimit, and native edit interaction where supported.',
    element: 'textview',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textview
          ref={ctx.targetRef}
          backgroundColor="#FFFFFF"
          border="2 solid #7C3AED"
          borderRadius={12}
          characterLimit={120}
          closesWhenReturnKeyPressed={false}
          color="#1F2937"
          enabled
          font="system 17"
          height={170}
          onChange={event => ctx.record(`textview:onChange:${event.text}`)}
          onEditBegin={event => ctx.record(`textview:onEditBegin:${event.text}`)}
          onEditEnd={event => ctx.record(`textview:onEditEnd:${event.text}:${event.reason}`)}
          onReturn={event => ctx.record(`textview:onReturn:${event.text}`)}
          onWillChange={event => {
            ctx.record(`textview:onWillChange:${event.text}`);
            return undefined;
          }}
          onWillDelete={event => ctx.record(`textview:onWillDelete:${event.text}`)}
          returnType="linereturn"
          textAlign="left"
          textGravity="top"
          value="First line of editable text.\nSecond line wraps through the field."
          width="100%"
        />
      </view>;
    },
    interact: interactTextInput,
  },
  {
    id: 'textview-selection-menu',
    name: 'TextView selection and custom edit menu',
    description:
      'Sets selection, selectable, selectTextOnFocus, onSelectionChange, onTextSelectionMenu, and onTextSelectionMenuAction on a textview.',
    element: 'textview',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textview
          ref={ctx.targetRef}
          backgroundColor="#FFFFFF"
          border="2 solid #0EA5E9"
          borderRadius={12}
          color="#1F2937"
          enabled
          font="system 17"
          height={150}
          onSelectionChange={event =>
            ctx.record(`textview:onSelectionChange:${event.selectionStart}-${event.selectionEnd}`)
          }
          onTextSelectionMenu={event => {
            ctx.record(`onTextSelectionMenu:${event.selectedText}`);
            return [{ id: 'mark', title: 'Mark' }];
          }}
          onTextSelectionMenuAction={event => ctx.record(`onTextSelectionMenuAction:${event.id}`)}
          selectable
          selectTextOnFocus
          selection={[0, 6]}
          tintColor="#0EA5E9"
          value="Select this text to show a custom edit menu item."
          width="100%"
        />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      if (getPlatform() === 'android') {
        ctx.record(
          'programmatic textview selection skipped: Android selection mutation can wedge this snapshot harness',
        );
        await ctx.waitForIdle();
        return;
      }
      ctx.targetRef.setAttribute('selection', [7, 16]);
      ctx.record('programmatic textview selection set to 7-16');
      await ctx.waitForIdle();
    },
  },
  {
    id: 'textview-keyboard-configuration',
    name: 'TextView keyboard configuration',
    description:
      'Renders textviews configured with placeholderColor, autocapitalization, autocorrection, keyboardAppearance, and enableInlinePredictions.',
    element: 'textview',
    render: (ctx: IntegrationTestRenderContext) => {
      ctx.record(
        'textview keyboard configuration is native input behavior; fields are not focused for deterministic screenshots.',
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textview
          autocapitalization="sentences"
          autocorrection="default"
          backgroundColor="#FFFFFF"
          border="2 solid #0EA5E9"
          borderRadius={12}
          color="#1F2937"
          enableInlinePredictions={false}
          enabled
          font="system 17"
          height={118}
          keyboardAppearance="light"
          placeholder="sentence placeholder"
          placeholderColor="#94A3B8"
          width="100%"
        />
        <textview
          autocapitalization="none"
          autocorrection="none"
          backgroundColor="#FFFFFF"
          border="2 solid #9333EA"
          borderRadius={12}
          color="#1F2937"
          enableInlinePredictions
          enabled
          font="system 17"
          height={118}
          keyboardAppearance="dark"
          marginTop={16}
          placeholder="raw entry"
          placeholderColor="#A78BFA"
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'textview-focused-programmatic',
    name: 'TextView programmatic focus',
    description:
      'Sets the focused interactive attribute on a textview programmatically, separate from text replacement.',
    element: 'textview',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textview
          ref={ctx.targetRef}
          backgroundColor="#FFFFFF"
          border="2 solid #DC2626"
          borderRadius={12}
          color="#1F2937"
          enabled
          font="system 17"
          height={150}
          onEditBegin={event => ctx.record(`textview-focused:onEditBegin:${event.text}`)}
          onEditEnd={event => ctx.record(`textview-focused:onEditEnd:${event.text}:${event.reason}`)}
          value="programmatic focus target"
          width="100%"
        />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      if (getPlatform() === 'android') {
        ctx.record('programmatic textview focus skipped: Android text focus can wedge this snapshot harness');
        await ctx.waitForIdle();
        return;
      }
      ctx.targetRef.setAttribute('focused', true);
      ctx.record('programmatic textview focused=true');
      await ctx.waitForIdle();
      ctx.targetRef.setAttribute('focused', false);
      ctx.record('programmatic textview focused=false');
      await ctx.waitForIdle();
    },
  },
  {
    id: 'image-object-fit-fill-contain',
    name: 'Image objectFit fill and contain',
    description:
      'Renders fill and contain objectFit values side by side with decode/load observations on the fill image.',
    element: 'image',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout flexDirection="row" flexWrap="wrap">
          <image
            src={res.image}
            objectFit="fill"
            width={150}
            height={110}
            margin={6}
            onAssetLoad={(success, error) => ctx.record(`image fill asset:${success}:${error ?? ''}`)}
            onImageDecoded={(w, h) => ctx.record(`image fill decoded:${w}x${h}`)}
          />
          <image src={res.image} objectFit="contain" width={150} height={110} margin={6} />
        </layout>
      </view>;
    },
  },
  {
    id: 'image-object-fit-cover-none',
    name: 'Image objectFit cover and none',
    description: 'Renders cover and none objectFit values in fixed-size image bounds.',
    element: 'image',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout flexDirection="row" flexWrap="wrap">
          <image src={res.image} objectFit="cover" width={150} height={110} margin={6} />
          <image src={res.image} objectFit="none" width={150} height={110} margin={6} />
        </layout>
      </view>;
    },
  },
  {
    id: 'image-tint',
    name: 'Image tint',
    description: 'Renders one transparent-mask image with tint applied and no filter.',
    element: 'image',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <image
          src={res.tintMask}
          backgroundColor="#FFFFFF"
          border="2 solid #CBD5E1"
          objectFit="contain"
          width={220}
          height={150}
          tint="rgba(59, 130, 246, 0.65)"
        />
      </view>;
    },
  },
  {
    id: 'image-filter-variants',
    name: 'Image filter variants',
    description: 'Renders grayscale and sepia ImageFilters on separate images.',
    element: 'image',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout flexDirection="row" flexWrap="wrap">
          <image
            src={ANIMATED_IMAGE_PNG_DATA_URL}
            backgroundColor="#FFFFFF"
            border="2 solid #CBD5E1"
            objectFit="contain"
            width={145}
            height={105}
            margin={6}
            filter={ImageFilters.grayscale(1)}
            onAssetLoad={(success, error) => ctx.record(`image grayscale asset:${success}:${error ?? ''}`)}
            onImageDecoded={(w, h) => ctx.record(`image grayscale decoded:${w}x${h}`)}
          />
          <image
            src={ANIMATED_IMAGE_PNG_DATA_URL}
            backgroundColor="#FFFFFF"
            border="2 solid #CBD5E1"
            objectFit="contain"
            width={145}
            height={105}
            margin={6}
            filter={ImageFilters.sepia(1)}
            onAssetLoad={(success, error) => ctx.record(`image sepia asset:${success}:${error ?? ''}`)}
            onImageDecoded={(w, h) => ctx.record(`image sepia decoded:${w}x${h}`)}
          />
        </layout>
      </view>;
    },
  },
  {
    id: 'image-content-transform',
    name: 'Image content scale and rotation',
    description: 'Renders contentScaleX, contentScaleY, and contentRotation on image content without RTL flipping.',
    element: 'image',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <image
          src={ANIMATED_IMAGE_PNG_DATA_URL}
          backgroundColor="#FFFFFF"
          border="2 solid #CBD5E1"
          objectFit="cover"
          width={220}
          height={140}
          contentScaleX={0.82}
          contentScaleY={1.18}
          contentRotation={0.24}
          onAssetLoad={(success, error) => ctx.record(`image transform asset:${success}:${error ?? ''}`)}
          onImageDecoded={(w, h) => ctx.record(`image transform decoded:${w}x${h}`)}
        />
      </view>;
    },
  },
  {
    id: 'image-rtl-flip',
    name: 'Image RTL flip flag',
    description: 'Renders one image with flipOnRtl inside an explicit RTL parent so horizontal mirroring is visible.',
    element: 'image',
    coverage: coverage('visual', ['flipOnRtl', 'direction']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout direction="rtl" width="100%">
          <image
            src={ANIMATED_IMAGE_PNG_DATA_URL}
            backgroundColor="#FFFFFF"
            border="2 solid #CBD5E1"
            objectFit="cover"
            width={220}
            height={140}
            flipOnRtl
            onAssetLoad={(success, error) => ctx.record(`image rtl asset:${success}:${error ?? ''}`)}
            onImageDecoded={(w, h) => ctx.record(`image rtl decoded:${w}x${h}`)}
          />
        </layout>
      </view>;
    },
  },
  {
    id: 'image-missing-source-callbacks',
    name: 'Image missing source callbacks',
    description:
      'Uses an invalid image source to exercise onAssetLoad failure reporting without relying on network access.',
    element: 'image',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <image
          src="integration-test-missing-image.png"
          objectFit="contain"
          width={220}
          height={140}
          backgroundColor="#E0F2FE"
          border="2 solid #0284C7"
          onAssetLoad={(success, error) => ctx.record(`image missing asset:${success}:${error ?? ''}`)}
          onImageDecoded={(w, h) => ctx.record(`image missing decoded:${w}x${h}`)}
        />
      </view>;
    },
  },
  {
    id: 'webview-empty-controller',
    name: 'WebView empty controller smoke test',
    description:
      'Creates a webview without a controller to validate native element creation and empty-controller behavior.',
    element: 'webview',
    render: (ctx: IntegrationTestRenderContext) => {
      ctx.record(
        'webview controller fixture intentionally omitted; validates element creation and empty controller path',
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <webview backgroundColor="#E0F2FE" border="2 solid #0284C7" borderRadius={12} height={180} width="100%" />
      </view>;
    },
  },
  {
    id: 'video-missing-source-error',
    name: 'Video missing source error callback',
    description: 'Uses a missing video source with onError to validate the native error callback path.',
    element: 'video',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <video
          backgroundColor="#111827"
          height={180}
          onError={error => ctx.record(`video:onError:${error}`)}
          src="integration-test-missing-video.mp4"
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'video-playback-state',
    name: 'Video playback, seek, and volume configuration',
    description:
      'Sets volume=0, playbackRate=0, and seekToTime on a video element while using a missing source for deterministic offline behavior.',
    element: 'video',
    render: (ctx: IntegrationTestRenderContext) => {
      ctx.record(
        'video playback configuration is applied to the native player when an asset is available; this fixture keeps the source missing to avoid nondeterministic playback frames.',
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <video
          backgroundColor="#1E293B"
          height={180}
          playbackRate={0}
          seekToTime={250}
          src="integration-test-missing-video.mp4"
          volume={0}
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'video-playback-callbacks',
    name: 'Video playback callback wiring',
    description:
      'Registers onVideoLoaded, onBeginPlaying, onProgressUpdated, and onCompleted on a video element without combining them with playback-state assertions.',
    element: 'video',
    render: (ctx: IntegrationTestRenderContext) => {
      ctx.record(
        'video callback wiring is registered; callbacks that require a valid asset may not fire with the deterministic missing source.',
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <video
          backgroundColor="#334155"
          height={180}
          onBeginPlaying={() => ctx.record('video:onBeginPlaying')}
          onCompleted={() => ctx.record('video:onCompleted')}
          onProgressUpdated={(time, duration) =>
            ctx.record(`video:onProgressUpdated:${Math.round(time)}/${Math.round(duration)}`)
          }
          onVideoLoaded={duration => ctx.record(`video:onVideoLoaded:${duration}`)}
          src="integration-test-missing-video.mp4"
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'scroll-vertical-offset',
    name: 'Scroll vertical offset',
    description:
      'Renders vertical scrolling content and programmatically sets staticContentHeight, contentOffsetY, and contentOffsetAnimated=false.',
    element: 'scroll',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <scroll
          ref={ctx.targetRef}
          backgroundColor="#FFFFFF"
          border="2 solid #CBD5E1"
          borderRadius={12}
          height={270}
          onContentSizeChange={event => ctx.record(`scroll vertical:size:${event.width}x${event.height}`)}
          onScroll={event => ctx.record(`scroll vertical:onScroll:${Math.round(event.x)},${Math.round(event.y)}`)}
          onScrollEnd={event => ctx.record(`scroll vertical:onScrollEnd:${Math.round(event.x)},${Math.round(event.y)}`)}
          showsVerticalScrollIndicator
          width="100%"
        >
          <view backgroundColor="#EF4444" height={110} margin={8} />
          <view backgroundColor="#F59E0B" height={110} margin={8} />
          <view backgroundColor="#10B981" height={110} margin={8} />
          <view backgroundColor="#3B82F6" height={110} margin={8} />
        </scroll>
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      ctx.targetRef.setAttribute('staticContentHeight', 500);
      ctx.targetRef.setAttribute('contentOffsetAnimated', false);
      ctx.targetRef.setAttribute('contentOffsetY', 120);
      await ctx.waitForIdle();
    },
  },
  {
    id: 'scroll-horizontal-paging',
    name: 'Scroll horizontal paging',
    description: 'Renders horizontal paging content with programmatic contentOffsetX and horizontal indicator enabled.',
    element: 'scroll',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <scroll
          ref={ctx.targetRef}
          backgroundColor="#FFFFFF"
          border="2 solid #CBD5E1"
          borderRadius={12}
          height={220}
          horizontal
          pagingEnabled
          showsHorizontalScrollIndicator
          width="100%"
        >
          <layout flexDirection="row" height={190} width={720}>
            <view backgroundColor="#EF4444" height={190} margin={8} width={150} />
            <view backgroundColor="#F59E0B" height={190} margin={8} width={150} />
            <view backgroundColor="#10B981" height={190} margin={8} width={150} />
            <view backgroundColor="#3B82F6" height={190} margin={8} width={150} />
          </layout>
        </scroll>
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      ctx.targetRef.setAttribute('staticContentWidth', 720);
      ctx.targetRef.setAttribute('contentOffsetAnimated', false);
      ctx.targetRef.setAttribute('contentOffsetX', 180);
      await ctx.waitForIdle();
    },
  },
  {
    id: 'scroll-indicator-visibility',
    name: 'Scroll indicator visibility',
    description:
      'Renders vertical and horizontal scroll views with showsVerticalScrollIndicator and showsHorizontalScrollIndicator explicitly enabled or disabled.',
    element: 'scroll',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <scroll
          backgroundColor="#FFFFFF"
          border="2 solid #CBD5E1"
          borderRadius={12}
          height={150}
          showsVerticalScrollIndicator
          width="100%"
        >
          <view backgroundColor="#EF4444" height={80} margin={8} />
          <view backgroundColor="#F59E0B" height={80} margin={8} />
          <view backgroundColor="#10B981" height={80} margin={8} />
        </scroll>
        <scroll
          backgroundColor="#FFFFFF"
          border="2 solid #CBD5E1"
          borderRadius={12}
          height={120}
          horizontal
          marginTop={18}
          showsHorizontalScrollIndicator={false}
          width="100%"
        >
          <layout flexDirection="row" height={95} width={560}>
            <view backgroundColor="#EF4444" height={95} margin={6} width={120} />
            <view backgroundColor="#F59E0B" height={95} margin={6} width={120} />
            <view backgroundColor="#10B981" height={95} margin={6} width={120} />
            <view backgroundColor="#3B82F6" height={95} margin={6} width={120} />
          </layout>
        </scroll>
      </view>;
    },
  },
  {
    id: 'scroll-fading-edges',
    name: 'Scroll fading edges',
    description:
      'Renders a vertical scroll with fadingEdgeLength, fadingEdgeStart, fadingEdgeEnd, and androidOnlyEnableExtendedFadingEdge enabled.',
    element: 'scroll',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <scroll
          androidOnlyEnableExtendedFadingEdge
          backgroundColor="#FFFFFF"
          border="2 solid #CBD5E1"
          borderRadius={12}
          fadingEdgeEnd
          fadingEdgeLength={32}
          fadingEdgeStart
          height={260}
          width="100%"
        >
          <view backgroundColor="#EF4444" height={90} margin={8} />
          <view backgroundColor="#F59E0B" height={90} margin={8} />
          <view backgroundColor="#10B981" height={90} margin={8} />
          <view backgroundColor="#3B82F6" height={90} margin={8} />
        </scroll>
      </view>;
    },
  },
  {
    id: 'scroll-bounce-deceleration',
    name: 'Scroll bounce and deceleration settings',
    description:
      'Renders a scroll view with bounces, drag-at-edge bounce flags, small-content bounce flags, circularRatio, scrollEnabled, and decelerationRate=fast.',
    element: 'scroll',
    render: (ctx: IntegrationTestRenderContext) => {
      ctx.record(
        'bounce and deceleration settings are primarily native interaction behavior; screenshot validates configured scroll construction.',
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <scroll
          backgroundColor="#FFFFFF"
          border="2 solid #CBD5E1"
          borderRadius={12}
          bounces
          bouncesFromDragAtEnd
          bouncesFromDragAtStart
          bouncesHorizontalWithSmallContent
          bouncesVerticalWithSmallContent
          circularRatio={0}
          decelerationRate="fast"
          height={220}
          scrollEnabled
          width="100%"
        >
          <view backgroundColor="#EF4444" height={90} margin={8} />
          <view backgroundColor="#F59E0B" height={90} margin={8} />
          <view backgroundColor="#10B981" height={90} margin={8} />
        </scroll>
      </view>;
    },
  },
  {
    id: 'scroll-keyboard-touch-cancel',
    name: 'Scroll keyboard dismissal and touch cancellation',
    description:
      'Renders a scroll view with cancelsTouchesOnScroll, dismissKeyboardOnDrag, dismissKeyboardOnDragMode, and stopScrollingOnTouch configured.',
    element: 'scroll',
    render: (ctx: IntegrationTestRenderContext) => {
      ctx.record(
        'keyboard dismissal and touch cancellation are native drag behavior; screenshot validates configured scroll construction.',
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <scroll
          backgroundColor="#FFFFFF"
          border="2 solid #CBD5E1"
          borderRadius={12}
          cancelsTouchesOnScroll
          dismissKeyboardOnDrag
          dismissKeyboardOnDragMode="touch-exit-below"
          height={220}
          stopScrollingOnTouch
          width="100%"
        >
          <view backgroundColor="#EF4444" height={90} margin={8} />
          <view backgroundColor="#F59E0B" height={90} margin={8} />
          <view backgroundColor="#10B981" height={90} margin={8} />
        </scroll>
      </view>;
    },
  },
  {
    id: 'scroll-viewport-extension',
    name: 'Scroll viewport extension',
    description: 'Renders a scroll view with viewportExtensionTop, Right, Bottom, and Left configured.',
    element: 'scroll',
    render: (ctx: IntegrationTestRenderContext) => {
      ctx.record(
        'viewportExtension expands native pre-render/visibility bounds; visual output confirms the configured scroll exists.',
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <scroll
          backgroundColor="#FFFFFF"
          border="2 solid #CBD5E1"
          borderRadius={12}
          height={220}
          viewportExtensionBottom={16}
          viewportExtensionLeft={16}
          viewportExtensionRight={16}
          viewportExtensionTop={16}
          width="100%"
        >
          <view backgroundColor="#EF4444" height={90} margin={8} />
          <view backgroundColor="#F59E0B" height={90} margin={8} />
          <view backgroundColor="#10B981" height={90} margin={8} />
        </scroll>
      </view>;
    },
  },
  {
    id: 'scroll-drag-callbacks-anchor',
    name: 'Scroll drag callbacks and anchor',
    description:
      'Dispatches a drag to a scroll view with onDragStart, onDragEnding, onDragEnd, maintainScrollAnchor, and static content sizing.',
    element: 'scroll',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <scroll
          ref={ctx.targetRef}
          backgroundColor="#FFFFFF"
          border="2 solid #CBD5E1"
          borderRadius={12}
          height={260}
          onDragEnd={event => ctx.record(`scroll:onDragEnd:${Math.round(event.x)},${Math.round(event.y)}`)}
          onDragEnding={event => {
            ctx.record(`scroll:onDragEnding:${Math.round(event.x)},${Math.round(event.y)}`);
            return undefined;
          }}
          onDragStart={event => ctx.record(`scroll:onDragStart:${Math.round(event.x)},${Math.round(event.y)}`)}
          onScroll={event => ctx.record(`scroll:onScroll:${Math.round(event.x)},${Math.round(event.y)}`)}
          onScrollEnd={event => ctx.record(`scroll:onScrollEnd:${Math.round(event.x)},${Math.round(event.y)}`)}
          width="100%"
        >
          <view backgroundColor="#EF4444" height={110} margin={8} />
          <view backgroundColor="#F59E0B" height={110} margin={8} />
          <view backgroundColor="#10B981" height={110} margin={8} scrollAnchorPosition="top" />
          <view backgroundColor="#3B82F6" height={110} margin={8} />
        </scroll>
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      ctx.targetRef.setAttribute('staticContentHeight', 500);
      ctx.targetRef.setAttribute('maintainScrollAnchor', true);
      await interactDrag(ctx);
    },
  },
  {
    id: 'scroll-nested',
    name: 'Nested scroll views',
    description:
      'Renders a vertical scroll containing an inner horizontal scroll to exercise nested native scroll handling.',
    element: 'scroll',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <scroll backgroundColor="#FFFFFF" border="2 solid #CBD5E1" borderRadius={12} height={300} width="100%">
          <view backgroundColor="#DBEAFE" height={80} margin={8} />
          <scroll
            backgroundColor="#F8FAFC"
            border="2 solid #94A3B8"
            borderRadius={10}
            height={130}
            horizontal
            margin={8}
            width="100%"
          >
            <layout flexDirection="row" height={100} width={560}>
              <view backgroundColor="#EF4444" height={100} margin={6} width={120} />
              <view backgroundColor="#F59E0B" height={100} margin={6} width={120} />
              <view backgroundColor="#10B981" height={100} margin={6} width={120} />
              <view backgroundColor="#3B82F6" height={100} margin={6} width={120} />
            </layout>
          </scroll>
          <view backgroundColor="#DCFCE7" height={80} margin={8} />
        </scroll>
      </view>;
    },
  },
  {
    id: 'spinner-color',
    name: 'Spinner color',
    description: 'Renders a native spinner with an explicit red color.',
    element: 'spinner',
    expectedFailureOnPlatforms: ['android'],
    expectedFailureReason:
      'Android spinner snapshot renders blank even though ValdiSpinnerView should show a tinted indeterminate ProgressBar',
    skipSnapshotOnPlatforms: ['ios'],
    skipSnapshotReason: 'spinner snapshot skipped: iOS UIActivityIndicatorView snapshot hangs the harness',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <spinner color="#DC2626" height={80} width={80} />
      </view>;
    },
  },
  {
    id: 'blur-light-and-dark',
    name: 'Blur light and dark materials',
    description: 'Renders light and systemMaterialDark blurStyle overlays over deterministic gradient content.',
    element: 'blur',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view height={240} width="100%">
          <view background="linear-gradient(45deg, #EF4444, #FACC15, #22C55E, #3B82F6)" height={240} width="100%" />
          <blur blurStyle="light" height={90} left={22} position="absolute" top={24} width={140} />
          <blur blurStyle="systemMaterialDark" height={90} left={178} position="absolute" top={120} width={140} />
        </view>
      </view>;
    },
  },
  {
    id: 'blur-material-variants',
    name: 'Blur material style variants',
    description:
      'Renders regular, prominent, systemThinMaterialLight, and systemChromeMaterialDark blurStyle overlays.',
    element: 'blur',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view height={280} width="100%">
          <view
            background="linear-gradient(135deg, #EF4444, #F59E0B, #22C55E, #0EA5E9, #7C3AED)"
            height={280}
            width="100%"
          />
          <blur blurStyle="regular" height={76} left={18} position="absolute" top={18} width={132} />
          <blur blurStyle="prominent" height={76} left={174} position="absolute" top={18} width={132} />
          <blur blurStyle="systemThinMaterialLight" height={76} left={18} position="absolute" top={144} width={132} />
          <blur blurStyle="systemChromeMaterialDark" height={76} left={174} position="absolute" top={144} width={132} />
        </view>
      </view>;
    },
  },
  {
    id: 'shape-fill-and-stroke',
    name: 'Shape fill and stroke',
    description: 'Renders a filled triangle with strokeWidth, strokeColor, fillColor, round cap, and round join.',
    element: 'shape',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <shape
          fillColor="#93C5FD"
          path={trianglePath}
          strokeCap="round"
          strokeColor="#1D4ED8"
          strokeJoin="round"
          strokeWidth={8}
          width={180}
          height={180}
        />
      </view>;
    },
  },
  {
    id: 'shape-stroke-caps',
    name: 'Shape stroke cap variants',
    description: 'Renders butt, round, and square strokeCap values on the same open line path.',
    element: 'shape',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <shape path={linePath} strokeCap="butt" strokeColor="#DC2626" strokeWidth={16} width={260} height={70} />
        <shape path={linePath} strokeCap="round" strokeColor="#2563EB" strokeWidth={16} width={260} height={70} />
        <shape path={linePath} strokeCap="square" strokeColor="#047857" strokeWidth={16} width={260} height={70} />
      </view>;
    },
  },
  {
    id: 'shape-stroke-joins',
    name: 'Shape stroke join variants',
    description: 'Renders miter, round, and bevel strokeJoin values on an angled open path.',
    element: 'shape',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout flexDirection="row" flexWrap="wrap">
          <shape
            path={cornerPath}
            strokeJoin="miter"
            strokeColor="#DC2626"
            strokeWidth={12}
            width={95}
            height={95}
            margin={6}
          />
          <shape
            path={cornerPath}
            strokeJoin="round"
            strokeColor="#2563EB"
            strokeWidth={12}
            width={95}
            height={95}
            margin={6}
          />
          <shape
            path={cornerPath}
            strokeJoin="bevel"
            strokeColor="#047857"
            strokeWidth={12}
            width={95}
            height={95}
            margin={6}
          />
        </layout>
      </view>;
    },
  },
  {
    id: 'shape-stroke-range',
    name: 'Shape stroke range',
    description: 'Renders the same curve with strokeStart and strokeEnd to validate partial path drawing.',
    element: 'shape',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <shape
          path={curvePath}
          strokeCap="round"
          strokeColor="#7C2D12"
          strokeEnd={0.84}
          strokeJoin="round"
          strokeStart={0.08}
          strokeWidth={12}
          width={220}
          height={160}
        />
      </view>;
    },
  },
  {
    id: 'shape-fill-gradient',
    name: 'Shape fill gradient',
    description: 'Renders a closed shape with fillGradient and a contrasting stroke.',
    element: 'shape',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <shape
          fillGradient="linear-gradient(#F97316, #FDE68A)"
          path={trianglePath}
          strokeColor="#7C2D12"
          strokeJoin="bevel"
          strokeWidth={8}
          width={180}
          height={180}
        />
      </view>;
    },
  },
  {
    id: 'animatedimage-object-fit',
    name: 'AnimatedImage object fit variants',
    description:
      'Renders contain, cover, fill, and none objectFit values through the animatedimage loader with a bundled Lottie resource.',
    element: 'animatedimage',
    render: (ctx: IntegrationTestRenderContext) => {
      const src = lottieResourceSource(ctx);
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout flexDirection="row" flexWrap="wrap">
          <animatedimage
            advanceRate={0}
            backgroundColor="#FFFFFF"
            border="2 solid #7C3AED"
            borderRadius={10}
            currentTime={0.5}
            height={110}
            loop
            objectFit="contain"
            src={src}
            width={110}
            margin={6}
          />
          <animatedimage
            advanceRate={0}
            backgroundColor="#FFFFFF"
            border="2 solid #7C3AED"
            borderRadius={10}
            currentTime={0.5}
            height={110}
            loop
            objectFit="cover"
            src={src}
            width={110}
            margin={6}
          />
          <animatedimage
            advanceRate={0}
            backgroundColor="#FFFFFF"
            border="2 solid #7C3AED"
            borderRadius={10}
            currentTime={0.5}
            height={110}
            loop
            objectFit="fill"
            src={src}
            width={110}
            margin={6}
          />
          <animatedimage
            advanceRate={0}
            backgroundColor="#FFFFFF"
            border="2 solid #7C3AED"
            borderRadius={10}
            currentTime={0.5}
            height={110}
            loop
            objectFit="none"
            src={src}
            width={110}
            margin={6}
          />
        </layout>
      </view>;
    },
    expectedFailureOnPlatforms: ['android'],
    expectedFailureReason:
      'Android AnimatedImageView decodes the Lottie resource but its SnapDrawing/TextureView content is not captured in takeSnapshot.',
  },
  {
    id: 'animatedimage-current-time-window',
    name: 'AnimatedImage current time and time window',
    description:
      'Renders currentTime with animationStartTime, animationEndTime, loop=false, and advanceRate=0 using the bundled Lottie resource.',
    element: 'animatedimage',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <animatedimage
          advanceRate={0}
          animationEndTime={1.5}
          animationStartTime={0}
          backgroundColor="#FFFFFF"
          border="2 solid #1D4ED8"
          borderRadius={14}
          currentTime={1.0}
          height={170}
          loop={false}
          objectFit="contain"
          src={lottieResourceSource(ctx)}
          width={170}
        />
      </view>;
    },
    interact: async (context: IntegrationTestInteractionContext) => {
      context.record(
        'animatedimage timing fixture uses the bundled Lottie resource so every platform receives bytes through the animated-image asset loader.',
      );
      await context.waitForIdle();
    },
    expectedFailureOnPlatforms: ['android'],
    expectedFailureReason:
      'Android AnimatedImageView decodes the Lottie resource but its SnapDrawing/TextureView content is not captured in takeSnapshot.',
  },
  {
    id: 'animatedimage-callbacks',
    name: 'AnimatedImage asset, decode, and progress callbacks',
    description: 'Registers onAssetLoad, onImageDecoded, and onProgress on one paused animatedimage fixture.',
    element: 'animatedimage',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <animatedimage
          advanceRate={0}
          backgroundColor="#ECFEFF"
          border="2 solid #0891B2"
          borderRadius={14}
          currentTime={0.5}
          height={170}
          objectFit="contain"
          onAssetLoad={(success, error) => ctx.record(`animatedimage:onAssetLoad:${success}:${error ?? ''}`)}
          onImageDecoded={(w, h) => ctx.record(`animatedimage:onImageDecoded:${w}x${h}`)}
          onProgress={event =>
            ctx.record(
              `animatedimage:onProgress:${Math.round(event.time * 100) / 100}/${Math.round(event.duration * 100) / 100}`,
            )
          }
          src={lottieResourceSource(ctx)}
          width={170}
        />
      </view>;
    },
    interact: async (context: IntegrationTestInteractionContext) => {
      await context.waitForIdle();
    },
    expectedFailureOnPlatforms: ['android'],
    expectedFailureReason:
      'Android AnimatedImageView decodes the Lottie resource but its SnapDrawing/TextureView content is not captured in takeSnapshot.',
  },
  {
    id: 'style-application-overrides',
    name: 'Style object application and inline overrides',
    description:
      'Applies reusable Style objects to layout, view, label, textfield, and textview, then overrides selected visual attributes inline.',
    element: 'view',
    coverage: [
      { kind: 'node-output', attributes: ['style'] },
      {
        kind: 'visual',
        attributes: ['backgroundColor', 'border', 'borderRadius', 'color', 'font', 'height', 'padding', 'width'],
      },
    ],
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout style={styleLayoutBase} height={420} gap={10}>
          <view style={styleViewCard} backgroundColor="#DCFCE7" border="3 solid #047857">
            <label style={styleLabelSample} color="#065F46" value="style applied, inline color wins" />
          </view>
          <textfield style={styleTextFieldSample} enabled={false} value="textfield styled fixture" />
          <textview style={styleTextViewSample} enabled={false} value="textview styled fixture with inline value" />
        </layout>
      </view>;
    },
  },
  {
    id: 'view-background-solid-and-gradient-stops',
    name: 'View solid background and gradient stops',
    description:
      'Renders backgroundColor, background as a solid semantic-style value, and background as a custom-stop linear gradient.',
    element: 'view',
    coverage: coverage('visual', ['backgroundColor', 'background']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#DBEAFE" borderRadius={14} height={88} marginBottom={14} width="100%" />
        <view background="#DCFCE7" borderRadius={14} height={88} marginBottom={14} width="100%" />
        <view
          background="linear-gradient(90deg, #0EA5E9 0%, #A855F7 46%, #F97316 100%)"
          borderRadius={14}
          height={88}
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'view-border-radius-edge-cases',
    name: 'View border radius edge cases',
    description: 'Renders numeric, percent, and four-value borderRadius forms with contrasting borders.',
    element: 'view',
    coverage: coverage('visual', ['border', 'borderWidth', 'borderColor', 'borderRadius']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view
          backgroundColor="#DBEAFE"
          border="4 solid #1D4ED8"
          borderRadius={0}
          height={78}
          marginBottom={14}
          width="100%"
        />
        <view
          backgroundColor="#DCFCE7"
          border="4 dashed #047857"
          borderRadius="50%"
          height={110}
          marginBottom={14}
          width={160}
        />
        <view backgroundColor="#FEF3C7" border="4 dotted #92400E" borderRadius="0 28 48 8" height={96} width="100%" />
      </view>;
    },
  },
  {
    id: 'view-complex-shadow',
    name: 'View complex shadow',
    description:
      'Renders simple and complex boxShadow syntax on rounded views so dynamic-shadow parsing is covered distinctly.',
    element: 'view',
    coverage: coverage('visual', ['boxShadow', 'borderRadius']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={26}>
        <view
          backgroundColor="#FFFFFF"
          borderRadius={22}
          boxShadow="0 8 18 rgba(15, 23, 42, 0.28)"
          height={118}
          marginBottom={28}
          width="100%"
        >
          <label value="simple shadow" color="#0F172A" font="system-bold 20" margin={24} />
        </view>
        <view
          backgroundColor="#FDE68A"
          borderRadius={22}
          boxShadow="complex 0 12 28 rgba(120, 53, 15, 0.35)"
          height={118}
          width="100%"
        >
          <label value="complex shadow" color="#78350F" font="system-bold 20" margin={24} />
        </view>
      </view>;
    },
  },
  {
    id: 'view-transform-precedence-origin',
    name: 'View transform precedence and origin',
    description:
      'Sets individual transform attributes together with transform and transformOrigin; transform should take precedence while origin still applies.',
    element: 'view',
    coverage: coverage('visual', [
      'scaleX',
      'scaleY',
      'rotation',
      'translationX',
      'translationY',
      'transform',
      'transformOrigin',
    ]),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#E5E7EB" borderRadius={12} height={260} width="100%">
          <view
            backgroundColor="#7C3AED"
            borderRadius={16}
            height={126}
            rotation={-0.6}
            scaleX={1.8}
            scaleY={0.55}
            transform="translateX(60) translateY(54) scale(0.92) rotate(0.32)"
            transformOrigin="top left"
            translationX={-70}
            translationY={-44}
            width={176}
          />
        </view>
      </view>;
    },
  },
  {
    id: 'view-mask-opacity-extremes',
    name: 'View mask opacity extremes',
    description: 'Compares maskOpacity=0 and maskOpacity=1 with the same maskPath over striped content.',
    element: 'view',
    coverage: coverage('visual', ['maskOpacity', 'maskPath']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout flexDirection="row" gap={16}>
          <view height={180} width={140}>
            <view backgroundColor="#EF4444" height={60} width="100%" />
            <view backgroundColor="#FACC15" height={60} width="100%" />
            <view backgroundColor="#2563EB" height={60} maskOpacity={0} maskPath={roundedMask} width="100%" />
          </view>
          <view height={180} width={140}>
            <view backgroundColor="#EF4444" height={60} width="100%" />
            <view backgroundColor="#FACC15" height={60} width="100%" />
            <view backgroundColor="#2563EB" height={60} maskOpacity={1} maskPath={roundedMask} width="100%" />
          </view>
        </layout>
      </view>;
    },
  },
  {
    id: 'view-touch-disabled-predicate-rejection',
    name: 'View disabled touch and predicate rejection',
    description:
      'Renders one touch-disabled view and dispatches a tap to a sibling whose onTapPredicate rejects the gesture.',
    element: 'view',
    coverage: [
      { kind: 'visual', attributes: ['touchEnabled'] },
      { kind: 'interaction', attributes: ['onTapPredicate', 'onTapDisabled', 'onTap'] },
    ],
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view
          backgroundColor="#94A3B8"
          borderRadius={18}
          height={126}
          marginBottom={18}
          touchEnabled={false}
          width="100%"
        >
          <label value="touch disabled" color="#FFFFFF" font="system-bold 22" margin={24} />
        </view>
        <view
          ref={ctx.targetRef}
          backgroundColor="#BE123C"
          borderRadius={18}
          height={126}
          onTap={() => ctx.record('predicate rejection failed:onTap fired')}
          onTapDisabled={false}
          onTapPredicate={event => {
            ctx.record(`rejecting onTapPredicate:${summarizeEvent(event)}`);
            return false;
          }}
          width="100%"
        >
          <label value="predicate rejects tap" color="#FFFFFF" font="system-bold 22" margin={24} />
        </view>
      </view>;
    },
    interact: interactTap,
  },
  {
    id: 'view-lifecycle-destroy-recreate',
    name: 'View lifecycle destroy and recreate',
    description:
      'Uses a stateful fixture to remove and recreate an allowReuse=false child and records lifecycle callbacks.',
    element: 'view',
    coverage: coverage('interaction', ['allowReuse', 'onViewCreate', 'onViewDestroy', 'onViewChange']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <LifecycleRecreateFixture renderContext={ctx} />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      await ctx.waitForIdle();
    },
  },
  {
    id: 'view-cursor-web-smoke',
    name: 'View cursor web smoke',
    description:
      'Sets cursor on fixed view tiles; this is a web-only visual attribute and node-output smoke elsewhere.',
    element: 'view',
    coverage: coverage('node-output', ['cursor']),
    render: (ctx: IntegrationTestRenderContext) => {
      if (getPlatform() !== 'web') {
        ctx.record('cursor visual behavior is web-only; non-web platforms record attribute wiring only.');
      }
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view
          backgroundColor="#DBEAFE"
          border="2 solid #1D4ED8"
          borderRadius={14}
          cursor="pointer"
          height={90}
          marginBottom={16}
          width="100%"
        >
          <label value="cursor pointer" color="#1D4ED8" font="system-bold 20" margin={22} />
        </view>
        <view
          backgroundColor="#FEE2E2"
          border="2 solid #BE123C"
          borderRadius={14}
          cursor="not-allowed"
          height={90}
          width="100%"
        >
          <label value="cursor not-allowed" color="#BE123C" font="system-bold 20" margin={22} />
        </view>
      </view>;
    },
  },
  {
    id: 'label-selection-menu-smoke',
    name: 'Label selection menu smoke',
    description:
      'Sets selectable label selection and custom menu callbacks; native menu action invocation remains host-blocked.',
    element: 'label',
    coverage: [
      { kind: 'node-output', attributes: ['selectable', 'selection', 'onSelectionChange', 'onTextSelectionMenu'] },
      { kind: 'blocked-needs-host', attributes: ['onTextSelectionMenuAction'] },
    ],
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <label
          ref={ctx.targetRef}
          backgroundColor="#FFFFFF"
          border="2 solid #0EA5E9"
          borderRadius={12}
          color="#075985"
          font="system 20"
          numberOfLines={0}
          onSelectionChange={event =>
            ctx.record(`label:onSelectionChange:${event.selectionStart}-${event.selectionEnd}`)
          }
          onTextSelectionMenu={event => {
            ctx.record(`label:onTextSelectionMenu:${event.selectedText}`);
            return [{ id: 'flag', title: 'Flag' }];
          }}
          onTextSelectionMenuAction={event => ctx.record(`label:onTextSelectionMenuAction:${event.id}`)}
          selectable
          selection={[0, 6]}
          value="Select this label text with a custom menu action."
          width="100%"
        />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      if (getPlatform() === 'android') {
        ctx.record('programmatic label selection skipped: Android selection mutation can wedge this snapshot harness');
        await ctx.waitForIdle();
        return;
      }
      ctx.targetRef.setAttribute('selection', [7, 17]);
      ctx.record('programmatic label selection set to 7-17; menu action still requires native host support');
      await ctx.waitForIdle();
    },
  },
  {
    id: 'label-font-spacing-variants',
    name: 'Label font and spacing variants',
    description:
      'Renders every supported system font weight and italic variant alongside unscaled and letter-spacing variations.',
    element: 'label',
    coverage: coverage('visual', ['value', 'font', 'color', 'letterSpacing']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <label value="system 18" color="#111827" font="system 18" width="100%" />
        <label value="system-medium 18" color="#111827" font="system-medium 18" marginTop={9} width="100%" />
        <label value="system-semibold 18" color="#111827" font="system-semibold 18" marginTop={9} width="100%" />
        <label value="system-demi-bold 18" color="#111827" font="system-demi-bold 18" marginTop={9} width="100%" />
        <label value="system-bold 18" color="#111827" font="system-bold 18" marginTop={9} width="100%" />
        <label value="system-italic 18" color="#111827" font="system-italic 18" marginTop={9} width="100%" />
        <label
          value="system-medium-italic 18"
          color="#111827"
          font="system-medium-italic 18"
          marginTop={9}
          width="100%"
        />
        <label
          value="system-semibold-italic 18"
          color="#111827"
          font="system-semibold-italic 18"
          marginTop={9}
          width="100%"
        />
        <label
          value="system-demi-bold-italic 18"
          color="#111827"
          font="system-demi-bold-italic 18"
          marginTop={9}
          width="100%"
        />
        <label
          value="system-bold-italic 18"
          color="#111827"
          font="system-bold-italic 18"
          marginTop={9}
          width="100%"
        />
        <label
          value="system-bold 22 unscaled"
          color="#1D4ED8"
          font="system-bold 22 unscaled 22"
          letterSpacing={0.5}
          marginTop={9}
          width="100%"
        />
        <label
          value="system-italic wide spacing"
          color="#047857"
          font="system-italic 21"
          letterSpacing={2.5}
          marginTop={9}
          width="100%"
        />
        <label
          value="small max-scaled label"
          color="#BE123C"
          font="system-bold 18 unscaled 18"
          letterSpacing={1}
          marginTop={9}
          width="100%"
        />
      </view>;
    },
  },
  {
    id: 'label-multiline-justify-lineheight-precedence',
    name: 'Label multiline justification line-height precedence',
    description:
      'Sets justified multiline text with both lineHeight and lineHeightAbsolute; lineHeightAbsolute should determine line spacing.',
    element: 'label',
    coverage: coverage('visual', ['numberOfLines', 'textAlign', 'lineHeight', 'lineHeightAbsolute']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#FFFFFF" border="2 solid #CBD5E1" borderRadius={12} padding={12} width="100%">
          <label
            color="#1E3A8A"
            font="system 17"
            lineHeightAbsolute={28}
            lineHeight={1.8}
            numberOfLines={0}
            textAlign="justified"
            value="Justified multiline label text wraps across several lines. The explicit lineHeightAbsolute should take precedence over the relative lineHeight value in this fixture."
            width="100%"
          />
        </view>
      </view>;
    },
  },
  {
    id: 'label-overflow-multiline',
    name: 'Label multiline overflow variants',
    description: 'Compares multiline ellipsis and clip overflow behavior with identical constrained text.',
    element: 'label',
    coverage: coverage('visual', ['numberOfLines', 'textOverflow']),
    render: (ctx: IntegrationTestRenderContext) => {
      const value =
        'Multiline overflow text should wrap into two visible lines and then demonstrate how hidden content is signaled by the overflow attribute.';
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#FEE2E2" borderRadius={10} height={86} padding={8} width="100%">
          <label
            color="#991B1B"
            font="system-bold 17"
            numberOfLines={2}
            textOverflow="ellipsis"
            value={value}
            width="100%"
          />
        </view>
        <view backgroundColor="#E0E7FF" borderRadius={10} height={86} marginTop={16} padding={8} width="100%">
          <label
            color="#3730A3"
            font="system-bold 17"
            numberOfLines={2}
            textOverflow="clip"
            value={value}
            width="100%"
          />
        </view>
      </view>;
    },
  },
  {
    id: 'label-inline-view-alignments',
    name: 'Label inline view alignments',
    description: 'Embeds label children with top, center, bottom, and baseline inline vertical alignment.',
    element: 'label',
    coverage: coverage('visual', ['value', 'children']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#FFFFFF" border="1 solid #CBD5E1" borderRadius={10} padding={14} width="100%">
          <label color="#334155" font="system-bold 14" marginBottom={12} value="Label inline alignments" width="100%" />
          <label
            color="#0F172A"
            font="system 23"
            lineHeight={1.45}
            numberOfLines={0}
            value={attributedInlineViewAlignmentValue()}
            width="100%"
          >
            <InlineViewMarker color="#DC2626" title="TOP" />
            <InlineViewMarker color="#7C3AED" title="MID" />
            <InlineViewMarker color="#047857" title="BOT" />
            <InlineViewMarker color="#0F766E" title="BASE" />
          </label>
        </view>
      </view>;
    },
  },
  {
    id: 'label-inline-citation-pill-metrics',
    name: 'Label inline citation pill metrics',
    description:
      'Reproduces an 18pt citation badge with a 12pt favicon, 12/16 caption labels, and separate source count.',
    element: 'label',
    coverage: coverage('visual', ['value', 'children', 'lineHeight']),
    render: (ctx: IntegrationTestRenderContext) => {
      const value = attributedCitationPillValue();
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#FFFFFF" border="1 solid #CBD5E1" borderRadius={10} padding={14} width="100%">
          <label color="#334155" font="system-bold 14" marginBottom={12} value="Citation pill text metrics" width="100%" />
          <label
            color="#0F172A"
            font="system 17"
            lineHeight={26 / 17}
            numberOfLines={0}
            value={value}
            width="100%"
          >
            <CitationPillFixture domain="axios.com" sourceCount={3} />
          </label>
        </view>
      </view>;
    },
  },
  {
    id: 'textview-inline-view-alignments',
    name: 'TextView inline view alignments',
    description: 'Embeds textview children with top, center, bottom, and baseline inline vertical alignment.',
    element: 'textview',
    coverage: coverage('visual', ['value', 'children']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#FFFFFF" border="1 solid #CBD5E1" borderRadius={10} padding={14} width="100%">
          <label color="#334155" font="system-bold 14" marginBottom={12} value="TextView inline alignments" width="100%" />
          <textview
            backgroundColor="#FFFFFF"
            color="#0F172A"
            enabled={false}
            font="system 23"
            height={150}
            lineHeight={1.45}
            numberOfLines={0}
            value={attributedInlineViewAlignmentValue()}
            width="100%"
          >
            <InlineViewMarker color="#DC2626" title="TOP" />
            <InlineViewMarker color="#7C3AED" title="MID" />
            <InlineViewMarker color="#047857" title="BOT" />
            <InlineViewMarker color="#0F766E" title="BASE" />
          </textview>
        </view>
      </view>;
    },
  },
  {
    id: 'label-inline-view-ltr-rtl',
    name: 'Label inline view LTR and RTL',
    description: 'Appends an inline label child at the end of LTR and RTL text runs.',
    element: 'label',
    coverage: coverage('visual', ['value', 'children', 'direction']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#FFFFFF" border="1 solid #CBD5E1" borderRadius={10} padding={14} width="100%">
          <label color="#334155" font="system-bold 14" marginBottom={12} value="Label LTR / RTL append" width="100%" />
          <view alignItems="center" flexDirection="row" marginBottom={12} width="100%">
            <label color="#475569" font="system-bold 12" marginRight={10} textAlign="center" value="LTR" width={34} />
            <view backgroundColor="#F8FAFC" border="1 solid #CBD5E1" borderRadius={8} direction="ltr" flexGrow={1} padding={10}>
              <label
                color="#0F172A"
                font="system 22"
                lineHeight={1.35}
                numberOfLines={0}
                value={attributedInlineViewLtrValue()}
                width="100%"
              >
                <InlineViewEndMarker color="#2563EB" />
              </label>
            </view>
          </view>
          <view alignItems="center" flexDirection="row" width="100%">
            <label color="#475569" font="system-bold 12" marginRight={10} textAlign="center" value="RTL" width={34} />
            <view backgroundColor="#F8FAFC" border="1 solid #CBD5E1" borderRadius={8} direction="rtl" flexGrow={1} padding={10}>
              <label
                color="#0F172A"
                font="system 22"
                lineHeight={1.35}
                numberOfLines={0}
                value={attributedInlineViewRtlValue()}
                width="100%"
              >
                <InlineViewEndMarker color="#7C3AED" />
              </label>
            </view>
          </view>
        </view>
      </view>;
    },
  },
  {
    id: 'textview-inline-view-ltr-rtl',
    name: 'TextView inline view LTR and RTL',
    description: 'Appends an inline textview child at the end of LTR and RTL text runs.',
    element: 'textview',
    coverage: coverage('visual', ['value', 'children', 'direction']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view backgroundColor="#FFFFFF" border="1 solid #CBD5E1" borderRadius={10} padding={14} width="100%">
          <label color="#334155" font="system-bold 14" marginBottom={12} value="TextView LTR / RTL append" width="100%" />
          <view alignItems="center" flexDirection="row" marginBottom={12} width="100%">
            <label color="#475569" font="system-bold 12" marginRight={10} textAlign="center" value="LTR" width={34} />
            <view backgroundColor="#F8FAFC" border="1 solid #CBD5E1" borderRadius={8} direction="ltr" flexGrow={1} padding={10}>
              <textview
                backgroundColor="#F8FAFC"
                color="#0F172A"
                enabled={false}
                font="system 22"
                height={42}
                lineHeight={1.35}
                numberOfLines={0}
                value={attributedInlineViewLtrValue()}
                width="100%"
              >
                <InlineViewEndMarker color="#2563EB" />
              </textview>
            </view>
          </view>
          <view alignItems="center" flexDirection="row" width="100%">
            <label color="#475569" font="system-bold 12" marginRight={10} textAlign="center" value="RTL" width={34} />
            <view backgroundColor="#F8FAFC" border="1 solid #CBD5E1" borderRadius={8} direction="rtl" flexGrow={1} padding={10}>
              <textview
                backgroundColor="#F8FAFC"
                color="#0F172A"
                enabled={false}
                font="system 22"
                height={42}
                lineHeight={1.35}
                numberOfLines={0}
                value={attributedInlineViewRtlValue()}
                width="100%"
              >
                <InlineViewEndMarker color="#7C3AED" />
              </textview>
            </view>
          </view>
        </view>
      </view>;
    },
  },
  {
    id: 'label-inline-view-stateful-resize',
    name: 'Label inline view stateful resize',
    description:
      'Renders collapsed and tapped-expanded inline label children in separate rows after a nested child state update.',
    element: 'label',
    coverage: [
      { kind: 'visual', attributes: ['value', 'children'] },
      { kind: 'interaction', attributes: ['onTap'] },
    ],
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <InlineViewExpandingRow
          interactive={false}
          kind="label"
          renderContext={ctx}
          title="Before tap: collapsed reference"
        />
        <view height={14} width="100%" />
        <InlineViewExpandingRow
          interactive
          kind="label"
          renderContext={ctx}
          title="After tap: interactive row expands"
        />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      await ctx.waitForIdle();
    },
  },
  {
    id: 'textview-inline-view-stateful-resize',
    name: 'TextView inline view stateful resize',
    description:
      'Renders collapsed and tapped-expanded inline textview children in separate rows after a nested child state update.',
    element: 'textview',
    coverage: [
      { kind: 'visual', attributes: ['value', 'children'] },
      { kind: 'interaction', attributes: ['onTap'] },
    ],
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <InlineViewExpandingRow
          interactive={false}
          kind="textview"
          renderContext={ctx}
          title="Before tap: collapsed reference"
        />
        <view height={14} width="100%" />
        <InlineViewExpandingRow
          interactive
          kind="textview"
          renderContext={ctx}
          title="After tap: interactive row expands"
        />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      await ctx.waitForIdle();
    },
  },
  {
    id: 'textfield-content-type-return-matrix',
    name: 'TextField content type and return key matrix',
    description: 'Renders phone, password-number, and no-suggestions fields with different returnKeyText values.',
    element: 'textfield',
    coverage: coverage('node-output', ['contentType', 'returnKeyText']),
    render: (ctx: IntegrationTestRenderContext) => {
      ctx.record(
        'contentType and returnKeyText primarily configure native keyboard chrome; fields stay unfocused for deterministic screenshots.',
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textfield style={styleTextFieldSample} contentType="phoneNumber" returnKeyText="join" value="+1 555 0100" />
        <textfield
          style={styleTextFieldSample}
          contentType="passwordNumber"
          marginTop={14}
          returnKeyText="next"
          value="123456"
        />
        <textfield
          style={styleTextFieldSample}
          contentType="noSuggestions"
          marginTop={14}
          returnKeyText="send"
          value="raw input"
        />
      </view>;
    },
  },
  {
    id: 'textfield-willchange-delete',
    name: 'TextField will-change and delete callbacks',
    description: 'Focuses a textfield, replaces text, and sends deterministic backspace where the host supports it.',
    element: 'textfield',
    coverage: coverage('interaction', ['onWillChange', 'onChange', 'onWillDelete']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textfield
          ref={ctx.targetRef}
          style={styleTextFieldSample}
          onChange={event =>
            ctx.record(`textfield delete:onChange:${event.text}:${event.selectionStart}-${event.selectionEnd}`)
          }
          onWillChange={event => {
            ctx.record(`textfield delete:onWillChange:${event.text}:${event.selectionStart}-${event.selectionEnd}`);
            return undefined;
          }}
          onWillDelete={event =>
            ctx.record(`textfield delete:onWillDelete:${event.text}:${event.selectionStart}-${event.selectionEnd}`)
          }
          value="delete me"
        />
      </view>;
    },
    interact: interactTextInputWithBackspace,
  },
  {
    id: 'textfield-selectable-disabled-focus',
    name: 'TextField selectable disabled focus',
    description: 'Renders a disabled-but-selectable textfield and toggles programmatic focus where the host allows it.',
    element: 'textfield',
    coverage: coverage('node-output', ['enabled', 'selectable', 'focused']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textfield
          ref={ctx.targetRef}
          style={styleTextFieldSample}
          color="#64748B"
          enabled={false}
          onEditBegin={event => ctx.record(`textfield disabled:onEditBegin:${event.text}`)}
          onEditEnd={event => ctx.record(`textfield disabled:onEditEnd:${event.text}:${event.reason}`)}
          selectable
          selection={[0, 8]}
          value="disabled selectable text"
        />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      if (getPlatform() === 'android') {
        ctx.record('disabled textfield focus skipped: Android text focus can wedge this snapshot harness');
        await ctx.waitForIdle();
        return;
      }
      ctx.targetRef.setAttribute('focused', true);
      ctx.record('programmatic disabled textfield focused=true');
      await ctx.waitForIdle();
      ctx.targetRef.setAttribute('focused', false);
      ctx.record('programmatic disabled textfield focused=false');
      await ctx.waitForIdle();
    },
  },
  {
    id: 'textview-return-type-matrix',
    name: 'TextView return type matrix',
    description: 'Renders textviews configured with linereturn, done, go, and send returnType values.',
    element: 'textview',
    coverage: coverage('node-output', ['returnType', 'closesWhenReturnKeyPressed']),
    render: (ctx: IntegrationTestRenderContext) => {
      ctx.record(
        'returnType configures native keyboard return behavior; this matrix keeps textviews unfocused for deterministic screenshots.',
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textview
          style={styleTextViewSample}
          enabled={false}
          height={82}
          returnType="linereturn"
          value="line return\nallowed"
        />
        <textview
          style={styleTextViewSample}
          closesWhenReturnKeyPressed
          height={82}
          marginTop={10}
          returnType="done"
          value="done return"
        />
        <textview
          style={styleTextViewSample}
          closesWhenReturnKeyPressed
          height={82}
          marginTop={10}
          returnType="go"
          value="go return"
        />
        <textview
          style={styleTextViewSample}
          closesWhenReturnKeyPressed
          height={82}
          marginTop={10}
          returnType="send"
          value="send return"
        />
      </view>;
    },
  },
  {
    id: 'textview-selection-action-smoke',
    name: 'TextView selection action smoke',
    description:
      'Sets selectable textview selection and custom edit-menu callbacks; native action invocation remains host-blocked.',
    element: 'textview',
    coverage: [
      { kind: 'node-output', attributes: ['selectable', 'selection', 'onSelectionChange', 'onTextSelectionMenu'] },
      { kind: 'blocked-needs-host', attributes: ['onTextSelectionMenuAction'] },
    ],
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <textview
          ref={ctx.targetRef}
          style={styleTextViewSample}
          enabled
          onSelectionChange={event =>
            ctx.record(`textview action:onSelectionChange:${event.selectionStart}-${event.selectionEnd}`)
          }
          onTextSelectionMenu={event => {
            ctx.record(`textview action:onTextSelectionMenu:${event.selectedText}`);
            return [{ id: 'annotate', title: 'Annotate' }];
          }}
          onTextSelectionMenuAction={event => ctx.record(`textview action:onTextSelectionMenuAction:${event.id}`)}
          selectable
          selection={[0, 8]}
          value="TextView custom selection action smoke fixture."
        />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      if (getPlatform() === 'android') {
        ctx.record(
          'programmatic textview selection skipped: Android selection mutation can wedge this snapshot harness',
        );
        await ctx.waitForIdle();
        return;
      }
      ctx.targetRef.setAttribute('selection', [9, 15]);
      ctx.record('programmatic textview selection set to 9-15; action selection still requires native host support');
      await ctx.waitForIdle();
    },
  },
  {
    id: 'image-filter-composition',
    name: 'Image filter composition',
    description: 'Renders composed brightness, contrast, and grayscale filters on one deterministic bundled image.',
    element: 'image',
    coverage: coverage('visual', ['filter']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <image
          style={styleImageSample}
          src={ANIMATED_IMAGE_PNG_DATA_URL}
          backgroundColor="#FFFFFF"
          filter={ImageFilters.compose(
            ImageFilters.brightness(1.18),
            ImageFilters.contrast(1.35),
            ImageFilters.grayscale(0.35),
          )}
          onAssetLoad={(success, error) => ctx.record(`image composed filter asset:${success}:${error ?? ''}`)}
          onImageDecoded={(w, h) => ctx.record(`image composed filter decoded:${w}x${h}`)}
        />
      </view>;
    },
  },
  {
    id: 'image-data-url-source',
    name: 'Image data URL source',
    description: 'Renders a data:image/png source through the regular image element and records load/decode callbacks.',
    element: 'image',
    coverage: coverage('visual', ['src', 'onAssetLoad', 'onImageDecoded']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <image
          style={styleImageSample}
          src={ANIMATED_IMAGE_PNG_DATA_URL}
          onAssetLoad={(success, error) => ctx.record(`image data url asset:${success}:${error ?? ''}`)}
          onImageDecoded={(w, h) => ctx.record(`image data url decoded:${w}x${h}`)}
        />
      </view>;
    },
  },
  {
    id: 'webview-controller-smoke',
    name: 'WebView controller smoke',
    description:
      'Sets a controller-shaped object on web for node-output coverage while native controller behavior remains blocked on host support.',
    element: 'webview',
    coverage: coverage('node-output', ['controller']),
    render: (ctx: IntegrationTestRenderContext) => {
      const controller = getPlatform() === 'web' ? ({} as IWebViewNativeController) : undefined;
      if (!controller) {
        ctx.record('native webview controller smoke requires a deterministic host-created valdi_webview controller.');
      }
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <webview style={styleWebViewSample} controller={controller} />
      </view>;
    },
  },
  {
    id: 'scroll-offset-static-size-matrix',
    name: 'Scroll offset and static size matrix',
    description:
      'Sets staticContentWidth, staticContentHeight, contentOffsetX/Y, and toggles contentOffsetAnimated on a nested scroll fixture.',
    element: 'scroll',
    coverage: coverage('interaction', [
      'staticContentWidth',
      'staticContentHeight',
      'contentOffsetX',
      'contentOffsetY',
      'contentOffsetAnimated',
    ]),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <scroll
          ref={ctx.targetRef}
          backgroundColor="#FFFFFF"
          border="2 solid #CBD5E1"
          borderRadius={12}
          height={260}
          horizontal
          width="100%"
        >
          <layout flexDirection="row" height={460} width={720}>
            <view backgroundColor="#EF4444" height={430} margin={8} width={150} />
            <view backgroundColor="#F59E0B" height={430} margin={8} width={150} />
            <view backgroundColor="#10B981" height={430} margin={8} width={150} />
            <view backgroundColor="#3B82F6" height={430} margin={8} width={150} />
          </layout>
        </scroll>
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      ctx.targetRef.setAttribute('staticContentWidth', 720);
      ctx.targetRef.setAttribute('staticContentHeight', 460);
      ctx.targetRef.setAttribute('contentOffsetAnimated', false);
      ctx.targetRef.setAttribute('contentOffsetX', 160);
      ctx.targetRef.setAttribute('contentOffsetY', 80);
      ctx.record('programmatic scroll offsets set to 160,80 with static content size 720x460');
      await ctx.waitForIdle();
    },
  },
  {
    id: 'blur-remaining-materials',
    name: 'Blur remaining material variants',
    description: 'Renders additional blurStyle values not covered by the smaller light/dark fixtures.',
    element: 'blur',
    coverage: coverage('visual', ['blurStyle']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <view height={330} width="100%">
          <view
            background="linear-gradient(135deg, #EF4444, #FACC15, #22C55E, #3B82F6, #7C3AED)"
            height={330}
            width="100%"
          />
          <blur style={styleBlurSample} blurStyle="extraLight" left={14} position="absolute" top={18} />
          <blur style={styleBlurSample} blurStyle="dark" left={170} position="absolute" top={18} />
          <blur style={styleBlurSample} blurStyle="systemUltraThinMaterial" left={14} position="absolute" top={126} />
          <blur style={styleBlurSample} blurStyle="systemMaterial" left={170} position="absolute" top={126} />
          <blur style={styleBlurSample} blurStyle="systemThickMaterialDark" left={92} position="absolute" top={234} />
        </view>
      </view>;
    },
  },
  {
    id: 'spinner-style-layout',
    name: 'Spinner style and layout',
    description: 'Applies a Style object to spinner and positions multiple spinners through regular layout attributes.',
    element: 'spinner',
    coverage: [
      { kind: 'visual', attributes: ['color'] },
      { kind: 'node-output', attributes: ['style'] },
    ],
    expectedFailureOnPlatforms: ['android'],
    expectedFailureReason:
      'Android spinner snapshot renders blank even though ValdiSpinnerView should show a tinted indeterminate ProgressBar',
    skipSnapshotOnPlatforms: ['ios'],
    skipSnapshotReason: 'spinner snapshot skipped: iOS UIActivityIndicatorView snapshot hangs the harness',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout flexDirection="row" alignItems="center" gap={28}>
          <spinner style={styleSpinnerSample} />
          <spinner style={styleSpinnerSample} color="#DC2626" width={56} height={56} />
        </layout>
      </view>;
    },
  },
  {
    id: 'shape-empty-and-precedence',
    name: 'Shape empty path and fill precedence',
    description:
      'Renders an empty shape beside a shape with both fillColor and fillGradient to cover empty and precedence behavior.',
    element: 'shape',
    coverage: coverage('visual', ['path', 'fillColor', 'fillGradient', 'strokeColor', 'strokeWidth']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <layout flexDirection="row" gap={20}>
          <shape style={styleShapeSample} path={undefined} fillColor="#FEE2E2" strokeColor="#BE123C" strokeWidth={6} />
          <shape
            style={styleShapeSample}
            path={trianglePath}
            fillColor="#FEE2E2"
            fillGradient="linear-gradient(#0EA5E9, #22C55E)"
            strokeColor="#14532D"
            strokeWidth={6}
          />
        </layout>
      </view>;
    },
  },
  {
    id: 'animatedimage-lottie-resource',
    name: 'AnimatedImage Lottie resource',
    description:
      'Renders the bundled Lottie JSON resource with deterministic timing, asset, decode, and progress callbacks.',
    element: 'animatedimage',
    coverage: coverage('visual', ['src', 'currentTime', 'advanceRate', 'onAssetLoad', 'onImageDecoded', 'onProgress']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <animatedimage
          style={styleAnimatedImageSample}
          advanceRate={0}
          currentTime={0.5}
          loop
          onAssetLoad={(success, error) => ctx.record(`lottie asset:${success}:${error ?? ''}`)}
          onImageDecoded={(w, h) => ctx.record(`lottie decoded:${w}x${h}`)}
          onProgress={event =>
            ctx.record(
              `lottie progress:${Math.round(event.time * 100) / 100}/${Math.round(event.duration * 100) / 100}`,
            )
          }
          src={lottieResourceSource(ctx)}
        />
      </view>;
    },
    expectedFailureOnPlatforms: ['android'],
    expectedFailureReason:
      'Android AnimatedImageView decodes the Lottie resource but its SnapDrawing/TextureView content is not captured in takeSnapshot.',
  },
  {
    id: 'animatedimage-lottie-font-provider-smoke',
    name: 'AnimatedImage Lottie font provider smoke',
    description:
      'Sets the drawing FontManager fontProvider on the bundled Lottie resource for configuration smoke coverage.',
    element: 'animatedimage',
    coverage: coverage('node-output', ['fontProvider']),
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <animatedimage
          style={styleAnimatedImageSample}
          advanceRate={0}
          currentTime={0.5}
          fontProvider={defaultLottieFontProvider(ctx)}
          loop={false}
          objectFit="contain"
          src={lottieResourceSource(ctx)}
        />
      </view>;
    },
    expectedFailureOnPlatforms: ['android'],
    expectedFailureReason:
      'Android AnimatedImageView decodes the Lottie resource but its SnapDrawing/TextureView content is not captured in takeSnapshot.',
  },
  {
    id: 'worker-message-round-trip',
    name: 'Worker message round trip',
    description: 'Starts a declared worker entrypoint and verifies a message can make a round trip.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <label value="Worker message round trip" font="system-bold 20" color="#111827" />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      const worker = new Worker('integration_test_app/src/WebWorkerProbe');
      try {
        await new Promise<void>((resolve, reject) => {
          worker.onmessage = event => {
            const response = event.data as { echoed?: unknown; source?: unknown };
            if (response.echoed !== 'probe' || response.source !== 'valdi-web-worker') {
              reject(new Error(`Unexpected worker response: ${JSON.stringify(response)}`));
              return;
            }
            ctx.record('worker echoed probe from valdi-web-worker');
            resolve();
          };
          worker.postMessage('probe');
        });
      } finally {
        worker.terminate();
      }
    },
  },
];

export type IntegrationCoverageLedgerElement = NativeTemplateElementName | 'slot';

export interface IntegrationAttributeCoverageLedgerEntry {
  readonly elements: readonly IntegrationCoverageLedgerElement[];
  readonly attributes: readonly string[];
  readonly kind: IntegrationTestCoverageKind;
  readonly caseIds: readonly string[];
  readonly notes?: string;
}

export const INTEGRATION_ATTRIBUTE_COVERAGE_LEDGER: readonly IntegrationAttributeCoverageLedgerEntry[] = [
  {
    elements: ['custom-view'],
    attributes: ['viewFactory', 'factoryText', 'backgroundColor', 'borderRadius'],
    kind: 'visual',
    caseIds: ['custom-view-host-view-factory'],
    notes: 'Host-provided factories are exercised on iOS and web.',
  },
  {
    elements: [
      'layout',
      'view',
      'label',
      'textfield',
      'textview',
      'image',
      'webview',
      'video',
      'scroll',
      'spinner',
      'blur',
      'shape',
      'animatedimage',
    ],
    attributes: ['width', 'height', 'minWidth', 'minHeight', 'maxWidth', 'maxHeight', 'aspectRatio'],
    kind: 'visual',
    caseIds: ['layout-size-constraints'],
  },
  {
    elements: [
      'layout',
      'view',
      'label',
      'textfield',
      'textview',
      'image',
      'webview',
      'video',
      'scroll',
      'spinner',
      'blur',
      'shape',
      'animatedimage',
    ],
    attributes: [
      'position',
      'top',
      'right',
      'bottom',
      'left',
      'margin',
      'marginTop',
      'marginRight',
      'marginBottom',
      'marginLeft',
    ],
    kind: 'visual',
    caseIds: ['layout-position-offsets', 'layout-spacing-shorthands'],
  },
  {
    elements: ['layout', 'view', 'scroll'],
    attributes: ['padding', 'paddingTop', 'paddingRight', 'paddingBottom', 'paddingLeft', 'gap', 'rowGap', 'columnGap'],
    kind: 'visual',
    caseIds: ['layout-spacing-shorthands', 'layout-flex-wrap-gaps'],
  },
  {
    elements: ['layout', 'view', 'scroll'],
    attributes: [
      'direction',
      'flexDirection',
      'justifyContent',
      'alignItems',
      'alignSelf',
      'flexGrow',
      'flexShrink',
      'flexBasis',
      'flexWrap',
      'alignContent',
    ],
    kind: 'visual',
    caseIds: [
      'layout-direction-rtl',
      'layout-flex-direction',
      'layout-justify-content',
      'layout-align-items-baseline',
      'layout-align-self',
      'layout-flex-grow-shrink-basis',
      'layout-flex-wrap-gaps',
      'layout-align-content',
    ],
  },
  {
    elements: ['layout', 'view'],
    attributes: ['display', 'overflow'],
    kind: 'visual',
    caseIds: ['layout-display-overflow'],
  },
  {
    elements: [
      'layout',
      'view',
      'label',
      'textfield',
      'textview',
      'image',
      'webview',
      'video',
      'scroll',
      'spinner',
      'blur',
      'shape',
      'animatedimage',
    ],
    attributes: ['zIndex'],
    kind: 'visual',
    caseIds: ['layout-zindex-overlap'],
  },
  {
    elements: ['layout', 'view', 'scroll'],
    attributes: [
      'onLayout',
      'onVisibilityChanged',
      'onViewportChanged',
      'onLayoutComplete',
      'lazyLayout',
      'onMeasure',
      'estimatedWidth',
      'estimatedHeight',
      'lazy',
      'limitToViewport',
      'ignoreParentViewport',
      'extendViewportWithChildren',
    ],
    kind: 'interaction',
    caseIds: ['layout-measure-estimates', 'layout-lifecycle-callbacks'],
  },
  {
    elements: [
      'layout',
      'view',
      'label',
      'textfield',
      'textview',
      'image',
      'webview',
      'video',
      'scroll',
      'spinner',
      'blur',
      'shape',
      'animatedimage',
    ],
    attributes: [
      'id',
      'key',
      'class',
      'animationsEnabled',
      'accessibilityCategory',
      'accessibilityNavigation',
      'accessibilityPriority',
      'accessibilityLabel',
      'accessibilityHint',
      'accessibilityValue',
      'accessibilityStateDisabled',
      'accessibilityStateSelected',
      'accessibilityStateLiveRegion',
    ],
    kind: 'node-output',
    caseIds: ['layout-accessibility-metadata'],
    notes: 'Accessibility behavior beyond serialized attributes needs a native accessibility inspector host.',
  },
  {
    elements: [
      'layout',
      'view',
      'label',
      'textfield',
      'textview',
      'image',
      'webview',
      'video',
      'scroll',
      'spinner',
      'blur',
      'shape',
      'animatedimage',
    ],
    attributes: ['colorPaletteName'],
    kind: 'visual',
    caseIds: ['layout-color-palette-subtree-override'],
  },
  {
    elements: [
      'layout',
      'view',
      'label',
      'textfield',
      'textview',
      'image',
      'webview',
      'video',
      'scroll',
      'spinner',
      'blur',
      'shape',
      'animatedimage',
    ],
    attributes: ['scrollAnchorPosition'],
    kind: 'node-output',
    caseIds: ['layout-scroll-anchor-bottom', 'scroll-drag-callbacks-anchor'],
  },
  {
    elements: [
      'layout',
      'view',
      'label',
      'textfield',
      'textview',
      'image',
      'webview',
      'video',
      'scroll',
      'spinner',
      'blur',
      'shape',
      'animatedimage',
    ],
    attributes: ['style'],
    kind: 'node-output',
    caseIds: ['style-application-overrides', 'spinner-style-layout'],
    notes: 'Style application is verified by visual snapshots; the style object itself is metadata/config.',
  },
  {
    elements: [
      'layout',
      'view',
      'label',
      'textfield',
      'textview',
      'image',
      'webview',
      'video',
      'scroll',
      'spinner',
      'blur',
      'shape',
      'animatedimage',
      'slot',
    ],
    attributes: ['ref'],
    kind: 'node-output',
    caseIds: [],
    notes: 'Refs are harness plumbing and are deliberately omitted from rendered-node JSON.',
  },
  {
    elements: ['slot'],
    attributes: ['key', 'name'],
    kind: 'node-output',
    caseIds: [],
    notes: 'Slot is intentionally out of snapshot coverage because it has no rendering attributes beyond key/name/ref.',
  },
  {
    elements: ['view', 'scroll', 'blur'],
    attributes: ['allowReuse', 'onViewCreate', 'onViewDestroy', 'onViewChange'],
    kind: 'interaction',
    caseIds: ['view-lifecycle-reuse', 'view-lifecycle-destroy-recreate'],
  },
  {
    elements: [
      'view',
      'scroll',
      'blur',
      'textfield',
      'textview',
      'label',
      'image',
      'video',
      'webview',
      'spinner',
      'shape',
      'animatedimage',
    ],
    attributes: [
      'background',
      'backgroundColor',
      'opacity',
      'slowClipping',
      'border',
      'borderWidth',
      'borderColor',
      'borderRadius',
      'boxShadow',
    ],
    kind: 'visual',
    caseIds: [
      'view-background-color',
      'view-background-gradient',
      'view-background-solid-and-gradient-stops',
      'view-border-styles',
      'view-border-radius-edge-cases',
      'view-opacity',
      'view-shadow-radius-clipping',
      'view-complex-shadow',
    ],
  },
  {
    elements: ['view', 'scroll', 'blur'],
    attributes: ['cursor'],
    kind: 'node-output',
    caseIds: ['view-cursor-web-smoke'],
  },
  {
    elements: ['view', 'scroll', 'blur'],
    attributes: [
      'touchEnabled',
      'hitTest',
      'onTouch',
      'onTouchStart',
      'onTouchEnd',
      'onTouchDelayDuration',
      'filterTouchesWhenObscured',
    ],
    kind: 'interaction',
    caseIds: ['view-touch-hit-testing', 'view-touch-disabled-predicate-rejection'],
  },
  {
    elements: ['view', 'scroll', 'blur'],
    attributes: [
      'onTapDisabled',
      'onTap',
      'onTapPredicate',
      'onDoubleTapDisabled',
      'onDoubleTap',
      'onDoubleTapPredicate',
      'longPressDuration',
      'onLongPressDisabled',
      'onLongPress',
      'onLongPressPredicate',
      'onDragDisabled',
      'onDrag',
      'onDragPredicate',
    ],
    kind: 'interaction',
    caseIds: [
      'view-gesture-tap',
      'view-touch-disabled-predicate-rejection',
      'view-gesture-double-tap',
      'view-gesture-long-press',
      'view-gesture-drag',
    ],
  },
  {
    elements: ['view', 'scroll', 'blur'],
    attributes: ['onPinchDisabled', 'onPinch', 'onPinchPredicate', 'onRotateDisabled', 'onRotate', 'onRotatePredicate'],
    kind: 'blocked-needs-host',
    caseIds: ['view-gesture-pinch-rotate-wiring'],
    notes:
      'The fixture registers callbacks; deterministic multi-touch dispatch needs host support before it is true behavior coverage.',
  },
  {
    elements: ['view', 'scroll', 'blur'],
    attributes: [
      'touchAreaExtension',
      'touchAreaExtensionTop',
      'touchAreaExtensionRight',
      'touchAreaExtensionBottom',
      'touchAreaExtensionLeft',
    ],
    kind: 'interaction',
    caseIds: ['view-touch-hit-testing'],
  },
  {
    elements: [
      'view',
      'scroll',
      'blur',
      'label',
      'textfield',
      'textview',
      'image',
      'webview',
      'video',
      'spinner',
      'shape',
      'animatedimage',
    ],
    attributes: ['scaleX', 'scaleY', 'rotation', 'translationX', 'translationY', 'transformOrigin', 'transform'],
    kind: 'visual',
    caseIds: [
      'view-transform-scale',
      'view-transform-rotation',
      'view-transform-translation',
      'view-transform-string-origin',
      'view-transform-precedence-origin',
    ],
  },
  {
    elements: ['view', 'scroll', 'blur'],
    attributes: ['accessibilityId', 'canAlwaysScrollHorizontal', 'canAlwaysScrollVertical'],
    kind: 'node-output',
    caseIds: ['view-background-color', 'view-scroll-intercept-flags'],
  },
  {
    elements: ['view', 'scroll', 'blur'],
    attributes: ['maskOpacity', 'maskPath'],
    kind: 'visual',
    caseIds: ['view-mask-path-opacity', 'view-mask-opacity-extremes'],
  },
  {
    elements: ['label', 'textfield', 'textview'],
    attributes: ['value', 'font', 'color', 'textGradient', 'textShadow'],
    kind: 'visual',
    caseIds: [
      'label-uniform-color',
      'label-font-spacing-variants',
      'label-text-gradient',
      'label-text-shadow',
      'textfield-value-disabled-state',
      'textview-multiline-gravity',
    ],
  },
  {
    elements: ['label'],
    attributes: [
      'numberOfLines',
      'textAlign',
      'textDecoration',
      'customUnderlineStyle',
      'lineHeight',
      'lineHeightAbsolute',
      'letterSpacing',
      'adjustsFontSizeToFitWidth',
      'minimumScaleFactor',
      'textOverflow',
    ],
    kind: 'visual',
    caseIds: [
      'label-single-line-alignments',
      'label-flex-shrink-defaults',
      'label-multiline-lineheight',
      'label-lineheight-multiple',
      'label-decoration-styles',
      'label-autoshrink',
      'label-overflow-ellipsis',
      'label-overflow-clip',
      'label-multiline-justify-lineheight-precedence',
      'label-overflow-multiline',
    ],
  },
  {
    elements: ['label'],
    attributes: ['selectable', 'selection', 'onSelectionChange', 'onTextSelectionMenu'],
    kind: 'node-output',
    caseIds: ['label-selection-menu-smoke'],
  },
  {
    elements: ['label'],
    attributes: ['onTextSelectionMenuAction'],
    kind: 'blocked-needs-host',
    caseIds: ['label-selection-menu-smoke'],
    notes: 'Needs a host helper that opens the native selection menu and chooses a custom action.',
  },
  {
    elements: ['textfield', 'textview'],
    attributes: [
      'tintColor',
      'placeholderColor',
      'placeholder',
      'enabled',
      'selectable',
      'textAlign',
      'autocapitalization',
      'autocorrection',
      'characterLimit',
      'selectTextOnFocus',
      'closesWhenReturnKeyPressed',
      'keyboardAppearance',
      'selection',
      'enableInlinePredictions',
    ],
    kind: 'node-output',
    caseIds: [
      'textfield-placeholder-empty-states',
      'textfield-value-disabled-state',
      'textfield-selection-character-limit',
      'textfield-autocapitalization-correction',
      'textfield-keyboard-appearance-return',
      'textfield-inline-predictions',
      'textfield-selectable-disabled-focus',
      'textview-keyboard-configuration',
      'textview-selection-menu',
      'textview-selection-action-smoke',
    ],
  },
  {
    elements: ['textfield', 'textview'],
    attributes: [
      'onWillChange',
      'onChange',
      'onEditBegin',
      'onEditEnd',
      'onReturn',
      'onWillDelete',
      'onSelectionChange',
    ],
    kind: 'interaction',
    caseIds: [
      'textfield-selection-character-limit',
      'textfield-willchange-delete',
      'textfield-focused-programmatic',
      'textview-edit-callbacks-return',
      'textview-focused-programmatic',
    ],
  },
  {
    elements: ['textfield', 'textview'],
    attributes: ['focused'],
    kind: 'interaction',
    caseIds: ['textfield-focused-programmatic', 'textfield-selectable-disabled-focus', 'textview-focused-programmatic'],
    notes: 'Programmatic-only attribute from the interactive interfaces.',
  },
  {
    elements: ['textfield'],
    attributes: ['contentType', 'returnKeyText'],
    kind: 'node-output',
    caseIds: [
      'textfield-secure-content-types',
      'textfield-keyboard-appearance-return',
      'textfield-content-type-return-matrix',
    ],
  },
  {
    elements: ['textview'],
    attributes: [
      'returnType',
      'textGravity',
      'numberOfLines',
      'textDecoration',
      'textOverflow',
      'lineHeight',
      'lineHeightAbsolute',
      'customUnderlineStyle',
      'backgroundEffectColor',
      'backgroundEffectBorderRadius',
      'backgroundEffectPadding',
    ],
    kind: 'visual',
    caseIds: [
      'textview-multiline-gravity',
      'textview-background-effect',
      'textview-decoration-selectable',
      'textview-return-type-matrix',
    ],
  },
  {
    elements: ['textview'],
    attributes: ['onTextSelectionMenu'],
    kind: 'node-output',
    caseIds: ['textview-selection-menu', 'textview-selection-action-smoke'],
  },
  {
    elements: ['textview'],
    attributes: ['onTextSelectionMenuAction'],
    kind: 'blocked-needs-host',
    caseIds: ['textview-selection-menu', 'textview-selection-action-smoke'],
    notes: 'Needs a host helper that opens the native selection menu and chooses a custom action.',
  },
  {
    elements: ['image'],
    attributes: [
      'src',
      'objectFit',
      'tint',
      'flipOnRtl',
      'contentScaleX',
      'contentScaleY',
      'contentRotation',
      'filter',
    ],
    kind: 'visual',
    caseIds: [
      'image-object-fit-fill-contain',
      'image-object-fit-cover-none',
      'image-tint',
      'image-rtl-flip',
      'image-content-transform',
      'image-filter-variants',
      'image-filter-composition',
      'image-data-url-source',
    ],
  },
  {
    elements: ['image'],
    attributes: ['onAssetLoad', 'onImageDecoded'],
    kind: 'interaction',
    caseIds: ['image-object-fit-fill-contain', 'image-missing-source-callbacks', 'image-data-url-source'],
  },
  {
    elements: ['webview'],
    attributes: ['controller'],
    kind: 'node-output',
    caseIds: ['webview-controller-smoke', 'webview-empty-controller'],
    notes: 'Positive native controller behavior needs a deterministic host-created valdi_webview controller.',
  },
  {
    elements: ['video'],
    attributes: ['src', 'onError'],
    kind: 'interaction',
    caseIds: ['video-missing-source-error'],
  },
  {
    elements: ['video'],
    attributes: [
      'volume',
      'playbackRate',
      'seekToTime',
      'onVideoLoaded',
      'onBeginPlaying',
      'onCompleted',
      'onProgressUpdated',
    ],
    kind: 'blocked-needs-host',
    caseIds: ['video-playback-state', 'video-playback-callbacks'],
    notes:
      'Current fixtures register/configure callbacks with a missing source; positive playback needs a bundled video asset and host playback stabilization.',
  },
  {
    elements: ['scroll'],
    attributes: [
      'onScroll',
      'onScrollEnd',
      'onDragStart',
      'onDragEnding',
      'onDragEnd',
      'onContentSizeChange',
      'contentOffsetX',
      'contentOffsetY',
      'contentOffsetAnimated',
      'staticContentWidth',
      'staticContentHeight',
      'maintainScrollAnchor',
    ],
    kind: 'interaction',
    caseIds: [
      'scroll-vertical-offset',
      'scroll-horizontal-paging',
      'scroll-drag-callbacks-anchor',
      'scroll-offset-static-size-matrix',
    ],
  },
  {
    elements: ['scroll'],
    attributes: [
      'bounces',
      'bouncesFromDragAtStart',
      'bouncesFromDragAtEnd',
      'bouncesVerticalWithSmallContent',
      'bouncesHorizontalWithSmallContent',
      'cancelsTouchesOnScroll',
      'stopScrollingOnTouch',
      'dismissKeyboardOnDrag',
      'dismissKeyboardOnDragMode',
      'pagingEnabled',
      'horizontal',
      'showsVerticalScrollIndicator',
      'showsHorizontalScrollIndicator',
      'scrollEnabled',
      'circularRatio',
      'fadingEdgeLength',
      'fadingEdgeStart',
      'fadingEdgeEnd',
      'androidOnlyEnableExtendedFadingEdge',
      'decelerationRate',
      'viewportExtensionTop',
      'viewportExtensionRight',
      'viewportExtensionBottom',
      'viewportExtensionLeft',
    ],
    kind: 'node-output',
    caseIds: [
      'scroll-horizontal-paging',
      'scroll-indicator-visibility',
      'scroll-fading-edges',
      'scroll-bounce-deceleration',
      'scroll-keyboard-touch-cancel',
      'scroll-viewport-extension',
      'scroll-nested',
    ],
  },
  {
    elements: ['scroll'],
    attributes: ['scrollPerfLoggerBridge'],
    kind: 'blocked-needs-host',
    caseIds: [],
    notes: 'Needs a deterministic native bridge or mock object to verify perf logger calls.',
  },
  {
    elements: ['spinner'],
    attributes: ['color'],
    kind: 'visual',
    caseIds: ['spinner-color', 'spinner-style-layout'],
  },
  {
    elements: ['blur'],
    attributes: ['blurStyle'],
    kind: 'visual',
    caseIds: ['blur-light-and-dark', 'blur-material-variants', 'blur-remaining-materials'],
  },
  {
    elements: ['shape'],
    attributes: [
      'path',
      'strokeWidth',
      'strokeColor',
      'fillColor',
      'fillGradient',
      'strokeCap',
      'strokeJoin',
      'strokeStart',
      'strokeEnd',
    ],
    kind: 'visual',
    caseIds: [
      'shape-fill-and-stroke',
      'shape-stroke-caps',
      'shape-stroke-joins',
      'shape-stroke-range',
      'shape-fill-gradient',
      'shape-empty-and-precedence',
    ],
  },
  {
    elements: ['animatedimage'],
    attributes: ['src', 'loop', 'advanceRate', 'currentTime', 'animationStartTime', 'animationEndTime', 'objectFit'],
    kind: 'visual',
    caseIds: ['animatedimage-object-fit', 'animatedimage-current-time-window', 'animatedimage-lottie-resource'],
  },
  {
    elements: ['animatedimage'],
    attributes: ['onAssetLoad', 'onImageDecoded', 'onProgress'],
    kind: 'interaction',
    caseIds: ['animatedimage-callbacks', 'animatedimage-lottie-resource'],
  },
  {
    elements: ['animatedimage'],
    attributes: ['fontProvider'],
    kind: 'node-output',
    caseIds: ['animatedimage-lottie-font-provider-smoke'],
  },
];

function assertLedgerClassified(entries: readonly IntegrationAttributeCoverageLedgerEntry[]): void {
  for (const entry of entries) {
    if (entry.attributes.length === 0) {
      throw new Error('Integration coverage ledger entry has no attributes.');
    }
    if (entry.elements.length === 0) {
      throw new Error(`Integration coverage ledger entry for ${entry.attributes.join(',')} has no elements.`);
    }
  }
}

assertLedgerClassified(INTEGRATION_ATTRIBUTE_COVERAGE_LEDGER);
