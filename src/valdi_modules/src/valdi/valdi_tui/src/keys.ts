/** Modifier and special-key flags of a key press, same shape as Ink's `Key`. */
export interface Key {
  upArrow: boolean;
  downArrow: boolean;
  leftArrow: boolean;
  rightArrow: boolean;
  pageDown: boolean;
  pageUp: boolean;
  home: boolean;
  end: boolean;
  return: boolean;
  escape: boolean;
  ctrl: boolean;
  shift: boolean;
  tab: boolean;
  backspace: boolean;
  delete: boolean;
  meta: boolean;
}

export interface KeyPress {
  /** Printable characters typed, or the letter for ctrl/meta combinations. Empty for special keys. */
  readonly input: string;
  readonly key: Key;
  /** The exact bytes received for this key press. */
  readonly raw: string;
}

export function emptyKey(): Key {
  return {
    upArrow: false,
    downArrow: false,
    leftArrow: false,
    rightArrow: false,
    pageDown: false,
    pageUp: false,
    home: false,
    end: false,
    return: false,
    escape: false,
    ctrl: false,
    shift: false,
    tab: false,
    backspace: false,
    delete: false,
    meta: false,
  };
}

const ESCAPE = '\x1b';

/**
 * How long a lone ESC, or an escape sequence cut off at the end of a read, waits for the rest before
 * it is taken as typed. Escape and Alt+`[` are prefixes of longer sequences, so only time tells them apart.
 */
export const ESCAPE_TIMEOUT_MS = 50;

// CSI final characters and `~`-terminated numeric codes.
const CSI_KEYS: { readonly [code: string]: keyof Key } = {
  A: 'upArrow',
  B: 'downArrow',
  C: 'rightArrow',
  D: 'leftArrow',
  H: 'home',
  F: 'end',
  Z: 'tab',
  '1~': 'home',
  '7~': 'home',
  '4~': 'end',
  '8~': 'end',
  '3~': 'delete',
  '5~': 'pageUp',
  '6~': 'pageDown',
};

const SS3_KEYS: { readonly [code: string]: keyof Key } = {
  A: 'upArrow',
  B: 'downArrow',
  C: 'rightArrow',
  D: 'leftArrow',
  H: 'home',
  F: 'end',
};

function applyModifier(key: Key, modifier: number): void {
  // xterm encodes modifiers as 1 + bitmask(shift=1, meta=2, ctrl=4).
  const mask = modifier - 1;
  if (mask & 1) {
    key.shift = true;
  }
  if (mask & 2) {
    key.meta = true;
  }
  if (mask & 4) {
    key.ctrl = true;
  }
}

function isParameterByte(code: number): boolean {
  return code >= 0x30 && code <= 0x3f;
}

function isIntermediateByte(code: number): boolean {
  return code >= 0x20 && code <= 0x2f;
}

function isFinalByte(code: number): boolean {
  return code >= 0x40 && code <= 0x7e;
}

type EscapeResult = { readonly press: KeyPress | undefined; readonly end: number } | undefined;

function csiPress(params: string, intermediates: string, final: string, raw: string): KeyPress | undefined {
  if (intermediates !== '' || !/^[0-9;]*$/.test(params)) {
    return undefined;
  }
  const parts = params === '' ? [] : params.split(';');
  const code = final === '~' ? `${parts[0] ?? ''}~` : final;
  const name = CSI_KEYS[code];
  if (!name) {
    return undefined;
  }
  const key = emptyKey();
  key[name] = true;
  if (code === 'Z') {
    // Back-tab is reported as its own final byte, without a modifier parameter.
    key.shift = true;
  }
  const modifier = parseInt((final === '~' ? parts[1] : (parts[1] ?? parts[0])) ?? '1', 10);
  if (!isNaN(modifier) && modifier > 1) {
    applyModifier(key, modifier);
  }
  return { input: '', key, raw };
}

function escapePress(): KeyPress {
  const key = emptyKey();
  key.escape = true;
  return { input: '', key, raw: ESCAPE };
}

/** ESC followed by a character is Alt/Meta + that character. */
function metaPress(start: number, char: string): EscapeResult {
  const inner = parse(char, true).presses[0];
  const key = { ...(inner?.key ?? emptyKey()), meta: true };
  return { press: { input: inner?.input ?? char, key, raw: ESCAPE + char }, end: start + 1 + char.length };
}

/**
 * Parses the escape sequence at `start` (which is ESC). CSI (`ESC [ params intermediates final`) and SS3
 * (`ESC O final`) sequences are always consumed whole; those that are not a known key yield no press.
 * Returns undefined when `data` ends inside the sequence and more input may follow (`atEnd` false).
 */
function parseEscape(data: string, start: number, atEnd: boolean): EscapeResult {
  const introducer = start + 1;
  if (introducer >= data.length) {
    return atEnd ? { press: escapePress(), end: introducer } : undefined;
  }
  const next = data.charAt(introducer);
  if (next === '[') {
    const paramsStart = introducer + 1;
    let index = paramsStart;
    while (index < data.length && isParameterByte(data.charCodeAt(index))) {
      index++;
    }
    const paramsEnd = index;
    while (index < data.length && isIntermediateByte(data.charCodeAt(index))) {
      index++;
    }
    if (index >= data.length && !atEnd) {
      return undefined;
    }
    if (index >= data.length || !isFinalByte(data.charCodeAt(index))) {
      // Cut short by the end of input or by a byte that cannot be part of a CSI sequence. With nothing after
      // the `[` it was Alt+`[`; otherwise the fragment is dropped and parsing resumes at the stray byte.
      return index === paramsStart ? metaPress(start, '[') : { press: undefined, end: index };
    }
    const raw = data.substring(start, index + 1);
    const press = csiPress(
      data.substring(paramsStart, paramsEnd),
      data.substring(paramsEnd, index),
      data.charAt(index),
      raw,
    );
    return { press, end: index + 1 };
  }
  if (next === 'O') {
    const finalIndex = introducer + 1;
    if (finalIndex >= data.length) {
      return atEnd ? metaPress(start, 'O') : undefined;
    }
    const final = data.charAt(finalIndex);
    if (!isFinalByte(data.charCodeAt(finalIndex))) {
      return metaPress(start, 'O');
    }
    const name = SS3_KEYS[final];
    if (!name) {
      return { press: undefined, end: finalIndex + 1 };
    }
    const key = emptyKey();
    key[name] = true;
    return { press: { input: '', key, raw: data.substring(start, finalIndex + 1) }, end: finalIndex + 1 };
  }
  if (next === ESCAPE) {
    return { press: escapePress(), end: introducer };
  }
  return metaPress(start, String.fromCodePoint(data.codePointAt(introducer)!));
}

function parse(data: string, atEnd: boolean): { presses: KeyPress[]; rest: string } {
  const presses: KeyPress[] = [];
  let index = 0;
  let pendingText = '';

  const flushText = (): void => {
    if (pendingText !== '') {
      const key = emptyKey();
      if (pendingText.length === 1 && pendingText !== pendingText.toLowerCase()) {
        key.shift = true;
      }
      presses.push({ input: pendingText, key, raw: pendingText });
      pendingText = '';
    }
  };

  while (index < data.length) {
    const char = data.charAt(index);
    const code = data.charCodeAt(index);

    if (char === ESCAPE) {
      flushText();
      const sequence = parseEscape(data, index, atEnd);
      if (!sequence) {
        return { presses, rest: data.substring(index) };
      }
      if (sequence.press) {
        presses.push(sequence.press);
      }
      index = sequence.end;
      continue;
    }

    if (char === '\r' || char === '\n') {
      flushText();
      const key = emptyKey();
      key.return = true;
      presses.push({ input: '', key, raw: char });
      index++;
      continue;
    }
    if (char === '\t') {
      flushText();
      const key = emptyKey();
      key.tab = true;
      presses.push({ input: '', key, raw: char });
      index++;
      continue;
    }
    if (char === '\x7f' || char === '\b') {
      flushText();
      const key = emptyKey();
      key.backspace = true;
      presses.push({ input: '', key, raw: char });
      index++;
      continue;
    }
    if (code < 0x20) {
      flushText();
      const key = emptyKey();
      key.ctrl = true;
      presses.push({ input: String.fromCharCode(code + 96), key, raw: char });
      index++;
      continue;
    }

    pendingText += char;
    index++;
  }
  flushText();
  return { presses, rest: '' };
}

/**
 * Splits a complete chunk of terminal input into key presses. Consecutive printable characters
 * (for example a paste) form a single press, matching Ink's `useInput`. A trailing lone ESC is Escape.
 * Input that arrives in reads should go through {@link InputParser} instead.
 */
export function parseInput(data: string): KeyPress[] {
  return parse(data, true).presses;
}

/**
 * Parses terminal input that arrives in separate reads. An escape sequence cut off at the end of one read is
 * kept and completed by the next; call {@link flush} once {@link ESCAPE_TIMEOUT_MS} pass without more input
 * so a lone ESC is delivered as Escape.
 */
export class InputParser {
  private pending = '';

  /** Whether part of an escape sequence is waiting for more input or for {@link flush}. */
  get hasPending(): boolean {
    return this.pending !== '';
  }

  push(data: string): KeyPress[] {
    const result = parse(this.pending + data, false);
    this.pending = result.rest;
    return result.presses;
  }

  /** Takes the waiting input as complete: a lone ESC becomes Escape, `ESC [` becomes Alt+`[`. */
  flush(): KeyPress[] {
    const pending = this.pending;
    this.pending = '';
    return pending === '' ? [] : parse(pending, true).presses;
  }
}
