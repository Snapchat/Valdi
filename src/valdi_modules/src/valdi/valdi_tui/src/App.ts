import { ComponentDisposable } from 'valdi_core/src/IRenderer';
import { Key, KeyPress } from './keys';
import { Disposable, ITerminal } from './Terminal';

export type InputHandler = (input: string, key: Key) => void;
export type ResizeHandler = (columns: number, rows: number) => void;

export interface FocusOptions {
  /** Focus this element when it is registered, if nothing else is focused. */
  readonly autoFocus?: boolean;
  /** Stable identifier so {@link FocusManager.focus} can target it. */
  readonly id?: string;
  /** Inactive elements are skipped when cycling focus. @default true */
  readonly isActive?: boolean;
}

export interface Focusable extends Disposable {
  readonly id: string;
  readonly isFocused: boolean;
  /** Whether focus can land on this element. */
  isActive: boolean;
  /** Called whenever `isFocused` changes. */
  onChange(callback: (isFocused: boolean) => void): Disposable;
}

/**
 * Tracks which focusable element is active. Tab and Shift+Tab move focus while focus is enabled.
 * Equivalent of Ink's `useFocus` and `useFocusManager`.
 */
export class FocusManager {
  private readonly entries: FocusEntry[] = [];
  private focusedId: string | undefined;
  private nextId = 0;
  enabled = true;

  register(options: FocusOptions = {}): Focusable {
    const id = options.id ?? `focusable-${++this.nextId}`;
    const entry = new FocusEntry(this, id, options.isActive ?? true);
    this.entries.push(entry);
    if (options.autoFocus && this.focusedId === undefined && entry.isActive) {
      this.focus(id);
    }
    return entry;
  }

  unregister(entry: FocusEntry): void {
    const index = this.entries.indexOf(entry);
    if (index >= 0) {
      this.entries.splice(index, 1);
    }
    if (this.focusedId === entry.id) {
      this.setFocused(undefined);
    }
  }

  get activeId(): string | undefined {
    return this.focusedId;
  }

  isFocused(id: string): boolean {
    return this.focusedId === id;
  }

  focus(id: string): void {
    const entry = this.entries.find(candidate => candidate.id === id);
    if (entry && entry.isActive) {
      this.setFocused(id);
    }
  }

  focusNext(): void {
    this.move(1);
  }

  focusPrevious(): void {
    this.move(-1);
  }

  enableFocus(): void {
    this.enabled = true;
  }

  /** Disables Tab handling and clears the current focus. */
  disableFocus(): void {
    this.enabled = false;
    this.setFocused(undefined);
  }

  /** Returns true when the key press was consumed as a focus change. */
  handleKey(press: KeyPress): boolean {
    if (!this.enabled || !press.key.tab) {
      return false;
    }
    if (press.key.shift) {
      this.focusPrevious();
    } else {
      this.focusNext();
    }
    return true;
  }

  private move(direction: 1 | -1): void {
    const active = this.entries.filter(entry => entry.isActive);
    if (active.length === 0) {
      this.setFocused(undefined);
      return;
    }
    const currentIndex = active.findIndex(entry => entry.id === this.focusedId);
    let nextIndex: number;
    if (currentIndex < 0) {
      nextIndex = direction === 1 ? 0 : active.length - 1;
    } else {
      nextIndex = (currentIndex + direction + active.length) % active.length;
    }
    this.setFocused(active[nextIndex]!.id);
  }

  private setFocused(id: string | undefined): void {
    if (this.focusedId === id) {
      return;
    }
    const previous = this.focusedId;
    this.focusedId = id;
    for (const entry of this.entries) {
      if (entry.id === previous || entry.id === id) {
        entry.notify();
      }
    }
  }
}

class FocusEntry implements Focusable {
  private readonly callbacks = new Set<(isFocused: boolean) => void>();
  private active: boolean;

  constructor(
    private readonly manager: FocusManager,
    readonly id: string,
    isActive: boolean,
  ) {
    this.active = isActive;
  }

  get isFocused(): boolean {
    return this.manager.isFocused(this.id);
  }

  get isActive(): boolean {
    return this.active;
  }

  set isActive(value: boolean) {
    this.active = value;
    if (!value && this.isFocused) {
      this.manager.focusNext();
    }
  }

  onChange(callback: (isFocused: boolean) => void): Disposable {
    this.callbacks.add(callback);
    return { dispose: () => this.callbacks.delete(callback) };
  }

  notify(): void {
    const focused = this.isFocused;
    for (const callback of Array.from(this.callbacks)) {
      callback(focused);
    }
  }

  dispose(): void {
    this.manager.unregister(this);
    this.callbacks.clear();
  }
}

/**
 * Services available to components of a rendered terminal application: keyboard input,
 * terminal size, focus and exiting. Obtain it with {@link getTerminalApp}.
 */
export class TerminalApp {
  readonly focus = new FocusManager();
  private readonly inputHandlers = new Set<InputHandler>();
  private readonly resizeHandlers = new Set<ResizeHandler>();
  private exitRequested = false;

  constructor(
    readonly terminal: ITerminal,
    private readonly requestExit: (error?: Error) => void,
  ) {}

  /** Unmounts the application. An error rejects `waitUntilExit()`. */
  exit(error?: Error): void {
    if (this.exitRequested) {
      return;
    }
    this.exitRequested = true;
    this.requestExit(error);
  }

  /** Subscribes to key presses. Dispose the result when the subscriber goes away. */
  onInput(handler: InputHandler): Disposable {
    this.inputHandlers.add(handler);
    return { dispose: () => this.inputHandlers.delete(handler) };
  }

  onResize(handler: ResizeHandler): Disposable {
    this.resizeHandlers.add(handler);
    return { dispose: () => this.resizeHandlers.delete(handler) };
  }

  dispatchInput(press: KeyPress): void {
    if (this.focus.handleKey(press)) {
      return;
    }
    for (const handler of Array.from(this.inputHandlers)) {
      handler(press.input, press.key);
    }
  }

  dispatchResize(columns: number, rows: number): void {
    for (const handler of Array.from(this.resizeHandlers)) {
      handler(columns, rows);
    }
  }
}

let activeApp: TerminalApp | undefined;

export function setActiveTerminalApp(app: TerminalApp | undefined): void {
  activeApp = app;
}

/**
 * The application most recently rendered with `render()`. Components use it to read input,
 * react to resizes or exit.
 */
export function getTerminalApp(): TerminalApp {
  if (!activeApp) {
    throw new Error('No terminal application is rendered. Call render() from valdi_tui first.');
  }
  return activeApp;
}

/** The part of `Component` these helpers need; any Valdi component satisfies it. */
export interface DisposableOwner {
  registerDisposable(disposable: ComponentDisposable): boolean;
}

function registerWithComponent<T extends Disposable>(component: DisposableOwner, disposable: T): T {
  component.registerDisposable(() => disposable.dispose());
  return disposable;
}

/**
 * Registers a key handler for the lifetime of `component`. Call it from `onCreate()`.
 *
 * ```ts
 * onCreate() {
 *   useInput(this, (input, key) => {
 *     if (input === 'q') getTerminalApp().exit();
 *   });
 * }
 * ```
 */
export function useInput(component: DisposableOwner, handler: InputHandler): Disposable {
  return registerWithComponent(component, getTerminalApp().onInput(handler));
}

/** Registers a resize handler for the lifetime of `component`. */
export function useResize(component: DisposableOwner, handler: ResizeHandler): Disposable {
  return registerWithComponent(component, getTerminalApp().onResize(handler));
}

export interface UseFocusOptions extends FocusOptions {
  /** Called when focus changes. Defaults to re-rendering the component. */
  readonly onChange?: (isFocused: boolean) => void;
}

/**
 * Makes `component` focusable for the lifetime of the component. Read `isFocused` in `onRender()`.
 */
export function useFocus(component: DisposableOwner, options: UseFocusOptions = {}): Focusable {
  const focusable = getTerminalApp().focus.register(options);
  focusable.onChange(isFocused => {
    if (options.onChange) {
      options.onChange(isFocused);
    } else {
      (component as unknown as { scheduleRender(): void }).scheduleRender();
    }
  });
  return registerWithComponent(component, focusable);
}
