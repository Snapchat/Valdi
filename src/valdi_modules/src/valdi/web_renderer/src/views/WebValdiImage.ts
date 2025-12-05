import { WebValdiLayout } from './WebValdiLayout';
import { UpdateAttributeDelegate } from '../ValdiWebRendererDelegate';
import { convertColor, hexToRGBColor } from '../styles/ValdiWebStyles';

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

  constructor(id: number, attributeDelegate?: UpdateAttributeDelegate) {
    super(id, attributeDelegate);
    this.img = new Image();
    // Allow cross-origin images to be used on canvas without tainting it
    this.img.crossOrigin = 'Anonymous';
    this.img.onload = () => {
      this._onAssetLoad?.({ width: this.img.naturalWidth, height: this.img.naturalHeight });
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

    return element;
  }

  private updateImage() {
    const canvas = this.htmlElement as HTMLCanvasElement;
    const ctx = canvas.getContext('2d');

    if (ctx === null) {
      throw new Error('Cannot get canvas context');
    }

    const { naturalWidth: iw, naturalHeight: ih } = this.img;
    if (iw === 0 || ih === 0) return;

    const { width: cw, height: ch } = canvas.getBoundingClientRect();
    if (cw === 0 || ch === 0) {
      // If canvas has no size from layout, use image size.
      canvas.width = iw;
      canvas.height = ih;
    } else {
      canvas.width = cw;
      canvas.height = ch;
    }

    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // Handle flipOnRtl
    const flip = this._flipOnRtl && document.dir === 'rtl';
    if (flip) {
      ctx.save();
      ctx.scale(-1, 1);
      ctx.translate(-canvas.width, 0);
    }

    // Handle content transforms (scale, rotation)
    ctx.save();
    ctx.translate(canvas.width / 2, canvas.height / 2);
    ctx.rotate((this._contentRotation * Math.PI) / 180);
    ctx.scale(this._contentScaleX, this._contentScaleY);
    ctx.translate(-canvas.width / 2, -canvas.height / 2);

    // Handle objectFit
    let dx = 0,
      dy = 0,
      dw = canvas.width,
      dh = canvas.height;
    if (this._objectFit !== 'fill') {
      const imageAspectRatio = iw / ih;
      const canvasAspectRatio = canvas.width / canvas.height;
      let scale = 1;
      if (this._objectFit === 'contain') {
        scale = Math.min(canvas.width / iw, canvas.height / ih);
      } else if (this._objectFit === 'cover') {
        scale = Math.max(canvas.width / iw, canvas.height / ih);
      } else if (this._objectFit === 'scale-down') {
        scale = Math.min(1, Math.min(canvas.width / iw, canvas.height / ih));
      } // 'none' means scale = 1

      dw = iw * scale;
      dh = ih * scale;
      dx = (canvas.width - dw) / 2;
      dy = (canvas.height - dh) / 2;
    }

    // Draw the image
    ctx.drawImage(this.img, dx, dy, dw, dh);
    ctx.restore(); // Restore from content transforms

    // Apply tint
    if (this._tint) {
      const tintColor = convertColor(this._tint);
      const { r: tr, g: tg, b: tb } = hexToRGBColor(tintColor);
      const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
      const data = imageData.data;

      // Apply tint to non-transparent pixels
      for (let i = 0; i < data.length; i += 4) {
        const alpha = data[i + 3];
        if (alpha === 0) continue;
        data[i] = (data[i] * tr) / 255; // R
        data[i + 1] = (data[i + 1] * tg) / 255; // G
        data[i + 2] = (data[i + 2] * tb) / 255; // B
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
    }

    super.changeAttribute(attributeName, attributeValue);
  }
}
