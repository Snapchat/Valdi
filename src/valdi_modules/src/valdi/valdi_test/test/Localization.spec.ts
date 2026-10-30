import myStrings from 'valdi_test/src/Strings';

import 'jasmine/src/jasmine';
import { Device } from 'valdi_core/src/Device';
import { overrideLocales, removeOverrideLocales } from 'valdi_core/src/LocalizableStrings';
import { Locale } from 'valdi_core/src/localization/Locale';
import { hasDeviceLocales } from './EngineSupport';

describe('Localization', () => {
  afterEach(() => {
    removeOverrideLocales(myStrings);
  });

  // CI hosts can report no device locales at all.
  (hasDeviceLocales() ? it : xit)('can resolve system locale', () => {
      const locales = Device.getDeviceLocales();
      expect(locales).not.toEqual([]);
  });

  it('can resolve string using system locale', () => {
      expect(myStrings.myFirst()).not.toBe('my_first');
  });

  it('can localize without format', () => {
    overrideLocales(myStrings, () => [Locale.parse('en')]);

    expect(myStrings.myFirst()).toBe('My first text');

    overrideLocales(myStrings, () => [Locale.parse('fr')]);

    expect(myStrings.myFirst()).toBe('Mon premier texte');
  });

  it('can localize with format', () => {
    overrideLocales(myStrings, () => [Locale.parse('en')]);

    expect(myStrings.mySecond('2')).toBe('My second text in a total of 2');

    overrideLocales(myStrings, () => [Locale.parse('fr')]);

    expect(myStrings.mySecond('2')).toBe('Mon second texte avec un total de 2');
  });
});
