import 'jasmine/src/jasmine';
import { WebValdiImage } from '../src/views/WebValdiImage';

// Minimal DOM stubs needed to instantiate WebValdiImage outside a browser

function makeCanvas(rectWidth = 0, rectHeight = 0) {
  let canvasWidth = 0;
  let canvasHeight = 0;
  let transform = { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 };

  const ctx = {
    clearRect: () => {},
    save: () => {},
    restore: () => {},
    scale: () => {},
    translate: () => {},
    rotate: () => {},
    drawImage: jasmine.createSpy('drawImage'),
    getImageData: () => ({ data: new Uint8ClampedArray(0) }),
    putImageData: () => {},
    setTransform: (a: number, _b: number, _c: number, d: number) => {
      transform = { a, b: 0, c: 0, d, e: 0, f: 0 };
    },
    _getTransform: () => transform,
  };

  const canvas = {
    getContext: () => ctx,
    getBoundingClientRect: () => ({ width: rectWidth, height: rectHeight }),
    addEventListener: () => {},
    removeEventListener: () => {},
    style: {} as Record<string, string>,
    get width() { return canvasWidth; },
    set width(v: number) { canvasWidth = v; },
    get height() { return canvasHeight; },
    set height(v: number) { canvasHeight = v; },
  };

  return { canvas, ctx };
}

// Stub global DOM APIs used by WebValdiImage
function installDomStubs(canvasRectW = 0, canvasRectH = 0) {
  const { canvas, ctx } = makeCanvas(canvasRectW, canvasRectH);

  (globalThis as any).document = {
    createElement: () => canvas,
    dir: 'ltr',
  };

  (globalThis as any).window = { devicePixelRatio: 1 };

  (globalThis as any).IntersectionObserver = function () {
    return { observe: () => {}, unobserve: () => {}, disconnect: () => {} };
  };

  // Stub Image constructor
  (globalThis as any).Image = function () {
    return {
      crossOrigin: '',
      naturalWidth: 0,
      naturalHeight: 0,
      src: '',
      onload: null as (() => void) | null,
    };
  };

  return { canvas, ctx };
}

function uninstallDomStubs() {
  delete (globalThis as any).document;
  delete (globalThis as any).window;
  delete (globalThis as any).Image;
  delete (globalThis as any).IntersectionObserver;
}

function makeImage(canvasRectW = 0, canvasRectH = 0) {
  const { canvas, ctx } = installDomStubs(canvasRectW, canvasRectH);
  const img = new WebValdiImage(1);
  return { img, canvas, ctx };
}

// Helper: simulate an image load with given natural dimensions
function triggerLoad(img: any, naturalWidth: number, naturalHeight: number) {
  img.img.naturalWidth = naturalWidth;
  img.img.naturalHeight = naturalHeight;
  img.img.onload?.();
}

// Helper: get the drawImage call args (centered draw uses -w/2, -h/2 offsets)
function getDrawnSize(ctx: any): { w: number; h: number } {
  expect(ctx.drawImage).toHaveBeenCalled();
  const args = ctx.drawImage.calls.mostRecent().args;
  // drawImage(_img, x, y, w, h) → w at index 3, h at index 4
  return { w: args[3], h: args[4] };
}

describe('WebValdiImage – 3x asset handling', () => {
  afterEach(() => uninstallDomStubs());

  const SCALE = 3;
  const naturalW = 300; // 3x asset → 100 logical px
  const naturalH = 150; // 3x asset → 50 logical px
  const logicalW = naturalW / SCALE; // 100
  const logicalH = naturalH / SCALE; // 50

  describe('objectFit: none', () => {
    it('draws at logical size, not natural size', () => {
      const { img, ctx } = makeImage(200, 200);
      triggerLoad(img, naturalW, naturalH);
      img.changeAttribute('objectFit', 'none');

      const { w, h } = getDrawnSize(ctx);
      expect(w).toBeCloseTo(logicalW);
      expect(h).toBeCloseTo(logicalH);
    });
  });

  describe('objectFit: scale-down', () => {
    it('uses logical size when image fits within canvas', () => {
      // Canvas is larger than the logical image → scale-down behaves like none
      const { img, ctx } = makeImage(400, 400);
      triggerLoad(img, naturalW, naturalH);
      img.changeAttribute('objectFit', 'scale-down');

      const { w, h } = getDrawnSize(ctx);
      expect(w).toBeCloseTo(logicalW);
      expect(h).toBeCloseTo(logicalH);
    });

    it('scales down when canvas is smaller than logical image', () => {
      // Canvas 50×50 is smaller than logical 100×50 → should scale down
      const { img, ctx } = makeImage(50, 50);
      triggerLoad(img, naturalW, naturalH);
      img.changeAttribute('objectFit', 'scale-down');

      const { w, h } = getDrawnSize(ctx);
      // scale = min(50/100, 50/50) = 0.5 → dw=50, dh=25
      expect(w).toBeCloseTo(50);
      expect(h).toBeCloseTo(25);
    });
  });

  describe('objectFit: contain', () => {
    it('fits logical dimensions into canvas preserving aspect ratio', () => {
      // Canvas 200×200, logical image 100×50 → scale = min(200/100, 200/50) = 2 → dw=200, dh=100
      const { img, ctx } = makeImage(200, 200);
      triggerLoad(img, naturalW, naturalH);
      img.changeAttribute('objectFit', 'contain');

      const { w, h } = getDrawnSize(ctx);
      expect(w).toBeCloseTo(200);
      expect(h).toBeCloseTo(100);
    });
  });

  describe('objectFit: cover', () => {
    it('covers canvas using logical dimensions', () => {
      // Canvas 200×200, logical image 100×50 → scale = max(200/100, 200/50) = 4 → dw=400, dh=200
      const { img, ctx } = makeImage(200, 200);
      triggerLoad(img, naturalW, naturalH);
      img.changeAttribute('objectFit', 'cover');

      const { w, h } = getDrawnSize(ctx);
      expect(w).toBeCloseTo(400);
      expect(h).toBeCloseTo(200);
    });
  });

  describe('onAssetLoad callback', () => {
    it('reports logical dimensions, not natural dimensions', () => {
      const { img } = makeImage();
      let reportedW = -1;
      let reportedH = -1;
      img.changeAttribute('onAssetLoad', (e: { width: number; height: number }) => {
        reportedW = e.width;
        reportedH = e.height;
      });
      triggerLoad(img, naturalW, naturalH);

      expect(reportedW).toBeCloseTo(logicalW);
      expect(reportedH).toBeCloseTo(logicalH);
    });
  });

  describe('auto-sizing (no explicit width/height)', () => {
    it('sets element style to logical dimensions', () => {
      const { img, canvas } = makeImage();
      triggerLoad(img, naturalW, naturalH);

      expect(canvas.style['width']).toBe(`${logicalW}px`);
      expect(canvas.style['height']).toBe(`${logicalH}px`);
    });
  });
});
