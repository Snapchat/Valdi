const chunkSize = 0x8000;

function bytesToBinaryString(uint8: Uint8Array): string {
  const parts: string[] = [];

  for (let i = 0; i < uint8.length; i += chunkSize) {
    const chunk = uint8.subarray(i, i + chunkSize);
    let part = '';
    for (let j = 0; j < chunk.length; j++) {
      part += String.fromCharCode(chunk[j]);
    }
    parts.push(part);
  }

  return parts.join('');
}

function binaryStringToBytes(binary: string): Uint8Array {
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) {
    bytes[i] = binary.charCodeAt(i);
  }
  return bytes;
}

function normalizeBase64Input(base64: string): string {
  const standardBase64 = base64.replace(/-/g, '+').replace(/_/g, '/');
  const missingPadding = standardBase64.length % 4;

  if (missingPadding === 0) {
    return standardBase64;
  }

  return standardBase64 + '='.repeat(4 - missingPadding);
}

export function encodeToBase64(uint8: Uint8Array, urlSafe: boolean): string {
  const base64String = btoa(bytesToBinaryString(uint8));

  if (urlSafe) {
    return base64String.replace(/\+/g, '-').replace(/\//g, '_').replace(/=/g, '');
  }

  return base64String;
}

export function decodeFromBase64(base64: string): Uint8Array {
  return binaryStringToBytes(atob(normalizeBase64Input(base64)));
}
