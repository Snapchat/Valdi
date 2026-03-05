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
  private _explicitWidth: number | string | undefined;
  private _explicitHeight: number | string | undefined;
  private _rotation = 0;

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

    if (!this._explicitWidth && !this._explicitHeight) {
      const isRotated90or270 = Math.abs(Math.abs(this._rotation) % Math.PI - Math.PI / 2) < 0.01;
      if (isRotated90or270) {
        this.htmlElement.style.width = `${ih}px`;
        this.htmlElement.style.height = `${iw}px`;
      } else {
        this.htmlElement.style.width = `${iw}px`;
        this.htmlElement.style.height = `${ih}px`;
      }
    }

    const { width: cw, height: ch } = canvas.getBoundingClientRect();
    if (cw === 0 || ch === 0) {
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

    // Handle objectFit - use rotated dimensions for 90°/270° rotations
    const isRotated90or270 = Math.abs(Math.abs(this._rotation) % Math.PI - Math.PI / 2) < 0.01;
    const effectiveIw = isRotated90or270 ? ih : iw;
    const effectiveIh = isRotated90or270 ? iw : ih;
    
    let dw = effectiveIw,
      dh = effectiveIh;
    if (this._objectFit !== 'fill') {
      let scale = 1;
      if (this._objectFit === 'contain') {
        scale = Math.min(canvas.width / effectiveIw, canvas.height / effectiveIh);
      } else if (this._objectFit === 'cover') {
        scale = Math.max(canvas.width / effectiveIw, canvas.height / effectiveIh);
      } else if (this._objectFit === 'scale-down') {
        scale = Math.min(1, Math.min(canvas.width / effectiveIw, canvas.height / effectiveIh));
      } // 'none' means scale = 1
      dw = effectiveIw * scale;
      dh = effectiveIh * scale;
    } else {
      dw = canvas.width;
      dh = canvas.height;
    }

    // Handle content transforms (scale, rotation) - draw centered and rotated
    const totalRotation = (this._contentRotation * Math.PI) / 180 + this._rotation;
    ctx.save();
    ctx.translate(canvas.width / 2, canvas.height / 2);
    ctx.rotate(totalRotation);
    ctx.scale(this._contentScaleX, this._contentScaleY);

    // Draw image centered at origin (rotation happens around center)
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
