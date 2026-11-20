import 'jasmine/src/jasmine';
import { Asset } from 'valdi_core/src/Asset';
import res from '../res';

// Sizes are in points: SVGs use their width/height attributes, PNGs their pixel size divided by
// the scale in the file name (@3x, or 3 for an unscaled .png).
const expectedSizes: [string, Asset, number, number][] = [
  ['svg_gradients.svg', res.svgGradients, 48, 48],
  ['svg_clip_path.svg', res.svgClipPath, 48, 48],
  ['svg_mask.svg', res.svgMask, 48, 48],
  ['svg_filters.svg', res.svgFilters, 48, 48],
  ['svg_transforms.svg', res.svgTransforms, 48, 48],
  ['svg_use_refs.svg', res.svgUseRefs, 48, 48],
  ['svg_embedded_style.svg', res.svgEmbeddedStyle, 48, 48],
  ['svg_large_viewbox.svg', res.svgLargeViewbox, 48, 48],
  ['svg_preserve_aspect_ratio.svg', res.svgPreserveAspectRatio, 72, 36],
  ['svg_text.svg', res.svgText, 96, 48],
  ['png_rgba_alpha@3x.png', res.pngRgbaAlpha, 16, 16],
  ['png_gray_alpha@3x.png', res.pngGrayAlpha, 16, 16],
  ['png_palette_trns@3x.png', res.pngPaletteTrns, 16, 16],
  ['png_palette_4bit@3x.png', res.pngPalette4bit, 16, 16],
  ['png_rgb16@3x.png', res.pngRgb16, 16, 16],
  ['png_rgba16@3x.png', res.pngRgba16, 16, 16],
  ['png_interlaced@3x.png', res.pngInterlaced, 16, 16],
  ['png_gamma_srgb@3x.png', res.pngGammaSrgb, 16, 16],
  ['png_large@3x.png', res.pngLarge, 1024, 1024],
];

describe('asset_corpus', () => {
  for (const [file, asset, width, height] of expectedSizes) {
    it(`records the size of ${file}`, () => {
      expect(asset).toBeDefined();
      expect(asset.width).toBe(width);
      expect(asset.height).toBe(height);
    });
  }

  it('records the size of nine_patch.9.png', () => {
    // The compiler drops the dot when it types the catalog (ninePatch9), but the runtime catalog
    // only splits on underscores (ninePatch.9), so the typed property is undefined.
    pending('res.ninePatch9 is undefined at runtime');
    expect(res.ninePatch9).toBeDefined();
    expect(res.ninePatch9.width).toBe(10);
    expect(res.ninePatch9.height).toBe(10);
  });
});
