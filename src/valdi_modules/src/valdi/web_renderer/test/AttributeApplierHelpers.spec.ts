import 'jasmine/src/jasmine';
import { parseCssLength, parseCssTrackList } from '../src/attributes/AttributeApplierHelpers';

describe('AttributeApplierHelpers', () => {
  it('adds px to Valdi CSS lengths without changing existing units', () => {
    expect(parseCssLength('8 12px -4', 'padding')).toBe('8px 12px -4px');
  });

  it('preserves repeat counts while adding px to grid track sizes', () => {
    expect(parseCssTrackList('repeat(2, minmax(40, 1fr) 20)', 'gridTemplateColumns')).toBe(
      'repeat(2, minmax(40px, 1fr) 20px)',
    );
  });
});
