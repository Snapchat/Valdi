import { ParsedCssColor } from '../utils/cssColor';
import { ImageFilterOperation } from '../utils/imageFilterOperations';
import type { ElementLayoutObserver } from '../core/ElementClass';
import { assignStyles } from './ElementClassSupport';
import {
  calculateObjectFitDrawSize,
  CanvasImageRenderer,
  getDecodedImageSize,
  getImageDisplaySize,
  getImageLogicalSize,
  ImageObjectFit,
  isQuarterTurnRotation,
  isRtlForImage,
} from './CanvasImageRenderer';

const CANVAS_IMAGE_RENDERER = new CanvasImageRenderer();

function isCrossOriginHttpSource(source: string): boolean {
  if (!/^https?:\/\//i.test(source)) {
    return false;
  }
  if (typeof window === 'undefined' || !window.location) {
    return true;
  }
  try {
    return new URL(source, window.location.href).origin !== window.location.origin;
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    console.warn(`Valdi web renderer could not parse image URL for origin comparison: ${message}`);
    return true;
  }
}

export interface ImageLogicalSize {
  width: number;
  height: number;
}

export interface ImageElementConfiguration {
  contentRotation: number;
  contentScaleX: number;
  contentScaleY: number;
  cssFilter: string;
  explicitHeight: string | undefined;
  explicitWidth: string | undefined;
  filterOperations: ImageFilterOperation[];
  flipOnRtl: boolean;
  logicalSize: ImageLogicalSize | undefined;
  objectFit: ImageObjectFit;
  source: string | undefined;
  tint: ParsedCssColor | undefined;
}

interface ImageLoadResult {
  success: boolean;
  errorMessage?: string;
  width?: number;
  height?: number;
}

export type ImageAssetLoadCallback = (success: boolean, errorMessage?: string) => void;
export type ImageDecodedCallback = (width: number, height: number) => void;

interface ImageDisplaySize {
  width: number;
  height: number;
  devicePixelRatio: number;
}

interface LayoutAnimationSize {
  width: number;
  height: number;
}

export class ImageElement implements ElementLayoutObserver {
  private readonly element: HTMLElement;
  private readonly enqueuePostLayoutCallback: (callback: () => void) => void;
  private readonly requestLayoutPass: () => void;
  private canvas: HTMLCanvasElement | undefined;
  private image: HTMLImageElement | undefined;
  private source: string | undefined;
  private imageLoaded = false;
  private imageCanvasSafe = false;
  private loadVersion = 0;
  private loadResult: ImageLoadResult | undefined;
  private onAssetLoad: ImageAssetLoadCallback | undefined;
  private onImageDecoded: ImageDecodedCallback | undefined;
  private notifiedOnAssetLoad: ImageAssetLoadCallback | undefined;
  private notifiedOnImageDecoded: ImageDecodedCallback | undefined;
  private tint: ParsedCssColor | undefined;
  private objectFit: ImageObjectFit = 'fill';
  private contentRotation = 0;
  private contentScaleX = 1;
  private contentScaleY = 1;
  private flipOnRtl = false;
  private filterOperations: ImageFilterOperation[] = [];
  private hasExplicitWidth = false;
  private hasExplicitHeight = false;
  private logicalWidthOverride: number | undefined;
  private logicalHeightOverride: number | undefined;
  private displaySize?: ImageDisplaySize;
  private layoutAnimationSize?: LayoutAnimationSize;
  private flip = false;

  constructor(
    element: HTMLElement,
    enqueuePostLayoutCallback: (callback: () => void) => void,
    requestLayoutPass: () => void,
  ) {
    this.element = element;
    this.enqueuePostLayoutCallback = enqueuePostLayoutCallback;
    this.requestLayoutPass = requestLayoutPass;
  }

  destroy(): void {
    this.loadVersion++;
    this.releaseImage();
  }

  setOnAssetLoad(callback: ImageAssetLoadCallback | undefined): void {
    this.onAssetLoad = callback;
    this.scheduleLoadCallbackReplay();
  }

  setOnImageDecoded(callback: ImageDecodedCallback | undefined): void {
    this.onImageDecoded = callback;
    this.scheduleLoadCallbackReplay();
  }

  configure(configuration: ImageElementConfiguration): void {
    const sourceChanged = this.source !== configuration.source;
    this.contentRotation = configuration.contentRotation;
    this.contentScaleX = configuration.contentScaleX;
    this.contentScaleY = configuration.contentScaleY;
    this.element.style.filter = configuration.cssFilter;
    this.hasExplicitHeight = configuration.explicitHeight !== undefined;
    this.hasExplicitWidth = configuration.explicitWidth !== undefined;
    this.element.style.height = configuration.explicitHeight ?? '';
    this.element.style.width = configuration.explicitWidth ?? '';
    this.filterOperations = configuration.filterOperations;
    this.flipOnRtl = configuration.flipOnRtl;
    this.logicalWidthOverride = configuration.logicalSize?.width;
    this.logicalHeightOverride = configuration.logicalSize?.height;
    this.objectFit = configuration.objectFit;
    this.tint = configuration.tint;
    if (sourceChanged) {
      if (configuration.source) {
        this.source = configuration.source;
        this.startLoad(this.requiresCanvas(), true);
      } else {
        this.clearSource();
      }
    }
    if (this.imageLoaded && this.image) {
      this.updateIntrinsicSize(this.image);
    }
  }

  private clearSource(): void {
    this.loadVersion++;
    this.releaseImage();
    this.source = undefined;
    this.image = undefined;
    this.imageLoaded = false;
    this.imageCanvasSafe = false;
    this.loadResult = undefined;
    this.notifiedOnAssetLoad = undefined;
    this.notifiedOnImageDecoded = undefined;
    this.element.replaceChildren();
  }

  private releaseImage(): void {
    if (!this.image) {
      return;
    }
    this.image.onload = null;
    this.image.onerror = null;
    this.image.onabort = null;
    this.image.removeAttribute('src');
  }

  private requiresCanvas(): boolean {
    return this.tint !== undefined || this.filterOperations.some(operation => operation.type === 'colorMatrix');
  }

  private startLoad(corsEnabled: boolean, resetLoadResult: boolean): void {
    const source = this.source;
    if (!source) {
      return;
    }
    const loadVersion = ++this.loadVersion;
    this.releaseImage();
    const image = new Image();
    this.image = image;
    this.imageLoaded = false;
    this.imageCanvasSafe = corsEnabled || !isCrossOriginHttpSource(source);
    if (resetLoadResult) {
      this.loadResult = undefined;
      this.notifiedOnAssetLoad = undefined;
      this.notifiedOnImageDecoded = undefined;
    }
    if (corsEnabled) {
      image.crossOrigin = 'anonymous';
    }
    image.onload = () => {
      if (loadVersion !== this.loadVersion) {
        return;
      }
      this.imageLoaded = true;
      const decodedSize = getDecodedImageSize(image, this.logicalWidthOverride, this.logicalHeightOverride);
      this.loadResult = {
        success: true,
        width: decodedSize.width,
        height: decodedSize.height,
      };
      this.updateIntrinsicSize(image);
      this.renderCurrentLayout();
      this.requestLayoutPass();
      this.scheduleLoadCallbackReplay();
    };
    image.onerror = () => {
      if (loadVersion !== this.loadVersion) {
        return;
      }
      if (corsEnabled && !this.requiresCanvas()) {
        this.startLoad(false, false);
        this.requestLayoutPass();
        return;
      }
      this.failLoad('Failed to load image');
    };
    image.onabort = () => {
      if (loadVersion === this.loadVersion) {
        this.failLoad('Image load aborted');
      }
    };
    image.src = source;
  }

  private failLoad(errorMessage: string): void {
    this.imageLoaded = false;
    this.loadResult = {
      success: false,
      errorMessage,
    };
    this.notifiedOnAssetLoad = undefined;
    this.notifiedOnImageDecoded = undefined;
    this.renderCurrentLayout();
    this.requestLayoutPass();
    this.scheduleLoadCallbackReplay();
  }

  private scheduleLoadCallbackReplay(): void {
    this.enqueuePostLayoutCallback(() => this.replayLoadCallbacks());
  }

  private replayLoadCallbacks(): void {
    const loadResult = this.loadResult;
    if (!loadResult) {
      return;
    }
    if (
      loadResult.success &&
      loadResult.width !== undefined &&
      loadResult.height !== undefined &&
      this.onImageDecoded &&
      this.notifiedOnImageDecoded !== this.onImageDecoded
    ) {
      this.notifiedOnImageDecoded = this.onImageDecoded;
      this.onImageDecoded(loadResult.width, loadResult.height);
    }
    if (this.onAssetLoad && this.notifiedOnAssetLoad !== this.onAssetLoad) {
      this.notifiedOnAssetLoad = this.onAssetLoad;
      this.onAssetLoad(loadResult.success, loadResult.errorMessage);
    }
  }

  onSizeChanged(width: number, height: number): void {
    const layoutAnimationSize = this.layoutAnimationSize;
    if (layoutAnimationSize) {
      width = layoutAnimationSize.width;
      height = layoutAnimationSize.height;
    }
    this.updateDisplaySize(width, height);
  }

  setLayoutAnimationSize(width: number, height: number): void {
    this.layoutAnimationSize = { width, height };
    this.updateDisplaySize(width, height);
    this.renderCurrentLayout();
  }

  clearLayoutAnimationSize(): void {
    this.layoutAnimationSize = undefined;
  }

  private updateDisplaySize(width: number, height: number): void {
    const image = this.image;
    if (!this.source || !image) {
      this.displaySize = undefined;
      return;
    }
    const logicalSize = getImageLogicalSize(image, this.logicalWidthOverride, this.logicalHeightOverride);
    this.displaySize = getImageDisplaySize(width, height, logicalSize.width, logicalSize.height);
    this.flip = this.flipOnRtl && isRtlForImage(this.element);
  }

  onCommit(_element: HTMLElement): void {
    this.renderCurrentLayout();
  }

  private renderCurrentLayout(): void {
    const displaySize = this.displaySize;
    if (displaySize) {
      this.render(displaySize, this.flip);
    }
  }

  private render(displaySize: ImageDisplaySize, flip: boolean): void {
    if (!this.source || !this.image) {
      this.element.replaceChildren();
      return;
    }
    if (this.requiresCanvas() && !this.imageCanvasSafe) {
      this.startLoad(true, false);
    }
    const image = this.image!;
    if (!this.imageLoaded) {
      if (this.requiresCanvas()) {
        this.renderCanvas(displaySize, flip);
      } else {
        this.element.replaceChildren();
      }
      return;
    }
    if (this.requiresCanvas()) {
      this.renderCanvas(displaySize, flip);
    } else {
      this.renderImage(image, displaySize, flip);
    }
  }

  private updateIntrinsicSize(image: HTMLImageElement): void {
    if (this.hasExplicitWidth || this.hasExplicitHeight) {
      return;
    }
    const logicalSize = getImageLogicalSize(image, this.logicalWidthOverride, this.logicalHeightOverride);
    const rotated = isQuarterTurnRotation(this.contentRotation);
    this.element.style.width = `${rotated ? logicalSize.height : logicalSize.width}px`;
    this.element.style.height = `${rotated ? logicalSize.width : logicalSize.height}px`;
  }

  private renderImage(image: HTMLImageElement, displaySize: ImageDisplaySize, flip: boolean): void {
    const logicalSize = getImageLogicalSize(image, this.logicalWidthOverride, this.logicalHeightOverride);
    const rotated = isQuarterTurnRotation(this.contentRotation);
    const effectiveImageWidth = rotated ? logicalSize.height : logicalSize.width;
    const effectiveImageHeight = rotated ? logicalSize.width : logicalSize.height;
    const contentDrawWidth =
      this.objectFit === 'none' ? (this.loadResult?.width ?? effectiveImageWidth) : effectiveImageWidth;
    const contentDrawHeight =
      this.objectFit === 'none' ? (this.loadResult?.height ?? effectiveImageHeight) : effectiveImageHeight;
    const drawSize = calculateObjectFitDrawSize(
      this.objectFit,
      displaySize.width,
      displaySize.height,
      contentDrawWidth,
      contentDrawHeight,
    );
    const finalDrawWidth = rotated ? drawSize.height : drawSize.width;
    const finalDrawHeight = rotated ? drawSize.width : drawSize.height;
    const flipScale = flip ? -1 : 1;
    const scaleX = this.contentScaleX * flipScale;
    const transforms: string[] = [];
    if (this.contentRotation !== 0) {
      transforms.push(`rotate(${this.contentRotation}rad)`);
    }
    if (scaleX !== 1 || this.contentScaleY !== 1) {
      transforms.push(`scale(${scaleX}, ${this.contentScaleY})`);
    }
    const blurFilters = this.filterOperations
      .filter((operation): operation is Extract<ImageFilterOperation, { type: 'blur' }> => operation.type === 'blur')
      .map(operation => `blur(${operation.radius}px)`);
    assignStyles(image, {
      filter: blurFilters.join(' '),
      height: `${finalDrawHeight}px`,
      left: `${(displaySize.width - finalDrawWidth) / 2}px`,
      position: 'absolute',
      top: `${(displaySize.height - finalDrawHeight) / 2}px`,
      width: `${finalDrawWidth}px`,
    });
    if (transforms.length > 0) {
      image.style.transform = transforms.join(' ');
    } else {
      image.style.removeProperty('transform');
    }
    this.setImplementation(image);
  }

  private renderCanvas(displaySize: ImageDisplaySize, flip: boolean): void {
    const image = this.image;
    if (!image) {
      return;
    }
    let canvas = this.canvas;
    if (!canvas) {
      canvas = document.createElement('canvas');
      assignStyles(canvas, {
        height: '100%',
        left: '0',
        position: 'absolute',
        top: '0',
        width: '100%',
      });
      this.canvas = canvas;
    }
    this.setImplementation(canvas);
    CANVAS_IMAGE_RENDERER.render(canvas, {
      contentRotation: this.contentRotation,
      contentScaleX: this.contentScaleX,
      contentScaleY: this.contentScaleY,
      devicePixelRatio: displaySize.devicePixelRatio,
      displayHeight: displaySize.height,
      displayWidth: displaySize.width,
      filterOperations: this.filterOperations,
      flip,
      img: image,
      isLoaded: this.imageLoaded && this.loadResult?.success === true,
      loadHeight: this.loadResult?.height,
      loadWidth: this.loadResult?.width,
      logicalHeightOverride: this.logicalHeightOverride,
      logicalWidthOverride: this.logicalWidthOverride,
      objectFit: this.objectFit,
      tint: this.tint,
    });
  }

  private setImplementation(implementation: HTMLElement): void {
    if (this.element.childNodes.length !== 1 || this.element.childNodes.item(0) !== implementation) {
      this.element.replaceChildren(implementation);
    }
  }
}
