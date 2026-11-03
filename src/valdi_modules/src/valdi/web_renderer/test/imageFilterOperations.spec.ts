import 'jasmine/src/jasmine';
import {
  applyColorMatrixToImageData,
  applyTintToImageData,
  parseImageFilterOperations,
} from '../src/utils/imageFilterOperations';

describe('imageFilterOperations', () => {
  it('parses serialized blur and color matrix operations', () => {
    const identityMatrix = [1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0];

    expect(parseImageFilterOperations([1, 3, 2, ...identityMatrix])).toEqual([
      { type: 'blur', radius: 3 },
      { type: 'colorMatrix', matrix: identityMatrix },
    ]);
    expect(parseImageFilterOperations('1,3')).toEqual([{ type: 'blur', radius: 3 }]);
    expect(parseImageFilterOperations('2,1,2')).toBeUndefined();
  });

  it('applies tint and color matrix operations to image data', () => {
    const imageData = {
      data: new Uint8ClampedArray([10, 20, 30, 255, 4, 5, 6, 0]),
    } as ImageData;

    applyTintToImageData(imageData, { r: 100, g: 120, b: 140, a: 0.5 });

    expect(Array.from(imageData.data)).toEqual([100, 120, 140, 128, 4, 5, 6, 0]);

    applyColorMatrixToImageData(imageData, [0, 0, 0, 0, 1, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0]);

    expect(Array.from(imageData.data)).toEqual([255, 128, 0, 128, 255, 128, 0, 0]);
  });
});
