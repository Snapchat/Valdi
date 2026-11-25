import { makeEntryPointRenderFunction } from 'valdi_core/src/EntryPointComponent';
import { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import { jsx } from 'valdi_core/src/JSXBootstrap';
import { RootComponentsManager } from 'valdi_core/src/RootComponentsManager';
import {
  UncaughtErrorHandlerResult,
  getCatchAllUncaughtErrorHandler,
  setCatchAllUncaughtErrorHandler,
} from 'valdi_core/src/UncaughtErrorHandler';
import { getValdiRuntime } from 'valdi_core/src/ValdiRuntimeProvider';
import { KeepAlive, beginKeepAlive, endKeepAlive } from 'valdi_core/src/utils/KeepAliveCallback';
import { Disposable, TerminalApp, TerminalInfo, activateTerminalApp, deactivateTerminalApp } from './App';
import {
  TerminalBackend,
  TerminalFrame,
  TerminalRootHandle,
  TerminalRootNative,
  TerminalSinkKind,
  getTerminalBackend,
} from './TerminalBackend';
import { makeTerminalRootContainer } from './TerminalRootContainer';
import { ColorLevel } from './ansi';
import { ESCAPE_TIMEOUT_MS, InputParser, KeyPress } from './keys';

export interface RenderOptions {
  /**
   * Where frames go. `tty` draws into the process terminal; `memory` records output instead,
   * for tests and headless hosts. @default 'tty'
   */
  readonly sink?: TerminalSinkKind;
  /** Exit when Ctrl+C is received as input. @default true */
  readonly exitOnCtrlC?: boolean;
  /** Route console output above the live frame instead of corrupting it. @default true */
  readonly patchConsole?: boolean;
  /** Append every frame instead of redrawing in place. */
  readonly debug?: boolean;
  /**
   * Size the root to the full terminal height instead of its content height, for
   * full-screen applications that fill the viewport with flexGrow.
   */
  readonly fitTerminal?: boolean;
  /** Forces the layout width instead of using the terminal's column count. */
  readonly columns?: number;
  /** Forces the layout height used with `fitTerminal` instead of the terminal's row count. */
  readonly rows?: number;
  /** Wrap the root component in Valdi's error boundary. */
  readonly errorBoundary?: boolean;
  /**
   * Memory sink only: whether the output behaves like an interactive terminal. When false it is
   * written once at unmount, like Ink in CI. @default true
   */
  readonly isTTY?: boolean;
  /** Memory sink only: how many colors the recorded output uses. @default ColorLevel.None */
  readonly colorLevel?: ColorLevel;
}

export interface RenderInstance<ViewModel> {
  readonly app: TerminalApp;
  /** Renders the root component again with a new view model. */
  rerender(viewModel: ViewModel): void;
  /**
   * Stops rendering, restores the terminal and releases the component tree. Every release step runs
   * even if an earlier one fails; the first failure rejects `waitUntilExit()` and is thrown after
   * cleanup, unless an `error` was passed, which takes precedence.
   */
  unmount(error?: Error): void;
  /** Resolves after `unmount()`; rejects when the application exited with an error. */
  waitUntilExit(): Promise<void>;
  /** Erases the live frame. */
  clear(): void;
  /** Lines of the most recent frame, without ANSI styling. */
  lastFrame(): ReadonlyArray<string> | undefined;
  /**
   * Resolves with the next frame that is produced, or with the last frame if the instance unmounts
   * first. Rejects when called after `unmount()`.
   */
  nextFrame(): Promise<ReadonlyArray<string>>;
  /**
   * Delivers text as if it had been typed. Reaches only this instance, which makes it the way tests
   * drive input into a memory sink.
   */
  feedInput(text: string): void;
  /** Resizes the terminal and lays the frame out again. */
  setSize(columns: number, rows: number): void;
  /** Everything written to a memory sink so far; empty for the `tty` sink. */
  output(): string;
}

declare const process:
  | {
      env?: { [name: string]: string | undefined };
    }
  | undefined;

const DEFAULT_COLUMNS = 80;
const DEFAULT_ROWS = 24;

const STDIN_FD = 0;
const STDOUT_FD = 1;

const CONSOLE_METHODS = ['log', 'info', 'warn', 'error', 'debug'] as const;

function formatConsoleArgs(args: unknown[]): string {
  return args
    .map(arg => {
      if (typeof arg === 'string') {
        return arg;
      }
      if (arg instanceof Error) {
        return arg.stack ?? arg.message;
      }
      try {
        return JSON.stringify(arg);
      } catch {
        return String(arg);
      }
    })
    .join(' ');
}

type ConsoleMethod = (...args: unknown[]) => void;

interface ConsolePatch {
  readonly originals: { [method: string]: ConsoleMethod };
  readonly wrappers: { [method: string]: ConsoleMethod };
}

// Every live renderer that patched the console, newest last. Console output goes to the newest one, so
// renderers can unmount in any order without one of them leaving the console pointed at a dead root.
const consoleRoutes: ((text: string) => void)[] = [];
let consolePatch: ConsolePatch | undefined;

function consoleTarget(): { [method: string]: ConsoleMethod } {
  return console as unknown as { [method: string]: ConsoleMethod };
}

/** Sends console output to `write` until the returned function is called. */
function routeConsole(write: (text: string) => void): () => void {
  if (consolePatch === undefined) {
    const target = consoleTarget();
    const originals: { [method: string]: ConsoleMethod } = {};
    const wrappers: { [method: string]: ConsoleMethod } = {};
    for (const method of CONSOLE_METHODS) {
      const original = target[method];
      if (typeof original !== 'function') {
        continue;
      }
      originals[method] = original;
      wrappers[method] = (...args: unknown[]) => {
        const route = consoleRoutes[consoleRoutes.length - 1];
        if (route) {
          route(formatConsoleArgs(args));
        } else {
          original.apply(console, args);
        }
      };
      target[method] = wrappers[method];
    }
    consolePatch = { originals, wrappers };
  }
  consoleRoutes.push(write);
  return () => {
    const index = consoleRoutes.lastIndexOf(write);
    if (index >= 0) {
      consoleRoutes.splice(index, 1);
    }
    if (consoleRoutes.length > 0 || consolePatch === undefined) {
      return;
    }
    const { originals, wrappers } = consolePatch;
    consolePatch = undefined;
    const target = consoleTarget();
    for (const method of Object.keys(originals)) {
      // Whoever patched the console after us keeps their patch; ours forwards to the original from now on.
      if (target[method] === wrappers[method]) {
        target[method] = originals[method];
      }
    }
  };
}

function envNumber(name: string): number | undefined {
  const raw = typeof process !== 'undefined' ? process?.env?.[name] : undefined;
  if (raw === undefined) {
    return undefined;
  }
  const value = parseInt(raw, 10);
  return isNaN(value) || value <= 0 ? undefined : value;
}

/** Keyboard input needs a terminal on both ends: raw mode changes the terminal that stdin is attached to. */
export function wantsRawMode(terminal: Pick<TerminalInfo, 'isInputTTY' | 'isOutputTTY'>): boolean {
  return terminal.isInputTTY && terminal.isOutputTTY;
}

function toError(value: unknown): Error {
  return value instanceof Error ? value : new Error(String(value));
}

class TerminalRenderer<ViewModel extends object, Context extends object> implements RenderInstance<ViewModel> {
  readonly app: TerminalApp;
  private readonly backend: TerminalBackend;
  private root: TerminalRootHandle | undefined;
  private contextId: string | undefined;
  private rootManager: RootComponentsManager | undefined;
  private readonly keepAlive: KeepAlive;
  private readonly terminal: TerminalInfo;
  private readonly disposables: Disposable[] = [];
  private readonly exitPromise: Promise<void>;
  private resolveExit!: () => void;
  private rejectExit!: (error: Error) => void;
  private readonly frameWaiters: ((frame: ReadonlyArray<string>) => void)[] = [];
  private restoreConsole: (() => void) | undefined;
  private lastPlainFrame: ReadonlyArray<string> | undefined;
  private lastSequence = 0;
  private unmounted = false;

  constructor(
    componentClass: ComponentConstructor<IComponent<ViewModel, Context>>,
    viewModel: ViewModel,
    componentContext: Context,
    private readonly options: RenderOptions,
  ) {
    this.backend = getTerminalBackend();
    const sink = options.sink ?? 'tty';
    this.terminal = this.makeTerminalInfo(sink);

    this.exitPromise = new Promise<void>((resolve, reject) => {
      this.resolveExit = resolve;
      this.rejectExit = reject;
    });
    // Unhandled rejections are not fatal here; callers opt in through waitUntilExit().
    this.exitPromise.catch(() => {});
    this.app = new TerminalApp(this.terminal, error => this.exitFromApp(error));
    activateTerminalApp(this.app);
    this.keepAlive = beginKeepAlive();

    // A constructor that throws hands the caller no instance to unmount, so it has to undo everything
    // itself; otherwise the keep-alive timer holds the process open and console stays patched.
    try {
      this.mount(sink, componentClass, viewModel, componentContext);
    } catch (error) {
      this.unmounted = true;
      this.release(/* finishOutput */ false);
      throw error;
    }
  }

  rerender(viewModel: ViewModel): void {
    if (this.unmounted || this.rootManager === undefined || this.contextId === undefined) {
      return;
    }
    this.rootManager.render(this.contextId, viewModel);
  }

  clear(): void {
    if (this.unmounted) {
      return;
    }
    this.backend.clearRoot(this.nativeRoot());
  }

  lastFrame(): ReadonlyArray<string> | undefined {
    return this.lastPlainFrame;
  }

  nextFrame(): Promise<ReadonlyArray<string>> {
    if (this.unmounted) {
      return Promise.reject(new Error('nextFrame() was called after unmount(): no more frames will be produced.'));
    }
    return new Promise(resolve => {
      this.frameWaiters.push(resolve);
    });
  }

  feedInput(text: string): void {
    this.backend.feedInput(this.nativeRoot(), text);
  }

  setSize(columns: number, rows: number): void {
    this.backend.setSize(this.nativeRoot(), columns, rows);
  }

  output(): string {
    return this.backend.getOutput(this.nativeRoot());
  }

  waitUntilExit(): Promise<void> {
    return this.exitPromise;
  }

  unmount(error?: Error): void {
    if (this.unmounted) {
      return;
    }
    this.unmounted = true;
    // Every release step runs and the exit promise always settles, even if one step throws.
    const cleanupError = this.release(/* finishOutput */ true);
    const waiters = this.frameWaiters.splice(0);
    for (const waiter of waiters) {
      waiter(this.lastPlainFrame ?? []);
    }
    const failure = error ?? cleanupError;
    if (failure) {
      this.rejectExit(failure);
    } else {
      this.resolveExit();
    }
    if (!error && cleanupError) {
      throw cleanupError;
    }
  }

  // Exits requested from inside the application (`exit()`, Ctrl+C) have nowhere to throw to; the
  // failure reaches the caller through waitUntilExit().
  private exitFromApp(error?: Error): void {
    try {
      this.unmount(error);
    } catch {
      // Already reported through waitUntilExit().
    }
  }

  private nativeRoot(): TerminalRootNative {
    if (this.root === undefined) {
      throw new Error('The terminal root is not available.');
    }
    return this.root.native;
  }

  private mount(
    sink: TerminalSinkKind,
    componentClass: ComponentConstructor<IComponent<ViewModel, Context>>,
    viewModel: ViewModel,
    componentContext: Context,
  ): void {
    const options = this.options;
    const root = this.backend.createRoot({
      sink,
      columns: options.columns,
      rows: options.rows,
      fitTerminal: options.fitTerminal,
      exitOnCtrlC: options.exitOnCtrlC,
      debug: options.debug,
      isTTY: options.isTTY,
      colorLevel: options.colorLevel,
    });
    this.root = root;
    const contextId = root.contextId;
    this.contextId = contextId;

    // The context was created natively, so this manager only drives the component tree.
    const runtime = getValdiRuntime();
    const rootManager = new RootComponentsManager(
      { makeRenderer: treeId => jsx.makeRendererWithAllowedRootElementTypes(undefined, treeId) },
      undefined,
      runtime.submitDebugMessage,
    );
    this.rootManager = rootManager;

    const frameObservation = this.backend.observeFrame(root.native, frame => this.onFrame(frame));
    this.disposables.push({ dispose: () => frameObservation.cancel() });

    this.setupInput(root.native, sink);
    const resizeObservation = this.backend.observeResize(root.native, (columns, rows) => {
      this.terminal.columns = columns;
      this.terminal.rows = rows;
      this.app.dispatchResize(columns, rows);
    });
    this.disposables.push({ dispose: () => resizeObservation.cancel() });

    if (options.patchConsole !== false && this.terminal.isOutputTTY && !options.debug) {
      this.patchConsole(root.native);
    }

    const container = makeTerminalRootContainer(componentClass);
    // The renderer reports an error thrown by a component through the runtime instead of throwing it,
    // which would leave a half-built application running. The initial render is synchronous and runs on
    // the caller's context rather than the new one, so only the catch-all handler sees its errors.
    const initialErrors: unknown[] = [];
    const previousCatchAll = getCatchAllUncaughtErrorHandler();
    setCatchAllUncaughtErrorHandler((_isRejection, error) => {
      initialErrors.push(error);
      return UncaughtErrorHandlerResult.IGNORE;
    });
    try {
      rootManager.createWithRenderFunction(
        contextId,
        { filePath: '<valdi_tui>', symbolName: componentClass.name },
        makeEntryPointRenderFunction(() => container, options.errorBoundary ?? false, undefined),
        viewModel,
        componentContext,
      );
    } finally {
      setCatchAllUncaughtErrorHandler(previousCatchAll);
    }
    if (initialErrors.length > 0) {
      throw toError(initialErrors[0]);
    }
  }

  /** Undoes everything `mount()` and the constructor set up. Returns the first error, after running every step. */
  private release(finishOutput: boolean): Error | undefined {
    let firstError: Error | undefined;
    const attempt = (step: () => void): void => {
      try {
        step();
      } catch (error) {
        firstError = firstError ?? toError(error);
      }
    };

    attempt(() => {
      if (this.restoreConsole) {
        this.restoreConsole();
        this.restoreConsole = undefined;
      }
    });
    // Without finishOutput, destroyRoot() abandons the output: a component that throws on its first render
    // leaves no partial frame, cursor move or trailing newline behind.
    if (finishOutput && this.root !== undefined) {
      // Painted before the component tree is released, so the last frame stays on screen. The final paint
      // runs on the main thread and its observer callback is queued, so read the frame back to keep
      // lastFrame() on it.
      const native = this.root.native;
      attempt(() => {
        this.backend.finishRoot(native);
        const frame = this.backend.getLastFrame(native);
        if (frame !== undefined) {
          this.onFrame(frame);
        }
      });
    }
    for (const disposable of this.disposables.splice(0)) {
      attempt(() => disposable.dispose());
    }
    const { rootManager, contextId, root } = this;
    if (rootManager !== undefined && contextId !== undefined) {
      attempt(() => rootManager.destroy(contextId));
    }
    if (root !== undefined) {
      attempt(() => this.backend.destroyRoot(root.native));
    }
    attempt(() => deactivateTerminalApp(this.app));
    attempt(() => endKeepAlive(this.keepAlive));
    return firstError;
  }

  private makeTerminalInfo(sink: TerminalSinkKind): TerminalInfo {
    if (sink === 'memory') {
      return {
        isOutputTTY: this.options.isTTY ?? true,
        // Input is delivered with feedInput() into a memory sink.
        isInputTTY: true,
        columns: this.options.columns ?? DEFAULT_COLUMNS,
        rows: this.options.rows ?? DEFAULT_ROWS,
      };
    }
    const size = this.backend.getSize();
    return {
      isOutputTTY: this.backend.isTTY(STDOUT_FD),
      isInputTTY: this.backend.isTTY(STDIN_FD),
      columns: this.options.columns ?? size?.[0] ?? envNumber('COLUMNS') ?? DEFAULT_COLUMNS,
      rows: this.options.rows ?? size?.[1] ?? envNumber('LINES') ?? DEFAULT_ROWS,
    };
  }

  private setupInput(native: TerminalRootNative, sink: TerminalSinkKind): void {
    // The host reads stdin only in raw mode. Without a terminal on stdin and stdout, or in a host that does not
    // own the terminal, setRawMode() is a no-op and the application runs output-only: raw mode with a redirected
    // stdout would leave the user's terminal unable to echo while nothing is drawn.
    if (sink === 'tty' && wantsRawMode(this.terminal) && this.backend.setRawMode(native, true)) {
      this.disposables.push({ dispose: () => this.backend.setRawMode(native, false) });
    }
    const parser = new InputParser();
    let escapeTimer: number | undefined;
    const cancelEscapeTimer = (): void => {
      if (escapeTimer !== undefined) {
        clearTimeout(escapeTimer);
        escapeTimer = undefined;
      }
    };
    const dispatch = (presses: KeyPress[]): void => {
      for (const press of presses) {
        if (this.unmounted) {
          return;
        }
        if (this.options.exitOnCtrlC !== false && press.key.ctrl && press.input === 'c') {
          this.exitFromApp();
          return;
        }
        this.app.dispatchInput(press);
      }
    };
    const observation = this.backend.observeInput(native, data => {
      if (this.unmounted) {
        return;
      }
      cancelEscapeTimer();
      dispatch(parser.push(data));
      if (parser.hasPending && !this.unmounted) {
        escapeTimer = setTimeout(() => {
          escapeTimer = undefined;
          dispatch(parser.flush());
        }, ESCAPE_TIMEOUT_MS);
      }
    });
    this.disposables.push({
      dispose: () => {
        observation.cancel();
        cancelEscapeTimer();
      },
    });
  }

  private patchConsole(native: TerminalRootNative): void {
    this.restoreConsole = routeConsole(text => this.backend.writeAbove(native, text));
  }

  private onFrame(frame: TerminalFrame): void {
    // Frames from different paint threads can arrive out of order; an older one never replaces a newer one.
    if (frame.sequence <= this.lastSequence) {
      return;
    }
    this.lastSequence = frame.sequence;
    this.lastPlainFrame = frame.lines;
    const waiters = this.frameWaiters.splice(0);
    for (const waiter of waiters) {
      waiter(frame.lines);
    }
  }
}

/**
 * Renders a component tree to the terminal and keeps it updated until `unmount()` is called
 * or the application exits. The returned instance mirrors Ink's `render()` result.
 *
 * ```tsx
 * const app = render(Counter, { start: 0 });
 * await app.waitUntilExit();
 * ```
 */
export function render<
  T extends IComponent<ViewModel, Context>,
  ViewModel extends object = {},
  Context extends object = {},
>(
  componentClass: ComponentConstructor<T>,
  viewModel: ViewModel = {} as ViewModel,
  options: RenderOptions = {},
  componentContext: Context = {} as Context,
): RenderInstance<ViewModel> {
  return new TerminalRenderer<ViewModel, Context>(
    componentClass as unknown as ComponentConstructor<IComponent<ViewModel, Context>>,
    viewModel,
    componentContext,
    options,
  );
}
