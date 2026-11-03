import 'jasmine/src/jasmine';
import { GeometricPathBuilder, GeometricPathScaleType } from 'valdi_core/src/GeometricPath';
import { geometricPathToSvgPath } from '../src/utils/geometricPath';

describe('geometricPath', () => {
  it('generates preserveAspectRatio for cover paths and SVG commands for curves', () => {
    const path = new GeometricPathBuilder(80, 40, GeometricPathScaleType.Cover)
      .moveTo(2, 4)
      .quadTo(6, 8, 10, 12)
      .cubicTo(1, 2, 3, 4, 5, 6)
      .close()
      .build();

    const svgPath = geometricPathToSvgPath(path);

    expect(svgPath.viewBox).toBe('0 0 80 40');
    expect(svgPath.preserveAspectRatio).toBe('xMidYMid slice');
    expect(svgPath.d).toBe('M 2 4 Q 6 8 10 12 C 1 2 3 4 5 6 Z');
  });

  it('clamps round rect radii and emits negative sweep arcs', () => {
    const path = new GeometricPathBuilder(20, 20, GeometricPathScaleType.Fill)
      .roundRectTo(0, 0, 10, 6, 20, 20)
      .arcTo(5, 5, 4, 0, -Math.PI / 2)
      .build();

    const svgPath = geometricPathToSvgPath(path);

    expect(svgPath.preserveAspectRatio).toBe('none');
    expect(svgPath.d).toContain('M 5 0 L 5 0 Q 10 0 10 3');
    expect(svgPath.d).toContain('A 4 4 0 0 0');
  });

  it('returns an empty fallback for invalid geometric path data', () => {
    expect(geometricPathToSvgPath(Float64Array.from([10, 20, 999]))).toEqual({
      d: '',
      viewBox: '0 0 1 1',
      preserveAspectRatio: 'none',
    });
  });
});
