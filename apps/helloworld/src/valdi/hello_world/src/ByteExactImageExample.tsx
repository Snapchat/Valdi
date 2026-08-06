import { Component } from 'valdi_core/src/Component';
import { getModuleFileEntryAsBytes } from 'valdi_core/src/Valdi';
import { systemBoldFont, systemFont } from 'valdi_core/src/SystemFont';
import { decodeBitmap } from 'drawing/src/BitmapFactory';
import { IBitmap } from 'drawing/src/IBitmap';

/**
 * Byte-exact bundled bitmap data (github.com/Snapchat/Valdi#116).
 *
 * Images placed in a module's `res/` folder go through the image-asset pipeline,
 * which re-encodes them into lossy, density-scaled WebP drawables — fine for
 * decorative UI, but it corrupts images used as literal per-pixel data.
 *
 * To bundle an image *byte-exact*, ship it as a `.bin` file in `srcs` (NOT `res/`).
 * `.bin` files are bundled verbatim and are retrievable via `getModuleFileEntryAsBytes`,
 * with zero re-encoding. Naming the file `<name>.<realext>.bin` (e.g. `foo.png.bin`)
 * keeps the real format visible while opting the file out of the image pipeline.
 *
 * The source here is a lossless 2x2 PNG of pure black/white pixels (the kind of hard
 * edges `res/` WebP would smear into grays). We load its exact bytes and decode them.
 */

const WIDTH = 2;
const HEIGHT = 2;

interface Pixel {
  label: string;
  rgba: string;
}

function loadCheckerBitmap(): IBitmap {
  // Path is relative to the module root, matching the file's location in `srcs`.
  // The exact PNG bytes come back untouched — no WebP, no density scaling.
  const bytes = getModuleFileEntryAsBytes('hello_world', 'src/patterns/checker_2x2.png.bin');
  return decodeBitmap(bytes);
}

function readPixels(bitmap: IBitmap): Pixel[] {
  const info = bitmap.getInfo();
  return bitmap.accessPixels((view) => {
    const pixels: Pixel[] = [];
    for (let y = 0; y < info.height; y++) {
      for (let x = 0; x < info.width; x++) {
        const offset = y * info.rowBytes + x * 4;
        const r = view.getUint8(offset);
        const g = view.getUint8(offset + 1);
        const b = view.getUint8(offset + 2);
        const a = view.getUint8(offset + 3);
        pixels.push({ label: `(${x}, ${y})`, rgba: `rgba(${r}, ${g}, ${b}, ${a})` });
      }
    }
    return pixels;
  });
}

export class ByteExactImageExample extends Component {
  private pixels: Pixel[] = [];

  onCreate(): void {
    const bitmap = loadCheckerBitmap();
    try {
      this.pixels = readPixels(bitmap);
    } finally {
      bitmap.dispose();
    }
  }

  onRender(): void {
    <layout padding={24}>
      <label value="Byte-exact bundled bitmap" font={systemBoldFont(16)} />
      <label
        value={`Loaded verbatim from src/patterns/checker_2x2.png.bin (${WIDTH}x${HEIGHT}) — no WebP, no resize`}
        font={systemFont(12)}
        marginBottom={8}
      />
      {this.pixels.map((pixel) => (
        <label value={`${pixel.label}  ->  ${pixel.rgba}`} font={systemFont(12)} />
      ))}
    </layout>;
  }
}
