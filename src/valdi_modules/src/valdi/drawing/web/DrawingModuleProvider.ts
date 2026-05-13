import { FontWeight, DrawingModule, FontSpecs, Font, Size, FontStyle, DrawingModuleProvider } from "../src/DrawingModuleProvider";

const registered = new Set<string>();

function getCanvas(): CanvasRenderingContext2D {
  const canvas = document.createElement('canvas');
  const ctx = canvas.getContext('2d');
  if (!ctx) throw new Error('2D canvas not available');
  return ctx;
}

function cssWeight(w: FontWeight): string {
  switch (w) {
    case FontWeight.LIGHT: return '300';
    case FontWeight.NORMAL: return '400';
    case FontWeight.MEDIUM: return '500';
    case FontWeight.DEMI_BOLD: return '600';
    case FontWeight.BOLD: return '700';
    case FontWeight.BLACK: return '900';
    default: return '400';
  }
}

const Drawing: DrawingModule = {
  getFont(specs: FontSpecs): Font {
    const ctx = getCanvas();
    ctx.font = specs.font; // Caller supplies a full CSS font string

    const defaultLineHeight = (() => {
      // crude estimate if not provided: 1.2 * font-size
      const m = /(\d+(?:\.\d+)?)px/.exec(specs.font);
      return m ? Math.ceil(parseFloat(m[1]) * 1.2) : 16;
    })();
    const lineHeight = specs.lineHeight ?? defaultLineHeight;

    return {
      measureText(text: string, maxWidth?: number, maxHeight?: number, maxLines?: number): Size {
        // naive single-line; clamp to maxWidth/Height if given
        const metrics = ctx.measureText(text);
        let width = Math.ceil(metrics.width);
        let height = lineHeight;
        if (maxWidth != null) width = Math.min(width, Math.floor(maxWidth));
        if (maxHeight != null) height = Math.min(height, Math.floor(maxHeight));
        // ignore wrapping/maxLines in this stub
        return { width, height };
      },
      measureAttributedText(attributedText: any, maxWidth?: number, maxHeight?: number, maxLines?: number): Size {
        // TODO: Implement this properly. For now just measure the plain text.
        const plainText = attributedText?.text || '';
        return this.measureText(plainText, maxWidth, maxHeight, maxLines);
      },
    };
  },

  isFontRegistered(fontName: string): boolean {
    return registered.has(fontName);
  },

  registerFont(fontName: string, weight: FontWeight, style: FontStyle, _filename: string): void {
    // Stub: record the logical registration; real loading is left to the app.
    // You could add a CSSFontFaceSet load() here if desired.
    const key = `${fontName}:${cssWeight(weight)}:${style}`;
    registered.add(key);
  },
};

const provider: DrawingModuleProvider = { Drawing };
export = provider;