import type * as Bridge from './TerminalBackendBridge';

export type {
  Cancelable,
  CreateRootOptions,
  TerminalFrame,
  TerminalRootHandle,
  TerminalRootNative,
  TerminalSinkKind,
} from './TerminalBackendBridge';

export type TerminalBackend = typeof Bridge;

declare const require: (id: string) => unknown;

let cachedBackend: TerminalBackend | undefined;

/**
 * The native terminal backend. Loaded on first use because only hosts that link the terminal
 * module provide it; everywhere else rendering to a terminal is not possible.
 */
export function getTerminalBackend(): TerminalBackend {
  if (cachedBackend === undefined) {
    try {
      cachedBackend = require('valdi_tui/src/TerminalBackendBridge') as TerminalBackend;
    } catch (error) {
      throw new Error(
        'valdi_tui needs the native terminal backend, which this runtime does not provide. ' +
          'Run it with valdi_standalone built with //valdi:valdi_terminal. ' +
          `(${error instanceof Error ? error.message : String(error)})`,
      );
    }
  }
  return cachedBackend;
}
