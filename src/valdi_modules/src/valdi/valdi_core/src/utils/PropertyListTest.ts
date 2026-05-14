import 'jasmine/src/jasmine';
import { hasProperty } from './PropertyList';

describe('PropertyList', () => {
  it('hasProperty only checks keys for array property lists', () => {
    const list = ['firstKey', 'targetProperty', 'secondKey', 'secondValue'];
    expect(hasProperty(list, 'targetProperty')).toBeFalse();
  });

  it('hasProperty returns true when key exists', () => {
    const list = ['firstKey', 'firstValue', 'targetProperty', 456];
    expect(hasProperty(list, 'targetProperty')).toBeTrue();
  });
});
