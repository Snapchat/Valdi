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

// CSI final characters and `~`-terminated numeric codes.
const CSI_KEYS: { readonly [code: string]: keyof Key } = {
  A: 'upArrow',
  B: 'downArrow',
  C: 'rightArrow',
  D: 'leftArrow',
  H: 'home',
  F: 'end',
  '1~': 'home',
  '7~': 'home',
  '4~': 'end',
  '8~': 'end',
  '3~': 'delete',
  '5~': 'pageUp',
  '6~': 'pageDown',
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

/**
 * Parses an escape sequence starting at `start` (which is ESC). Returns the key press and the
 * index after the sequence, or undefined when the bytes are not a recognized sequence.
 */
function parseEscapeSequence(data: string, start: number): [KeyPress, number] | undefined {
  const key = emptyKey();
  const next = data.charAt(start + 1);
  if (next === '[' || next === 'O') {
    let index = start + 2;
    let params = '';
    while (index < data.length && /[0-9;]/.test(data.charAt(index))) {
      params += data.charAt(index);
      index++;
    }
    if (index >= data.length) {
      return undefined;
    }
    const final = data.charAt(index);
    index++;
    const parts = params.split(';');
    let code: string;
    if (final === '~') {
      code = `${parts[0] ?? ''}~`;
    } else {
      code = final;
    }
    const name = CSI_KEYS[code];
    if (!name) {
      return undefined;
    }
    key[name] = true;
    const modifier = final === '~' ? parseInt(parts[1] ?? '1', 10) : parseInt(parts[1] ?? parts[0] ?? '1', 10);
    if (!isNaN(modifier) && modifier > 1) {
      applyModifier(key, modifier);
    }
    return [{ input: '', key, raw: data.substring(start, index) }, index];
  }
  return undefined;
}

/**
 * Splits a chunk of terminal input into key presses. Consecutive printable characters
 * (for example a paste) form a single press, matching Ink's `useInput`.
 */
export function parseInput(data: string): KeyPress[] {
  const presses: KeyPress[] = [];
  let index = 0;
  let pendingText = '';

  const flushText = () => {
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
      const sequence = parseEscapeSequence(data, index);
      if (sequence) {
        presses.push(sequence[0]);
        index = sequence[1];
        continue;
      }
      const next = data.charAt(index + 1);
      if (index + 1 < data.length && next !== ESCAPE) {
        // ESC followed by a character is Alt/Meta + character.
        const key = emptyKey();
        key.meta = true;
        const inner = parseInput(next);
        const innerKey = inner[0]?.key ?? emptyKey();
        Object.assign(key, { ...innerKey, meta: true });
        presses.push({ input: inner[0]?.input ?? next, key, raw: data.substring(index, index + 2) });
        index += 2;
        continue;
      }
      const key = emptyKey();
      key.escape = true;
      presses.push({ input: '', key, raw: ESCAPE });
      index++;
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
  return presses;
}
