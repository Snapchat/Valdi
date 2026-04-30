import { NavigationPage } from 'valdi_navigation/src/NavigationPage';
import { NavigationPageStatefulComponent } from 'valdi_navigation/src/NavigationPageComponent';
import { Style } from 'valdi_core/src/Style';
import { Component } from 'valdi_core/src/Component';
import { systemBoldFont, systemFont } from 'valdi_core/src/SystemFont';
import { Device } from 'valdi_core/src/Device';
import { Label, ScrollView } from 'valdi_tsx/src/NativeTemplateElements';
import { Stopwatch } from 'valdi_core/src/utils/Stopwatch';
import { GeometricPathBuilder, GeometricPathScaleType } from 'valdi_core/src/GeometricPath';
import { createManagedContext } from 'drawing/src/ManagedContextFactory';
import { createBitmap } from 'drawing/src/BitmapFactory';
import { BitmapAlphaType, BitmapColorType } from 'drawing/src/IBitmap';
import { IManagedContext } from 'drawing/src/IManagedContext';
import { IBitmap } from 'drawing/src/IBitmap';
import { setTimeoutInterruptible } from 'valdi_core/src/SetTimeout';

// ---------------------------------------------------------------------------
// Complex synthetic scene -- approximates SnapEditor-level complexity
// ~100-150 nodes, 8-10 levels deep, exercises all major drawing primitives
// ---------------------------------------------------------------------------

interface ComplexBenchmarkSceneViewModel {
  frameIndex: number;
}

const ds = Device.getDisplayScale();
const titleFont = systemBoldFont(34 * ds);
const bodyFont = systemFont(20 * ds);
const smallFont = systemFont(12 * ds);
const tinyFont = systemFont(9 * ds);

/**
 * Convert HSL+alpha to an rgba() string, since Valdi doesn't support hsla().
 * h: 0-360, s: 0-100, l: 0-100, a: 0-1
 */
function hsla(h: number, s: number, l: number, a: number): string {
  h = ((h % 360) + 360) % 360;
  const sn = s / 100;
  const ln = l / 100;
  const c = (1 - Math.abs(2 * ln - 1)) * sn;
  const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
  const m = ln - c / 2;
  let r = 0;
  let g = 0;
  let b = 0;
  if (h < 60) { r = c; g = x; b = 0; }
  else if (h < 120) { r = x; g = c; b = 0; }
  else if (h < 180) { r = 0; g = c; b = x; }
  else if (h < 240) { r = 0; g = x; b = c; }
  else if (h < 300) { r = x; g = 0; b = c; }
  else { r = c; g = 0; b = x; }
  const ri = Math.round((r + m) * 255);
  const gi = Math.round((g + m) * 255);
  const bi = Math.round((b + m) * 255);
  return `rgba(${ri},${gi},${bi},${a})`;
}

/**
 * Generates a unique geometric path for each frame to defeat draw caching.
 */
function makeAnimatedPath(frameIndex: number, variant: number): ReturnType<GeometricPathBuilder['build']> {
  const phase = ((frameIndex + variant * 17) % 60) / 60;
  const builder = new GeometricPathBuilder(1, 1, GeometricPathScaleType.Contain);

  // Varying arc sweep
  builder.arcTo(0.5, 0.5, 0.4, Math.PI * phase, Math.PI * 2 * (0.3 + phase * 0.7));

  // Additional line segments for complexity
  const points = 5 + (variant % 4);
  for (let i = 0; i < points; i++) {
    const angle = (Math.PI * 2 * i) / points + phase * Math.PI;
    const r = 0.3 + 0.15 * Math.sin(angle * 2 + variant);
    builder.lineTo(0.5 + r * Math.cos(angle), 0.5 + r * Math.sin(angle));
  }
  return builder.build();
}

/**
 * Generates drawing stroke paths that vary per frame (for the "drawing" entity).
 */
function makeStrokePath(frameIndex: number, strokeIdx: number): ReturnType<GeometricPathBuilder['build']> {
  const seed = frameIndex * 13 + strokeIdx * 7;
  const phase = (seed % 100) / 100;
  const builder = new GeometricPathBuilder(1, 1, GeometricPathScaleType.Contain);

  const segments = 4 + (strokeIdx % 3);
  const startX = 0.05 + phase * 0.1;
  const startY = 0.1 + (strokeIdx / 20) * 0.8;
  builder.moveTo(startX, startY);

  for (let j = 1; j <= segments; j++) {
    const t = j / segments;
    const x = startX + t * 0.9;
    const y = startY + 0.05 * Math.sin(t * Math.PI * 3 + phase * Math.PI * 2);
    builder.lineTo(x, y);
  }
  return builder.build();
}

const STROKE_COLORS = [
  'red', 'blue', 'green', 'orange', 'purple',
  'cyan', 'magenta', 'yellow', '#FF6B6B', '#4ECDC4',
  '#45B7D1', '#96CEB4', '#FFEAA7', '#DFE6E9', '#6C5CE7',
  '#FD79A8', '#00B894', '#E17055', '#0984E3', '#636E72',
];

const STROKE_CAPS: Array<'round' | 'butt' | 'square'> = ['round', 'butt', 'square'];

class ComplexBenchmarkScene extends Component<ComplexBenchmarkSceneViewModel> {
  onRender(): void {
    const idx = this.viewModel.frameIndex;
    const bgHue = (idx * 6) % 360;

    // -- Root container with clipping (like playback bounds) --
    <view
      backgroundColor="black"
      width="100%"
      height="100%"
      slowClipping
      borderRadius={20 * ds}
    >
      {/* Background layer: solid color + gradient overlay */}
      <view
        position="absolute"
        top={0} left={0} right={0} bottom={0}
        background={`linear-gradient(180deg, ${hsla(bgHue, 50, 20, 1)}, ${hsla((bgHue + 60) % 360, 40, 10, 1)})`}
      />

      {/* Semi-transparent gradient overlay */}
      <view
        position="absolute"
        top={0} left={0} right={0} bottom={0}
        background={`linear-gradient(45deg, rgba(255,255,255,0.05), rgba(255,255,255,0.15), rgba(255,255,255,0.05))`}
      />

      {/* --- Entity stack (absolute positioned layers, like ZStack) --- */}

      {/* Sticker entity 1: rotated shape with nested containers */}
      {this.renderStickerEntity(idx, 0, '10%', '10%', 0.8)}

      {/* Sticker entity 2 */}
      {this.renderStickerEntity(idx, 1, '55%', '5%', 0.7)}

      {/* Sticker entity 3 */}
      {this.renderStickerEntity(idx, 2, '5%', '45%', 0.9)}

      {/* Sticker entity 4 */}
      {this.renderStickerEntity(idx, 3, '60%', '40%', 0.6)}

      {/* Sticker entity 5 */}
      {this.renderStickerEntity(idx, 4, '30%', '65%', 0.75)}

      {/* Caption entity: styled text with background, border, rotation */}
      {this.renderCaptionEntity(idx)}

      {/* Drawing entity: many stroke paths */}
      {this.renderDrawingEntity(idx)}

      {/* Filter overlay: semi-transparent gradient with text */}
      {this.renderFilterOverlay(idx)}

      {/* Bottom bar: nested flex layout with 8 items */}
      {this.renderBottomBar(idx)}

      {/* Additional nested clipping regions */}
      {this.renderClippingRegions(idx)}
    </view>;
  }

  private renderStickerEntity(
    idx: number, variant: number,
    leftPos: string, topPos: string, scaleFactor: number,
  ): void {
    const rotation = (Math.PI / 6) * Math.sin((idx + variant * 20) * 0.05);
    const scale = scaleFactor + 0.1 * Math.sin(idx * 0.1 + variant);
    const size = 25 + variant * 3;
    const path = makeAnimatedPath(idx, variant);
    const hue = (idx * 11 + variant * 73) % 360;

    <view
      position="absolute"
      left={leftPos}
      top={topPos}
      rotation={rotation}
      scaleX={scale}
      scaleY={scale}
    >
      {/* Level 1: outer container */}
      <view
        width={`${size}%`}
        aspectRatio={1}
        backgroundColor={hsla(hue, 60, 50, 0.3)}
        borderRadius={12 * ds}
        padding={4 * ds}
      >
        {/* Level 2: inner container */}
        <view
          width="100%"
          height="100%"
          backgroundColor={hsla((hue + 120) % 360, 50, 60, 0.4)}
          borderRadius={8 * ds}
          alignItems="center"
          justifyContent="center"
          slowClipping
        >
          {/* Level 3: shape content */}
          <shape
            width="80%"
            aspectRatio={1}
            strokeColor={hsla((hue + 240) % 360, 80, 70, 0.9)}
            strokeWidth={(3 + variant) * ds}
            strokeCap="round"
            path={path}
          />
          <view
            position="absolute"
            width="60%"
            aspectRatio={1}
            backgroundColor={hsla((hue + 60) % 360, 70, 50, 0.25)}
            borderRadius={'50%'}
            slowClipping
          />
        </view>
      </view>
    </view>;
  }

  private renderCaptionEntity(idx: number): void {
    const rotation = Math.PI / 24 * Math.sin(idx * 0.08);
    const hue = (idx * 4 + 200) % 360;

    <view
      position="absolute"
      left="10%"
      right="10%"
      top="30%"
      rotation={rotation}
    >
      <view
        backgroundColor={hsla(hue, 40, 15, 0.75)}
        borderRadius={16 * ds}
        padding={12 * ds}
        alignItems="center"
      >
        <view
          width="100%"
          borderRadius={12 * ds}
          padding={8 * ds}
          backgroundColor={hsla(hue, 30, 20, 0.5)}
        >
          <label
            font={titleFont}
            color={hsla((hue + 180) % 360, 80, 80, 1)}
            value={`Caption #${idx}`}
          />
          <label
            font={bodyFont}
            color="rgba(255,255,255,0.7)"
            value={`Frame ${idx} | ${new Date().toISOString().slice(11, 19)}`}
            numberOfLines={2}
          />
        </view>
      </view>
    </view>;
  }

  private renderDrawingEntity(idx: number): void {
    const NUM_STROKES = 20;

    <view
      position="absolute"
      top="15%"
      left="5%"
      right="5%"
      height="70%"
    >
      {Array.from({ length: NUM_STROKES }, (_, i) => {
        const strokePath = makeStrokePath(idx, i);
        const color = STROKE_COLORS[i % STROKE_COLORS.length];
        const width = (2 + (i % 5) * 1.5) * ds;
        const cap = STROKE_CAPS[i % STROKE_CAPS.length];

        <shape
          position="absolute"
          width="100%"
          height="100%"
          strokeColor={color}
          strokeWidth={width}
          strokeCap={cap}
          path={strokePath}
          opacity={0.4 + 0.3 * Math.sin(idx * 0.1 + i)}
        />;
      })}
    </view>;
  }

  private renderFilterOverlay(idx: number): void {
    const hue = (idx * 3 + 100) % 360;

    <view
      position="absolute"
      top={0} left={0} right={0} bottom={0}
      background={`linear-gradient(180deg, ${hsla(hue, 50, 50, 0.1)}, ${hsla((hue + 180) % 360, 50, 30, 0.15)})`}
    >
      <view
        position="absolute"
        bottom="15%"
        left="5%"
        right="5%"
        alignItems="center"
      >
        <label
          font={smallFont}
          color="rgba(255,255,255,0.5)"
          value={`Filter: Vivid #${idx % 12}`}
        />
      </view>
    </view>;
  }

  private renderBottomBar(idx: number): void {
    const barHue = (idx * 5) % 360;

    <view
      position="absolute"
      bottom={10 * ds}
      left={8 * ds}
      right={8 * ds}
      height={48 * ds}
      flexDirection="row"
      borderRadius={12 * ds}
      background={`linear-gradient(90deg, ${hsla(barHue, 30, 15, 0.8)}, ${hsla((barHue + 60) % 360, 30, 20, 0.8)})`}
      padding={4 * ds}
      slowClipping
    >
      {Array.from({ length: 8 }, (_, i) => {
        const itemHue = (barHue + i * 40) % 360;
        <view
          flexGrow={1}
          marginLeft={i > 0 ? 3 * ds : 0}
          backgroundColor={hsla(itemHue, 50, 50 + (idx + i) % 20, 0.7)}
          borderRadius={6 * ds}
          alignItems="center"
          justifyContent="center"
        >
          <label
            font={tinyFont}
            color="white"
            value={`${i + 1}`}
          />
        </view>;
      })}
    </view>;
  }

  private renderClippingRegions(idx: number): void {
    const hue1 = (idx * 7 + 30) % 360;
    const hue2 = (idx * 7 + 150) % 360;

    {/* Clipping region 1 */}
    <view
      position="absolute"
      right="5%"
      top="8%"
      width="18%"
      aspectRatio={1}
      slowClipping
      borderRadius={'50%'}
      backgroundColor={hsla(hue1, 60, 50, 0.3)}
    >
      <view
        width="100%"
        height="100%"
        background={`linear-gradient(135deg, ${hsla(hue1, 70, 60, 0.6)}, ${hsla(hue1, 70, 30, 0.6)})`}
        alignItems="center"
        justifyContent="center"
      >
        <label font={smallFont} color="white" value={`${idx}`} />
      </view>
    </view>;

    {/* Clipping region 2 */}
    <view
      position="absolute"
      left="3%"
      bottom="18%"
      width="15%"
      aspectRatio={1.5}
      slowClipping
      borderRadius={10 * ds}
      backgroundColor={hsla(hue2, 50, 40, 0.3)}
    >
      <view
        width="100%"
        height="100%"
        flexDirection="column"
      >
        <view flexGrow={1} backgroundColor={hsla(hue2, 60, 60, 0.4)} />
        <view flexGrow={1} backgroundColor={hsla((hue2 + 90) % 360, 60, 60, 0.4)} />
      </view>
    </view>;

    {/* Clipping region 3: nested circles */}
    <view
      position="absolute"
      right="8%"
      bottom="25%"
      width="12%"
      aspectRatio={1}
      slowClipping
      borderRadius={'50%'}
      backgroundColor={hsla((hue1 + 90) % 360, 50, 50, 0.25)}
    >
      <view
        width="100%"
        height="100%"
        alignItems="center"
        justifyContent="center"
      >
        <view
          width="60%"
          aspectRatio={1}
          slowClipping
          borderRadius={'50%'}
          backgroundColor={hsla((hue1 + 180) % 360, 60, 60, 0.4)}
        />
      </view>
    </view>;
  }
}

// ---------------------------------------------------------------------------
// Stats helpers
// ---------------------------------------------------------------------------

interface PhaseStats {
  name: string;
  min: number;
  max: number;
  avg: number;
  p50: number;
  p95: number;
  total: number;
}

function computeStats(name: string, values: number[]): PhaseStats {
  if (values.length === 0) {
    return { name, min: 0, max: 0, avg: 0, p50: 0, p95: 0, total: 0 };
  }
  const sorted = [...values].sort((a, b) => a - b);
  const total = sorted.reduce((s, v) => s + v, 0);
  const avg = total / sorted.length;
  const p50 = sorted[Math.floor(sorted.length * 0.5)];
  const p95 = sorted[Math.floor(sorted.length * 0.95)];
  return {
    name,
    min: sorted[0],
    max: sorted[sorted.length - 1],
    avg,
    p50,
    p95,
    total,
  };
}

function formatMs(ms: number): string {
  return ms.toFixed(2);
}

function statsBlock(s: PhaseStats): string {
  const f = formatMs;
  return [
    `[${s.name}]`,
    `  avg ${f(s.avg)}  p50 ${f(s.p50)}  p95 ${f(s.p95)}`,
    `  min ${f(s.min)}  max ${f(s.max)}  tot ${f(s.total)}`,
  ].join('\n');
}

// ---------------------------------------------------------------------------
// Benchmark page
// ---------------------------------------------------------------------------

const FRAME_COUNTS = [30, 60, 120];
const BENCHMARK_WIDTH = 1080;
const BENCHMARK_HEIGHT = 1920;

interface FrameTiming {
  render: number;
  layout: number;
  drawTotal: number;
  drawMainThread: number;
  drawOverhead: number;
  raster: number;
  jsThread: number;
  mainThread: number;
  total: number;
}

interface State {
  frameCountIndex: number;
  running: boolean;
  resultText: string;
}

export interface ViewModel {}
export interface ComponentContext {}

@NavigationPage(module)
export class DrawFrameBenchmark extends NavigationPageStatefulComponent<ViewModel, ComponentContext> {
  state: State = {
    frameCountIndex: 1, // default 60
    running: false,
    resultText: 'Tap "Run Benchmark" to start.',
  };

  onRender(): void {
    const frameCount = FRAME_COUNTS[this.state.frameCountIndex];
    <view backgroundColor="white" width="100%" height="100%">
      <scroll style={styles.scroll} padding={16}>
        <layout flexDirection="column" width="100%">
          <label
            style={styles.title}
            value="DrawFrame Benchmark"
            font={systemBoldFont(20)}
          />

          <layout flexDirection="row" marginTop={12} marginBottom={12}>
            <view
              backgroundColor="cornflowerblue"
              padding={12}
              borderRadius={12}
              marginRight={8}
              onTap={this.cycleFrameCount}
            >
              <label value={`Frames: ${frameCount}`} font={systemBoldFont(14)} color="white" />
            </view>

            <view
              backgroundColor={this.state.running ? 'gray' : 'green'}
              padding={12}
              borderRadius={12}
              onTap={this.state.running ? undefined : this.runBenchmark}
            >
              <label
                value={this.state.running ? 'Running...' : 'Run Benchmark'}
                font={systemBoldFont(14)}
                color="white"
              />
            </view>
          </layout>

          <label
            value={this.state.resultText}
            font={monoSpaceFont(8)}
            color="black"
            numberOfLines={0}
          />
        </layout>
      </scroll>
    </view>;
  }

  private cycleFrameCount = (): void => {
    if (this.state.running) return;
    this.setState({
      frameCountIndex: (this.state.frameCountIndex + 1) % FRAME_COUNTS.length,
    });
  };

  private runBenchmark = (): void => {
    this.setState({ running: true, resultText: 'Starting benchmark...\n' });
    setTimeoutInterruptible(() => {
      void this.executeBenchmark();
    });
  };

  private async executeBenchmark(): Promise<void> {
    const frameCount = FRAME_COUNTS[this.state.frameCountIndex];
    const width = BENCHMARK_WIDTH;
    const height = BENCHMARK_HEIGHT;
    const bytesPerPixel = 4;

    const context: IManagedContext = createManagedContext();
    const bitmap: IBitmap = createBitmap({
      width,
      height,
      colorType: BitmapColorType.RGBA8888,
      alphaType: BitmapAlphaType.OPAQUE,
      rowBytes: width * bytesPerPixel,
    });

    const timings: FrameTiming[] = [];

    const overallSw = Stopwatch.createAndStart();

    for (let i = 0; i < frameCount; i++) {
      const frameSw = Stopwatch.createAndStart();

      // -- Render phase (JS thread) --
      const renderSw = Stopwatch.createAndStart();
      context.render(() => {
        <ComplexBenchmarkScene frameIndex={i} />;
      });
      const renderMs = renderSw.elapsedMs();

      // -- Layout phase (async: JS -> main thread -> callback) --
      const layoutSw = Stopwatch.createAndStart();
      await context.layout(width, height, false);
      const layoutMs = layoutSw.elapsedMs();

      // -- Draw phase (async: JS -> main thread -> callback) --
      const drawSw = Stopwatch.createAndStart();
      const { frame, mainThreadMs } = await context.draw();
      const drawTotalMs = drawSw.elapsedMs();
      const drawOverheadMs = Math.max(0, drawTotalMs - mainThreadMs);

      // -- Raster phase (JS thread) --
      const rasterSw = Stopwatch.createAndStart();
      frame.rasterInto(bitmap, true);
      const rasterMs = rasterSw.elapsedMs();

      frame.dispose();

      const totalMs = frameSw.elapsedMs();

      // JS thread = render + layout + raster + draw overhead (scheduling)
      const jsThreadMs = renderMs + layoutMs + rasterMs + drawOverheadMs;

      timings.push({
        render: renderMs,
        layout: layoutMs,
        drawTotal: drawTotalMs,
        drawMainThread: mainThreadMs,
        drawOverhead: drawOverheadMs,
        raster: rasterMs,
        jsThread: jsThreadMs,
        mainThread: mainThreadMs,
        total: totalMs,
      });

      if (i % 10 === 0 || i === frameCount - 1) {
        console.log(
          `[Benchmark] #${i}/${frameCount}: ` +
          `js=${formatMs(jsThreadMs)} main=${formatMs(mainThreadMs)} total=${formatMs(totalMs)}`,
        );
      }
    }

    const overallMs = overallSw.elapsedMs();

    bitmap.dispose();
    context.dispose();

    // Compute aggregate stats per phase
    const renderStats = computeStats('Render(JS)', timings.map(t => t.render));
    const layoutStats = computeStats('Layout(JS)', timings.map(t => t.layout));
    const drawTotalStats = computeStats('Draw total', timings.map(t => t.drawTotal));
    const drawMainStats = computeStats('Draw(main)', timings.map(t => t.drawMainThread));
    const drawOhStats = computeStats('Draw sched', timings.map(t => t.drawOverhead));
    const rasterStats = computeStats('Raster(JS)', timings.map(t => t.raster));
    const jsStats = computeStats('JS thread', timings.map(t => t.jsThread));
    const mainStats = computeStats('Main thrd', timings.map(t => t.mainThread));
    const totalStats = computeStats('Frame tot', timings.map(t => t.total));

    const fps = frameCount / (overallMs / 1000);

    const f = formatMs;
    const lines = [
      `DrawFrame Benchmark (complex scene)`,
      `${frameCount} frames @ ${width}x${height}`,
      `${f(overallMs)}ms total | ~${fps.toFixed(1)} fps`,
      ``,
      `=== Thread summary (ms) ===`,
      statsBlock(jsStats),
      statsBlock(mainStats),
      ``,
      `=== Phase breakdown (ms) ===`,
      statsBlock(renderStats),
      statsBlock(layoutStats),
      statsBlock(drawTotalStats),
      statsBlock(drawMainStats),
      statsBlock(drawOhStats),
      statsBlock(rasterStats),
      statsBlock(totalStats),
      ``,
      `=== Per frame (ms) ===`,
      ` #  Rndr  Lyot DrTot DrMn DrOh Rstr  JS  Main Total`,
    ];

    for (let i = 0; i < frameCount; i++) {
      const t = timings[i];
      lines.push(
        `${String(i).padStart(2)}` +
        ` ${f(t.render).padStart(5)}` +
        ` ${f(t.layout).padStart(5)}` +
        ` ${f(t.drawTotal).padStart(5)}` +
        ` ${f(t.drawMainThread).padStart(4)}` +
        ` ${f(t.drawOverhead).padStart(4)}` +
        ` ${f(t.raster).padStart(5)}` +
        ` ${f(t.jsThread).padStart(4)}` +
        ` ${f(t.mainThread).padStart(4)}` +
        ` ${f(t.total).padStart(5)}`,
      );
    }

    const resultText = lines.join('\n');
    console.log(resultText);

    if (!this.isDestroyed()) {
      this.setState({ running: false, resultText });
    }
  }
}

const styles = {
  scroll: new Style<ScrollView>({
    alignItems: 'center',
    height: '100%',
  }),

  title: new Style<Label>({
    color: 'black',
    accessibilityCategory: 'header',
    width: '100%',
  }),
};

function monoSpaceFont(size: number): string {
  if (Device.isIOS()) {
    return `Menlo ${size}`;
  } else {
    return `monospace ${size}`;
  }
}
