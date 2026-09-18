import 'jasmine/src/jasmine';
import { hasSingleWebLocationQueryParameter, hasWebLocationQueryParameter } from '../src/utils/LocationQuery';

describe('hasWebLocationQueryParameter', () => {
  it('matches an explicitly enabled development flag', () => {
    expect(hasWebLocationQueryParameter('?valdiDebugger=1', 'valdiDebugger', '1')).toBeTrue();
  });

  it('finds a development flag among other application parameters', () => {
    expect(hasWebLocationQueryParameter('?fixture=Text_0&valdiTrace=chrome', 'valdiTrace', 'chrome')).toBeTrue();
  });

  it('does not enable missing or differently valued flags', () => {
    expect(hasWebLocationQueryParameter(undefined, 'valdiTrace', 'chrome')).toBeFalse();
    expect(hasWebLocationQueryParameter('?valdiTrace=disabled', 'valdiTrace', 'chrome')).toBeFalse();
    expect(hasWebLocationQueryParameter('?fixture=Text_0', 'valdiTrace', 'chrome')).toBeFalse();
  });

  it('uses the first occurrence of a query flag', () => {
    expect(hasWebLocationQueryParameter('?valdiDebugger=0&valdiDebugger=1', 'valdiDebugger', '1')).toBeFalse();
  });

  it('matches query strings without a leading question mark', () => {
    expect(hasWebLocationQueryParameter('valdiOwlDebugger=1', 'valdiOwlDebugger', '1')).toBeTrue();
  });

  it('does not confuse a parameter prefix for an exact query flag', () => {
    expect(hasWebLocationQueryParameter('?valdiDebuggerExtra=1', 'valdiDebugger', '1')).toBeFalse();
  });
});

describe('hasSingleWebLocationQueryParameter', () => {
  it('matches exactly one explicitly valued flag', () => {
    expect(hasSingleWebLocationQueryParameter('?fixture=Text_0&valdiTrace=chrome', 'valdiTrace', 'chrome')).toBeTrue();
  });

  it('rejects missing values and literal duplicate flags in either order', () => {
    expect(hasSingleWebLocationQueryParameter('?valdiTrace', 'valdiTrace', 'chrome')).toBeFalse();
    expect(
      hasSingleWebLocationQueryParameter('?valdiTrace=chrome&valdiTrace=chrome', 'valdiTrace', 'chrome'),
    ).toBeFalse();
    expect(
      hasSingleWebLocationQueryParameter('?valdiTrace=chrome&valdiTrace=disabled', 'valdiTrace', 'chrome'),
    ).toBeFalse();
    expect(
      hasSingleWebLocationQueryParameter('?valdiTrace=disabled&valdiTrace=chrome', 'valdiTrace', 'chrome'),
    ).toBeFalse();
  });

  it('rejects encoded aliases before or after the literal flag', () => {
    expect(
      hasSingleWebLocationQueryParameter('?valdi%54race=disabled&valdiTrace=chrome', 'valdiTrace', 'chrome'),
    ).toBeFalse();
    expect(
      hasSingleWebLocationQueryParameter('?valdiTrace=chrome&valdi%54race=disabled', 'valdiTrace', 'chrome'),
    ).toBeFalse();
  });

  it('rejects encoded reserved keys and values even without duplicates', () => {
    expect(hasSingleWebLocationQueryParameter('?valdi%54race=chrome', 'valdiTrace', 'chrome')).toBeFalse();
    expect(hasSingleWebLocationQueryParameter('?valdiTrace=chr%6Fme', 'valdiTrace', 'chrome')).toBeFalse();
  });

  it('rejects malformed percent escapes in names and values', () => {
    expect(hasSingleWebLocationQueryParameter('?valdi%Trace=chrome', 'valdiTrace', 'chrome')).toBeFalse();
    expect(hasSingleWebLocationQueryParameter('?valdiTrace=chrome%', 'valdiTrace', 'chrome')).toBeFalse();
  });
});
