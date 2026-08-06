import { decodeFromBase64, encodeToBase64 } from '../Base64Native';
import { describe, expect, it } from './jest_globals';

const helloBytes = new Uint8Array([104, 101, 108, 108, 111]);

// Bytes whose standard base64 encoding contains both '+' and '/', so the url-safe
// variant has to substitute both.
const symbolBytes = new Uint8Array([0xfb, 0xff, 0xbf]);

describe('web Base64Native', () => {
  it('round trips through standard base64', () => {
    const encoded = encodeToBase64(helloBytes, false);
    expect(encoded).toBe('aGVsbG8=');
    expect(Array.from(decodeFromBase64(encoded))).toEqual(Array.from(helloBytes));
  });

  it('round trips through url-safe base64', () => {
    const encoded = encodeToBase64(symbolBytes, true);
    expect(encoded).toBe('-_-_');
    expect(Array.from(decodeFromBase64(encoded))).toEqual(Array.from(symbolBytes));
  });

  it('strips padding when url-safe', () => {
    expect(encodeToBase64(helloBytes, true)).toBe('aGVsbG8');
  });

  it('decodes input whose padding was stripped', () => {
    expect(Array.from(decodeFromBase64('aGVsbG8'))).toEqual(Array.from(helloBytes));
  });

  it('round trips empty input', () => {
    expect(encodeToBase64(new Uint8Array(), false)).toBe('');
    expect(Array.from(decodeFromBase64(''))).toEqual([]);
  });

  // atob throws a DOMException; callers and Base64.spec.ts expect the same message the
  // native implementations produce.
  it('reports invalid input with the shared error message', () => {
    expect(() => decodeFromBase64('Z')).toThrow('Invalid base64 string');
    expect(() => decodeFromBase64('Zg!')).toThrow('Invalid base64 string');
    expect(() => decodeFromBase64('Z=g=')).toThrow('Invalid base64 string');
  });
});
