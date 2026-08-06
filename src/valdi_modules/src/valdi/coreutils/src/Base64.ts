import { decodeFromBase64, encodeToBase64 } from './Base64Native';

export namespace Base64 {
  interface Base64Options {
    urlSafe?: boolean;
  }

  export function fromByteArray(uint8: Uint8Array, { urlSafe }: Base64Options = {}): string {
    return encodeToBase64(uint8, urlSafe === true);
  }

  export function toByteArray(b64: string): Uint8Array {
    return decodeFromBase64(b64);
  }

  // base64 is 4/3 + up to two characters of the original data
  export function byteLength(b64: string): number {
    let validLen = b64.indexOf('=');
    if (validLen === -1) validLen = b64.length;

    if (validLen % 4 === 1) {
      throw new Error('Invalid string. Length must be a valid Base64 length');
    }

    const placeHoldersLen = validLen === b64.length ? (4 - (validLen % 4)) % 4 : 4 - (validLen % 4);
    return ((validLen + placeHoldersLen) * 3) / 4 - placeHoldersLen;
  }
}
