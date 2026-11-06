import 'jasmine/src/jasmine';
import { parseInput } from '../src/keys';

describe('parseInput', () => {
  it('parses arrows and navigation keys', () => {
    expect(parseInput('\x1b[A')[0]!.key.upArrow).toBe(true);
    expect(parseInput('\x1b[B')[0]!.key.downArrow).toBe(true);
    expect(parseInput('\x1bOC')[0]!.key.rightArrow).toBe(true);
    expect(parseInput('\x1b[D')[0]!.key.leftArrow).toBe(true);
    expect(parseInput('\x1b[5~')[0]!.key.pageUp).toBe(true);
    expect(parseInput('\x1b[6~')[0]!.key.pageDown).toBe(true);
    expect(parseInput('\x1b[3~')[0]!.key.delete).toBe(true);
    expect(parseInput('\x1b[H')[0]!.key.home).toBe(true);
    expect(parseInput('\x1b[F')[0]!.key.end).toBe(true);
  });

  it('parses modifiers on escape sequences', () => {
    const press = parseInput('\x1b[1;5A')[0]!;
    expect(press.key.upArrow).toBe(true);
    expect(press.key.ctrl).toBe(true);
    const shifted = parseInput('\x1b[1;2B')[0]!;
    expect(shifted.key.downArrow).toBe(true);
    expect(shifted.key.shift).toBe(true);
  });

  it('parses control characters as ctrl + letter', () => {
    const press = parseInput('\x03')[0]!;
    expect(press.input).toBe('c');
    expect(press.key.ctrl).toBe(true);
  });

  it('parses return, tab, backspace and escape', () => {
    expect(parseInput('\r')[0]!.key.return).toBe(true);
    expect(parseInput('\n')[0]!.key.return).toBe(true);
    expect(parseInput('\t')[0]!.key.tab).toBe(true);
    expect(parseInput('\x7f')[0]!.key.backspace).toBe(true);
    expect(parseInput('\x1b')[0]!.key.escape).toBe(true);
  });

  it('parses meta combinations', () => {
    const press = parseInput('\x1bx')[0]!;
    expect(press.input).toBe('x');
    expect(press.key.meta).toBe(true);
  });

  it('groups printable text and flags uppercase as shift', () => {
    const presses = parseInput('hello');
    expect(presses.length).toBe(1);
    expect(presses[0]!.input).toBe('hello');
    expect(parseInput('Q')[0]!.key.shift).toBe(true);
    expect(parseInput('q')[0]!.key.shift).toBe(false);
  });

  it('splits mixed input into presses', () => {
    const presses = parseInput('ab\x1b[A\r');
    expect(presses.map(press => press.input)).toEqual(['ab', '', '']);
    expect(presses[1]!.key.upArrow).toBe(true);
    expect(presses[2]!.key.return).toBe(true);
  });
});
