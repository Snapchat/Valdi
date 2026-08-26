import type {
  NativeChildProcessExitListener,
  NativeChildProcessPipe,
} from '../src/ChildProcessNative';

function unavailable(): never {
  throw new Error('valdi_cli ChildProcess is unavailable in web builds.');
}

export function spawn(
  _command: string,
  _args: string[],
  _cwd?: string,
  _stdout?: NativeChildProcessPipe,
  _stderr?: NativeChildProcessPipe,
  _exitListener?: NativeChildProcessExitListener,
  _inheritOutput?: boolean,
): number {
  return unavailable();
}

export function sendToStdin(_pid: number, _data: Uint8Array): void {
  unavailable();
}

export function kill(_pid: number): void {
  unavailable();
}
