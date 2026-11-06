import 'jasmine/src/jasmine';
import { ANSI } from '../src/ansi';
import { FrameWriter } from '../src/FrameWriter';
import { MemoryTerminal } from '../src/Terminal';

describe('FrameWriter', () => {
  it('redraws frames in place on a tty', () => {
    const terminal = new MemoryTerminal({ rows: 10 });
    const writer = new FrameWriter(terminal);
    writer.write(['a', 'b'], []);
    expect(terminal.writes).toEqual([ANSI.cursorHide, 'a\nb\n']);
    writer.write(['c'], []);
    expect(terminal.writes[2]).toBe(`${ANSI.eraseLines(3)}c\n`);
    writer.finish();
    expect(terminal.writes[terminal.writes.length - 1]).toBe(ANSI.cursorShow);
  });

  it('skips identical frames', () => {
    const terminal = new MemoryTerminal({ rows: 10 });
    const writer = new FrameWriter(terminal);
    writer.write(['same'], []);
    writer.write(['same'], []);
    expect(terminal.writes.length).toBe(2);
  });

  it('writes static lines above the frame once', () => {
    const terminal = new MemoryTerminal({ rows: 10 });
    const writer = new FrameWriter(terminal);
    writer.write(['live'], ['log 1']);
    expect(terminal.writes[1]).toBe('log 1\nlive\n');
    writer.write(['live'], ['log 2']);
    expect(terminal.writes[2]).toBe(`${ANSI.eraseLines(2)}log 2\nlive\n`);
  });

  it('clears the terminal for frames taller than the viewport', () => {
    const terminal = new MemoryTerminal({ rows: 2 });
    const writer = new FrameWriter(terminal);
    writer.write(['1', '2', '3'], []);
    expect(terminal.writes[1]!.startsWith(ANSI.clearTerminal)).toBe(true);
  });

  it('defers the live frame until finish when stdout is not a tty', () => {
    const terminal = new MemoryTerminal({ isOutputTTY: false });
    const writer = new FrameWriter(terminal);
    writer.write(['first'], ['static']);
    writer.write(['second'], []);
    expect(terminal.output).toBe('static\n');
    writer.finish();
    expect(terminal.output).toBe('static\nsecond\n');
  });
});
