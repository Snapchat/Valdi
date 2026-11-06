import type * as TerminalBridge from 'valdi_standalone/src/TerminalBridge';
import { ColorLevel, detectColorLevel } from './ansi';

declare const require: (id: string) => any;
declare const process:
  | {
      env?: { [name: string]: string | undefined };
      stdout?: { write(text: string): unknown };
    }
  | undefined;

export interface Disposable {
  dispose(): void;
}

/**
 * The terminal a renderer draws into. {@link Terminal} wraps the real one; {@link MemoryTerminal}
 * records output for tests.
 */
export interface ITerminal {
  /** Whether stdout is a terminal. When false, output is written once at unmount, like Ink in CI. */
  readonly isOutputTTY: boolean;
  /** Whether stdin is a terminal and raw keyboard input is possible. */
  readonly isInputTTY: boolean;
  readonly columns: number;
  readonly rows: number;
  readonly colorLevel: ColorLevel;
  write(text: string): void;
  setRawMode(enabled: boolean): boolean;
  onInput(callback: (data: string) => void): Disposable;
  onResize(callback: (columns: number, rows: number) => void): Disposable;
}

export interface TerminalOptions {
  /** Overrides color support detection. */
  readonly colorLevel?: ColorLevel;
}

const DEFAULT_COLUMNS = 80;
const DEFAULT_ROWS = 24;

type Bridge = typeof TerminalBridge;

let cachedBridge: Bridge | null | undefined;

// The bridge only exists under the standalone (CLI) runtime; everywhere else the terminal
// degrades to environment-derived size and stdout writes.
function getBridge(): Bridge | null {
  if (cachedBridge === undefined) {
    try {
      cachedBridge = require('valdi_standalone/src/TerminalBridge') as Bridge;
    } catch {
      cachedBridge = null;
    }
  }
  return cachedBridge;
}

function envValue(name: string): string | undefined {
  if (typeof process === 'undefined' || !process?.env) {
    return undefined;
  }
  return process.env[name];
}

function envNumber(name: string): number | undefined {
  const raw = envValue(name);
  if (raw === undefined) {
    return undefined;
  }
  const value = parseInt(raw, 10);
  return isNaN(value) || value <= 0 ? undefined : value;
}

export class Terminal implements ITerminal {
  readonly isOutputTTY: boolean;
  readonly isInputTTY: boolean;
  readonly colorLevel: ColorLevel;
  private readonly bridge: Bridge | null;

  constructor(options: TerminalOptions = {}) {
    this.bridge = getBridge();
    this.isOutputTTY = this.bridge ? this.bridge.isTTY(1) : false;
    this.isInputTTY = this.bridge ? this.bridge.isTTY(0) : false;
    const env: { [name: string]: string | undefined } =
      typeof process !== 'undefined' && process?.env ? process.env : {};
    this.colorLevel = options.colorLevel ?? detectColorLevel(env, this.isOutputTTY);
  }

  /** Whether the native terminal bridge is available. */
  get isNative(): boolean {
    return this.bridge !== null;
  }

  get columns(): number {
    return this.size()[0];
  }

  get rows(): number {
    return this.size()[1];
  }

  private size(): [number, number] {
    const native = this.bridge?.getSize();
    if (native) {
      return native;
    }
    return [envNumber('COLUMNS') ?? DEFAULT_COLUMNS, envNumber('LINES') ?? DEFAULT_ROWS];
  }

  write(text: string): void {
    if (text === '') {
      return;
    }
    if (this.bridge) {
      this.bridge.write(1, text);
    } else if (typeof process !== 'undefined' && process?.stdout) {
      process.stdout.write(text);
    } else {
      console.log(text);
    }
  }

  setRawMode(enabled: boolean): boolean {
    if (!this.bridge || !this.isInputTTY) {
      return false;
    }
    return this.bridge.setRawMode(enabled);
  }

  onInput(callback: (data: string) => void): Disposable {
    if (!this.bridge) {
      return { dispose: () => {} };
    }
    const cancelable = this.bridge.observeInput(callback);
    return { dispose: () => cancelable.cancel() };
  }

  onResize(callback: (columns: number, rows: number) => void): Disposable {
    if (!this.bridge) {
      return { dispose: () => {} };
    }
    const cancelable = this.bridge.observeResize(callback);
    return { dispose: () => cancelable.cancel() };
  }
}

export interface MemoryTerminalOptions {
  readonly columns?: number;
  readonly rows?: number;
  readonly isOutputTTY?: boolean;
  readonly isInputTTY?: boolean;
  readonly colorLevel?: ColorLevel;
}

/** An in-memory terminal that records everything written to it. Intended for tests. */
export class MemoryTerminal implements ITerminal {
  columns: number;
  rows: number;
  readonly isOutputTTY: boolean;
  readonly isInputTTY: boolean;
  readonly colorLevel: ColorLevel;
  /** Every write, in order. */
  readonly writes: string[] = [];
  rawMode = false;

  private readonly inputCallbacks = new Set<(data: string) => void>();
  private readonly resizeCallbacks = new Set<(columns: number, rows: number) => void>();

  constructor(options: MemoryTerminalOptions = {}) {
    this.columns = options.columns ?? DEFAULT_COLUMNS;
    this.rows = options.rows ?? DEFAULT_ROWS;
    this.isOutputTTY = options.isOutputTTY ?? true;
    this.isInputTTY = options.isInputTTY ?? true;
    this.colorLevel = options.colorLevel ?? ColorLevel.None;
  }

  /** Everything written so far, concatenated. */
  get output(): string {
    return this.writes.join('');
  }

  write(text: string): void {
    this.writes.push(text);
  }

  setRawMode(enabled: boolean): boolean {
    if (!this.isInputTTY) {
      return false;
    }
    this.rawMode = enabled;
    return true;
  }

  onInput(callback: (data: string) => void): Disposable {
    this.inputCallbacks.add(callback);
    return { dispose: () => this.inputCallbacks.delete(callback) };
  }

  onResize(callback: (columns: number, rows: number) => void): Disposable {
    this.resizeCallbacks.add(callback);
    return { dispose: () => this.resizeCallbacks.delete(callback) };
  }

  /** Simulates keyboard input. */
  sendInput(data: string): void {
    for (const callback of Array.from(this.inputCallbacks)) {
      callback(data);
    }
  }

  /** Simulates a terminal resize. */
  resize(columns: number, rows: number): void {
    this.columns = columns;
    this.rows = rows;
    for (const callback of Array.from(this.resizeCallbacks)) {
      callback(columns, rows);
    }
  }
}
