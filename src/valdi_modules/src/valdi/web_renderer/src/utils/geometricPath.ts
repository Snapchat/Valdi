import { GeometricPathScaleType, visitGeometricPath } from 'valdi_core/src/GeometricPath';
import type { GeometricPath, GeometricPathVisitor } from 'valdi_core/src/GeometricPath';

export interface SvgGeometricPath {
  d: string;
  viewBox: string;
  preserveAspectRatio: string;
}

function preserveAspectRatioForScaleType(scaleType: number): string {
  switch (scaleType) {
    case GeometricPathScaleType.Contain:
      return 'xMidYMid meet';
    case GeometricPathScaleType.Cover:
      return 'xMidYMid slice';
    case GeometricPathScaleType.None:
      return 'xMidYMid meet';
    default:
      return 'none';
  }
}

export function isGeometricPathValue(value: unknown): value is GeometricPath {
  return value instanceof Float64Array;
}

class SvgGeometricPathVisitor implements GeometricPathVisitor {
  private readonly parts: string[] = [];
  private extentWidth = 1;
  private extentHeight = 1;
  private scaleType = GeometricPathScaleType.Fill;

  beginPath(width: number, height: number, pathScaleType: GeometricPathScaleType): void {
    this.extentWidth = width;
    this.extentHeight = height;
    this.scaleType = pathScaleType;
  }

  moveTo(x: number, y: number): void {
    this.parts.push(`M ${x} ${y}`);
  }

  lineTo(x: number, y: number): void {
    this.parts.push(`L ${x} ${y}`);
  }

  quadTo(controlX: number, controlY: number, x: number, y: number): void {
    this.parts.push(`Q ${controlX} ${controlY} ${x} ${y}`);
  }

  cubicTo(controlX1: number, controlY1: number, controlX2: number, controlY2: number, x: number, y: number): void {
    this.parts.push(`C ${controlX1} ${controlY1} ${controlX2} ${controlY2} ${x} ${y}`);
  }

  roundRectTo(x: number, y: number, width: number, height: number, radiusX: number, radiusY: number): void {
    const rx = Math.min(radiusX, width / 2);
    const ry = Math.min(radiusY, height / 2);
    if (rx <= 0 && ry <= 0) {
      this.parts.push(`M ${x} ${y} h ${width} v ${height} h ${-width} Z`);
    } else {
      this.parts.push(
        `M ${x + rx} ${y} L ${x + width - rx} ${y} Q ${x + width} ${y} ${x + width} ${y + ry} L ${x + width} ${y + height - ry} Q ${x + width} ${y + height} ${x + width - rx} ${y + height} L ${x + rx} ${y + height} Q ${x} ${y + height} ${x} ${y + height - ry} L ${x} ${y + ry} Q ${x} ${y} ${x + rx} ${y} Z`,
      );
    }
  }

  arcTo(centerX: number, centerY: number, radius: number, startAngle: number, sweepAngle: number): void {
    const startX = centerX + radius * Math.cos(startAngle);
    const startY = centerY + radius * Math.sin(startAngle);
    const endX = centerX + radius * Math.cos(startAngle + sweepAngle);
    const endY = centerY + radius * Math.sin(startAngle + sweepAngle);
    const largeArc = Math.abs(sweepAngle) >= Math.PI ? 1 : 0;
    const sweepFlag = sweepAngle > 0 ? 1 : 0;
    this.parts.push(`M ${startX} ${startY} A ${radius} ${radius} 0 ${largeArc} ${sweepFlag} ${endX} ${endY}`);
  }

  close(): void {
    this.parts.push('Z');
  }

  getResult(): SvgGeometricPath {
    return {
      d: this.parts.join(' '),
      viewBox: `0 0 ${this.extentWidth} ${this.extentHeight}`,
      preserveAspectRatio: preserveAspectRatioForScaleType(this.scaleType),
    };
  }
}

export function geometricPathToSvgPath(data: GeometricPath): SvgGeometricPath {
  const visitor = new SvgGeometricPathVisitor();

  if (!visitGeometricPath(data, visitor)) {
    return { d: '', viewBox: '0 0 1 1', preserveAspectRatio: 'none' };
  }

  return visitor.getResult();
}
