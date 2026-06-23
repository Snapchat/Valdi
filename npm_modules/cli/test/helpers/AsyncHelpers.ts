import type { SubCommand } from './CommandHelpers';
import { onEachLine } from './StreamHelpers';

/**
 * Create a Promise that rejects after ms milliseconds from the last time it is "touched".
 *
 * Returns a promise, a function to reset the timeout, and a function to clear the timeout.
 *
 * The returned promise will never resolve.
 */
export function timeoutOnIdle(ms = 30_000): [timeout: Promise<never>, touch: () => void, clear: () => void] {
  let timer: NodeJS.Timeout;
  let resetTimer: () => void;
  let clearTimer: () => void;
  let complete = false;

  const timeout = new Promise<never>((_resolve, reject) => {
    clearTimer = () => {
      complete = true;
      clearTimeout(timer);
    };

    resetTimer = () => {
      clearTimeout(timer);
      if (complete) {
        return;
      }
      timer = setTimeout(() => {
        complete = true;
        reject(new Error('timeout'));
      }, ms);
    };

    resetTimer();
  });

  return [timeout, () => resetTimer(), () => clearTimer()];
}

/**
 * Given a task created with CommandHelpers.run, watches the STDOUT and/or STDERR
 * for the given strings or expressions.
 *
 * Will timeeout if there's no output after 30 seconds.
 *
 * Rejects if there are queries remaning after the
 */
export function lookForOutput(
  { stdout = [], stderr = [], idleTimeoutMs }: { stdout?: (string | RegExp)[]; stderr?: (string | RegExp)[]; idleTimeoutMs?: number },
  subcommand: SubCommand,
): Promise<true> {
  const [idleTimer, touch, complete] = timeoutOnIdle(idleTimeoutMs);

  const monitor = (requiredQueries: (string | RegExp)[]) => {
    let queries = requiredQueries.slice();
    let onDone: (value: true) => void, onError: (error: Error) => void;

    const promise = new Promise((resolve, reject) => {
      onDone = resolve;
      onError = reject;

      // If queries are empty to start with resolve immediately
      if (queries.length === 0) {
        resolve(true);
      }
    });

    const isDone = () => queries.length === 0;

    return {
      promise,
      isDone,
      onEachLine: onEachLine(line => {
        touch();
        queries = queries.filter((query): boolean => {
          if (typeof query === 'string') {
            return !line.toString().includes(query);
          }
          if (query instanceof RegExp) {
            return !query.test(line.toString('utf8'));
          }
          onError(new Error('invalid query'));
          return false;
        });

        if (isDone()) {
          onDone(true);
          return;
        }
      }),
    };
  };

  const stdoutMonitor = monitor(stdout);
  const stderrMonitor = monitor(stderr);

  subcommand.process.stdout.compose(stdoutMonitor.onEachLine).pipe(process.stdout);
  subcommand.process.stderr.compose(stderrMonitor.onEachLine).pipe(process.stderr);

  return Promise.race([
    idleTimer,
    subcommand
      .wait()
      .then((): true => {
        const allFound = [stderrMonitor, stderrMonitor].every(monitor => monitor.isDone());
        if (allFound) {
          return true;
        }
        throw new Error('Not all queries found: ' + [stdout, stderr].map(q => q.toString()).join('\n - '));
      })
      .finally(() => complete()),
  ]);
}
