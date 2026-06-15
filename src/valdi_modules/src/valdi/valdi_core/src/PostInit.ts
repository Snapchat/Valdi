// Initialize the globals.
// This should be loaded right after the ModuleLoader is loaded

import { Console } from 'valdi_core/src/Console';
import { ValdiRuntime } from './ValdiRuntime';
import { ModuleLoader } from './ModuleLoader';
import { arePromiseUtterlyBroken, polyfillPromise } from './PromisePolyfill';
import {
  __tsn_async_generator_helper,
  __tsn_async_helper,
  __tsn_get_async_iterator,
  __tsn_get_iterator,
} from './TsnHelper';

declare const global: any;
declare const runtime: ValdiRuntime;

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
  // On web, browser console and timing functions are already correct —
  // overwriting them breaks dev tools and webpack-dev-server HMR.
  const isBrowser = typeof window !== 'undefined' && typeof window.document !== 'undefined';

  if (!isBrowser) {
    global.console = new Console(runtime.outputLog);
  }

  if (!isBrowser && !global.realTimingFunctions) {
    // We only configure the timing functions once to avoid messing up jasmine's internal checks
    // when it installs the mocked jasmine.Clock.
    global.realTimingFunctions = {
      setTimeout: global.setTimeout,
      clearTimeout: global.clearTimeout,
      setInterval: global.setInterval,
      clearInterval: global.clearInterval,
    };
    global.setTimeout = setTimeout;
    global.clearTimeout = clearTimeout;
    global.setInterval = setInterval;
    global.clearInterval = clearInterval;
  }

  if (arePromiseUtterlyBroken(runtime.getCurrentPlatform)) {
    polyfillPromise();
  }

  global.__tsn_async_helper = __tsn_async_helper;
  global.__tsn_get_iterator = __tsn_get_iterator;
  global.__tsn_get_async_iterator = __tsn_get_async_iterator;
  global.__tsn_async_generator_helper = __tsn_async_generator_helper;

  const moduleLoader = global.moduleLoader as ModuleLoader;
  moduleLoader.onModuleRegistered('coreutils/src/unicode/UnicodeNative', () => {
    const textCoding = moduleLoader.load('coreutils/src/unicode/TextCoding', true);
    global.TextDecoder = textCoding.TextDecoder;
    global.TextEncoder = textCoding.TextEncoder;
  });

  // Without this, parsing worker code that does something like:
  // onmessage = e => { /* blah */ };
  // fails with:
  // ReferenceError: 'onmessage' is not defined
  //
  // see: src/valdi/worker/test/workers/TestWorker.ts
  global.onmessage = () => {};
}
