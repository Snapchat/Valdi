import 'jasmine/src/jasmine';

// Regression test for "Illegal invocation" in scheduleWorkItem.
// Native timing functions require `window` as their receiver; without .bind(window)
// they throw when called as timing.setTimeout(...).

function makeFakeWindow() {
  const pending = new Map<number, () => void>();
  let nextId = 0;
  const win = {
    setTimeout(this: any, cb: () => void, _: number): number {
      if (this !== win) throw new TypeError('Illegal invocation');
      const id = ++nextId;
      pending.set(id, cb);
      return id;
    },
    clearTimeout(this: any, id: number): void {
      if (this !== win) throw new TypeError('Illegal invocation');
      pending.delete(id);
    },
  };
  const flush = () => { const cbs = [...pending.values()]; pending.clear(); cbs.forEach(cb => cb()); };
  return { win, flush };
}

describe('__originalTimingFunctions__ binding', () => {
  it('unbound call throws Illegal invocation', () => {
    const { win } = makeFakeWindow();
    expect(() => win.setTimeout(() => {}, 0)).not.toThrow(); // with receiver: fine
    const unbound = win.setTimeout;
    expect(() => unbound(() => {}, 0)).toThrowError(/Illegal invocation/);
  });

  it('bound call works and fires callback', () => {
    const { win, flush } = makeFakeWindow();
    const boundSetTimeout = win.setTimeout.bind(win);
    let called = false;
    boundSetTimeout(() => { called = true; }, 0);
    flush();
    expect(called).toBe(true);
  });
});

describe('scheduleWorkItem', () => {
  // Mirrors the implementation in ValdiWebRuntime.ts
  class MinimalRuntime {
    private _counter = 1;
    private _tasks = new Map<number, number>();
    scheduleWorkItem(cb: () => void, delayMs: number): number {
      const taskId = this._counter++;
      const timing = (globalThis as any).__originalTimingFunctions__;
      const timeoutId = timing.setTimeout(() => { this._tasks.delete(taskId); cb(); }, delayMs || 0);
      this._tasks.set(taskId, timeoutId);
      return taskId;
    }
    unscheduleWorkItem(taskId: number): void {
      const id = this._tasks.get(taskId);
      if (id !== undefined) {
        (globalThis as any).__originalTimingFunctions__.clearTimeout(id);
        this._tasks.delete(taskId);
      }
    }
  }

  let saved: any;
  let flush: () => void;

  beforeEach(() => {
    saved = (globalThis as any).__originalTimingFunctions__;
    const pending = new Map<number, () => void>();
    let id = 0;
    (globalThis as any).__originalTimingFunctions__ = {
      setTimeout: (cb: () => void) => { const i = ++id; pending.set(i, cb); return i; },
      clearTimeout: (i: number) => pending.delete(i),
    };
    flush = () => { const cbs = [...pending.values()]; pending.clear(); cbs.forEach(cb => cb()); };
  });

  afterEach(() => { (globalThis as any).__originalTimingFunctions__ = saved; });

  it('fires callback on flush', () => {
    const rt = new MinimalRuntime();
    let fired = false;
    rt.scheduleWorkItem(() => { fired = true; }, 0);
    flush();
    expect(fired).toBe(true);
  });

  it('does not fire after unschedule', () => {
    const rt = new MinimalRuntime();
    let fired = false;
    const id = rt.scheduleWorkItem(() => { fired = true; }, 0);
    rt.unscheduleWorkItem(id);
    flush();
    expect(fired).toBe(false);
  });
});
