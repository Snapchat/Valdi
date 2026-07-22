import 'jasmine/src/jasmine';
import { ModuleLoader } from 'valdi_core/src/ModuleLoader';

declare const runtime: any;

/**
 * Covers the cooperative-yielding path added to preloadBatch. The default (no chunkSize /
 * chunkSize <= 0) must stay a single synchronous pass; a positive chunkSize must evaluate at
 * most chunkSize modules per JS task and reschedule the remainder via runtime.scheduleWorkItem,
 * evaluating every module exactly once (deduped) across the chunks.
 */
describe('ModuleLoader.preloadBatch', () => {
  let evalOrder: string[];
  let loader: ModuleLoader;
  let scheduled: Array<() => void>;
  let originalScheduleWorkItem: any;

  function register(paths: string[]) {
    for (const path of paths) {
      loader.registerModule(path, () => {
        evalOrder.push(path);
        return {};
      });
    }
  }

  // Run every continuation that preloadBatch handed to the scheduler, in order, until drained.
  // Draining can enqueue further continuations (one per remaining chunk), so loop until empty.
  function drainScheduler() {
    while (scheduled.length > 0) {
      const next = scheduled.shift()!;
      next();
    }
  }

  beforeEach(() => {
    evalOrder = [];
    scheduled = [];
    loader = new ModuleLoader(
      () => undefined,
      undefined,
      undefined,
      false,
    );
    originalScheduleWorkItem = runtime.scheduleWorkItem;
    runtime.scheduleWorkItem = (cb: () => void) => {
      scheduled.push(cb);
      return scheduled.length;
    };
  });

  afterEach(() => {
    runtime.scheduleWorkItem = originalScheduleWorkItem;
  });

  it('evaluates the whole batch synchronously when chunkSize is omitted', () => {
    const paths = ['a/src/M0', 'a/src/M1', 'a/src/M2'];
    register(paths);

    loader.preloadBatch(paths, 0);

    expect(evalOrder).toEqual(paths);
    expect(scheduled.length).toEqual(0);
  });

  it('evaluates synchronously when chunkSize is 0', () => {
    const paths = ['a/src/M0', 'a/src/M1', 'a/src/M2'];
    register(paths);

    loader.preloadBatch(paths, 0, 0);

    expect(evalOrder).toEqual(paths);
    expect(scheduled.length).toEqual(0);
  });

  it('does not reschedule when chunkSize covers the whole batch', () => {
    const paths = ['a/src/M0', 'a/src/M1'];
    register(paths);

    loader.preloadBatch(paths, 0, 5);

    expect(evalOrder).toEqual(paths);
    expect(scheduled.length).toEqual(0);
  });

  it('evaluates at most chunkSize modules per task and yields between chunks', () => {
    const paths = ['a/src/M0', 'a/src/M1', 'a/src/M2', 'a/src/M3', 'a/src/M4'];
    register(paths);

    loader.preloadBatch(paths, 0, 2);

    // First chunk only, then a single pending continuation.
    expect(evalOrder).toEqual(['a/src/M0', 'a/src/M1']);
    expect(scheduled.length).toEqual(1);

    // Second chunk runs on the next scheduled task.
    scheduled.shift()!();
    expect(evalOrder).toEqual(['a/src/M0', 'a/src/M1', 'a/src/M2', 'a/src/M3']);
    expect(scheduled.length).toEqual(1);

    // Final partial chunk, no further reschedule.
    scheduled.shift()!();
    expect(evalOrder).toEqual(paths);
    expect(scheduled.length).toEqual(0);
  });

  it('evaluates every module exactly once and dedupes across chunks', () => {
    const paths = ['a/src/M0', 'a/src/M1', 'a/src/M0', 'a/src/M2'];
    register(['a/src/M0', 'a/src/M1', 'a/src/M2']);

    loader.preloadBatch(paths, 0, 1);
    drainScheduler();

    // The duplicate 'a/src/M0' is visited once; each distinct module evaluated exactly once.
    expect(evalOrder).toEqual(['a/src/M0', 'a/src/M1', 'a/src/M2']);
  });
});
