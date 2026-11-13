/**
 * The native terminal backend, registered by hosts that link `//valdi:valdi_terminal`: the module
 * test runner (memory sink only) and CLI applications built with `valdi_cli_application(terminal = True)`,
 * which also own the process terminal. Contexts created here render through the
 * terminal view manager, so `<view>` and `<label>` paint into a cell grid instead of platform views.
 */

export interface TerminalRootNative {
  brand?: 'TerminalRootNative';
}

export interface Cancelable {
  cancel(): void;
}

export type TerminalSinkKind = 'tty' | 'memory';

export interface CreateRootOptions {
  /** `tty` writes to the process stdout; `memory` records output for tests and headless hosts. */
  sink: TerminalSinkKind;
  /** Overrides the width reported by the sink. */
  columns?: number;
  /** Overrides the height reported by the sink. */
  rows?: number;
  /** Lay out to the full terminal height instead of the content height. */
  fitTerminal?: boolean;
  /**
   * `tty` roots: when the host receives Ctrl+C it restores the terminal and exits the process (status 130)
   * without involving JS; when false the byte is delivered to input observers instead. Memory roots never
   * exit natively; the TypeScript layer handles the Ctrl+C that {@link feedInput} delivers.
   * @default true
   */
  exitOnCtrlC?: boolean;
  /** Append every frame instead of redrawing in place. */
  debug?: boolean;
  /** Memory sink only: whether the sink behaves like an interactive terminal. @default true */
  isTTY?: boolean;
  /** Memory sink only: 0 none, 1 basic, 2 ansi256, 3 truecolor. @default 0 */
  colorLevel?: number;
}

export interface TerminalRootHandle {
  /** Opaque id of the context, passed straight to the renderer. */
  contextId: string;
  native: TerminalRootNative;
}

/** A painted frame. */
export interface TerminalFrame {
  /** Rows of the live region without styling, trailing blanks trimmed. */
  lines: string[];
  /** The rows joined with newlines, with ANSI styling at the sink's color level. */
  ansi: string;
  columns: number;
  height: number;
  /**
   * Increases with every frame the root paints. Observers can be called out of order when frames
   * are painted on different threads, so compare it to drop a frame older than one already seen.
   */
  sequence: number;
}

export function createRoot(options: CreateRootOptions): TerminalRootHandle;

/** Paints the final frame and leaves the output consistent. Call before `destroyRoot`. */
export function finishRoot(native: TerminalRootNative): void;

export function destroyRoot(native: TerminalRootNative): void;

/**
 * Observes every painted frame. Frames are painted on the main thread and handed to the JS queue.
 * Painting that runs on the JS thread itself, such as the final frame of {@link finishRoot},
 * calls the callback synchronously on that thread.
 */
export function observeFrame(native: TerminalRootNative, callback: (frame: TerminalFrame) => void): Cancelable;

export function getLastFrame(native: TerminalRootNative): TerminalFrame | undefined;

/** Lays the root out for a terminal of this size and notifies resize observers. */
export function setSize(native: TerminalRootNative, columns: number, rows: number): void;

/** Writes lines above the live frame; they are never erased. */
export function writeAbove(native: TerminalRootNative, text: string): void;

/** Erases the live frame; the next paint draws it again. */
export function clearRoot(native: TerminalRootNative): void;

/** Everything written to a memory sink so far; empty for other sinks. */
export function getOutput(native: TerminalRootNative): string;

/**
 * Delivers terminal input to the observers that {@link observeInput} registered for this root.
 * Memory-sink tests use it to type; a TTY host reader calls the same native path.
 */
export function feedInput(native: TerminalRootNative, text: string): void;

/**
 * Observes raw terminal input for one root. For the `tty` sink the host delivers keyboard input while
 * {@link setRawMode} is enabled; memory roots receive what {@link feedInput} delivers.
 */
export function observeInput(native: TerminalRootNative, callback: (text: string) => void): Cancelable;

/**
 * Puts the process terminal in raw mode for a `tty` root, which is what makes keyboard input reach
 * {@link observeInput} observers. The host restores the terminal when the last root that enabled it
 * is destroyed, and when the process exits or is signalled. Returns false for memory roots, when
 * stdin is not a terminal, and in hosts that do not own the terminal.
 */
export function setRawMode(native: TerminalRootNative, enabled: boolean): boolean;

/**
 * Observes the terminal size changes of one root: its own {@link setSize} calls and, for `tty` roots
 * without an explicit size, resizes of the process terminal.
 */
export function observeResize(
  native: TerminalRootNative,
  callback: (columns: number, rows: number) => void,
): Cancelable;

/** Size of the process terminal as `[columns, rows]`, or undefined when stdout is not a terminal. */
export function getSize(): [number, number] | undefined;

export function isTTY(fd: number): boolean;
