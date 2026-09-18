import { WebValdiLayout } from './WebValdiLayout';
import { UpdateAttributeDelegate } from '../ValdiWebRendererDelegate';
import { convertColor } from '../styles/ValdiWebStyles';

/**
 * Web implementation of ShapeView using SVG.
 * Decodes GeometricPath (Float64Array) from valdi_core to SVG path d string,
 * matching Android (GeometricPath.kt) and iOS (CGPathFromGeometricPathData) behavior.
 */
const SVG_NS = 'http://www.w3.org/2000/svg';

const enum PathComponent {
  Move = 1,
  Line = 2,
  Quad = 3,
  Cubic = 4,
  RoundRect = 5,
  Arc = 6,
  Close = 7,
}

function geometricPathToSvgD(data: Float64Array): { d: string; viewBox: string } {
  if (data.length < 3) return { d: '', viewBox: '0 0 1 1' };
  let i = 0;
  const extentWidth = data[i++];
  const extentHeight = data[i++];
  const scaleType = data[i++];
  const viewBox = `0 0 ${extentWidth} ${extentHeight}`;
  const parts: string[] = [];
  while (i < data.length) {
    const cmd = data[i++] as PathComponent;
    switch (cmd) {
      case PathComponent.Move:
        parts.push(`M ${data[i++]} ${data[i++]}`);
        break;
      case PathComponent.Line:
        parts.push(`L ${data[i++]} ${data[i++]}`);
        break;
      case PathComponent.Quad: {
        const cx = data[i++];
        const cy = data[i++];
        const x = data[i++];
        const y = data[i++];
        parts.push(`Q ${cx} ${cy} ${x} ${y}`);
        break;
      }
      case PathComponent.Cubic: {
        const c1x = data[i++];
        const c1y = data[i++];
        const c2x = data[i++];
        const c2y = data[i++];
        const x = data[i++];
        const y = data[i++];
        parts.push(`C ${c1x} ${c1y} ${c2x} ${c2y} ${x} ${y}`);
        break;
      }
      case PathComponent.RoundRect: {
        const x = data[i++];
        const y = data[i++];
        const w = data[i++];
        const h = data[i++];
        const rx = Math.min(data[i++], w / 2);
        const ry = Math.min(data[i++], h / 2);
        if (rx <= 0 && ry <= 0) {
          parts.push(`M ${x} ${y} h ${w} v ${h} h ${-w} Z`);
        } else {
          parts.push(
            `M ${x + rx} ${y} L ${x + w - rx} ${y} Q ${x + w} ${y} ${x + w} ${y + ry} L ${x + w} ${y + h - ry} Q ${x + w} ${y + h} ${x + w - rx} ${y + h} L ${x + rx} ${y + h} Q ${x} ${y + h} ${x} ${y + h - ry} L ${x} ${y + ry} Q ${x} ${y} ${x + rx} ${y} Z`,
          );
        }
        break;
      }
      case PathComponent.Arc: {
        const cx = data[i++];
        const cy = data[i++];
        const r = data[i++];
        const start = data[i++];
        const sweep = data[i++];
        const startX = cx + r * Math.cos(start);
        const startY = cy + r * Math.sin(start);
        const endX = cx + r * Math.cos(start + sweep);
        const endY = cy + r * Math.sin(start + sweep);
        const largeArc = Math.abs(sweep) >= Math.PI ? 1 : 0;
        const sweepFlag = sweep > 0 ? 1 : 0;
        parts.push(`M ${startX} ${startY} A ${r} ${r} 0 ${largeArc} ${sweepFlag} ${endX} ${endY}`);
        break;
      }
      case PathComponent.Close:
        parts.push('Z');
        break;
      default:
        break;
    }
  }
  return { d: parts.join(' '), viewBox };
}

export class WebValdiShape extends WebValdiLayout {
  public type = 'shape';
  private pathEl!: SVGPathElement;
  private svgEl!: SVGSVGElement;
  private _strokeStart = 0;
  private _strokeEnd = 1;

  constructor(id: number, attributeDelegate?: UpdateAttributeDelegate) {
    super(id, attributeDelegate);
  }

  createHtmlElement(): HTMLElement {
    const pathEl = document.createElementNS(SVG_NS, 'path') as SVGPathElement;
    this.pathEl = pathEl;
    pathEl.setAttribute('fill', 'none');

    const wrapper = document.createElement('div');
    Object.assign(wrapper.style, {
      width: '100%',
      height: '100%',
      display: 'block',
      margin: 0,
      padding: 0,
      pointerEvents: 'auto',
    });
    const svg = document.createElementNS(SVG_NS, 'svg') as SVGSVGElement;
    this.svgEl = svg;
    svg.setAttribute('viewBox', '0 0 1 1');
    svg.setAttribute('preserveAspectRatio', 'none');
    Object.assign(svg.style, {
      width: '100%',
      height: '100%',
      display: 'block',
    });
    svg.appendChild(pathEl);
    wrapper.appendChild(svg);
    return wrapper;
  }

  private pathToString(path: unknown): string | undefined {
    if (typeof path === 'string') return path;
    if (path instanceof Float64Array) {
      const arr = path;
      const { d, viewBox } = geometricPathToSvgD(arr);
      this.svgEl.setAttribute('viewBox', viewBox);
      return d || undefined;
    }
    return undefined;
  }

  private applyStrokeDash(): void {
    const path = this.pathEl;
    const total = path.getTotalLength();
    if (total <= 0) return;
    const start = this._strokeStart * total;
    const length = (this._strokeEnd - this._strokeStart) * total;
    path.setAttribute('stroke-dasharray', `${length} ${total}`);
    path.setAttribute('stroke-dashoffset', String(-start));
  }

  changeAttribute(attributeName: string, attributeValue: unknown): void {
    switch (attributeName) {
      case 'path': {
        const d = this.pathToString(attributeValue);
        if (d !== undefined) {
          this.pathEl.setAttribute('d', d);
          this.applyStrokeDash();
        } else {
          this.pathEl.removeAttribute('d');
        }
        return;
      }
      case 'strokeWidth':
        this.pathEl.setAttribute('stroke-width', String(Number(attributeValue ?? 0)));
        return;
      case 'strokeColor':
        this.pathEl.setAttribute('stroke', convertColor(String(attributeValue ?? 'transparent')));
        return;
      case 'fillColor':
        this.pathEl.setAttribute('fill', convertColor(String(attributeValue ?? 'transparent')));
        return;
      case 'strokeCap':
        this.pathEl.setAttribute('stroke-linecap', String(attributeValue ?? 'butt'));
        return;
      case 'strokeJoin':
        this.pathEl.setAttribute('stroke-linejoin', String(attributeValue ?? 'miter'));
        return;
      case 'strokeStart':
        this._strokeStart = Number(attributeValue ?? 0);
        this.applyStrokeDash();
        return;
      case 'strokeEnd':
        this._strokeEnd = Number(attributeValue ?? 1);
        this.applyStrokeDash();
        return;
      default:
        break;
    }
    super.changeAttribute(attributeName, attributeValue);
  }
}
