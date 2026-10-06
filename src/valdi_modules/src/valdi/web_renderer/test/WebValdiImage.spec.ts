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

  // Stub Image constructor. `src` counts assignments and backs getAttribute the way the DOM does,
  // so a spec can tell "already holds this URL" from "requested it again".
  (globalThis as any).Image = function () {
    let src = '';
    let srcAssignments = 0;
    return {
      crossOrigin: '',
      naturalWidth: 0,
      naturalHeight: 0,
      get src() {
        return src;
      },
      set src(value: string) {
        src = value;
        srcAssignments += 1;
      },
      get srcAssignments() {
        return srcAssignments;
      },
      getAttribute: function (name: string) {
        return name === 'src' ? src || null : null;
      },
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

describe('WebValdiImage – src resolution', () => {
  afterEach(() => uninstallDomStubs());

  it('resolves a plain string URL', () => {
    const { img } = makeImage();
    img.changeAttribute('src', 'https://example.com/photo.png');
    expect(img.img.src).toBe('https://example.com/photo.png');
  });

  it('resolves an object with src field (getAssets catalog shape)', () => {
    const { img } = makeImage();
    img.changeAttribute('src', { path: 'logo', src: 'data:image/png;base64,abc' });
    expect(img.img.src).toBe('data:image/png;base64,abc');
  });

  it('resolves a nested src object', () => {
    const { img } = makeImage();
    img.changeAttribute('src', { src: { src: 'https://example.com/nested.png' } });
    expect(img.img.src).toBe('https://example.com/nested.png');
  });

  it('resolves a makeAssetFromUrl-shaped object (path + src + width + height)', () => {
    const { img } = makeImage();
    img.changeAttribute('src', { path: 'https://example.com/photo.png', src: 'https://example.com/photo.png', width: 100, height: 100 });
    expect(img.img.src).toBe('https://example.com/photo.png');
  });

  it('does NOT set src when given undefined', () => {
    const { img } = makeImage();
    img.changeAttribute('src', undefined);
    expect(img.img.src).toBe('');
  });

  it('does NOT set src when given an object with no src field', () => {
    const { img } = makeImage();
    img.changeAttribute('src', { path: 'logo', width: 100, height: 100 });
    expect(img.img.src).toBe('');
  });
});

describe('WebValdiImage – error handling', () => {
  afterEach(() => uninstallDomStubs());

  it('calls onAssetLoad with zero dimensions on error', () => {
    const { img } = makeImage();
    let reportedW = -1;
    let reportedH = -1;
    img.changeAttribute('onAssetLoad', (e: { width: number; height: number }) => {
      reportedW = e.width;
      reportedH = e.height;
    });
    img.img.onerror?.({} as any);

    expect(reportedW).toBe(0);
    expect(reportedH).toBe(0);
  });

  it('sets onerror handler on the internal img element', () => {
    const { img } = makeImage();
    expect(img.img.onerror).not.toBeNull();
  });
});

describe('WebValdiImage – CORS mode', () => {
  afterEach(() => uninstallDomStubs());

  const URL_A = 'https://example.com/3/screenshot.webp';
  const URL_B = 'https://example.com/3/other.webp';

  function failOnce(img: any) {
    img.img.onerror?.({} as any);
  }

  it('requests in CORS mode so the canvas stays exportable', () => {
    const { img } = makeImage();
    img.changeAttribute('src', URL_A);

    expect(img.img.crossOrigin).toBe('anonymous');
    expect(img.img.src).toBe(URL_A);
  });

  it('retries without CORS when the CORS request fails, instead of dropping the image', () => {
    const { img } = makeImage();
    let reported = false;
    img.changeAttribute('onAssetLoad', () => {
      reported = true;
    });
    img.changeAttribute('src', URL_A);
    failOnce(img);

    expect(img.img.crossOrigin).toBeFalsy();
    expect(img.img.src).toBe(URL_A);
    // The retry is still in flight, so no zero-size load has been reported yet.
    expect(reported).toBe(false);
  });

  it('reports the failure when the non-CORS retry fails too', () => {
    const { img } = makeImage();
    let reportedW = -1;
    let reportedH = -1;
    img.changeAttribute('onAssetLoad', (e: { width: number; height: number }) => {
      reportedW = e.width;
      reportedH = e.height;
    });
    img.changeAttribute('src', URL_A);
    failOnce(img);
    failOnce(img);

    expect(reportedW).toBe(0);
    expect(reportedH).toBe(0);
  });

  it('gives a different URL its own CORS attempt after a fallback', () => {
    const { img } = makeImage();
    img.changeAttribute('src', URL_A);
    failOnce(img);
    img.changeAttribute('src', URL_B);

    expect(img.img.crossOrigin).toBe('anonymous');
    expect(img.img.src).toBe(URL_B);
  });

  it('drops CORS again when a reused view returns to a URL that already fell back', () => {
    // Without clearing it, 'anonymous' leaks in from URL_B, URL_A is requested with CORS a second
    // time, and onerror has no retry left for it — the image never loads again.
    const { img } = makeImage();
    img.changeAttribute('src', URL_A);
    failOnce(img);
    img.changeAttribute('src', URL_B);
    img.changeAttribute('src', URL_A);

    expect(img.img.crossOrigin).toBeFalsy();
    expect(img.img.src).toBe(URL_A);
  });

  it('gives a URL CORS again when the retry failed too, so a blip does not latch', () => {
    // Both attempts failing means the error was a 404 or a dropped connection,
    // not a CORS refusal. Keeping the downgrade would taint the canvas for the
    // life of the view once the host recovers.
    const { img } = makeImage();
    img.changeAttribute('src', URL_A);
    failOnce(img);
    failOnce(img);
    img.changeAttribute('src', URL_B);
    img.changeAttribute('src', URL_A);

    expect(img.img.crossOrigin).toBe('anonymous');
  });

  it('does not re-request a relative URL it already holds', () => {
    // `img.src` reads back as an absolute URL in a browser, so comparing against it made every
    // relative URL look new and re-entered CORS mode on each update.
    const { img } = makeImage();
    img.changeAttribute('src', '/res/placeholder_creative_image.png');
    img.changeAttribute('src', '/res/placeholder_creative_image.png');

    expect((img.img as unknown as { srcAssignments: number }).srcAssignments).toBe(1);
  });
});

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
