import { createBitmap } from 'drawing/src/BitmapFactory';
import { BitmapAlphaType, BitmapColorType, BitmapInfo, IBitmap, ImageEncoding } from 'drawing/src/IBitmap';
import { createManagedContext } from 'drawing/src/ManagedContextFactory';
import { beginKeepAlive, endKeepAlive } from 'valdi_core/src/utils/KeepAliveCallback';
import { onIdleInterruptible } from 'valdi_core/src/utils/OnIdle';

function waitForIdle(): Promise<void> {
  return new Promise(resolve => {
    onIdleInterruptible(resolve);
  });
}

function withKeepAlive(cb: () => Promise<void>): Promise<void> {
  let keepAlive = beginKeepAlive();
  return cb().finally(() => endKeepAlive(keepAlive));
}

/**
 * Evaluate a render function and render it as a bitmap with the provided BitmapInfo
 */
export async function renderAsBitmap(bitmapInfo: BitmapInfo, renderFn: () => void): Promise<IBitmap> {
  const context = createManagedContext();
  context.render(renderFn);
  await context.layout(bitmapInfo.width, bitmapInfo.height, false);

  await withKeepAlive(async () => {
    await waitForIdle();
    await context.onAllAssetsLoaded();
  });

  try {
    const { frame } = await context.draw();

    const bitmap = createBitmap(bitmapInfo);
    frame.rasterInto(bitmap, true);
    frame.dispose();

    return bitmap;
  } finally {
    context.dispose();
  }
}

export interface GenerateImageParams {
  width: number;
  height: number;
  imageEncoding: ImageEncoding;
  encodeQuality: number;
}

/**
 * Evaluate a render function function and render it as an encoded image with the given generate params
 */
export async function renderImage(params: GenerateImageParams, renderFn: () => void): Promise<ArrayBuffer> {
  const bitmap = await renderAsBitmap(
    {
      width: params.width,
      height: params.height,
      colorType: BitmapColorType.RGBA8888,
      alphaType: BitmapAlphaType.Premul,
      rowBytes: params.width * 4,
    },
    renderFn,
  );

  return bitmap.encode(params.imageEncoding, params.encodeQuality);
}
