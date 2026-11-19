import 'jasmine/src/jasmine';
import { diffEncodedImages } from '../src/ImageDiffNative';
import { createRgbaBitmap, encodePng } from '../src/png';

describe('ImageDiffNative', () => {
  it('compares two identical one-pixel images', () => {
    const png = encodePng(createRgbaBitmap(1, 1, new Uint8Array([255, 0, 0, 255])));
    const diff = diffEncodedImages(png, png, 0);
    expect(diff.totalPixels).toBe(1);
    expect(diff.changedPixels).toBe(0);
  });
});
