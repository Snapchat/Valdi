import 'jasmine/src/jasmine';
import { applyCssColorOpacity, parseCssColor } from '../src/utils/cssColor';

describe('cssColor', () => {
  it('parses and applies CSS color opacity without regex-specific assumptions', () => {
    expect(parseCssColor('#0f8')).toEqual({ r: 0, g: 255, b: 136, a: 1 });
    expect(parseCssColor('rgba(260, -4, 10.4, 0.25)')).toEqual({ r: 255, g: 0, b: 10, a: 0.25 });
    expect(applyCssColorOpacity('rgba(10, 20, 30, 0.8)', '0.5')).toBe('rgba(10, 20, 30, 0.4)');
  });
});
