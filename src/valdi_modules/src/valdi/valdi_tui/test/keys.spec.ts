import 'jasmine/src/jasmine';
import { InputParser, parseInput } from '../src/keys';

describe('parseInput', () => {
  it('parses arrows and navigation keys', () => {
    expect(parseInput('\x1b[A')[0].key.upArrow).toBe(true);
    expect(parseInput('\x1b[B')[0].key.downArrow).toBe(true);
    expect(parseInput('\x1bOC')[0].key.rightArrow).toBe(true);
    expect(parseInput('\x1b[D')[0].key.leftArrow).toBe(true);
    expect(parseInput('\x1b[5~')[0].key.pageUp).toBe(true);
    expect(parseInput('\x1b[6~')[0].key.pageDown).toBe(true);
    expect(parseInput('\x1b[3~')[0].key.delete).toBe(true);
    expect(parseInput('\x1b[H')[0].key.home).toBe(true);
    expect(parseInput('\x1b[F')[0].key.end).toBe(true);
  });

  it('parses modifiers on escape sequences', () => {
    const press = parseInput('\x1b[1;5A')[0];
    expect(press.key.upArrow).toBe(true);
    expect(press.key.ctrl).toBe(true);
    const shifted = parseInput('\x1b[1;2B')[0];
    expect(shifted.key.downArrow).toBe(true);
    expect(shifted.key.shift).toBe(true);
  });

  it('parses control characters as ctrl + letter', () => {
    const press = parseInput('\x03')[0];
    expect(press.input).toBe('c');
    expect(press.key.ctrl).toBe(true);
  });

  it('parses return, tab, backspace and escape', () => {
    expect(parseInput('\r')[0].key.return).toBe(true);
    expect(parseInput('\n')[0].key.return).toBe(true);
    expect(parseInput('\t')[0].key.tab).toBe(true);
    expect(parseInput('\x7f')[0].key.backspace).toBe(true);
    expect(parseInput('\x1b')[0].key.escape).toBe(true);
  });

  it('parses back-tab as shift+tab', () => {
    const press = parseInput('\x1b[Z')[0];
    expect(press.key.tab).toBe(true);
    expect(press.key.shift).toBe(true);
    expect(parseInput('\t')[0].key.shift).toBe(false);
  });

  it('parses meta combinations', () => {
    const press = parseInput('\x1bx')[0];
    expect(press.input).toBe('x');
    expect(press.key.meta).toBe(true);
  });

  it('groups printable text and flags uppercase as shift', () => {
    const presses = parseInput('hello');
    expect(presses.length).toBe(1);
    expect(presses[0].input).toBe('hello');
    expect(parseInput('Q')[0].key.shift).toBe(true);
    expect(parseInput('q')[0].key.shift).toBe(false);
  });

  it('splits mixed input into presses', () => {
    const presses = parseInput('ab\x1b[A\r');
    expect(presses.map(press => press.input)).toEqual(['ab', '', '']);
    expect(presses[1].key.upArrow).toBe(true);
    expect(presses[2].key.return).toBe(true);
  });

  it('consumes unmapped CSI and SS3 sequences without typing them', () => {
    const f5 = parseInput('a\x1b[15~b');
    expect(f5.map(press => press.input)).toEqual(['a', 'b']);
    expect(f5.some(press => press.key.meta)).toBe(false);
    expect(parseInput('\x1b[I')).toEqual([]);
    expect(parseInput('\x1b[O')).toEqual([]);
    expect(parseInput('\x1bOP')).toEqual([]);
    expect(parseInput('\x1b[<0;10;5M')).toEqual([]);
    expect(parseInput('\x1b[1;5C')[0].key.rightArrow).toBe(true);
  });

  it('parses Alt with a character outside the BMP as one code point', () => {
    const presses = parseInput('\x1b😀');
    expect(presses.length).toBe(1);
    expect(presses[0].input).toBe('😀');
    expect(presses[0].key.meta).toBe(true);
    expect(presses[0].raw).toBe('\x1b😀');
  });

  it('treats ESC [ and ESC O at the end of complete input as Alt combinations', () => {
    const bracket = parseInput('\x1b[');
    expect(bracket.length).toBe(1);
    expect(bracket[0].input).toBe('[');
    expect(bracket[0].key.meta).toBe(true);
    const letter = parseInput('\x1bO');
    expect(letter[0].input).toBe('O');
    expect(letter[0].key.meta).toBe(true);
  });
});

describe('InputParser', () => {
  it('completes an escape sequence split after ESC', () => {
    const parser = new InputParser();
    expect(parser.push('\x1b')).toEqual([]);
    expect(parser.hasPending).toBe(true);
    const presses = parser.push('[A');
    expect(presses.length).toBe(1);
    expect(presses[0].key.upArrow).toBe(true);
    expect(presses[0].key.escape).toBe(false);
    expect(parser.hasPending).toBe(false);
  });

  it('completes an escape sequence split inside its parameters', () => {
    const parser = new InputParser();
    expect(parser.push('\x1b[')).toEqual([]);
    const presses = parser.push('1;5C');
    expect(presses.length).toBe(1);
    expect(presses[0].key.rightArrow).toBe(true);
    expect(presses[0].key.ctrl).toBe(true);
    expect(presses[0].input).toBe('');
  });

  it('delivers text before a trailing ESC and keeps the ESC', () => {
    const parser = new InputParser();
    const presses = parser.push('ab\x1b');
    expect(presses.map(press => press.input)).toEqual(['ab']);
    expect(parser.hasPending).toBe(true);
  });

  it('turns a lone ESC into Escape when flushed after the timeout', () => {
    const parser = new InputParser();
    expect(parser.push('\x1b')).toEqual([]);
    const presses = parser.flush();
    expect(presses.length).toBe(1);
    expect(presses[0].key.escape).toBe(true);
    expect(parser.hasPending).toBe(false);
    expect(parser.flush()).toEqual([]);
  });

  it('drops an unmapped sequence split across reads', () => {
    const parser = new InputParser();
    expect(parser.push('\x1b[1')).toEqual([]);
    expect(parser.push('5~x').map(press => press.input)).toEqual(['x']);
  });
});
