import { ANSI } from './ansi';
import { ITerminal } from './Terminal';

export interface FrameWriterOptions {
  /** Append every frame instead of replacing the previous one. */
  readonly debug?: boolean;
}

/**
 * Writes frames to the terminal the way Ink's log-update does: the previous frame is erased
 * and redrawn in place, static content is emitted once above it, and when stdout is not a
 * terminal the live frame is only written when the renderer finishes.
 */
export class FrameWriter {
  private previousLineCount = 0;
  private previousOutput: string | undefined;
  private lastFrame: string[] = [];
  private cursorHidden = false;
  private finished = false;

  constructor(
    private readonly terminal: ITerminal,
    private readonly options: FrameWriterOptions = {},
  ) {}

  /** The most recent live frame, without trailing newline. */
  get currentFrame(): ReadonlyArray<string> {
    return this.lastFrame;
  }

  write(frame: ReadonlyArray<string>, staticLines: ReadonlyArray<string>): void {
    if (this.finished) {
      return;
    }
    this.lastFrame = frame.slice();
    const staticOutput = staticLines.length > 0 ? staticLines.join('\n') + '\n' : '';
    const output = frame.join('\n');

    if (this.options.debug) {
      this.terminal.write(staticOutput + output + '\n');
      return;
    }

    if (!this.terminal.isOutputTTY) {
      if (staticOutput) {
        this.terminal.write(staticOutput);
      }
      return;
    }

    if (!this.cursorHidden) {
      this.terminal.write(ANSI.cursorHide);
      this.cursorHidden = true;
    }

    if (!staticOutput && output === this.previousOutput) {
      return;
    }

    let text = '';
    if (frame.length >= this.terminal.rows) {
      // Taller than the viewport: erasing by line count would scroll, so clear and repaint.
      text += ANSI.clearTerminal;
    } else {
      text += ANSI.eraseLines(this.previousLineCount);
    }
    text += staticOutput;
    text += output + '\n';
    this.terminal.write(text);
    this.previousOutput = output;
    this.previousLineCount = frame.length + 1;
  }

  /** Erases the live frame. */
  clear(): void {
    if (this.finished || !this.terminal.isOutputTTY || this.options.debug) {
      return;
    }
    this.terminal.write(ANSI.eraseLines(this.previousLineCount));
    this.previousLineCount = 0;
    this.previousOutput = undefined;
  }

  /** Leaves the last frame on screen and restores the cursor. */
  finish(): void {
    if (this.finished) {
      return;
    }
    this.finished = true;
    if (!this.terminal.isOutputTTY && !this.options.debug && this.lastFrame.length > 0) {
      this.terminal.write(this.lastFrame.join('\n') + '\n');
    }
    if (this.cursorHidden) {
      this.terminal.write(ANSI.cursorShow);
      this.cursorHidden = false;
    }
  }
}
