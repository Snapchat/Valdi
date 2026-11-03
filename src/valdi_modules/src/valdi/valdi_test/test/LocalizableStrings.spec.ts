import 'jasmine/src/jasmine';
import { buildModuleWithResolver } from 'valdi_core/src/LocalizableStrings';
import { IUnparsedLocalizableStringResolver } from 'valdi_core/src/localization/IUnparsedLocalizableStringResolver';
import {
  InlineUnparsedLocalizableStringResolver,
  LocalizationJsonFile,
} from 'valdi_core/src/localization/InlineUnparsedLocalizableStringResolver';
import { Locale } from 'valdi_core/src/localization/Locale';
import { LocalizableStringParameterType } from 'valdi_core/src/localization/LocalizableString';

function makeUnparsedLocalizableStringResolver(
  localization: Map<string, string>,
  accessHistory?: string[],
): IUnparsedLocalizableStringResolver {
  return {
    resolveUnparsedLocalizableString(key) {
      if (accessHistory) {
        accessHistory.push(key);
      }

      return localization.get(key);
    },
  };
}

interface InlineLocalization {
  locale: string;
  filePath: string;
  localization: [string, string][];
}

function makeInlineUnparsedLocalizableStringResolver(
  localizations: InlineLocalization[],
  currentLocales: string[],
): IUnparsedLocalizableStringResolver {
  return new InlineUnparsedLocalizableStringResolver(
    localizations.map(l => l.locale),
    localizations.map(l => l.filePath),
    (filePath: string): LocalizationJsonFile | undefined => {
      const inlineLocalization = localizations.find(l => l.filePath === filePath);
      if (!inlineLocalization) {
        return undefined;
      }

      const out: LocalizationJsonFile = {};
      for (const [key, value] of inlineLocalization.localization) {
        out[key] = {
          defaultMessage: value,
        };
      }

      return out;
    },
    () => currentLocales.map(Locale.parse),
  );
}

describe('LocalizableStrings', () => {
  it('canInferPropertyName', () => {
    const out = buildModuleWithResolver(
      makeUnparsedLocalizableStringResolver(new Map()),
      ['hello', 'hello_world', 'nice_'],
      [undefined, undefined, undefined],
    );

    expect(out.hello).toBeTruthy();
    expect(out.helloWorld).toBeTruthy();
    expect(out.nice).toBeTruthy();

    expect(typeof out.hello === 'function').toBeTruthy();
    expect(typeof out.helloWorld === 'function').toBeTruthy();
    expect(typeof out.nice === 'function').toBeTruthy();
  });

  it('canResolveLocalizableString', () => {
    const getLocalizedStringFunc = makeUnparsedLocalizableStringResolver(new Map([['hello_world', '42']]));

    const out = buildModuleWithResolver(getLocalizedStringFunc, ['hello_world'], [undefined]);

    const result = out.helloWorld() as string;
    expect(result).toBe('42');
  });

  it('cachesResolvedStrings', () => {
    const accessHistory: string[] = [];
    const getLocalizedStringFunc = makeUnparsedLocalizableStringResolver(
      new Map([
        ['hello_world', '42'],
        ['nice', '84'],
      ]),
      accessHistory,
    );

    const out = buildModuleWithResolver(getLocalizedStringFunc, ['hello_world', 'nice'], [undefined]);

    out.helloWorld();

    expect(accessHistory).toEqual(['hello_world']);

    out.helloWorld();

    expect(accessHistory).toEqual(['hello_world']);

    out.nice();

    expect(accessHistory).toEqual(['hello_world', 'nice']);
  });

  it('canInjectParameter', () => {
    const getLocalizedStringFunc = makeUnparsedLocalizableStringResolver(
      new Map([
        ['hello', 'Hello $0'],
        ['goodbye', '$0, goodbye!'],
      ]),
    );

    const out = buildModuleWithResolver(
      getLocalizedStringFunc,
      ['hello', 'goodbye'],
      [[LocalizableStringParameterType.STRING], [LocalizableStringParameterType.STRING]],
    );

    const result1 = out.hello('Saniul');
    expect(result1).toBe('Hello Saniul');

    const result2 = out.goodbye('Vincent');
    expect(result2).toBe('Vincent, goodbye!');
  });

  it('canInjectMultipleParameters', () => {
    const getLocalizedStringFunc = makeUnparsedLocalizableStringResolver(
      new Map([
        ['goodbye', 'Goodbye $0, see you $1!'],
        ['please', '$1, please come $0!'],
        ['chain', '$0$1$2'],
      ]),
    );

    const out = buildModuleWithResolver(
      getLocalizedStringFunc,
      ['goodbye', 'please', 'chain'],
      [
        [LocalizableStringParameterType.STRING, LocalizableStringParameterType.STRING],
        [LocalizableStringParameterType.STRING, LocalizableStringParameterType.STRING],
        [
          LocalizableStringParameterType.STRING,
          LocalizableStringParameterType.STRING,
          LocalizableStringParameterType.STRING,
        ],
      ],
    );

    const result1 = out.goodbye('Saniul', 'soon');
    expect(result1).toBe('Goodbye Saniul, see you soon!');

    const result2 = out.please('again', 'Vincent');
    expect(result2).toBe('Vincent, please come again!');

    const result3 = out.chain('what', 'the', 'hell');
    expect(result3).toBe('whatthehell');
  });

  it('throwsWhenParametersAreOutOfBounds', () => {
    const out = buildModuleWithResolver(
      makeUnparsedLocalizableStringResolver(
        new Map([
          ['one_param', '$0'],
          ['two_params', '$0 $1'],
          ['out_of_sync_params', '$0 $1'],
        ]),
      ),
      ['hello', 'world', 'goodbye'],
      [
        [LocalizableStringParameterType.STRING],
        [LocalizableStringParameterType.STRING, LocalizableStringParameterType.STRING],
        [LocalizableStringParameterType.STRING],
      ],
    );

    expect(() => {
      out.oneParam();
    }).toThrow();

    expect(() => {
      out.twoParams();
    }).toThrow();
    expect(() => {
      out.twoParams('hey');
    }).toThrow();

    expect(() => {
      out.outOfSyncParams('nice', 'boy');
    }).toThrow();
  });

  it('canFormatNumbers', () => {
    const getLocalizedStringFunc = makeUnparsedLocalizableStringResolver(new Map([['count', 'We have $0 items']]));

    const out = buildModuleWithResolver(getLocalizedStringFunc, ['count'], [[LocalizableStringParameterType.NUMBER]]);

    const result = out.count(42);
    expect(result).toBe('We have 42 items');
  });

  it('can use inline translations', () => {
    const stringResolver = makeInlineUnparsedLocalizableStringResolver(
      [{ locale: 'en', filePath: 'src/strings-en.json', localization: [['hello', 'world']] }],
      ['en'],
    );
    const out = buildModuleWithResolver(stringResolver, ['hello'], [undefined]);

    const result = out.hello();
    expect(result).toBe('world');
  });

  it('can format using inline translations', () => {
    const stringResolver = makeInlineUnparsedLocalizableStringResolver(
      [{ locale: 'en', filePath: 'src/strings-en.json', localization: [['count', 'We have {count} items']] }],
      ['en'],
    );

    const out = buildModuleWithResolver(stringResolver, ['count'], [[LocalizableStringParameterType.NUMBER]]);

    const result = out.count(42);
    expect(result).toBe('We have 42 items');
  });

  it('can format strings starting with a number', () => {
    const stringResolver = makeInlineUnparsedLocalizableStringResolver(
      [{ locale: 'en', filePath: 'src/strings-en.json', localization: [['count', '1 member have {count} items']] }],
      ['en'],
    );

    const out = buildModuleWithResolver(stringResolver, ['count'], [[LocalizableStringParameterType.NUMBER]]);

    const result = out.count(42);
    expect(result).toBe('1 member have 42 items');
  });
});
