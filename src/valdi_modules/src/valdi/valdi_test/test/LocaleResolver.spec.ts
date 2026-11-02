import { Locale } from 'valdi_core/src/localization/Locale';
import { resolveLocale } from 'valdi_core/src/localization/LocaleResolver';

import 'jasmine/src/jasmine';

describe('LocaleResolver', () => {
  it('handles perfect language and region match', () => {
    const locale = new Locale('en', 'US');

    const candidateLocales = [new Locale('fr', 'FR'), new Locale('en', 'US'), new Locale('es', 'ES')];

    const index = resolveLocale([locale], candidateLocales);
    expect(index).toBe(1);
  });

  it('prioritizes language match', () => {
    const locales = [Locale.parse('en-US'), Locale.parse('zh-Hans-US')];

    const candidateLocales = [new Locale('en', undefined), new Locale('zh', 'Hans')];

    const index = resolveLocale(locales, candidateLocales);
    expect(index).toBe(0);
  });

  it('uses perfect matching locale if it exists', () => {
    const locales = [Locale.parse('en-US'), Locale.parse('zh-Hans-US')];

    const candidateLocales = [new Locale('en', undefined), new Locale('zh', 'Hans'), new Locale('en', 'US')];

    const index = resolveLocale(locales, candidateLocales);
    expect(index).toBe(2);
  });

  it('handles language match and mismatch region', () => {
    const locale = new Locale('en', 'GB');

    const candidateLocales = [
      new Locale('fr', 'FR'),
      new Locale('gu', 'IN'),
      new Locale('en', 'US'),
      new Locale('es', 'ES'),
    ];

    const index = resolveLocale([locale], candidateLocales);
    expect(index).toBe(2);
  });

  it('returns english as fallback', () => {
    const locale = new Locale('it', 'IT');

    const candidateLocales = [
      new Locale('fr', 'FR'),
      new Locale('gu', 'IN'),
      new Locale('en', 'US'),
      new Locale('es', 'ES'),
    ];

    const index = resolveLocale([locale], candidateLocales);
    expect(index).toBe(2);
  });

  it('returns english as fallback when locales empty', () => {
    const candidateLocales = [
      new Locale('fr', 'FR'),
      new Locale('gu', 'IN'),
      new Locale('en', 'US'),
      new Locale('es', 'ES'),
    ];

    const index = resolveLocale([], candidateLocales);
    expect(index).toBe(2);
  });

  it('uses secondary language if first one is not available', () => {
    const locale1 = new Locale('it', 'IT');
    const locale2 = new Locale('gu', 'IN');

    const candidateLocales = [
      new Locale('fr', 'FR'),
      new Locale('gu', 'IN'),
      new Locale('en', 'US'),
      new Locale('es', 'ES'),
    ];

    const index = resolveLocale([locale1, locale2], candidateLocales);
    expect(index).toBe(1);
  });
});
