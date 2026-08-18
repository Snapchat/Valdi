import 'jasmine/src/jasmine';
import { detectImageMimeType, svgViewBoxIntrinsicSize } from '../src/utils/imageSource';

describe('imageSource', () => {
  it('detects the MIME types supported by byte-backed image assets', () => {
    const textEncoder = new TextEncoder();

    expect(detectImageMimeType(new Uint8Array([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]))).toBe(
      'image/png',
    );
    expect(detectImageMimeType(new Uint8Array([0xff, 0xd8, 0xff]))).toBe('image/jpeg');
    expect(detectImageMimeType(textEncoder.encode('GIF89a'))).toBe('image/gif');
    expect(detectImageMimeType(textEncoder.encode('RIFF1234WEBP'))).toBe('image/webp');
    expect(detectImageMimeType(textEncoder.encode(' \n<svg viewBox="0 0 24 24"></svg>'))).toBe('image/svg+xml');
    expect(detectImageMimeType(textEncoder.encode('<?xml version="1.0"?><svg viewBox="0 0 24 24"></svg>'))).toBe(
      'image/svg+xml',
    );
    expect(detectImageMimeType(textEncoder.encode('{"value":1}'))).toBe('application/octet-stream');
  });

  it('reads SVG viewBox intrinsic size from text data URLs', () => {
    const svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0, 0, 120, 80"></svg>';

    expect(svgViewBoxIntrinsicSize(`data:image/svg+xml,${encodeURIComponent(svg)}`)).toEqual({
      width: 120,
      height: 80,
    });
  });

  it('rejects missing or invalid SVG viewBox data', () => {
    expect(svgViewBoxIntrinsicSize('asset.svg')).toBeUndefined();
    expect(svgViewBoxIntrinsicSize('data:image/svg+xml,<svg viewBox="0 0 0 80"></svg>')).toBeUndefined();
  });
});
