// Initialize the globals.
// This should be loaded right after the ModuleLoader is loaded

import { Console } from 'valdi_core/src/Console';
import { getValdiRuntime } from './ValdiRuntimeProvider';
import { ModuleLoader } from './ModuleLoader';
import { arePromiseUtterlyBroken, polyfillPromise } from './PromisePolyfill';
import {
  __tsn_async_generator_helper,
  __tsn_async_helper,
  __tsn_get_async_iterator,
  __tsn_get_iterator,
} from './TsnHelper';

const runtime = getValdiRuntime();

const valdiGlobalThis = globalThis as any;

function setTimeout(handler: (...args: any[]) => void, timeout?: number, ...args: any[]): number {
  return runtime.scheduleWorkItem(args.length ? handler.bind(undefined, args) : handler, timeout || 0);
}

function clearTimeout(handler: number): void {
  runtime.unscheduleWorkItem(handler);
}

class Timer {
  private handler: (() => void) | undefined;
  private currentTaskId: number | undefined;

  constructor(handler: () => void, readonly timeout: number) {
    this.handler = handler;
  }

  invalidate(): void {
    this.handler = undefined;
    if (this.currentTaskId) {
      runtime.unscheduleWorkItem(this.currentTaskId);
      this.currentTaskId = undefined;
    }
  }

  schedule(): boolean {
    if (!this.handler || this.currentTaskId) {
      return false;
    }

    this.currentTaskId = runtime.scheduleWorkItem(() => {
      this.tick();
    }, this.timeout);

    return true;
  }

  private tick() {
    if (this.handler) {
      try {
        this.handler();
      } catch (err: any) {
        runtime.onUncaughtError('TimerCallback', err);
      }
    }

    this.currentTaskId = undefined;

    this.schedule();
  }
}

function setInterval(handler: (...args: any[]) => void, timeout?: number, ...args: any[]): number {
  const timer = new Timer(args.length ? handler.bind(undefined, args) : handler, timeout || 0);
  timer.schedule();
  return timer as any;
}

function clearInterval(handler: number): void {
  const timer: Timer = handler as any;
  if (!(timer instanceof Timer)) {
    return;
  }

  timer.invalidate();
}

Long.prototype.valueOf = function (this: Long) {
  if (this.greaterThan(Number.MAX_SAFE_INTEGER) || this.lessThan(Number.MIN_SAFE_INTEGER)) {
    throw new Error(`Long value ${this.toString()} is too large to be represented as a primitive number`);
  }

  return this.toNumber();
};

export function postInit(): void {
  // Browser windows and web workers already provide the correct console and
  // timing functions. Replacing them breaks dev tools, HMR, and worker timers.
  const isWeb = typeof globalThis.location !== 'undefined';

  if (!isWeb) {
    valdiGlobalThis.console = new Console(runtime.outputLog);
  }

  if (!isWeb && !valdiGlobalThis.realTimingFunctions) {
    // We only configure the timing functions once to avoid messing up jasmine's internal checks
    // when it installs the mocked jasmine.Clock.
    valdiGlobalThis.realTimingFunctions = {
      setTimeout: valdiGlobalThis.setTimeout,
      clearTimeout: valdiGlobalThis.clearTimeout,
      setInterval: valdiGlobalThis.setInterval,
      clearInterval: valdiGlobalThis.clearInterval,
    };
    valdiGlobalThis.setTimeout = setTimeout;
    valdiGlobalThis.clearTimeout = clearTimeout;
    valdiGlobalThis.setInterval = setInterval;
    valdiGlobalThis.clearInterval = clearInterval;
  }

  if (arePromiseUtterlyBroken(runtime.getCurrentPlatform)) {
    polyfillPromise();
  }

  valdiGlobalThis.__tsn_async_helper = __tsn_async_helper;
  valdiGlobalThis.__tsn_get_iterator = __tsn_get_iterator;
  valdiGlobalThis.__tsn_get_async_iterator = __tsn_get_async_iterator;
  valdiGlobalThis.__tsn_async_generator_helper = __tsn_async_generator_helper;

  const moduleLoader = valdiGlobalThis.moduleLoader as ModuleLoader;
  moduleLoader.onModuleRegistered('coreutils/src/unicode/UnicodeNative', () => {
    // Web runtimes ship native TextEncoder/TextDecoder. Overwriting them with
    // the Valdi wrapper recurses: the wrapper's encode() calls encodeUtf8 in
    // web/UnicodeNative.ts, which does `new TextEncoder()` and hits the wrapper
    // again. Leave the native impls alone when they exist (browsers, Node) and
    // only install the polyfill on runtimes without them (Hermes without Intl).
    if (typeof valdiGlobalThis.TextEncoder !== 'undefined' && typeof valdiGlobalThis.TextDecoder !== 'undefined') {
      return;
    }
    const textCoding = moduleLoader.load('coreutils/src/unicode/TextCoding', true);
    valdiGlobalThis.TextDecoder = textCoding.TextDecoder;
    valdiGlobalThis.TextEncoder = textCoding.TextEncoder;
  });

  // Provide the standard `WeakRef` global on engines without native support (e.g.
  // QuickJS), backed by the runtime's engine-independent weak-reference machinery.
  if (typeof valdiGlobalThis.WeakRef === 'undefined') {
    class ValdiWeakRef {
      private _handle: unknown;
      constructor(target: object) {
        if (target === null || (typeof target !== 'object' && typeof target !== 'function')) {
          throw new TypeError('WeakRef: target must be an object');
        }
        this._handle = runtime.newWeakRef(target);
      }
      deref(): object | undefined {
        return runtime.derefWeakRef(this._handle);
      }
    }
    valdiGlobalThis.WeakRef = ValdiWeakRef;
  }

  // Without this, parsing worker code that does something like:
  // onmessage = e => { /* blah */ };
  // fails with:
  // ReferenceError: 'onmessage' is not defined
  //
  // see: src/valdi/worker/test/workers/TestWorker.ts
  valdiGlobalThis.onmessage = () => {};
}
