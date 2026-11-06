/**
 * Native terminal bridge provided by the standalone runtime (see TerminalModule.cpp).
 * Only available when running under the Valdi CLI runtime; load it lazily and fall back
 * when the module cannot be resolved.
 */

export interface TerminalCancelable {
  cancel(): void;
}

/** Standard file descriptors accepted by isTTY() and write(). */
export const enum TerminalFd {
  STDIN = 0,
  STDOUT = 1,
  STDERR = 2,
}

/** Whether the given file descriptor is attached to a terminal. */
export function isTTY(fd: TerminalFd): boolean;

/** Current terminal size as [columns, rows], or undefined when no terminal is attached. */
export function getSize(): [number, number] | undefined;

/**
 * Enables or disables raw keyboard input on stdin (no echo, no line buffering, no signal keys).
 * Returns false when stdin is not a terminal. The previous attributes are restored at exit.
 */
export function setRawMode(enabled: boolean): boolean;

/** Writes text to the file descriptor without buffering or log decoration. */
export function write(fd: TerminalFd, text: string): void;

/**
 * Observes stdin. Chunks are delivered as UTF-8 strings on the JS thread; a chunk may
 * contain several key presses or a single escape sequence.
 */
export function observeInput(observer: (data: string) => void): TerminalCancelable;

/** Observes terminal size changes. Only changes are reported, read the initial size with getSize(). */
export function observeResize(observer: (columns: number, rows: number) => void): TerminalCancelable;
