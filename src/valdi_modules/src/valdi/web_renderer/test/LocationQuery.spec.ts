import 'jasmine/src/jasmine';
import { hasWebLocationQueryParameter } from '../src/utils/LocationQuery';

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
