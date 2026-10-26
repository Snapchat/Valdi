/**
 * A GeometricPath is a serialized data object
 * containing path instructions. It can be built
 * through the GeometricPathBuilder class.
 */
export type GeometricPath = Float64Array;

const enum GeometricPathComponent {
  Move = 1,
  Line = 2,
  Quad = 3,
  Cubic = 4,
  RoundRect = 5,
  Arc = 6,
  Close = 7,
}

/**
 * Defines how the GeometricPath should scale relative to
 * the container view where it is being used.
 */
export const enum GeometricPathScaleType {
  /**
   * The path will scale such that the path will entirely
   * fill the bounds of the container without expanding beyond it.
   * Aspect ratio of the path will not be preserved.
   * This is the default option.
   */
  Fill = 1,

  /**
   * The path will scale such that the path will fit within the bounds of the
   * container. Aspect ratio of the path will be preserved.
   */
  Contain = 2,

  /**
   * The path will scale such that the path entirely fill the bounds of
   * the container, with sides potentially expanding beyond it.
   * Aspect ratio of the path will be preserved.
   */
  Cover = 3,

  /**
   * The path will not be scaled relative to the container. In this mode,
   * the extentWidth and extentHeight will be treated as the absolute size
   * of the path, and the path will be centered in the container.
   */
  None = 4,
}

/**
 * A GeometricPathBuilder is a utility class for building
 * GeometricPath objects that can be provided to the "shape" elements.
 * A path builder is created with an extent width and height, which represent
 * the coordinate space from which all the path operation values derive from.
 * Whenever the path is used into a container that has a different size than the
 * path extent, the path operations are scaled by applying a ratio between
 * the extent and the actual container size.
 */
export class GeometricPathBuilder {
  private data: number[] = [];

  constructor(extentWidth: number, extentHeight: number, scaleType?: GeometricPathScaleType) {
    this.data.push(extentWidth, extentHeight, scaleType ?? GeometricPathScaleType.Fill);
  }

  /**
   * Move the path position to given x and y.
   */
  moveTo(x: number, y: number): GeometricPathBuilder {
    this.data.push(GeometricPathComponent.Move, x, y);
    return this;
  }

  /**
   * Add a line from the current path position to the given x and y
   */
  lineTo(x: number, y: number): GeometricPathBuilder {
    this.data.push(GeometricPathComponent.Line, x, y);
    return this;
  }

  /**
   * Add a rect at the given x and y with the given width and height.
   */
  rectTo(x: number, y: number, width: number, height: number): GeometricPathBuilder {
    return this.roundRectTo(x, y, width, height, 0, 0);
  }

  /**
   * Add a round rect at the given x and y with the given width and height, with
   * the rect corners rounded using radiusX and radiusY.
   */
  roundRectTo(
    x: number,
    y: number,
    width: number,
    height: number,
    radiusX: number,
    radiusY: number,
  ): GeometricPathBuilder {
    this.data.push(GeometricPathComponent.RoundRect, x, y, width, height, radiusX, radiusY);
    return this;
  }

  /**
   * Add a arc at the given centerX and centerY with the given radius, starting from
   * the given startAngle and sweeping by the given sweepAngle.
   * A positive sweepAngle extends the arc clockwise, a negative angle extends
   * it counterclockwise.
   */
  arcTo(
    centerX: number,
    centerY: number,
    radius: number,
    startAngle: number,
    sweepAngle: number,
  ): GeometricPathBuilder {
    this.data.push(GeometricPathComponent.Arc, centerX, centerY, radius, startAngle, sweepAngle);
    return this;
  }

  /**
   * Add an oval at the given x and y with the given width and height.
   */
  ovalTo(x: number, y: number, width: number, height: number): GeometricPathBuilder {
    return this.roundRectTo(x, y, width, height, width / 2.0, height / 2.0);
  }

  /**
   * Add a cubic curve from the current path position to the given x and y,
   * using controlX1 and controlY1 as the first control point of the curve,
   * and controlX2 and controlY2 as the second control point of the curve.
   */
  cubicTo(
    controlX1: number,
    controlY1: number,
    controlX2: number,
    controlY2: number,
    x: number,
    y: number,
  ): GeometricPathBuilder {
    this.data.push(GeometricPathComponent.Cubic, controlX1, controlY1, controlX2, controlY2, x, y);
    return this;
  }

  /**
   * Add a quad curve from the current path position to the given x and y,
   * using controlX and controlY as the control point.
   */
  quadTo(controlX: number, controlY: number, x: number, y: number): GeometricPathBuilder {
    this.data.push(GeometricPathComponent.Quad, controlX, controlY, x, y);
    return this;
  }

  /**
   * Close the current path contour, connecting a line between the first point and the last point.
   */
  close(): GeometricPathBuilder {
    this.data.push(GeometricPathComponent.Close);
    return this;
  }

  /**
   * Build and return a GeometricPath object containing the path instructions.
   * The object can be then passed as to the "path" property of "shape" element.
   */
  build(): GeometricPath {
    return Float64Array.from(this.data);
  }
}

export interface GeometricPathVisitor {
  beginPath?(extentWidth: number, extentHeight: number, scaleType: GeometricPathScaleType): void;
  moveTo(x: number, y: number): void;
  lineTo(x: number, y: number): void;
  quadTo(controlX: number, controlY: number, x: number, y: number): void;
  cubicTo(controlX1: number, controlY1: number, controlX2: number, controlY2: number, x: number, y: number): void;
  roundRectTo(x: number, y: number, width: number, height: number, radiusX: number, radiusY: number): void;
  arcTo(centerX: number, centerY: number, radius: number, startAngle: number, sweepAngle: number): void;
  close(): void;
}

function isValidScaleType(scaleType: number): scaleType is GeometricPathScaleType {
  return (
    scaleType === GeometricPathScaleType.Fill ||
    scaleType === GeometricPathScaleType.Contain ||
    scaleType === GeometricPathScaleType.Cover ||
    scaleType === GeometricPathScaleType.None
  );
}

function hasValues(path: GeometricPath, index: number, count: number): boolean {
  return index + count <= path.length;
}

export function visitGeometricPath(path: GeometricPath, visitor: GeometricPathVisitor): boolean {
  if (path.length < 3) {
    return false;
  }

  let index = 0;
  const extentWidth = path[index++];
  const extentHeight = path[index++];
  const scaleType = path[index++];
  if (extentWidth <= 0 || extentHeight <= 0 || !isValidScaleType(scaleType)) {
    return false;
  }
  visitor.beginPath?.(extentWidth, extentHeight, scaleType);

  while (index < path.length) {
    const component = path[index++] as GeometricPathComponent;
    switch (component) {
      case GeometricPathComponent.Move:
        if (!hasValues(path, index, 2)) {
          return false;
        }
        visitor.moveTo(path[index++], path[index++]);
        break;
      case GeometricPathComponent.Line:
        if (!hasValues(path, index, 2)) {
          return false;
        }
        visitor.lineTo(path[index++], path[index++]);
        break;
      case GeometricPathComponent.Quad:
        if (!hasValues(path, index, 4)) {
          return false;
        }
        visitor.quadTo(path[index++], path[index++], path[index++], path[index++]);
        break;
      case GeometricPathComponent.Cubic:
        if (!hasValues(path, index, 6)) {
          return false;
        }
        visitor.cubicTo(path[index++], path[index++], path[index++], path[index++], path[index++], path[index++]);
        break;
      case GeometricPathComponent.RoundRect:
        if (!hasValues(path, index, 6)) {
          return false;
        }
        visitor.roundRectTo(path[index++], path[index++], path[index++], path[index++], path[index++], path[index++]);
        break;
      case GeometricPathComponent.Arc:
        if (!hasValues(path, index, 5)) {
          return false;
        }
        visitor.arcTo(path[index++], path[index++], path[index++], path[index++], path[index++]);
        break;
      case GeometricPathComponent.Close:
        visitor.close();
        break;
      default:
        return false;
    }
  }

  return true;
}
