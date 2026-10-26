import 'jasmine/src/jasmine';
import {
  GeometricPathBuilder,
  GeometricPathScaleType,
  GeometricPathVisitor,
  visitGeometricPath,
} from '../src/GeometricPath';

function recordingVisitor(calls: unknown[][]): GeometricPathVisitor {
  return {
    beginPath: (...args) => calls.push(['begin', ...args]),
    moveTo: (...args) => calls.push(['move', ...args]),
    lineTo: (...args) => calls.push(['line', ...args]),
    quadTo: (...args) => calls.push(['quad', ...args]),
    cubicTo: (...args) => calls.push(['cubic', ...args]),
    roundRectTo: (...args) => calls.push(['roundRect', ...args]),
    arcTo: (...args) => calls.push(['arc', ...args]),
    close: () => calls.push(['close']),
  };
}

describe('visitGeometricPath', () => {
  it('visits every component a builder emits', () => {
    const path = new GeometricPathBuilder(10, 20, GeometricPathScaleType.Contain)
      .moveTo(1, 2)
      .lineTo(3, 4)
      .quadTo(5, 6, 7, 8)
      .close()
      .build();
    const calls: unknown[][] = [];

    expect(visitGeometricPath(path, recordingVisitor(calls))).toBeTrue();
    expect(calls).toEqual([
      ['begin', 10, 20, GeometricPathScaleType.Contain],
      ['move', 1, 2],
      ['line', 3, 4],
      ['quad', 5, 6, 7, 8],
      ['close'],
    ]);
  });

  it('rejects a missing or empty extent', () => {
    expect(visitGeometricPath(new Float64Array([10, 20]), recordingVisitor([]))).toBeFalse();
    expect(visitGeometricPath(new Float64Array([0, 20, GeometricPathScaleType.Fill]), recordingVisitor([]))).toBeFalse();
  });

  it('rejects a truncated component', () => {
    const path = new GeometricPathBuilder(10, 10).lineTo(1, 2).build();

    expect(visitGeometricPath(path.subarray(0, path.length - 1), recordingVisitor([]))).toBeFalse();
  });
});
