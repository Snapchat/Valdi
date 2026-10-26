import 'jasmine/src/jasmine';
import { beginTrace, endTrace, instantTrace, instantTraceWithArgs, isTracingSupported, trace } from '../src/utils/Trace';

type TraceRuntime = {
  trace?: (tag: string, callback: () => unknown) => unknown;
  beginTrace?: (tag: string) => void;
  endTrace?: () => void;
  instantTrace?: (tag: string, args?: readonly unknown[]) => void;
};

function withRuntime<T>(runtime: TraceRuntime, block: () => T): T {
  const globals = globalThis as unknown as { runtime: unknown };
  const previous = globals.runtime;
  globals.runtime = runtime;
  try {
    return block();
  } finally {
    globals.runtime = previous;
  }
}

describe('Trace', () => {
  it('uses the scoped runtime trace when available', () => {
    const calls: string[] = [];
    const runtime: TraceRuntime = {
      trace: (tag, callback) => {
        calls.push(`trace:${tag}`);
        return callback();
      },
      beginTrace: tag => calls.push(`begin:${tag}`),
      endTrace: () => calls.push('end'),
    };

    const result = withRuntime(runtime, () => trace('work', () => 42));

    expect(result).toBe(42);
    expect(calls).toEqual(['trace:work']);
  });

  it('falls back to begin/end sections and closes them on throw', () => {
    const calls: string[] = [];
    const runtime: TraceRuntime = {
      beginTrace: tag => calls.push(`begin:${tag}`),
      endTrace: () => calls.push('end'),
    };

    withRuntime(runtime, () => {
      expect(trace('ok', () => 'done')).toBe('done');
      expect(() =>
        trace('fails', () => {
          throw new Error('boom');
        }),
      ).toThrowError('boom');
    });

    expect(calls).toEqual(['begin:ok', 'end', 'begin:fails', 'end']);
  });

  it('reports support for scoped-only and section-only runtimes', () => {
    expect(withRuntime({ trace: (_, callback) => callback() }, isTracingSupported)).toBeTrue();
    expect(withRuntime({ beginTrace: () => {}, endTrace: () => {} }, isTracingSupported)).toBeTrue();
    expect(withRuntime({ beginTrace: () => {} }, isTracingSupported)).toBeFalse();
    expect(withRuntime({}, isTracingSupported)).toBeFalse();
  });

  it('records instants natively, or as zero-length traces on scoped-only runtimes', () => {
    const instants: unknown[][] = [];
    withRuntime({ instantTrace: (tag, args) => instants.push([tag, args]) }, () => {
      instantTrace('a');
      instantTraceWithArgs('b', ['count', 1] as const);
    });
    expect(instants).toEqual([
      ['a', undefined],
      ['b', ['count', 1]],
    ]);

    const scoped: string[] = [];
    withRuntime(
      {
        trace: (tag, callback) => {
          scoped.push(tag);
          return callback();
        },
      },
      () => {
        instantTrace('c');
        instantTraceWithArgs('d', ['count', 2] as const);
      },
    );
    expect(scoped).toEqual(['c', 'd']);
  });

  it('ignores begin/end on runtimes without sections', () => {
    expect(() =>
      withRuntime({}, () => {
        beginTrace('x');
        endTrace();
        instantTrace('y');
      }),
    ).not.toThrow();
  });
});
