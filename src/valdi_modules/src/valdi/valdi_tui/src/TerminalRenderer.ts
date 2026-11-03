import { makeEntryPointRenderFunction } from 'valdi_core/src/EntryPointComponent';
import { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import { jsx } from 'valdi_core/src/JSXBootstrap';
import { RootComponentsManager } from 'valdi_core/src/RootComponentsManager';
import { getValdiRuntime } from 'valdi_core/src/ValdiRuntimeProvider';
import { KeepAlive, beginKeepAlive, endKeepAlive } from 'valdi_core/src/utils/KeepAliveCallback';
import { Disposable, TerminalApp, TerminalInfo, setActiveTerminalApp } from './App';
import { TerminalBackend, TerminalFrame, TerminalRootHandle, TerminalSinkKind, getTerminalBackend } from './TerminalBackend';
import { ColorLevel } from './ansi';
import { parseInput } from './keys';

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
  /** Stops rendering, restores the terminal and releases the component tree. */
  unmount(error?: Error): void;
  /** Resolves after `unmount()`; rejects when the application exited with an error. */
  waitUntilExit(): Promise<void>;
  /** Erases the live frame. */
  clear(): void;
  /** Lines of the most recent frame, without ANSI styling. */
  lastFrame(): ReadonlyArray<string> | undefined;
  /** Resolves with the next frame that is produced. */
  nextFrame(): Promise<ReadonlyArray<string>>;
  /**
   * Delivers text as if it had been typed. Reaches every application rendered in the runtime,
   * which makes it the way tests drive input into a memory sink.
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

function envNumber(name: string): number | undefined {
  const raw = typeof process !== 'undefined' ? process?.env?.[name] : undefined;
  if (raw === undefined) {
    return undefined;
  }
  const value = parseInt(raw, 10);
  return isNaN(value) || value <= 0 ? undefined : value;
}

class TerminalRenderer<ViewModel extends object, Context extends object> implements RenderInstance<ViewModel> {
  readonly app: TerminalApp;
  private readonly backend: TerminalBackend;
  private readonly root: TerminalRootHandle;
  private readonly contextId: string;
  private readonly rootManager: RootComponentsManager;
  private readonly keepAlive: KeepAlive;
  private readonly terminal: TerminalInfo;
  private readonly disposables: Disposable[] = [];
  private readonly exitPromise: Promise<void>;
  private resolveExit!: () => void;
  private rejectExit!: (error: Error) => void;
  private readonly frameWaiters: ((frame: ReadonlyArray<string>) => void)[] = [];
  private restoreConsole: (() => void) | undefined;
  private lastPlainFrame: ReadonlyArray<string> | undefined;
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
    this.app = new TerminalApp(this.terminal, error => this.unmount(error));
    setActiveTerminalApp(this.app);
    this.keepAlive = beginKeepAlive();

    this.root = this.backend.createRoot({
      sink,
      columns: options.columns,
      rows: options.rows,
      fitTerminal: options.fitTerminal,
      exitOnCtrlC: options.exitOnCtrlC,
      debug: options.debug,
      isTTY: options.isTTY,
      colorLevel: options.colorLevel,
    });
    this.contextId = this.root.contextId;

    // The context was created natively, so this manager only drives the component tree.
    const runtime = getValdiRuntime();
    this.rootManager = new RootComponentsManager(
      { makeRenderer: treeId => jsx.makeRendererWithAllowedRootElementTypes(undefined, treeId) },
      undefined,
      runtime.submitDebugMessage,
    );

    const frameObservation = this.backend.observeFrame(this.root.native, frame => this.onFrame(frame));
    this.disposables.push({ dispose: () => frameObservation.cancel() });

    this.setupInput();
    const resizeObservation = this.backend.observeResize((columns, rows) => {
      this.terminal.columns = columns;
      this.terminal.rows = rows;
      this.app.dispatchResize(columns, rows);
    });
    this.disposables.push({ dispose: () => resizeObservation.cancel() });

    if (options.patchConsole !== false && this.terminal.isOutputTTY && !options.debug) {
      this.patchConsole();
    }

    this.rootManager.createWithRenderFunction(
      this.contextId,
      { filePath: '<valdi_tui>', symbolName: componentClass.name },
      makeEntryPointRenderFunction(() => componentClass, options.errorBoundary ?? false, undefined),
      viewModel,
      componentContext,
    );
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

  private setupInput(): void {
    // For the tty sink no reader feeds this yet, so applications run output-only.
    const observation = this.backend.observeInput(data => {
      if (this.unmounted) {
        return;
      }
      for (const press of parseInput(data)) {
        if (this.options.exitOnCtrlC !== false && press.key.ctrl && press.input === 'c') {
          this.unmount();
          return;
        }
        this.app.dispatchInput(press);
      }
    });
    this.disposables.push({ dispose: () => observation.cancel() });
  }

  private patchConsole(): void {
    const target = console as unknown as { [method: string]: (...args: unknown[]) => void };
    const originals: { [method: string]: (...args: unknown[]) => void } = {};
    for (const method of CONSOLE_METHODS) {
      const original = target[method];
      if (typeof original !== 'function') {
        continue;
      }
      originals[method] = original;
      target[method] = (...args: unknown[]) => {
        this.backend.writeAbove(this.root.native, formatConsoleArgs(args));
      };
    }
    this.restoreConsole = () => {
      for (const method of Object.keys(originals)) {
        target[method] = originals[method]!;
      }
    };
  }

  private onFrame(frame: TerminalFrame): void {
    this.lastPlainFrame = frame.lines;
    const waiters = this.frameWaiters.splice(0);
    for (const waiter of waiters) {
      waiter(frame.lines);
    }
  }

  rerender(viewModel: ViewModel): void {
    if (this.unmounted) {
      return;
    }
    this.rootManager.render(this.contextId, viewModel);
  }

  clear(): void {
    this.backend.clearRoot(this.root.native);
  }

  lastFrame(): ReadonlyArray<string> | undefined {
    return this.lastPlainFrame;
  }

  nextFrame(): Promise<ReadonlyArray<string>> {
    return new Promise(resolve => {
      this.frameWaiters.push(resolve);
    });
  }

  feedInput(text: string): void {
    this.backend.feedInput(this.root.native, text);
  }

  setSize(columns: number, rows: number): void {
    this.backend.setSize(this.root.native, columns, rows);
  }

  output(): string {
    return this.backend.getOutput(this.root.native);
  }

  waitUntilExit(): Promise<void> {
    return this.exitPromise;
  }

  unmount(error?: Error): void {
    if (this.unmounted) {
      return;
    }
    this.unmounted = true;
    if (this.restoreConsole) {
      this.restoreConsole();
      this.restoreConsole = undefined;
    }
    // Painted before the component tree is released, so the last frame stays on screen. The
    // frame observer is still attached here, which keeps lastFrame() on that final frame.
    this.backend.finishRoot(this.root.native);
    for (const disposable of this.disposables.splice(0)) {
      disposable.dispose();
    }
    try {
      this.rootManager.destroy(this.contextId);
    } catch (destroyError) {
      console.error('valdi_tui: failed to release the component tree', destroyError);
    }
    this.backend.destroyRoot(this.root.native);
    setActiveTerminalApp(undefined);
    endKeepAlive(this.keepAlive);
    const waiters = this.frameWaiters.splice(0);
    for (const waiter of waiters) {
      waiter(this.lastPlainFrame ?? []);
    }
    if (error) {
      this.rejectExit(error);
    } else {
      this.resolveExit();
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
