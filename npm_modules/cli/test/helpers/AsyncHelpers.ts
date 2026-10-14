import type { Readable } from 'node:stream';
import type { SubCommand } from './CommandHelpers';
import { onEachLine } from './StreamHelpers';

/**
 * Watch a long-running command's output for the first line matching `pattern` and
 * resolve with the RegExp match (so callers can pull out capture groups, e.g. a port).
 *
 * Unlike {@link lookForOutput}, this resolves as soon as the line appears and does NOT
 * wait for the process to exit — the intended use is a persistent process (the
 * hot-reload daemon) that the caller keeps alive and later kills.
 */
export function waitForLine(
  subcommand: SubCommand,
  pattern: RegExp,
  { stream = 'both', timeoutMs = 120_000 }: { stream?: 'stdout' | 'stderr' | 'both'; timeoutMs?: number } = {},
): Promise<RegExpMatchArray> {
  const streams: Readable[] =
    stream === 'stdout'
      ? [subcommand.process.stdout]
      : stream === 'stderr'
        ? [subcommand.process.stderr]
        : [subcommand.process.stdout, subcommand.process.stderr];

  return new Promise((resolve, reject) => {
    // The process may have already exited before we were called (e.g. between two
    // sequential waitForLine calls); the 'exit' event won't fire again, so check up
    // front and fail fast rather than blocking for the full timeout.
    if (subcommand.process.exitCode !== null || subcommand.process.signalCode !== null) {
      reject(
        new Error(
          `process already exited (code ${subcommand.process.exitCode ?? 'null'}, signal ${subcommand.process.signalCode ?? 'null'}) before matching ${pattern.toString()}`,
        ),
      );
      return;
    }

    const readers = streams.map(source => {
      // Buffer across chunk boundaries so a line split between two reads still matches.
      let buffer = '';
      const onChunk = (chunk: Buffer) => {
        buffer += chunk.toString('utf8');
        let newline = buffer.indexOf('\n');
        while (newline !== -1) {
          const line = buffer.slice(0, newline);
          buffer = buffer.slice(newline + 1);
          const match = line.match(pattern);
          if (match) {
            finish(() => resolve(match));
            return;
          }
          newline = buffer.indexOf('\n');
        }
      };
      source.on('data', onChunk);
      return { source, onChunk };
    });

    const timer = setTimeout(() => {
      finish(() => reject(new Error(`timed out waiting for ${pattern.toString()}`)));
    }, timeoutMs);

    // If the process dies before the line appears (e.g. the build fails), fail fast
    // instead of blocking for the full timeout.
    const onExit = (code: number | null) => {
      finish(() => reject(new Error(`process exited (code ${code ?? 'null'}) before matching ${pattern.toString()}`)));
    };
    subcommand.process.once('exit', onExit);

    function finish(settle: () => void) {
      clearTimeout(timer);
      subcommand.process.off('exit', onExit);
      for (const { source, onChunk } of readers) {
        source.off('data', onChunk);
      }
      settle();
    }
  });
}

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
