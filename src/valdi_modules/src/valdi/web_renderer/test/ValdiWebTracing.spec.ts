import 'jasmine/src/jasmine';
import {
  ValdiWebTracing,
  beginValdiWebTrace,
  endValdiWebTrace,
  instantValdiWebTrace,
  isValdiWebTracingEnabled,
  makeValdiWebTraceProxy,
  setValdiWebTracing,
  valdiWebTraceArguments,
} from '../src/tracing/ValdiWebTracing';
import { configureValdiWebTracingFromLocation } from '../src/tracing/WebTracingConfiguration';

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
      const handle = beginValdiWebTrace('disabled');
      instantValdiWebTrace('disabled', undefined);
      endValdiWebTrace(handle);
    }).not.toThrow();
  });

  it('enables Chromium tracing only through both explicit development query flags', () => {
    setValdiWebTracing(undefined);

    expect(configureValdiWebTracingFromLocation('?valdiDevTools=1&valdiTrace=chrome')).toBeTrue();
    expect(isValdiWebTracingEnabled()).toBeTrue();
  });

  it('leaves normal, malformed, and ambiguous web pages uninstrumented', () => {
    const rejectedQueries = [
      undefined,
      '?fixture=Text_0',
      '?valdiTrace=unsupported',
      '?valdiTrace=chrome',
      '?valdiDevTools=1',
      '?valdiDevTools&valdiTrace=chrome',
      '?valdiDevTools=1&valdiTrace',
      '?valdiDevTools=1&valdiTrace=chrome&valdiTrace=chrome',
      '?valdiDevTools=1&valdiDevTools=1&valdiTrace=chrome',
      '?valdi%44evTools=1&valdiTrace=chrome',
      '?valdiDevTools=1&valdiTrace=chrome&valdi%54race=disabled',
      '?valdiDevTools=1&valdi%54race=disabled&valdiTrace=chrome',
      '?valdiDevTools=1&valdiTrace=chr%6Fme',
      '?valdiDevTools=1&valdiTrace=chrome%',
    ];

    for (const query of rejectedQueries) {
      setValdiWebTracing(undefined);
      expect(configureValdiWebTracingFromLocation(query)).toBeFalse();
      expect(isValdiWebTracingEnabled()).toBeFalse();
    }
  });

  it('does not replace an application-provided tracing implementation', () => {
    const tracing = new RecordingTracing();
    setValdiWebTracing(tracing);

    expect(configureValdiWebTracingFromLocation('?valdiDevTools=1&valdiTrace=chrome')).toBeFalse();
    const handle = beginValdiWebTrace('existing');
    endValdiWebTrace(handle);

    expect(tracing.events).toEqual(['begin:existing', 'end']);
  });

  it('forwards duration and instant traces to the configured implementation', () => {
    const tracing = new RecordingTracing();
    setValdiWebTracing(tracing);

    const handle = beginValdiWebTrace('work');
    instantValdiWebTrace('event', ['key', 42]);
    endValdiWebTrace(handle);

    expect(tracing.events).toEqual(['begin:work', 'instant:event:["key",42]', 'end']);
  });

  it('stops forwarding traces when disabled', () => {
    const tracing = new RecordingTracing();

    setValdiWebTracing(tracing);
    const enabledHandle = beginValdiWebTrace('enabled');
    endValdiWebTrace(enabledHandle);
    setValdiWebTracing(undefined);
    const disabledHandle = beginValdiWebTrace('disabled');
    endValdiWebTrace(disabledHandle);

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
    let failedHandle: ReturnType<typeof beginValdiWebTrace> = undefined;
    expect(() => {
      failedHandle = beginValdiWebTrace('begin');
    }).not.toThrow();
    expect(failedHandle).toBeUndefined();
    expect(() => instantValdiWebTrace('instant', undefined)).not.toThrow();

    tracing.beginTrace = () => {};
    const successfulHandle = beginValdiWebTrace('end');
    expect(() => endValdiWebTrace(successfulHandle)).not.toThrow();

    expect(errorSpy).toHaveBeenCalledTimes(3);
  });

  it('activates runtime trace proxies created before tracing was configured', () => {
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

  it('does not close an outer trace when a nested begin fails', () => {
    const events: string[] = [];
    const activeTags: string[] = [];
    const errorSpy = spyOn(console, 'error');
    const tracing: ValdiWebTracing = {
      beginTrace: tag => {
        events.push(`begin:${tag}`);
        if (tag === 'inner') {
          throw new Error('inner begin failed');
        }
        activeTags.push(tag);
      },
      endTrace: () => {
        events.push(`end:${activeTags.pop()}`);
      },
      instantTrace: () => {},
    };
    const inner = makeValdiWebTraceProxy('inner', () => {
      events.push('callback:inner');
      return 3;
    });
    const outer = makeValdiWebTraceProxy('outer', () => inner() + 4);
    setValdiWebTracing(tracing);

    expect(outer()).toBe(7);
    expect(events).toEqual(['begin:outer', 'begin:inner', 'callback:inner', 'end:outer']);
    expect(activeTags).toEqual([]);
    expect(errorSpy).toHaveBeenCalledTimes(1);
  });

  it('ends the original tracer when a callback replaces the active tracer', () => {
    const originalTracing = new RecordingTracing();
    const replacementTracing = new RecordingTracing();
    const wrapped = makeValdiWebTraceProxy('swap', () => {
      setValdiWebTracing(replacementTracing);
      return 'result';
    });
    setValdiWebTracing(originalTracing);

    expect(wrapped()).toBe('result');
    expect(originalTracing.events).toEqual(['begin:swap', 'end']);
    expect(replacementTracing.events).toEqual([]);
  });

  it('preserves hostile argument keys as own serializable data with last-write semantics', () => {
    const argumentsObject = valdiWebTraceArguments([
      '__proto__',
      { valdiTracePolluted: true },
      'constructor',
      'first',
      '__proto__',
      'safe',
      'constructor',
      'last',
    ]);
    if (!argumentsObject) {
      throw new Error('Expected trace arguments.');
    }

    expect(Object.getPrototypeOf(argumentsObject)).toBe(Object.prototype);
    expect(Object.keys(argumentsObject)).toEqual(['__proto__', 'constructor']);
    expect(Object.prototype.hasOwnProperty.call(argumentsObject, '__proto__')).toBeTrue();
    expect(Object.prototype.hasOwnProperty.call(argumentsObject, 'constructor')).toBeTrue();
    expect(argumentsObject['__proto__']).toBe('safe');
    expect(argumentsObject['constructor']).toBe('last');
    expect(Object.prototype.hasOwnProperty.call(Object.prototype, 'valdiTracePolluted')).toBeFalse();

    const serialized = JSON.parse(JSON.stringify(argumentsObject)) as Record<string, unknown>;
    expect(Object.keys(serialized)).toEqual(['__proto__', 'constructor']);
    expect(serialized['__proto__']).toBe('safe');
    expect(serialized['constructor']).toBe('last');
  });
});
