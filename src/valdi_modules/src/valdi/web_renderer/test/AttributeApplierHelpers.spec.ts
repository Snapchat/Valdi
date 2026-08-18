import 'jasmine/src/jasmine';
import { parseCssLength } from '../src/attributes/AttributeApplierHelpers';

describe('AttributeApplierHelpers', () => {
  it('adds px to Valdi CSS lengths without changing existing units', () => {
    expect(parseCssLength('8 12px -4', 'padding')).toBe('8px 12px -4px');
  });
});
