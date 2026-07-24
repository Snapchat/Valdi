import { Base64 } from 'coreutils/src/Base64';
import 'jasmine/src/jasmine';

describe('coreutils > Base64', () => {
  // Byte array for '<<???>>'
  const byteArray = new Uint8Array([60, 60, 63, 63, 63, 62, 62]);

  function expectToThrowMessage(fn: () => void, message: string) {
    let thrownMessage: string | undefined;
    try {
      fn();
    } catch (error) {
      thrownMessage = error instanceof Error ? error.message : String(error);
    }
    expect(thrownMessage).toEqual(message);
  }

  it('should Base64 encode correctly', () => {
    expect(Base64.fromByteArray(byteArray)).toEqual('PDw/Pz8+Pg==');
  });

  it('should url-safe Base64 encode correctly', () => {
    expect(Base64.fromByteArray(byteArray, { urlSafe: true })).toEqual('PDw_Pz8-Pg');
  });

  it('should Base64 decode correctly with padding', () => {
    expect(Base64.toByteArray('PDw/Pz8+Pg==')).toEqual(byteArray);
  });

  it('should Base64 decode correctly without padding', () => {
    expect(Base64.toByteArray('PDw/Pz8+Pg')).toEqual(byteArray);
  });

  it('should url-safe Base64 decode correctly without padding', () => {
    expect(Base64.toByteArray('PDw_Pz8-Pg')).toEqual(byteArray);
  });

  it('should url-safe Base64 decode correctly with padding', () => {
    expect(Base64.toByteArray('PDw_Pz8-Pg=')).toEqual(byteArray);
  });

  it('should calculate byte length correctly without padding', () => {
    expect(Base64.byteLength('PDw/Pz8+Pg')).toEqual(byteArray.length);
  });

  it('should encode padding boundaries correctly', () => {
    expect(Base64.fromByteArray(new Uint8Array([]))).toEqual('');
    expect(Base64.fromByteArray(new Uint8Array([102]))).toEqual('Zg==');
    expect(Base64.fromByteArray(new Uint8Array([102, 111]))).toEqual('Zm8=');
    expect(Base64.fromByteArray(new Uint8Array([102, 111, 111]))).toEqual('Zm9v');
  });

  it('should decode padding boundaries correctly', () => {
    expect(Base64.toByteArray('')).toEqual(new Uint8Array([]));
    expect(Base64.toByteArray('Zg==')).toEqual(new Uint8Array([102]));
    expect(Base64.toByteArray('Zm8=')).toEqual(new Uint8Array([102, 111]));
    expect(Base64.toByteArray('Zm9v')).toEqual(new Uint8Array([102, 111, 111]));
  });

  it('should calculate byte length for padded and unpadded boundaries', () => {
    expect(Base64.byteLength('')).toEqual(0);
    expect(Base64.byteLength('Zg==')).toEqual(1);
    expect(Base64.byteLength('Zg')).toEqual(1);
    expect(Base64.byteLength('Zm8=')).toEqual(2);
    expect(Base64.byteLength('Zm8')).toEqual(2);
    expect(Base64.byteLength('Zm9v')).toEqual(3);
    expect(Base64.byteLength('Zm9vYg')).toEqual(4);
    expect(Base64.byteLength('Zm9vYmE')).toEqual(5);
    expect(Base64.byteLength('Zm9vYmFy')).toEqual(6);
  });

  it('should calculate byte length for URL-safe strings', () => {
    expect(Base64.byteLength('_P-_')).toEqual(3);
    expect(Base64.byteLength('PDw_Pz8-Pg')).toEqual(byteArray.length);
  });

  it('should round trip arbitrary binary bytes', () => {
    const bytes = new Uint8Array([0, 1, 2, 3, 250, 251, 252, 253, 254, 255]);
    expect(Base64.toByteArray(Base64.fromByteArray(bytes))).toEqual(bytes);
  });

  it('should encode and decode URL-safe binary bytes', () => {
    const bytes = new Uint8Array([252, 255, 191]);
    const encoded = Base64.fromByteArray(bytes, { urlSafe: true });
    expect(encoded).toEqual('_P-_');
    expect(Base64.toByteArray(encoded)).toEqual(bytes);
    expect(Base64.byteLength(encoded)).toEqual(bytes.length);
  });

  it('should decode Base64 with newlines', () => {
    expect(Base64.toByteArray('PDw/\nPz8+\r\nPg==')).toEqual(byteArray);
    expect(Base64.toByteArray('PDw_\nPz8-\r\nPg')).toEqual(byteArray);
  });

  it('should encode standard characters that become URL-safe replacements', () => {
    const bytes = new Uint8Array([252, 255, 191]);
    expect(Base64.fromByteArray(bytes)).toEqual('/P+/');
    expect(Base64.fromByteArray(bytes, { urlSafe: true })).toEqual('_P-_');
  });

  it('should round trip a larger payload', () => {
    const bytes = new Uint8Array(100000);
    for (let i = 0; i < bytes.length; i++) {
      bytes[i] = (i * 31 + 17) & 0xff;
    }

    const encoded = Base64.fromByteArray(bytes);
    expect(Base64.byteLength(encoded)).toEqual(bytes.length);
    expect(Base64.toByteArray(encoded)).toEqual(bytes);

    const urlSafeEncoded = Base64.fromByteArray(bytes, { urlSafe: true });
    expect(Base64.byteLength(urlSafeEncoded)).toEqual(bytes.length);
    expect(Base64.toByteArray(urlSafeEncoded)).toEqual(bytes);
  });

  it('should reject invalid Base64', () => {
    expectToThrowMessage(() => Base64.toByteArray('Z'), 'Invalid base64 string');
    expectToThrowMessage(() => Base64.toByteArray('Zg!'), 'Invalid base64 string');
    expectToThrowMessage(() => Base64.toByteArray('Z=g='), 'Invalid base64 string');
    expectToThrowMessage(() => Base64.byteLength('Z'), 'Invalid string. Length must be a valid Base64 length');
  });
});
