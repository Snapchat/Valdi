import 'jasmine/src/jasmine';
import { WEB_IMAGE_NATURAL_SCALE, getDecodedImageSize } from '../src/elements/CanvasImageRenderer';

describe('CanvasImageRenderer', () => {
  it('reports logical SVG overrides using the web natural scale', () => {
    const image = { naturalWidth: 30, naturalHeight: 15 } as HTMLImageElement;

    expect(getDecodedImageSize(image, 120, 80)).toEqual({
      width: 120 * WEB_IMAGE_NATURAL_SCALE,
      height: 80 * WEB_IMAGE_NATURAL_SCALE,
    });
    expect(getDecodedImageSize(image, undefined, undefined)).toEqual({ width: 30, height: 15 });
  });
});
