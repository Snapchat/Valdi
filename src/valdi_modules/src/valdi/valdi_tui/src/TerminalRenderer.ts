import { ComponentPath } from 'valdi_core/src/ComponentPath';
import { makeEntryPointRenderFunction } from 'valdi_core/src/EntryPointComponent';
import { EntryPointRenderFunction } from 'valdi_core/src/EntryPointRenderFunction';
import { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import { jsx } from 'valdi_core/src/JSXBootstrap';
import { Renderer } from 'valdi_core/src/Renderer';
import { RendererFactory } from 'valdi_core/src/RendererFactory';
import { RootComponentsManager } from 'valdi_core/src/RootComponentsManager';
import { getValdiRuntime } from 'valdi_core/src/ValdiRuntimeProvider';
import { DaemonClientManager } from 'valdi_core/src/debugging/DaemonClientManager';
import { SubmitDebugMessageFunc } from 'valdi_core/src/debugging/DebugMessage';
import { KeepAlive, beginKeepAlive, endKeepAlive } from 'valdi_core/src/utils/KeepAliveCallback';
import { RenderedElement, getStandaloneRuntime } from 'valdi_standalone/src/ValdiStandalone';
import { TerminalApp, setActiveTerminalApp } from './App';
import { FrameWriter } from './FrameWriter';
import { PaintNode, paintTree } from './Painter';
import { Screen } from './Screen';
import { Disposable, ITerminal, Terminal } from './Terminal';
import { parseInput } from './keys';

export interface RenderOptions {
  /** Where to draw. Defaults to the process terminal. */
  readonly terminal?: ITerminal;
  /** Exit when Ctrl+C is pressed in raw mode. @default true */
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
  /** Wrap the root component in Valdi's error boundary. */
  readonly errorBoundary?: boolean;
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
}

const MEASURE_MODE_UNSPECIFIED = 0;
const MEASURE_MODE_EXACTLY = 1;

declare const process: { env?: { [name: string]: string | undefined } } | undefined;

// VALDI_TUI_DEBUG=1 logs layout decisions and the rendered tree to stderr.
const DEBUG_ENABLED = typeof process !== 'undefined' && process?.env?.VALDI_TUI_DEBUG === '1';

// Captured before render() can patch console, so debug output bypasses the live frame.
const debugWrite: (message: string) => void = console.error.bind(console);

function debugLog(message: string): void {
  if (DEBUG_ENABLED) {
    debugWrite(`[valdi_tui] ${message}`);
  }
}

function describeTree(node: PaintNode, depth: number, out: string[]): void {
  const f = node.frame;
  const indent = '  '.repeat(depth);
  const runs = node.attributes?.tuiRuns;
  const text = Array.isArray(runs) ? ` "${runs.map((run: { text: string }) => run.text).join('')}"` : '';
  out.push(`${indent}${node.viewClass} @${f.x},${f.y} ${f.width}x${f.height}${text}`);
  for (const child of node.children) {
    describeTree(child, depth + 1, out);
  }
}

class TUIRootComponentsManager extends RootComponentsManager {
  constructor(
    rendererFactory: RendererFactory,
    daemonClientManager: DaemonClientManager | undefined,
    submitDebugMessage: SubmitDebugMessageFunc,
    readonly renderFunction: EntryPointRenderFunction,
    readonly componentPath: ComponentPath,
    readonly initialViewModel: unknown,
    readonly componentContext: unknown,
  ) {
    super(rendererFactory, daemonClientManager, submitDebugMessage);
  }

  create(componentId: string, _componentPath: string, _viewModel: unknown, _componentContext: unknown): void {
    this.createWithRenderFunction(
      componentId,
      this.componentPath,
      this.renderFunction,
      this.initialViewModel,
      this.componentContext,
    );
  }
}

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

class TerminalRenderer<ViewModel extends object, Context extends object> implements RenderInstance<ViewModel> {
  readonly app: TerminalApp;
  private readonly terminal: ITerminal;
  private readonly writer: FrameWriter;
  private readonly rootManager: TUIRootComponentsManager;
  private readonly contextId: string;
  private readonly renderer: Renderer;
  private readonly keepAlive: KeepAlive;
  private readonly disposables: Disposable[] = [];
  private readonly exitPromise: Promise<void>;
  private resolveExit!: () => void;
  private rejectExit!: (error: Error) => void;
  private readonly frameWaiters: ((frame: ReadonlyArray<string>) => void)[] = [];
  private emittedStaticLines: number[] = [];
  private pendingConsoleLines: string[] = [];
  private restoreConsole: (() => void) | undefined;
  private framePending = false;
  private frameInFlight = false;
  private frameRequestedDuringFlight = false;
  private ignoreRenderEvents = false;
  private layoutWidth = -1;
  private layoutHeight = -1;
  private lastPlainFrame: ReadonlyArray<string> | undefined;
  private unmounted = false;
  private rawMode = false;

  constructor(
    componentClass: ComponentConstructor<IComponent<ViewModel, Context>>,
    viewModel: ViewModel,
    componentContext: Context,
    private readonly options: RenderOptions,
  ) {
    this.terminal = options.terminal ?? new Terminal();
    this.writer = new FrameWriter(this.terminal, { debug: options.debug });
    this.exitPromise = new Promise<void>((resolve, reject) => {
      this.resolveExit = resolve;
      this.rejectExit = reject;
    });
    // Unhandled rejections are not fatal here; callers opt in through waitUntilExit().
    this.exitPromise.catch(() => {});
    this.app = new TerminalApp(this.terminal, error => this.unmount(error));
    setActiveTerminalApp(this.app);
    this.keepAlive = beginKeepAlive();

    const runtime = getValdiRuntime();
    this.rootManager = new TUIRootComponentsManager(
      { makeRenderer: treeId => jsx.makeRendererWithAllowedRootElementTypes(undefined, treeId) },
      undefined,
      runtime.submitDebugMessage,
      makeEntryPointRenderFunction(() => componentClass, options.errorBoundary ?? false, undefined),
      { filePath: '<valdi_tui>', symbolName: componentClass.name },
      viewModel,
      componentContext,
    );
    this.contextId = runtime.createContext(this.rootManager);
    const handle = this.rootManager.rootComponents[this.contextId];
    if (!handle) {
      throw new Error('Could not resolve the root renderer after the context was created');
    }
    this.renderer = handle.renderer;
    this.renderer.setEventListener({
      onRenderBegin: () => {},
      onRenderEnd: () => {
        if (!this.ignoreRenderEvents) {
          this.scheduleFrame();
        }
      },
      onComponentBegin: () => {},
      onComponentEnd: () => {},
      onBypassComponentRender: () => {},
      onComponentViewModelPropertyChange: () => {},
    });

    this.setupInput();
    this.disposables.push(
      this.terminal.onResize((columns, rows) => {
        this.app.dispatchResize(columns, rows);
        this.scheduleFrame();
      }),
    );
    if (options.patchConsole !== false && this.terminal.isOutputTTY && !options.debug) {
      this.patchConsole();
    }
    this.scheduleFrame();
  }

  private setupInput(): void {
    if (!this.terminal.isInputTTY) {
      return;
    }
    this.rawMode = this.terminal.setRawMode(true);
    this.disposables.push(
      this.terminal.onInput(data => {
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
      }),
    );
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
        this.pendingConsoleLines.push(...formatConsoleArgs(args).split('\n'));
        this.scheduleFrame();
      };
    }
    this.restoreConsole = () => {
      for (const method of Object.keys(originals)) {
        target[method] = originals[method]!;
      }
    };
  }

  private scheduleFrame(): void {
    if (this.unmounted) {
      return;
    }
    if (this.frameInFlight) {
      this.frameRequestedDuringFlight = true;
      return;
    }
    if (this.framePending) {
      return;
    }
    this.framePending = true;
    setTimeout(() => {
      this.framePending = false;
      this.produceFrame();
    }, 0);
  }

  private produceFrame(): void {
    if (this.unmounted) {
      return;
    }
    this.frameInFlight = true;
    const runtime = getValdiRuntime();
    const columns = Math.max(1, this.options.columns ?? this.terminal.columns);
    debugLog('produceFrame');

    // A render that invalidates layout marks the tree's layout specs dirty, and the runtime skips
    // layout until the host supplies specs again (on device the platform layout pass does this).
    // Re-sending the current specs is what unblocks the layout-complete callback below; the first
    // call also creates the view node tree.
    const provisionalHeight = this.layoutHeight >= 0 ? this.layoutHeight : Math.max(1, this.terminal.rows);
    runtime.setLayoutSpecs(this.contextId, columns, provisionalHeight, false);

    // The callback runs once the runtime has applied the pending render and laid the tree out,
    // so the measured height and the dumped frames belong to the render that triggered this frame.
    this.afterLayout(() => {
      let height: number;
      if (this.options.fitTerminal) {
        height = Math.max(1, this.terminal.rows);
      } else {
        const measured = runtime.measureContext(
          this.contextId,
          columns,
          MEASURE_MODE_EXACTLY,
          0,
          MEASURE_MODE_UNSPECIFIED,
          false,
        );
        height = Math.max(0, Math.ceil(measured[1]));
        debugLog(`measured ${measured[0]}x${measured[1]} at ${columns} columns`);
      }
      if (columns === this.layoutWidth && height === this.layoutHeight) {
        this.dumpFrame(columns, height);
        return;
      }
      this.layoutWidth = columns;
      this.layoutHeight = height;
      runtime.setLayoutSpecs(this.contextId, columns, height, false);
      this.afterLayout(() => this.dumpFrame(columns, height));
    });
  }

  private afterLayout(callback: () => void): void {
    // onLayoutComplete() outside a render pass submits a tiny render request of its own, which
    // ends with onRenderEnd(); that must not be mistaken for an application render.
    this.ignoreRenderEvents = true;
    try {
      this.renderer.onLayoutComplete(() => {
        debugLog('layout complete');
        if (this.unmounted) {
          this.frameInFlight = false;
          return;
        }
        callback();
      });
    } finally {
      this.ignoreRenderEvents = false;
    }
  }

  private dumpFrame(columns: number, height: number): void {
    debugLog(`dump ${columns}x${height}`);
    getStandaloneRuntime().getRenderedElement(this.contextId, (element, error) => {
      this.frameInFlight = false;
      if (this.unmounted) {
        return;
      }
      if (error || !element) {
        console.error(`valdi_tui: could not read the rendered tree: ${error ?? 'no root element'}`);
      } else {
        if (DEBUG_ENABLED) {
          const lines: string[] = [];
          describeTree(element as PaintNode, 0, lines);
          debugLog(`tree for ${columns}x${height}:\n${lines.join('\n')}`);
        }
        this.emitFrame(element, columns, height);
      }
      if (this.frameRequestedDuringFlight) {
        this.frameRequestedDuringFlight = false;
        this.scheduleFrame();
      }
    });
  }

  private emitFrame(element: RenderedElement, columns: number, height: number): void {
    const painted = paintTree(element as PaintNode, columns, height);
    const level = this.terminal.colorLevel;
    const frame = painted.screen.toLines(level);

    const staticLines: string[] = [];
    if (this.pendingConsoleLines.length > 0) {
      staticLines.push(...this.pendingConsoleLines);
      this.pendingConsoleLines = [];
    }
    painted.staticScreens.forEach((screen: Screen, index: number) => {
      const lines = screen.toLines(level);
      const emitted = this.emittedStaticLines[index] ?? 0;
      if (lines.length > emitted) {
        staticLines.push(...lines.slice(emitted));
        this.emittedStaticLines[index] = lines.length;
      }
    });

    this.writer.write(frame, staticLines);
    this.lastPlainFrame = painted.screen.toLines(0);
    const waiters = this.frameWaiters.splice(0);
    for (const waiter of waiters) {
      waiter(this.lastPlainFrame);
    }
  }

  rerender(viewModel: ViewModel): void {
    if (this.unmounted) {
      return;
    }
    this.rootManager.render(this.contextId, viewModel);
  }

  clear(): void {
    this.writer.clear();
  }

  lastFrame(): ReadonlyArray<string> | undefined {
    return this.lastPlainFrame;
  }

  nextFrame(): Promise<ReadonlyArray<string>> {
    return new Promise(resolve => {
      this.frameWaiters.push(resolve);
    });
  }

  waitUntilExit(): Promise<void> {
    return this.exitPromise;
  }

  unmount(error?: Error): void {
    if (this.unmounted) {
      return;
    }
    this.unmounted = true;
    for (const disposable of this.disposables.splice(0)) {
      disposable.dispose();
    }
    if (this.rawMode) {
      this.terminal.setRawMode(false);
    }
    if (this.restoreConsole) {
      this.restoreConsole();
      this.restoreConsole = undefined;
    }
    this.writer.finish();
    if (this.pendingConsoleLines.length > 0) {
      this.terminal.write(this.pendingConsoleLines.join('\n') + '\n');
      this.pendingConsoleLines = [];
    }
    try {
      getValdiRuntime().destroyContext(this.contextId);
    } catch (destroyError) {
      console.error('valdi_tui: failed to destroy the root context', destroyError);
    }
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
