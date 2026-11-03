// TS call native and record
// - TS enter time (1)
// - native enter time (2)
// - native leave time (3)
// - TS leave time (4)
//
// TS to native marshalling time = (2) - (1)
// Native to TS marshalling time = (4) - (3)

import { setTimeoutInterruptible } from 'valdi_core/src/SetTimeout';
import { Style } from 'valdi_core/src/Style';
import { systemBoldFont, systemFont } from 'valdi_core/src/SystemFont';
import { NavigationPage } from 'valdi_navigation/src/NavigationPage';
import { NavigationPageStatefulComponent } from 'valdi_navigation/src/NavigationPageComponent';
import { Label, ScrollView } from 'valdi_tsx/src/NativeTemplateElements';
import {
  FunctionTiming,
  measureStrings,
  clearNativeFunctionTiming,
  getNativeFunctionTiming,
  measureStringMaps,
} from './NativeHelper';
import { now } from './cpp';

interface Result {
  avgJsToNative: number;
  p10JsToNative: number;
  p50JsToNative: number;
  p90JsToNative: number;

  avgNativeToJs: number;
  p10NativeToJs: number;
  p50NativeToJs: number;
  p90NativeToJs: number;
}

// The tests take different data types, so the body accepts the data it was paired with in allTests.
function runTest(body: (data: never) => unknown, data: unknown, iterations: number): Result {
  clearNativeFunctionTiming();
  const jsTiming: FunctionTiming[] = [];
  for (let i = 0; i < iterations; ++i) {
    const jsEnter = now();
    body(data as never);
    const jsLeave = now();
    jsTiming.push({ enterTime: jsEnter, leaveTime: jsLeave });
  }
  const nativeTiming = getNativeFunctionTiming();

  const jsToNativeTimes: number[] = [];
  const nativeToJsTimes: number[] = [];
  for (let i = 0; i < iterations; ++i) {
    jsToNativeTimes.push(nativeTiming[i].enterTime - jsTiming[i].enterTime);
    nativeToJsTimes.push(jsTiming[i].leaveTime - nativeTiming[i].leaveTime);
  }
  jsToNativeTimes.sort((a, b) => a - b);
  nativeToJsTimes.sort((a, b) => a - b);
  return {
    avgJsToNative: average(jsToNativeTimes),
    p10JsToNative: pvalue(jsToNativeTimes, 10),
    p50JsToNative: pvalue(jsToNativeTimes, 50),
    p90JsToNative: pvalue(jsToNativeTimes, 90),

    avgNativeToJs: average(nativeToJsTimes),
    p10NativeToJs: pvalue(nativeToJsTimes, 10),
    p50NativeToJs: pvalue(nativeToJsTimes, 50),
    p90NativeToJs: pvalue(nativeToJsTimes, 90),
  };
}

function pvalue(times: number[], p: number): number {
  if (times.length === 0 || p < 0 || p > 100) {
    throw new Error('Invalid input');
  }
  const rank = (p / 100) * (times.length - 1);
  const lower = Math.floor(rank);
  const upper = Math.ceil(rank);
  if (lower === upper) {
    return times[lower];
  } else {
    // Linear interpolation between closest ranks
    const weight = rank - lower;
    return times[lower] * (1 - weight) + times[upper] * weight;
  }
}

function average(numbers: number[]): number {
  if (numbers.length === 0) {
    throw new Error('Cannot compute average of an empty array');
  }
  const sum = numbers.reduce((acc, value) => acc + value, 0);
  return sum / numbers.length;
}

const allTests = [
  {
    name: 'Small Strings',
    func: measureStrings,
    data: Array.from({ length: 1000 }, () => 'Hello'),
    iterations: 100,
  },
  {
    name: 'Large Strings',
    func: measureStrings,
    data: Array.from({ length: 1000 }, () => 'Hello'.repeat(1000)),
    iterations: 100,
  },
  {
    name: 'String Maps',
    func: measureStringMaps,
    data: Array.from({ length: 2500 }, () =>
      Object.fromEntries(Array.from({ length: 20 }, (_, i) => [`key${i}`, `$value${i}`])),
    ),
    iterations: 10,
  },
] as const;

export interface ViewModel {}
export interface ComponentContext {}

interface State {
  results: Result[];
}

@NavigationPage(module)
export class MarshallingTest extends NavigationPageStatefulComponent<ViewModel, ComponentContext> {
  state: State = {
    results: [],
  };

  onCreate(): void {
    setTimeoutInterruptible(() => {
      const results: Result[] = [];
      for (const t of allTests) {
        results.push(runTest(t.func, t.data, t.iterations));
        this.setState({ results: [...results] });
      }
    });
  }

  onRender(): void {
    <view backgroundColor="white">
      <scroll style={styles.scroll} padding={16}>
        <layout flexDirection="column">
          <label style={styles.title} value={`Marshalling Benchmark!`} font={systemFont(20)} />
          {this.renderResults()}
        </layout>
      </scroll>
    </view>;
  }

  private renderResults(): void {
    for (let i = 0; i < this.state.results.length; i++) {
      <label marginTop={15} value={allTests[i].name} font={systemBoldFont(12)} />;
      const r = this.state.results[i];
      const jsToNative = `JS=> Avg: ${r.avgJsToNative.toFixed(2)} P10: ${r.p10JsToNative.toFixed(
        2,
      )} P50 ${r.p50JsToNative.toFixed(2)} P90 ${r.p90JsToNative.toFixed(2)}`;
      <label value={jsToNative} font={systemFont(10)} />;
      const nativeToJs = `=>JS Avg: ${r.avgNativeToJs.toFixed(2)} P10: ${r.p10NativeToJs.toFixed(
        2,
      )} P50 ${r.p50NativeToJs.toFixed(2)} P90 ${r.p90NativeToJs.toFixed(2)}`;
      <label value={nativeToJs} font={systemFont(10)} />;
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
