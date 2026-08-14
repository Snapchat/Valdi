import 'jasmine/src/jasmine';
import {
  ValdiWebTracing,
  beginValdiWebTrace,
  endValdiWebTrace,
  instantValdiWebTrace,
  isValdiWebTracingEnabled,
  makeValdiWebTraceProxy,
  setValdiWebTracing,
} from '../src/tracing/ValdiWebTracing';

class RecordingTracing implements ValdiWebTracing {
  readonly events: string[] = [];

  beginTrace(tag: string): void {
    this.events.push(`begin:${tag}`);
  }

  endTrace(): void {
    this.events.push('end');
  }

  instantTrace(tag: string, args: readonly unknown[] | undefined): void {
    this.events.push(`instant:${tag}:${JSON.stringify(args)}`);
  }
}

describe('ValdiWebTracing', () => {
  afterEach(() => {
    setValdiWebTracing(undefined);
  });

  it('is disabled by default', () => {
    setValdiWebTracing(undefined);

    expect(isValdiWebTracingEnabled()).toBeFalse();
    expect(() => {
      beginValdiWebTrace('disabled');
      instantValdiWebTrace('disabled', undefined);
      endValdiWebTrace();
    }).not.toThrow();
  });

  it('forwards duration and instant traces to the configured implementation', () => {
    const tracing = new RecordingTracing();
    setValdiWebTracing(tracing);

    beginValdiWebTrace('work');
    instantValdiWebTrace('event', ['key', 42]);
    endValdiWebTrace();

    expect(tracing.events).toEqual(['begin:work', 'instant:event:["key",42]', 'end']);
  });

  it('stops forwarding traces when disabled', () => {
    const tracing = new RecordingTracing();

    setValdiWebTracing(tracing);
    beginValdiWebTrace('enabled');
    endValdiWebTrace();
    setValdiWebTracing(undefined);
    beginValdiWebTrace('disabled');
    endValdiWebTrace();

    expect(tracing.events).toEqual(['begin:enabled', 'end']);
  });

  it('logs tracing implementation errors without changing control flow', () => {
    const errorSpy = spyOn(console, 'error');
    const tracing: ValdiWebTracing = {
      beginTrace: () => {
        throw new Error('begin failure');
      },
      endTrace: () => {
        throw new Error('end failure');
      },
      instantTrace: () => {
        throw new Error('instant failure');
      },
    };

    setValdiWebTracing(tracing);
    expect(() => beginValdiWebTrace('begin')).not.toThrow();
    expect(() => instantValdiWebTrace('instant', undefined)).not.toThrow();

    tracing.beginTrace = () => {};
    beginValdiWebTrace('end');
    expect(() => endValdiWebTrace()).not.toThrow();

    expect(errorSpy).toHaveBeenCalledTimes(3);
  });

  it('activates trace proxies that were created before tracing was configured', () => {
    setValdiWebTracing(undefined);
    const wrapped = makeValdiWebTraceProxy('Proxy.call', function (this: { base: number }, value: number) {
      return this.base + value;
    });
    const tracing = new RecordingTracing();
    setValdiWebTracing(tracing);

    expect(wrapped.call({ base: 4 }, 3)).toBe(7);
    expect(tracing.events).toEqual(['begin:Proxy.call', 'end']);
  });

  it('ends a proxied trace when the wrapped function throws', () => {
    const wrapped = makeValdiWebTraceProxy('Proxy.throw', () => {
      throw new Error('callback failure');
    });
    const tracing = new RecordingTracing();
    setValdiWebTracing(tracing);

    expect(() => wrapped()).toThrowError('callback failure');
    expect(tracing.events).toEqual(['begin:Proxy.throw', 'end']);
  });
});
