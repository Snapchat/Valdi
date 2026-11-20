/* eslint-disable @snap/valdi/jsx-no-lambda -- each case's render callback is a plain function, not a
   component, so there is no instance to hold stable handlers; every handler records into its own case's probe. */
import { AnimationCurve } from 'valdi_core/src/AnimationOptions';
import { Component, StatefulComponent } from 'valdi_core/src/Component';
import { Device } from 'valdi_core/src/Device';
import { Lazy } from 'valdi_core/src/Lazy';
import { lazyImport } from 'valdi_core/src/LazyImport';
import {
  InlineUnparsedLocalizableStringResolver,
  LocalizationJsonFile,
} from 'valdi_core/src/localization/InlineUnparsedLocalizableStringResolver';
import { Locale } from 'valdi_core/src/localization/Locale';
import { createProviderComponentWithKeyName } from 'valdi_core/src/provider/createProvider';
import { ProvidersValuesViewModel, withProviders } from 'valdi_core/src/provider/withProviders';

import res from '../res';
import { getPlatform } from './IntegrationTestHost';
import {
  IntegrationTestCase,
  IntegrationTestInteractionContext,
  IntegrationTestRenderContext,
} from './IntegrationTestTypes';
import type * as LazyProbeModule from './PlatformLazyProbeModule';
import Strings from './Strings';

const WIDTH = 360;
const HEIGHT = 560;
const CARD = '#F8FAFC';
const IDLE_POLL_LIMIT = 40;
const CURVE_SECONDS = 0.4;
const GREETING_EN = 'Hello from the module';
const GREETING_FR = 'Bonjour depuis le module';
const ROW_COUNT = 2000;
const ROW_HEIGHT = 40;

/**
 * Per-case state shared between the fixture components (which run during render) and the case's
 * `interact` callback (which asserts). A probe is created on first render and reused afterwards, so
 * re-renders of the app shell do not reset counters.
 */
const probes: { [caseId: string]: unknown } = {};

function probeFor<T>(caseId: string, init: () => T): T {
  if (probes[caseId] === undefined) {
    probes[caseId] = init();
  }
  return probes[caseId] as T;
}

function assert(condition: boolean, message: string): void {
  if (!condition) {
    throw new Error(message);
  }
}

function assertEqual<T>(actual: T, expected: T, label: string): void {
  if (actual !== expected) {
    throw new Error(`${label}: expected ${String(expected)}, got ${String(actual)}`);
  }
}

async function waitUntil(
  ctx: IntegrationTestInteractionContext,
  label: string,
  predicate: () => boolean,
): Promise<void> {
  for (let i = 0; i < IDLE_POLL_LIMIT; i++) {
    if (predicate()) {
      return;
    }
    await ctx.waitForIdle();
  }
  throw new Error(`timed out waiting for ${label}`);
}

// --- components: stateless and stateful ---

interface ComponentsProbe {
  statefulRenders: number;
  lastCount: number;
  staticRenders: number;
}

interface StaticChildViewModel {
  probe: ComponentsProbe;
}

class StaticChild extends Component<StaticChildViewModel> {
  onRender(): void {
    this.viewModel.probe.staticRenders++;
    <label value="static child" font="system 16" color="#334155" />;
  }
}

interface CounterFixtureViewModel {
  probe: ComponentsProbe;
}

class CounterFixture extends StatefulComponent<CounterFixtureViewModel, { count: number }> {
  state = { count: 0 };
  private scheduled = false;

  onRender(): void {
    const count = this.state?.count ?? 0;
    const probe = this.viewModel.probe;
    probe.statefulRenders++;
    probe.lastCount = count;
    if (!this.scheduled) {
      this.scheduled = true;
      this.setTimeoutDisposable(() => {
        this.setState({ count: 1 });
        this.setTimeoutDisposable(() => this.setState({ count: 2 }), 0);
      }, 0);
    }
    <view width="100%" padding={12} backgroundColor="#FFFFFF">
      <label value={`count ${count}`} font="system-bold 22" color="#111827" />
      <StaticChild probe={probe} />
    </view>;
  }
}

// --- slots ---

interface SlotsProbe {
  created: string[];
  destroyed: string[];
}

interface SlotContainerViewModel {}

class SlotContainer extends Component<SlotContainerViewModel> {
  onRender(): void {
    <view width="100%" padding={12} backgroundColor="#FFFFFF">
      <view backgroundColor="#DBEAFE" padding={8}>
        <slot name="header" />
      </view>
      <view backgroundColor="#DCFCE7" padding={8} marginTop={8}>
        <slot />
      </view>
    </view>;
  }
}

interface SlotHostViewModel {
  probe: SlotsProbe;
}

class SlotHost extends StatefulComponent<SlotHostViewModel, { variant: 'a' | 'b' }> {
  state = { variant: 'a' as 'a' | 'b' };
  private scheduled = false;

  onRender(): void {
    const variant = this.state?.variant ?? 'a';
    const probe = this.viewModel.probe;
    if (!this.scheduled) {
      this.scheduled = true;
      this.setTimeoutDisposable(() => this.setState({ variant: 'b' }), 0);
    }
    const track = (name: string): { onViewCreate: () => void; onViewDestroy: () => void } => ({
      onViewCreate: () => probe.created.push(name),
      onViewDestroy: () => probe.destroyed.push(name),
    });
    <SlotContainer>
      <slotted slot="header">
        <view
          key={`header-${variant}`}
          height={24}
          width={120}
          backgroundColor="#2563EB"
          {...track(`header-${variant}`)}
        />
      </slotted>
      <slotted>
        <view key={`body-${variant}`} height={24} width={120} backgroundColor="#16A34A" {...track(`body-${variant}`)} />
      </slotted>
    </SlotContainer>;
  }
}

// --- providers ---

interface ProvidersProbe {
  seen: { [consumer: string]: string[] };
}

const TestValueProvider = createProviderComponentWithKeyName<string>('integration_test_value_provider');

interface ConsumerViewModel extends ProvidersValuesViewModel<[string]> {
  name: string;
  probe: ProvidersProbe;
}

class ConsumerComponent extends Component<ConsumerViewModel> {
  onRender(): void {
    const [value] = this.viewModel.providersValues;
    const seen = (this.viewModel.probe.seen[this.viewModel.name] ??= []);
    seen.push(value);
    <label value={`${this.viewModel.name}: ${value}`} font="system 16" color="#111827" />;
  }
}

const ProvidedConsumer = withProviders(TestValueProvider)(ConsumerComponent);

interface ProvidersHostViewModel {
  probe: ProvidersProbe;
}

class ProvidersHost extends StatefulComponent<ProvidersHostViewModel, { value: string }> {
  state = { value: 'one' };
  private scheduled = false;

  onRender(): void {
    const probe = this.viewModel.probe;
    if (!this.scheduled) {
      this.scheduled = true;
      this.setTimeoutDisposable(() => this.setState({ value: 'two' }), 0);
    }
    <TestValueProvider value={this.state?.value ?? 'one'}>
      <view width="100%" padding={12} backgroundColor="#FFFFFF">
        <ProvidedConsumer name="shallow" probe={probe} />
        <view padding={8} marginTop={8} backgroundColor="#F1F5F9">
          <view padding={8} backgroundColor="#E2E8F0">
            <ProvidedConsumer name="deep" probe={probe} />
          </view>
        </view>
      </view>
    </TestValueProvider>;
  }
}

// --- renderer hooks ---

interface RendererHooksProbe {
  events: string[];
  tickAtNextRender: number;
}

interface RendererHooksFixtureViewModel {
  probe: RendererHooksProbe;
}

class RendererHooksFixture extends StatefulComponent<RendererHooksFixtureViewModel, { tick: number }> {
  state = { tick: 0 };
  private registered = false;

  onRender(): void {
    const probe = this.viewModel.probe;
    if (!this.registered) {
      this.registered = true;
      this.renderer.onRenderComplete(() => probe.events.push('render'));
      this.renderer.onLayoutComplete(() => probe.events.push('layout'));
      this.setTimeoutDisposable(() => {
        this.renderer.onNextRenderComplete(() => {
          probe.events.push('nextRender');
          probe.tickAtNextRender = this.state.tick;
        });
        this.renderer.onLayoutComplete(() => probe.events.push('layout2'));
        this.setState({ tick: 1 });
      }, 0);
    }
    <label value={`tick ${this.state?.tick ?? 0}`} font="system 16" color="#111827" />;
  }
}

// --- animations ---

interface AnimationsProbe {
  curveDone: boolean;
  springDone: boolean;
  opacity: number;
  minOpacity: number;
  curveMs: number;
  springMs: number;
}

interface AnimationsFixtureViewModel {
  probe: AnimationsProbe;
}

class AnimationsFixture extends StatefulComponent<AnimationsFixtureViewModel, { opacity: number }> {
  state = { opacity: 1 };
  private started = false;

  onRender(): void {
    const opacity = this.state?.opacity ?? 1;
    this.viewModel.probe.opacity = opacity;
    this.viewModel.probe.minOpacity = Math.min(this.viewModel.probe.minOpacity, opacity);
    if (!this.started) {
      this.started = true;
      this.setTimeoutDisposable(() => void this.runAnimations(), 0);
    }
    <view width={160} height={160} backgroundColor="#7C3AED" opacity={opacity} borderRadius={16} />;
  }

  private async runAnimations(): Promise<void> {
    const probe = this.viewModel.probe;
    let started = Date.now();
    await this.animatePromise({ duration: CURVE_SECONDS, curve: AnimationCurve.Linear }, () =>
      this.setState({ opacity: 0.25 }),
    );
    probe.curveMs = Date.now() - started;
    probe.curveDone = true;
    started = Date.now();
    await this.animatePromise({ stiffness: 300, damping: 30 }, () => this.setState({ opacity: 1 }));
    probe.springMs = Date.now() - started;
    probe.springDone = true;
  }
}

// --- lazy ---

interface LazyProbe {
  visibleCreated: number;
  offscreenCreated: number;
}

interface LazyProbeChildViewModel {
  onCreateCallback: () => void;
}

class LazyProbeChild extends Component<LazyProbeChildViewModel> {
  onCreate(): void {
    this.viewModel.onCreateCallback();
  }

  onRender(): void {
    <view width="100%" height="100%" backgroundColor="#F59E0B" />;
  }
}

// --- helpers for cases ---

function newProbe<T>(init: T): () => T {
  return () => init;
}

const lazyProbeModule = lazyImport<typeof LazyProbeModule>(
  // eslint-disable-next-line @typescript-eslint/no-var-requires, @typescript-eslint/no-unsafe-return -- the documented lazyImport loader form
  () => require('./PlatformLazyProbeModule', true),
);

export const PLATFORM_FEATURE_TEST_CASES: readonly IntegrationTestCase[] = [
  {
    id: 'platform-components-stateless-stateful',
    name: 'Stateless and stateful components',
    description:
      'A stateful component updates its own state twice while a stateless child, given an unchanged view model, must not re-render.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      const probe = probeFor<ComponentsProbe>(
        ctx.caseId,
        newProbe({ statefulRenders: 0, lastCount: -1, staticRenders: 0 }),
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <CounterFixture probe={probe} />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      const probe = probes[ctx.caseId] as ComponentsProbe;
      await waitUntil(ctx, 'count to reach 2', () => probe.lastCount === 2);
      ctx.record(
        `statefulRenders:${probe.statefulRenders} lastCount:${probe.lastCount} staticRenders:${probe.staticRenders}`,
      );
      assert(
        probe.statefulRenders >= 3,
        `stateful component rendered ${probe.statefulRenders} times, expected at least 3`,
      );
      assertEqual(probe.staticRenders, 1, 'stateless child render count');
    },
  },
  {
    id: 'platform-slots-default-named-swap',
    name: 'Slots: default, named and swapped content',
    description:
      'Slots content into a container through a default and a named slot, then swaps both children by key and expects the old ones destroyed.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      const probe = probeFor<SlotsProbe>(ctx.caseId, () => ({ created: [], destroyed: [] }));
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <SlotHost probe={probe} />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      const probe = probes[ctx.caseId] as SlotsProbe;
      await waitUntil(ctx, 'swapped slot content', () => probe.created.indexOf('body-b') !== -1);
      ctx.record(`created:${probe.created.join(',')} destroyed:${probe.destroyed.join(',')}`);
      for (const name of ['header-a', 'body-a', 'header-b', 'body-b']) {
        assert(probe.created.indexOf(name) !== -1, `slot content ${name} was never created`);
      }
      for (const name of ['header-a', 'body-a']) {
        assert(probe.destroyed.indexOf(name) !== -1, `slot content ${name} was not destroyed after the swap`);
      }
    },
  },
  {
    id: 'platform-providers-value-change',
    name: 'Providers: nested consumers and value change',
    description: 'Two consumers at different depths read a provided value; changing the value must reach both.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      const probe = probeFor<ProvidersProbe>(ctx.caseId, () => ({ seen: {} }));
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <ProvidersHost probe={probe} />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      const probe = probes[ctx.caseId] as ProvidersProbe;
      const last = (name: string): string | undefined => {
        const seen = probe.seen[name];
        return seen ? seen[seen.length - 1] : undefined;
      };
      await waitUntil(ctx, 'both consumers to see "two"', () => last('shallow') === 'two' && last('deep') === 'two');
      ctx.record(`shallow:${(probe.seen['shallow'] ?? []).join('>')} deep:${(probe.seen['deep'] ?? []).join('>')}`);
      for (const name of ['shallow', 'deep']) {
        assertEqual((probe.seen[name] ?? [])[0], 'one', `${name} first value`);
      }
    },
  },
  {
    id: 'platform-renderer-hooks',
    name: 'Renderer hooks',
    description:
      'Registers onRenderComplete, onLayoutComplete and onNextRenderComplete across two render passes and expects each to fire exactly once, render before layout, and the second layout after the next-render hook.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      const probe = probeFor<RendererHooksProbe>(ctx.caseId, () => ({ events: [], tickAtNextRender: -1 }));
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <RendererHooksFixture probe={probe} />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      const probe = probes[ctx.caseId] as RendererHooksProbe;
      await waitUntil(ctx, 'second layout pass', () => probe.events.indexOf('layout2') !== -1);
      // Let any duplicate callbacks arrive before asserting "exactly once".
      await ctx.waitForIdle();
      await ctx.waitForIdle();
      ctx.record(`events:${probe.events.join('>')}`);
      const at = (name: string): number => probe.events.indexOf(name);
      assertEqual(probe.events.length, 4, 'number of hook callbacks');
      for (const name of ['render', 'layout', 'nextRender', 'layout2']) {
        assert(at(name) !== -1, `${name} hook did not fire`);
      }
      assert(at('render') < at('layout'), `onRenderComplete fired after onLayoutComplete: ${probe.events.join('>')}`);
      assert(at('nextRender') < at('layout2'), `second layout fired before next render: ${probe.events.join('>')}`);
    },
  },
  {
    id: 'platform-renderer-hooks-next-render-state',
    name: 'onNextRenderComplete sees the update that triggered it',
    description:
      'Registers onNextRenderComplete and then calls setState in the same task. The callback must fire after the render that applies the state, so the component sees the new value.',
    element: 'view',
    expectedFailureOnPlatforms: ['web'],
    expectedFailureReason:
      'Web fires onNextRenderComplete before the render that applies the state update (the callback saw the old state).',
    render: (ctx: IntegrationTestRenderContext) => {
      const probe = probeFor<RendererHooksProbe>(ctx.caseId, () => ({ events: [], tickAtNextRender: -1 }));
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <RendererHooksFixture probe={probe} />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      const probe = probes[ctx.caseId] as RendererHooksProbe;
      await waitUntil(ctx, 'next render hook', () => probe.events.indexOf('nextRender') !== -1);
      ctx.record(`tickAtNextRender:${probe.tickAtNextRender}`);
      const message = `onNextRenderComplete saw tick ${probe.tickAtNextRender}, expected 1`;
      if (probe.tickAtNextRender !== 1 && getPlatform() === 'web') {
        // Recorded instead of thrown so the runner reports it through expectedFailureOnPlatforms.
        ctx.record(message);
        return;
      }
      assertEqual(probe.tickAtNextRender, 1, 'state seen by onNextRenderComplete');
    },
  },
  {
    id: 'platform-animations-curve-and-spring',
    name: 'Imperative animations: curve and spring',
    description:
      'Runs animatePromise with a 400ms linear curve and with a spring on opacity. Each promise must take real time to resolve (a renderer that skips animation resolves immediately), and the end state must be applied.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      const probe = probeFor<AnimationsProbe>(
        ctx.caseId,
        newProbe({ curveDone: false, springDone: false, opacity: 1, minOpacity: 1, curveMs: -1, springMs: -1 }),
      );
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <AnimationsFixture probe={probe} />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      const probe = probes[ctx.caseId] as AnimationsProbe;
      await waitUntil(ctx, 'curve animation', () => probe.curveDone);
      ctx.record(`curve animation resolved, opacity state:${probe.opacity}`);
      await waitUntil(ctx, 'spring animation', () => probe.springDone);
      ctx.record(`spring animation resolved, opacity state:${probe.opacity}`);
      await waitUntil(ctx, 'final opacity', () => probe.opacity === 1);
      ctx.record(`curveMs:${probe.curveMs} springMs:${probe.springMs}`);
      assert(
        probe.curveMs >= CURVE_SECONDS * 1000 * 0.75,
        `linear animation of ${CURVE_SECONDS * 1000}ms resolved after ${probe.curveMs}ms`,
      );
      assert(probe.curveMs < 2500, `linear animation took ${probe.curveMs}ms`);
      assert(probe.springMs >= 50, `spring animation resolved after ${probe.springMs}ms`);
      assertEqual(probe.minOpacity, 0.25, 'lowest opacity rendered during the curve animation');
    },
  },
  {
    id: 'platform-lazy-component',
    name: 'Lazy component renders on first visibility',
    description:
      'A Lazy node inside the viewport creates its child; a Lazy node positioned far outside the viewport must not.',
    element: 'layout',
    render: (ctx: IntegrationTestRenderContext) => {
      const probe = probeFor<LazyProbe>(ctx.caseId, newProbe({ visibleCreated: 0, offscreenCreated: 0 }));
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD}>
        <Lazy width={120} height={60} left={18} top={18}>
          <LazyProbeChild onCreateCallback={() => probe.visibleCreated++} />
        </Lazy>
        <Lazy width={120} height={60} left={18} top={4000}>
          <LazyProbeChild onCreateCallback={() => probe.offscreenCreated++} />
        </Lazy>
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      const probe = probes[ctx.caseId] as LazyProbe;
      await waitUntil(ctx, 'visible lazy child', () => probe.visibleCreated > 0);
      await ctx.waitForIdle();
      ctx.record(`visibleCreated:${probe.visibleCreated} offscreenCreated:${probe.offscreenCreated}`);
      assertEqual(probe.visibleCreated, 1, 'visible lazy child create count');
      assertEqual(probe.offscreenCreated, 0, 'offscreen lazy child create count');
    },
  },
  {
    id: 'platform-lazy-import',
    name: 'lazyImport defers module load',
    description: 'A lazyImport reference must not load its module until `.get` is first read, then must cache it.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <label value="lazyImport" font="system-bold 20" color="#111827" />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      const globalProbe = globalThis as { integrationLazyProbeLoads?: number };
      assertEqual(globalProbe.integrationLazyProbeLoads ?? 0, 0, 'module loads before .get');
      const first = lazyProbeModule.get;
      assertEqual(globalProbe.integrationLazyProbeLoads, 1, 'module loads after first .get');
      const second = lazyProbeModule.get;
      assertEqual(globalProbe.integrationLazyProbeLoads, 1, 'module loads after second .get');
      assert(first === second, 'lazyImport returned a different module object on the second read');
      assertEqual(first.PROBE_VALUE, 'lazy-probe', 'lazy module export');
      ctx.record(`lazy module loaded once, export:${first.PROBE_VALUE}`);
    },
  },
  {
    id: 'platform-list-limit-to-viewport',
    name: 'Long scroll list only creates views for the viewport',
    description:
      'Renders 2000 rows in a scroll view and expects only a window of native views to be created, not one per row.',
    element: 'scroll',
    expectedFailureOnPlatforms: ['web'],
    expectedFailureReason:
      'Web renders a DOM node for every row: limitToViewport is not honored, so 2000 of 2000 views are created.',
    render: (ctx: IntegrationTestRenderContext) => {
      const probe = probeFor<{ created: number; destroyed: number }>(ctx.caseId, () => ({ created: 0, destroyed: 0 }));
      const rows: number[] = [];
      for (let i = 0; i < ROW_COUNT; i++) {
        rows.push(i);
      }
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD}>
        <scroll width="100%" height="100%" ref={ctx.targetRef}>
          {rows.forEach(i => {
            <view
              key={`row-${i}`}
              height={ROW_HEIGHT}
              width="100%"
              backgroundColor={i % 2 === 0 ? '#E2E8F0' : '#CBD5E1'}
              onViewCreate={() => probe.created++}
              onViewDestroy={() => probe.destroyed++}
            />;
          })}
        </scroll>
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      const probe = probes[ctx.caseId] as { created: number; destroyed: number };
      await waitUntil(ctx, 'rows to be created', () => probe.created > 0);
      await ctx.waitForIdle();
      ctx.record(`rows:${ROW_COUNT} created:${probe.created} destroyed:${probe.destroyed}`);
      const message = `created ${probe.created} views for ${ROW_COUNT} rows, expected a viewport-sized window`;
      if (probe.created >= ROW_COUNT / 10 && getPlatform() === 'web') {
        // Recorded instead of thrown so the runner reports it through expectedFailureOnPlatforms.
        ctx.record(message);
        return;
      }
      assert(probe.created < ROW_COUNT / 10, message);
    },
  },
  {
    id: 'platform-logging-console',
    name: 'console logging (smoke)',
    description:
      'Calls console.log, info, warn, error and debug with strings, objects, errors, circular structures and unusual values and expects none to throw. This does not assert where the output goes.',
    element: 'view',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <label value="console logging" font="system-bold 20" color="#111827" />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      const circular: { name: string; self?: unknown } = { name: 'circular' };
      circular.self = circular;
      const errorWithCause: Error & { cause?: unknown } = new Error('outer');
      errorWithCause.cause = new Error('inner');
      const payloads: unknown[][] = [
        ['integration-test string'],
        ['integration-test object', { nested: { ok: true, list: [1, 2, 3] } }],
        ['integration-test error', new Error('integration-test error')],
        ['integration-test error with cause', errorWithCause],
        ['integration-test circular', circular],
        ['integration-test nullish', undefined, null],
        ['integration-test function and symbol', () => 1, Symbol('integration-test')],
        ['integration-test format %s %d', 'text', 42],
        ['integration-test large string', 'x'.repeat(200000)],
      ];
      const levels = ['log', 'info', 'warn', 'error', 'debug'] as const;
      let calls = 0;
      for (const level of levels) {
        assertEqual(typeof console[level], 'function', `console.${level}`);
        for (const payload of payloads) {
          console[level](...payload);
          calls++;
        }
      }
      ctx.record(`${calls} console calls did not throw`);
    },
  },
  {
    id: 'platform-strings-localization',
    name: 'Strings and localization',
    description:
      'Reads module strings, including a placeholder, and expects the translation for the device language, then drives the real locale resolver with injected locales to cover French selection, language-only match, fallback and preference order.',
    element: 'label',
    render: (ctx: IntegrationTestRenderContext) => {
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <label value={Strings.platformGreeting()} font="system-bold 20" color="#111827" />
        <label value={Strings.platformItemCount(3)} font="system 16" color="#334155" marginTop={8} />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      const locales = Device.getDeviceLocales();
      const french = (locales[0] ?? '').toLowerCase().indexOf('fr') === 0;
      const greeting = Strings.platformGreeting();
      const count = Strings.platformItemCount(3);
      ctx.record(`locale:${locales[0] ?? 'none'} greeting:${greeting} count:${count}`);
      assertEqual(greeting, french ? GREETING_FR : GREETING_EN, 'greeting');
      assertEqual(count, french ? '3 éléments' : '3 items', 'placeholder string');

      // The device locale only exercises one branch, so drive the real resolver with injected locales.
      const files: { [path: string]: LocalizationJsonFile } = {
        'strings-en.json': {
          platform_greeting: { defaultMessage: GREETING_EN },
          platform_item_count: { defaultMessage: '{count%d} items' },
        },
        'strings-fr.json': {
          platform_greeting: { defaultMessage: GREETING_FR },
          platform_item_count: { defaultMessage: '{count%d} éléments' },
        },
      };
      const resolve = (key: string, ...tags: string[]): string | undefined =>
        new InlineUnparsedLocalizableStringResolver(
          ['en', 'fr'],
          ['strings-en.json', 'strings-fr.json'],
          path => files[path],
          () => tags.map(Locale.parse),
        ).resolveUnparsedLocalizableString(key);
      assertEqual(resolve('platform_greeting', 'fr-FR'), GREETING_FR, 'fr-FR picks French');
      assertEqual(resolve('platform_greeting', 'fr-CA'), GREETING_FR, 'fr-CA matches by language');
      assertEqual(resolve('platform_item_count', 'fr-FR'), '$0 éléments', 'placeholder index in French');
      assertEqual(resolve('platform_greeting', 'en-GB'), GREETING_EN, 'en-GB picks English');
      assertEqual(resolve('platform_greeting', 'de-DE'), GREETING_EN, 'unavailable language falls back to English');
      assertEqual(resolve('platform_greeting', 'de-DE', 'fr-FR'), GREETING_FR, 'second preference beats fallback');
      assertEqual(resolve('missing_key', 'fr-FR'), undefined, 'unknown key');
      ctx.record('resolver picked fr, en, fallback and second-preference locales correctly');
    },
  },
  {
    id: 'platform-images-res-asset',
    name: 'Images from res/',
    description:
      'Loads a module res asset and expects the asset-load callback to succeed and the decode callback to report a non-zero size.',
    element: 'image',
    render: (ctx: IntegrationTestRenderContext) => {
      const probe = probeFor<{ loaded?: boolean; error?: string; width: number; height: number }>(ctx.caseId, () => ({
        width: 0,
        height: 0,
      }));
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <image
          src={res.image}
          width={200}
          height={140}
          objectFit="contain"
          onAssetLoad={(success, error) => {
            probe.loaded = success;
            probe.error = error;
          }}
          onImageDecoded={(width, height) => {
            probe.width = width;
            probe.height = height;
          }}
        />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      const probe = probes[ctx.caseId] as { loaded?: boolean; error?: string; width: number; height: number };
      await waitUntil(ctx, 'res asset load', () => probe.loaded !== undefined);
      ctx.record(`loaded:${probe.loaded} error:${probe.error ?? ''} size:${probe.width}x${probe.height}`);
      assertEqual(probe.loaded, true, 'res asset load success');
      await waitUntil(ctx, 'res asset decode', () => probe.width > 0 && probe.height > 0);
    },
  },
  {
    id: 'platform-images-url-error',
    name: 'Images by URL: network failure',
    description:
      'Points an image at an unreachable local URL and expects the asset-load callback to report failure with a message.',
    element: 'image',
    render: (ctx: IntegrationTestRenderContext) => {
      const probe = probeFor<{ loaded?: boolean; error?: string }>(ctx.caseId, () => ({}));
      <view key={ctx.caseId} ref={ctx.rootRef} width={WIDTH} height={HEIGHT} backgroundColor={CARD} padding={18}>
        <image
          src="http://127.0.0.1:9/integration-test-missing.png"
          width={200}
          height={140}
          onAssetLoad={(success, error) => {
            probe.loaded = success;
            probe.error = error;
          }}
        />
      </view>;
    },
    interact: async (ctx: IntegrationTestInteractionContext) => {
      const probe = probes[ctx.caseId] as { loaded?: boolean; error?: string };
      await waitUntil(ctx, 'url asset load callback', () => probe.loaded !== undefined);
      ctx.record(`loaded:${probe.loaded} error:${probe.error ?? ''}`);
      assertEqual(probe.loaded, false, 'unreachable url load success');
      assert((probe.error ?? '').length > 0, 'unreachable url load reported no error message');
    },
  },
];
