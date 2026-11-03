/**
 * The native terminal backend, registered by hosts that link `//valdi:valdi_terminal`
 * (the module test runner and the standalone runtime). Contexts created here render through the
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
  /** Reserved for hosts that own input; Ctrl+C handling happens in the TypeScript layer. */
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
}

export function createRoot(options: CreateRootOptions): TerminalRootHandle;

/** Paints the final frame and leaves the output consistent. Call before `destroyRoot`. */
export function finishRoot(native: TerminalRootNative): void;

export function destroyRoot(native: TerminalRootNative): void;

/** Observes every painted frame. Callbacks run on the JS queue. */
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
 * Delivers terminal input to the observers registered with {@link observeInput}. Memory-sink
 * tests use it to type; a TTY host reader calls the same native path.
 */
export function feedInput(native: TerminalRootNative, text: string): void;

/**
 * Observes raw terminal input. Nothing produces input for the `tty` sink yet, so a registered
 * observer only fires for {@link feedInput}.
 */
export function observeInput(callback: (text: string) => void): Cancelable;

/** Observes terminal size changes reported with {@link setSize}. */
export function observeResize(callback: (columns: number, rows: number) => void): Cancelable;

/** Size of the process terminal as `[columns, rows]`, or undefined when stdout is not a terminal. */
export function getSize(): [number, number] | undefined;

export function isTTY(fd: number): boolean;
