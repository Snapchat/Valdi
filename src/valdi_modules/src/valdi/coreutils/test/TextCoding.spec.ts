import { TextDecoder, TextEncoder } from 'coreutils/src/unicode/TextCoding';
import 'jasmine/src/jasmine';

describe('TextDecoder', () => {
  it('can decode utf8', () => {
    const textDecoder = new TextDecoder('utf-8');
    const str = textDecoder.decode(new Uint8Array([0xf0, 0x9f, 0x91, 0x8d, 0xf0, 0x9f, 0x8f, 0xbb]));
    expect(str).toBe('👍🏻');
  });

  it('can decode utf16', () => {
    const textDecoder = new TextDecoder('utf-16');
    const str = textDecoder.decode(new Uint16Array([0xd83d, 0xdc4d, 0xd83c, 0xdffb]));
    expect(str).toBe('👍🏻');
  });

  it('can decode utf32', () => {
    const textDecoder = new TextDecoder('utf-32');
    const str = textDecoder.decode(new Uint32Array([0x1f44d, 0x1f3fb]));
    expect(str).toBe('👍🏻');
  });

  it('is assigned in globalThis', () => {
    expect(globalThis.TextDecoder).toBeDefined();
  });
});

describe('TextEncoder', () => {
  it('can encode utf8', () => {
    const textEncoder = new TextEncoder('utf-8');

    const result = textEncoder.encode('👍🏻');

    expect(result).toEqual(new Uint8Array([0xf0, 0x9f, 0x91, 0x8d, 0xf0, 0x9f, 0x8f, 0xbb]));
  });

  it('can encode utf16', () => {
    const textEncoder = new TextEncoder('utf-16');

    const result = textEncoder.encode('👍🏻');

    expect(new Uint16Array(result.buffer)).toEqual(new Uint16Array([0xd83d, 0xdc4d, 0xd83c, 0xdffb]));
  });

  it('can encode utf32', () => {
    const textEncoder = new TextEncoder('utf-32');

    const result = textEncoder.encode('👍🏻');

    expect(new Uint32Array(result.buffer)).toEqual(new Uint32Array([0x1f44d, 0x1f3fb]));
  });

  it('is assigned in globalThis', () => {
    expect(globalThis.TextDecoder).toBeDefined();
  });
});
