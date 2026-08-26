import 'jasmine/src/jasmine';
import { ChromeDevToolsTracing, WebTracingClock } from '../src/tracing/ChromeDevToolsTracing';

class SequenceClock implements WebTracingClock {
  constructor(private readonly values: number[]) {}

  now(): number {
    const value = this.values.shift();
    if (value === undefined) {
      throw new Error('The deterministic tracing clock is exhausted.');
    }
    return value;
  }
}

describe('ChromeDevToolsTracing', () => {
  let timeStampSpy: any;
  let originalTimeStamp: typeof console.timeStamp | undefined;

  beforeEach(() => {
    originalTimeStamp = console.timeStamp;
    timeStampSpy = jasmine.createSpy('timeStamp');
    Object.defineProperty(console, 'timeStamp', { configurable: true, value: timeStampSpy });
  });

  afterEach(() => {
    if (originalTimeStamp) {
      Object.defineProperty(console, 'timeStamp', { configurable: true, value: originalTimeStamp });
    } else {
      Reflect.deleteProperty(console, 'timeStamp');
    }
  });

  it('emits nested duration traces on the Valdi custom track', () => {
    const tracing = new ChromeDevToolsTracing(new SequenceClock([10, 20, 30, 40]));

    tracing.beginTrace('outer');
    tracing.beginTrace('inner');
    tracing.endTrace();
    tracing.endTrace();

    expect(timeStampSpy.calls.allArgs()).toEqual([
      ['Valdi.inner', 20, 30, 'Valdi JS', 'Valdi', 'primary'],
      ['Valdi.outer', 10, 40, 'Valdi JS', 'Valdi', 'primary'],
    ]);
  });

  it('emits instant traces with arguments', () => {
    const tracing = new ChromeDevToolsTracing(new SequenceClock([25]));

    tracing.instantTrace('event', ['nodeId', 12, 'attributeName', 'opacity']);

    expect(timeStampSpy).toHaveBeenCalledWith('Valdi.event', 25, 25, 'Valdi JS', 'Valdi', 'primary', {
      nodeId: 12,
      attributeName: 'opacity',
    });
  });

  it('ignores unmatched trace ends', () => {
    new ChromeDevToolsTracing(new SequenceClock([])).endTrace();

    expect(timeStampSpy).not.toHaveBeenCalled();
  });
});
