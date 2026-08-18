import { arrayToString } from 'coreutils/src/Uint8ArrayUtils';
import { makeKeepAliveCallback } from 'valdi_core/src/utils/KeepAliveCallback';

import { kill, sendToStdin, spawn } from './ChildProcessNative';

export interface ChildProcessPipe {
  onData(data: Uint8Array): void;
}

export class ChildProcessPipeBuffer implements ChildProcessPipe {
  private buffers: Uint8Array[] = [];

  get length(): number {
    let out = 0;
    for (const buffer of this.buffers) {
      out += buffer.length;
    }
    return out;
  }

  clear(): void {
    this.buffers = [];
  }

  toString(): string {
    return arrayToString(this.toData());
  }

  toData(): Uint8Array {
    const out = new Uint8Array(this.length);
    let offset = 0;
    for (const buffer of this.buffers) {
      out.set(buffer, offset);
      offset += buffer.length;
    }
    return out;
  }

  onData(data: Uint8Array): void {
    if (data.length > 0) {
      this.buffers.push(data);
    }
  }
}

export interface ChildProcessExitListener {
  onExit(errorCode: number): void;
}

export interface ChildProcessResult {
  errorCode: number;
  stdout: Uint8Array;
  stderr: Uint8Array;
}

export class ChildProcess {
  cwd?: string;
  args: string[] = [];
  stdout?: ChildProcessPipe;
  stderr?: ChildProcessPipe;
  exitListener?: ChildProcessExitListener;
  inheritOutput = false;

  private pid: number | undefined;
  private errorCode: number | undefined;
  private exitPromise: Promise<number> | undefined;
  private resolveExit: ((errorCode: number) => void) | undefined;

  constructor(private readonly command: string) {}

  get processId(): number | undefined {
    return this.pid;
  }

  get exitCode(): number | undefined {
    return this.errorCode;
  }

  sendToStdin(data: Uint8Array): void {
    if (this.pid === undefined) {
      throw new Error(`Cannot send stdin to ${this.command} before launch()`);
    }
    sendToStdin(this.pid, data);
  }

  async launch(): Promise<void> {
    if (this.pid !== undefined) {
      throw new Error(`${this.command} is already running`);
    }
    this.exitPromise = new Promise(resolve => {
      this.resolveExit = resolve;
    });
    let didSpawn = false;
    const exitListener: ChildProcessExitListener = {
      onExit: makeKeepAliveCallback((errorCode: number): void => {
        if (!didSpawn) {
          return;
        }
        this.errorCode = errorCode;
        this.exitListener?.onExit(errorCode);
        this.resolveExit?.(errorCode);
      }),
    };
    let pid: number;
    try {
      pid = spawn(this.command, this.args, this.cwd, this.stdout, this.stderr, exitListener, this.inheritOutput);
      didSpawn = true;
    } catch (error) {
      exitListener.onExit(0);
      throw error;
    }
    this.pid = pid;
  }

  async waitForExit(): Promise<number> {
    if (this.exitPromise === undefined) {
      throw new Error(`Cannot wait for ${this.command} before launch()`);
    }
    return await this.exitPromise;
  }

  kill(): void {
    if (this.pid !== undefined && this.errorCode === undefined) {
      kill(this.pid);
    }
  }

  static async run(command: string, args: string[], cwd?: string): Promise<ChildProcessResult> {
    const child = new ChildProcess(command);
    const stdout = new ChildProcessPipeBuffer();
    const stderr = new ChildProcessPipeBuffer();
    child.args = args;
    child.cwd = cwd;
    child.stdout = stdout;
    child.stderr = stderr;
    await child.launch();
    const errorCode = await child.waitForExit();
    return {
      errorCode: errorCode,
      stdout: stdout.toData(),
      stderr: stderr.toData(),
    };
  }
}
