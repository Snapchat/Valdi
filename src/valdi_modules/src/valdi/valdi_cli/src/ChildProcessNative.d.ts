/**
 * @ExportModule
 */

/**
 * @ExportProxy
 */
export interface NativeChildProcessPipe {
  onData(data: Uint8Array): void;
}

/**
 * @ExportProxy
 */
export interface NativeChildProcessExitListener {
  onExit(errorCode: number): void;
}

// @ExportFunction
export function spawn(
  command: string,
  args: string[],
  cwd?: string,
  stdout?: NativeChildProcessPipe,
  stderr?: NativeChildProcessPipe,
  exitListener?: NativeChildProcessExitListener,
  inheritOutput?: boolean,
): number;

// @ExportFunction
export function sendToStdin(pid: number, data: Uint8Array): void;

// @ExportFunction
export function kill(pid: number): void;
