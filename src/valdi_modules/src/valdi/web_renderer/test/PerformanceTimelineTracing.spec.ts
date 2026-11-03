import 'jasmine/src/jasmine';
import { PerformanceTimelineTracing } from '../src/tracing/PerformanceTimelineTracing';

describe('PerformanceTimelineTracing', () => {
  let markSpy: any;
  let measureSpy: any;
  let originalMark: typeof performance.mark | undefined;
  let originalMeasure: typeof performance.measure | undefined;

  beforeEach(() => {
    originalMark = performance.mark;
    originalMeasure = performance.measure;
    markSpy = jasmine.createSpy('mark');
    measureSpy = jasmine.createSpy('measure');
    Object.defineProperty(performance, 'mark', { configurable: true, value: markSpy });
    Object.defineProperty(performance, 'measure', { configurable: true, value: measureSpy });
  });

  afterEach(() => {
    restorePerformanceMethod('mark', originalMark);
    restorePerformanceMethod('measure', originalMeasure);
  });

  it('emits nested duration measures', () => {
    spyOn(performance, 'now').and.returnValues(10, 20, 30, 40);
    const tracing = new PerformanceTimelineTracing();

    tracing.beginTrace('outer');
    tracing.beginTrace('inner');
    tracing.endTrace();
    tracing.endTrace();

    expect(measureSpy.calls.allArgs()).toEqual([
      ['Valdi.inner', { start: 20, end: 30 }],
      ['Valdi.outer', { start: 10, end: 40 }],
    ]);
  });

  it('emits instant marks with arguments as detail', () => {
    const tracing = new PerformanceTimelineTracing();

    tracing.instantTrace('event', ['nodeId', 12, 'attributeName', 'opacity']);

    expect(markSpy).toHaveBeenCalledWith('Valdi.event', {
      detail: { nodeId: 12, attributeName: 'opacity' },
    });
  });

  it('retries an instant mark without detail when the browser cannot clone it', () => {
    markSpy.and.callFake((name: string, options?: PerformanceMarkOptions) => {
      if (options) {
        const error = new Error('Uncloneable detail');
        error.name = 'DataCloneError';
        throw error;
      }
      return { name } as PerformanceMark;
    });
    const tracing = new PerformanceTimelineTracing();

    tracing.instantTrace('event', ['callback', () => {}]);

    expect(markSpy.calls.allArgs()).toEqual([
      ['Valdi.event', { detail: { callback: jasmine.any(Function) } }],
      ['Valdi.event'],
    ]);
  });

  it('ignores unmatched trace ends', () => {
    new PerformanceTimelineTracing().endTrace();

    expect(measureSpy).not.toHaveBeenCalled();
  });
});

function restorePerformanceMethod(
  name: 'mark' | 'measure',
  original: typeof performance.mark | typeof performance.measure | undefined,
): void {
  if (original) {
    Object.defineProperty(performance, name, { configurable: true, value: original });
  } else {
    Reflect.deleteProperty(performance, name);
  }
}
