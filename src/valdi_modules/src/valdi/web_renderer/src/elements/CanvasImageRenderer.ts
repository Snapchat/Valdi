import { ParsedCssColor } from '../utils/cssColor';
import {
  ImageFilterOperation,
  applyColorMatrixToImageData,
  applyTintToImageData,
} from '../utils/imageFilterOperations';

export const WEB_IMAGE_NATURAL_SCALE = 3;

export type ImageObjectFit = 'fill' | 'contain' | 'cover' | 'none' | 'scale-down';

export type CanvasImageRenderOptions = {
  contentRotation: number;
  contentScaleX: number;
  contentScaleY: number;
  devicePixelRatio: number;
  displayHeight: number;
  displayWidth: number;
  filterOperations: ImageFilterOperation[];
  flip: boolean;
  img: HTMLImageElement;
  isLoaded: boolean;
  loadHeight: number | undefined;
  loadWidth: number | undefined;
  logicalHeightOverride: number | undefined;
  logicalWidthOverride: number | undefined;
  objectFit: ImageObjectFit;
  tint: ParsedCssColor | undefined;
};

export function getDecodedImageSize(
  img: HTMLImageElement,
  logicalWidthOverride: number | undefined,
  logicalHeightOverride: number | undefined,
): { width: number; height: number } {
  if (logicalWidthOverride !== undefined && logicalHeightOverride !== undefined) {
    return {
      width: logicalWidthOverride * WEB_IMAGE_NATURAL_SCALE,
      height: logicalHeightOverride * WEB_IMAGE_NATURAL_SCALE,
    };
  }
  return {
    width: img.naturalWidth,
    height: img.naturalHeight,
  };
}

export function getImageLogicalSize(
  img: HTMLImageElement,
  logicalWidthOverride: number | undefined,
  logicalHeightOverride: number | undefined,
): { width: number; height: number } {
  return {
    width: logicalWidthOverride ?? img.naturalWidth / WEB_IMAGE_NATURAL_SCALE,
    height: logicalHeightOverride ?? img.naturalHeight / WEB_IMAGE_NATURAL_SCALE,
  };
}

export function getImageDisplaySize(
  width: number,
  height: number,
  fallbackWidth: number,
  fallbackHeight: number,
): {
  width: number;
  height: number;
  devicePixelRatio: number;
} {
  const devicePixelRatio = typeof window !== 'undefined' && window.devicePixelRatio ? window.devicePixelRatio : 1;
  return {
    width: width > 0 ? width : fallbackWidth,
    height: height > 0 ? height : fallbackHeight,
    devicePixelRatio,
  };
}

export function calculateObjectFitDrawSize(
  objectFit: ImageObjectFit,
  displayWidth: number,
  displayHeight: number,
  contentWidth: number,
  contentHeight: number,
): { width: number; height: number } {
  if (objectFit === 'fill') {
    return { width: displayWidth, height: displayHeight };
  }

  let scale = 1;
  if (objectFit === 'contain') {
    scale = Math.min(displayWidth / contentWidth, displayHeight / contentHeight);
  } else if (objectFit === 'cover') {
    scale = Math.max(displayWidth / contentWidth, displayHeight / contentHeight);
  } else if (objectFit === 'scale-down') {
    scale = Math.min(1, Math.min(displayWidth / contentWidth, displayHeight / contentHeight));
  }
  return { width: contentWidth * scale, height: contentHeight * scale };
}

export function isQuarterTurnRotation(contentRotation: number): boolean {
  return Math.abs((Math.abs(contentRotation) % Math.PI) - Math.PI / 2) < 0.01;
}

export function isRtlForImage(element: HTMLElement): boolean {
  if (typeof window !== 'undefined' && typeof window.getComputedStyle === 'function') {
    return window.getComputedStyle(element).direction === 'rtl';
  }
  return typeof document !== 'undefined' && document.dir === 'rtl';
}

function clearCanvas(canvas: HTMLCanvasElement, ctx: CanvasRenderingContext2D): void {
  ctx.setTransform(1, 0, 0, 1, 0, 0);
  ctx.clearRect(0, 0, canvas.width, canvas.height);
}

export class CanvasImageRenderer {
  render(canvas: HTMLCanvasElement, options: CanvasImageRenderOptions): void {
    const ctx = canvas.getContext('2d');
    if (ctx === null) {
      throw new Error('Cannot get canvas context');
    }

    const { naturalWidth, naturalHeight } = options.img;
    if (!options.isLoaded || naturalWidth === 0 || naturalHeight === 0) {
      clearCanvas(canvas, ctx);
      return;
    }

    const logicalSize = getImageLogicalSize(options.img, options.logicalWidthOverride, options.logicalHeightOverride);
    const logicalWidth = logicalSize.width;
    const logicalHeight = logicalSize.height;
    const isRotated90Or270 = isQuarterTurnRotation(options.contentRotation);

    const baseImageWidth = logicalWidth;
    const baseImageHeight = logicalHeight;
    const effectiveImageWidth = isRotated90Or270 ? baseImageHeight : baseImageWidth;
    const effectiveImageHeight = isRotated90Or270 ? baseImageWidth : baseImageHeight;
    const displayWidth = options.displayWidth;
    const displayHeight = options.displayHeight;
    const devicePixelRatio = options.devicePixelRatio;
    canvas.width = Math.max(1, Math.round(displayWidth * devicePixelRatio));
    canvas.height = Math.max(1, Math.round(displayHeight * devicePixelRatio));
    ctx.setTransform(devicePixelRatio, 0, 0, devicePixelRatio, 0, 0);
    ctx.clearRect(0, 0, displayWidth, displayHeight);

    const flip = options.flip;
    if (flip) {
      ctx.save();
      ctx.scale(-1, 1);
      ctx.translate(-displayWidth, 0);
    }

    const drawContext = ctx as CanvasRenderingContext2D & { filter?: string };
    const blurFilters = options.filterOperations
      .filter((operation): operation is Extract<ImageFilterOperation, { type: 'blur' }> => operation.type === 'blur')
      .map(operation => `blur(${operation.radius}px)`);
    if (blurFilters.length > 0 && drawContext.filter !== undefined) {
      drawContext.filter = blurFilters.join(' ');
    }

    const contentDrawWidth =
      options.objectFit === 'none' ? (options.loadWidth ?? effectiveImageWidth) : effectiveImageWidth;
    const contentDrawHeight =
      options.objectFit === 'none' ? (options.loadHeight ?? effectiveImageHeight) : effectiveImageHeight;
    const { width: drawWidth, height: drawHeight } = calculateObjectFitDrawSize(
      options.objectFit,
      displayWidth,
      displayHeight,
      contentDrawWidth,
      contentDrawHeight,
    );

    ctx.save();
    ctx.translate(displayWidth / 2, displayHeight / 2);
    ctx.rotate(options.contentRotation);
    ctx.scale(options.contentScaleX, options.contentScaleY);
    const finalDrawWidth = isRotated90Or270 ? drawHeight : drawWidth;
    const finalDrawHeight = isRotated90Or270 ? drawWidth : drawHeight;
    ctx.drawImage(options.img, -finalDrawWidth / 2, -finalDrawHeight / 2, finalDrawWidth, finalDrawHeight);
    ctx.restore();
    if (blurFilters.length > 0 && drawContext.filter !== undefined) {
      drawContext.filter = 'none';
    }

    const colorMatrixOperations = options.filterOperations.filter(
      (operation): operation is Extract<ImageFilterOperation, { type: 'colorMatrix' }> =>
        operation.type === 'colorMatrix',
    );
    if (options.tint || colorMatrixOperations.length > 0) {
      const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
      for (const operation of colorMatrixOperations) {
        applyColorMatrixToImageData(imageData, operation.matrix);
      }
      if (options.tint) {
        applyTintToImageData(imageData, options.tint);
      }
      ctx.putImageData(imageData, 0, 0);
    }

    if (flip) {
      ctx.restore();
    }
  }
}
