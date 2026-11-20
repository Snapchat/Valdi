# asset_corpus

Synthetic images that exercise the compiler's image pipeline the way real app modules do. Nothing here
is shipped: the module exists so CI feeds these files through the real compiler on every run.

It sits next to `hello_world` so HelloWorld's CI covers it, but it is not a dependency of the
HelloWorld app (its visibility is private), so none of these assets end up in the app bundle.

Building the module (or `//apps/helloworld/src/valdi/asset_corpus:test`, which `tools/ci/test_modules.sh` runs) makes the compiler:

- render every `.svg` with resvg into each exported variant (iOS `@2x`/`@3x` PNG, Android
  `drawable-*` WebP, web PNG),
- decode every `.png`, resize it into the same variants with image_toolbox, and
- run pngquant over every PNG it writes.

A toolchain change that starts rejecting one of these files fails the build. `test/AssetCorpus.spec.ts`
also checks the size the compiler recorded for each image.

## Files

| File | What it covers |
|------|----------------|
| `svg_gradients.svg` | linear and radial gradients, `gradientUnits`, `spreadMethod`, `gradientTransform`, `href` inheritance |
| `svg_clip_path.svg` | `clipPath` in user space and `objectBoundingBox` units, nested clip paths, `clip-rule` |
| `svg_mask.svg` | luminance and alpha masks, `maskUnits`, `maskContentUnits` |
| `svg_filters.svg` | drop shadow chain, `feColorMatrix`, `feTurbulence` + `feDisplacementMap`, `feMorphology`, CSS `filter` functions |
| `svg_transforms.svg` | nested `translate`/`rotate`/`scale`/`skewX`/`skewY`/`matrix`, `vector-effect` |
| `svg_use_refs.svg` | `<use>` with `href` and `xlink:href`, `<use>` of a group, `<symbol>` with its own viewBox, `currentColor` |
| `svg_embedded_style.svg` | `<style>` with CDATA, class/id/child/attribute/pseudo-class selectors, `@media`, inline `style` |
| `svg_large_viewbox.svg` | a 100000-unit viewBox with a negative origin, drawn at 48x48 |
| `svg_preserve_aspect_ratio.svg` | `slice`, `meet` and `none` on the root and on nested `<svg>` elements |
| `svg_text.svg` | `<text>`/`<tspan>` with generic font families, `text-anchor`, `dy`, `letter-spacing` |
| `png_rgba_alpha@3x.png` | 8-bit RGBA with an alpha gradient |
| `png_gray_alpha@3x.png` | 8-bit grayscale + alpha |
| `png_palette_trns@3x.png` | 8-bit palette with a `tRNS` alpha table |
| `png_palette_4bit@3x.png` | 4-bit palette (sub-byte packed samples) |
| `png_rgb16@3x.png` | 16-bit RGB |
| `png_rgba16@3x.png` | 16-bit RGBA |
| `png_interlaced@3x.png` | Adam7 interlaced RGBA |
| `png_gamma_srgb@3x.png` | `gAMA` and `sRGB` chunks |
| `png_large@3x.png` | 3072x3072, a full-screen background at @3x |
| `nine_patch.9.png` | an Android nine-patch with stretch and padding markers |

## Known gaps

Every file compiles today, but two come out wrong. The build doesn't catch these:

- `svg_text.svg`: the text is dropped. Only the background rect renders, since image_toolbox
  doesn't load any fonts into resvg.
- `nine_patch.9.png`: it's handled as a plain image. The marker border is resized along with the
  rest, and Android gets `nine_patch.9.webp`, which aapt2 treats as a bitmap rather than a nine-patch.
  The generated type also calls it `res.ninePatch9`, while the runtime catalog only splits on
  underscores and stores it as `ninePatch.9`. That's why its spec is pending.

The PNGs are written by `generate_assets.py` (standard library only, so each variant is exactly the
encoding named above). Rerun it after changing the script. The SVGs are hand written.
