import { WebValdiLayout } from './WebValdiLayout';
import { UpdateAttributeDelegate } from '../ValdiWebRendererDelegate';
import { convertColor, hexToRGBColor } from '../styles/ValdiWebStyles';

const WEB_IMAGE_NATURAL_SCALE = 3;

export class WebValdiImage extends WebValdiLayout {
  public type = 'image';
  img: HTMLImageElement;
  private _tint: string | null = null;
  private _objectFit: 'fill' | 'contain' | 'cover' | 'none' | 'scale-down' = 'fill';
  private _onAssetLoad?: (event: { width: number; height: number }) => void;
  private _onImageDecoded?: () => void;
  private _contentRotation = 0;
  private _contentScaleX = 1;
  private _contentScaleY = 1;
  private _flipOnRtl = false;
  private _explicitWidth: number | string | undefined;
  private _explicitHeight: number | string | undefined;
  private _rotation = 0;
  private _observer?: ResizeObserver;

  constructor(id: number, attributeDelegate?: UpdateAttributeDelegate) {
    super(id, attributeDelegate);
    this.img = new Image();
    // Allow cross-origin images to be used on canvas without tainting it
    this.img.crossOrigin = 'Anonymous';
    this.img.onload = () => {
      const w = this.img.naturalWidth / WEB_IMAGE_NATURAL_SCALE;
      const h = this.img.naturalHeight / WEB_IMAGE_NATURAL_SCALE;
      this._onAssetLoad?.({ width: w, height: h });
      this.updateImage();
      this._onImageDecoded?.();
    };
  }

  createHtmlElement() {
    const element = document.createElement('canvas');

    Object.assign(element.style, {
      // Inherit layout styles from WebValdiLayout
      backgroundColor: 'transparent',
      border: '0 solid black',
      boxSizing: 'border-box',
      display: 'flex',
      listStyle: 'none',
      margin: 0,
      padding: 0,
      position: 'relative',
      pointerEvents: 'auto',
    });

    // Redraw when the layout engine changes the canvas's CSS size, ensuring
    // the backing buffer stays in sync with the display size (fixes blurriness
    // on Retina/HiDPI displays when the first draw happens before layout settles).
    if (typeof ResizeObserver !== 'undefined') {
      this._observer = new ResizeObserver(() => this.updateImage());
      this._observer.observe(element);
    }

    return element;
  }

  destroy() {
    this._observer?.disconnect();
    super.destroy();
  }

  private updateImage() {
    const canvas = this.htmlElement as HTMLCanvasElement;
    const ctx = canvas.getContext('2d');

    if (ctx === null) {
      throw new Error('Cannot get canvas context');
    }

    const { naturalWidth: iw, naturalHeight: ih } = this.img;
    if (iw === 0 || ih === 0) return;

    // Logical dimensions (assume 3x assets for web)
    const logicalW = iw / WEB_IMAGE_NATURAL_SCALE;
    const logicalH = ih / WEB_IMAGE_NATURAL_SCALE;

    if (!this._explicitWidth && !this._explicitHeight) {
      const isRotated90or270 = Math.abs(Math.abs(this._rotation) % Math.PI - Math.PI / 2) < 0.01;
      if (isRotated90or270) {
        this.htmlElement.style.width = `${logicalH}px`;
        this.htmlElement.style.height = `${logicalW}px`;
      } else {
        this.htmlElement.style.width = `${logicalW}px`;
        this.htmlElement.style.height = `${logicalH}px`;
      }
    }

    const isRotated90or270 = Math.abs(Math.abs(this._rotation) % Math.PI - Math.PI / 2) < 0.01;
    const effectiveIw = isRotated90or270 ? logicalH : logicalW;
    const effectiveIh = isRotated90or270 ? logicalW : logicalH;

    // Use element's display size for canvas so aspect ratio matches layout (avoids squishing when layout gives e.g. square box)
    const rect = canvas.getBoundingClientRect();
    const dpr = typeof window !== 'undefined' && window.devicePixelRatio ? window.devicePixelRatio : 1;
    const displayW = rect.width > 0 ? rect.width : logicalW;
    const displayH = rect.height > 0 ? rect.height : logicalH;
    const backingW = Math.max(1, Math.round(displayW * dpr));
    const backingH = Math.max(1, Math.round(displayH * dpr));

    canvas.width = backingW;
    canvas.height = backingH;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

    ctx.clearRect(0, 0, displayW, displayH);

    // Handle flipOnRtl
    const flip = this._flipOnRtl && document.dir === 'rtl';
    if (flip) {
      ctx.save();
      ctx.scale(-1, 1);
      ctx.translate(-displayW, 0);
    }

    // objectFit: fit image (effectiveIw x effectiveIh) into (displayW x displayH) to preserve aspect ratio
    let dw = displayW,
      dh = displayH;
    const boxAspect = displayW / displayH;
    const imageAspect = effectiveIw / effectiveIh;
    const aspectMismatch = Math.abs(boxAspect - imageAspect) > 0.01;
    // When layout box and image aspect ratios differ, use contain so we don't squish (e.g. wide icon in square slot)
    const effectiveFit =
      this._objectFit === 'fill' && aspectMismatch ? 'contain' : this._objectFit;
    if (effectiveFit !== 'fill') {
      let scale = 1;
      if (effectiveFit === 'contain') {
        scale = Math.min(displayW / effectiveIw, displayH / effectiveIh);
      } else if (effectiveFit === 'cover') {
        scale = Math.max(displayW / effectiveIw, displayH / effectiveIh);
      } else if (effectiveFit === 'scale-down') {
        scale = Math.min(1, Math.min(displayW / effectiveIw, displayH / effectiveIh));
      } // 'none' means scale = 1
      dw = effectiveIw * scale;
      dh = effectiveIh * scale;
    }

    // Handle content transforms (scale, rotation) - draw centered and rotated
    const totalRotation = (this._contentRotation * Math.PI) / 180 + this._rotation;
    ctx.save();
    ctx.translate(displayW / 2, displayH / 2);
    ctx.rotate(totalRotation);
    ctx.scale(this._contentScaleX, this._contentScaleY);

    const drawW = isRotated90or270 ? dh : dw;
    const drawH = isRotated90or270 ? dw : dh;
    ctx.drawImage(this.img, -drawW / 2, -drawH / 2, drawW, drawH);
    ctx.restore(); // Restore from content transforms

    // Apply tint
    if (this._tint) {
      const tintColor = convertColor(this._tint);
      const { r: tr, g: tg, b: tb } = hexToRGBColor(tintColor);
      const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
      const data = imageData.data;

      // Replace color of non-transparent pixels with tint color (matching native behavior)
      for (let i = 0; i < data.length; i += 4) {
        const alpha = data[i + 3];
        if (alpha === 0) continue;
        data[i] = tr; // R
        data[i + 1] = tg; // G
        data[i + 2] = tb; // B
      }
      ctx.putImageData(imageData, 0, 0);
    }

    if (flip) {
      ctx.restore(); // Restore from flip
    }
  }

  // Finds the first valid src string in a nested object.
  private recursivelyResolveSrc(src: Record<string, any> | string | undefined): string | undefined {
    if (!src) {
      return undefined;
    }

    if (typeof src === 'string') {
      return src;
    }

    return this.recursivelyResolveSrc(src?.src);
  }

  changeAttribute(attributeName: string, attributeValue: any): void {
    switch (attributeName) {
      case 'src':
        const src = this.recursivelyResolveSrc(attributeValue);

        if (src && this.img.src !== src) {
          this.img.src = src;
        }
        return;
      case 'objectFit':
        this._objectFit = attributeValue;
        this.updateImage();
        return;
      case 'onAssetLoad':
        this._onAssetLoad = attributeValue;
        return;
      case 'onImageDecoded':
        this._onImageDecoded = attributeValue;
        return;
      case 'tint':
        this._tint = attributeValue;
        this.updateImage();
        return;
      case 'flipOnRtl':
        this._flipOnRtl = !!attributeValue;
        this.updateImage();
        return;
      case 'contentScaleX':
        this._contentScaleX = Number(attributeValue) || 1;
        this.updateImage();
        return;
      case 'contentScaleY':
        this._contentScaleY = Number(attributeValue) || 1;
        this.updateImage();
        return;
      case 'contentRotation':
        this._contentRotation = Number(attributeValue) || 0;
        this.updateImage();
        return;
      case 'filter':
        this.htmlElement.style.filter = attributeValue;
        return;
      case 'ref':
        // This is likely for framework-level component references. No-op at this level.
        return;
      case 'width':
        this._explicitWidth = attributeValue;
        super.changeAttribute(attributeName, attributeValue);
        return;
      case 'height':
        this._explicitHeight = attributeValue;
        super.changeAttribute(attributeName, attributeValue);
        return;
      case 'rotation':
        this._rotation = Number(attributeValue) || 0;
        this.updateImage();
        return;
    }

    super.changeAttribute(attributeName, attributeValue);
  }
}
