import { IUnparsedLocalizableStringResolver } from './IUnparsedLocalizableStringResolver';
import { Locale } from './Locale';
import { resolveLocale } from './LocaleResolver';

function convertStringParametersToIndexes(str: string): string {
  let argumentIndex = 0;
  // eslint-disable-next-line no-constant-condition
  while (true) {
    const startIndex = str.indexOf('{');
    if (startIndex < 0) {
      break;
    }
    const endIndex = str.indexOf('}');
    if (endIndex < 0) {
      throw new Error(`Mismatching string parameter at ${str}`);
    }

    const prefix = str.substr(0, startIndex);
    const suffix = str.substr(endIndex + 1);

    str = `${prefix}\$${argumentIndex}${suffix}`;
    argumentIndex++;
  }

  return str;
}

export interface LocalizableJsonFileEntry {
  defaultMessage: string;
}

export interface LocalizationJsonFile {
  [key: string]: LocalizableJsonFileEntry | undefined;
}

export type GetJsonFileFn = (filePath: string) => LocalizationJsonFile | undefined;

export type GetCurrentLocalesFn = () => readonly Locale[];

export interface AvailableLocalization {
  language: string;
  path: string;
}

export class InlineUnparsedLocalizableStringResolver implements IUnparsedLocalizableStringResolver {
  private localizationJsonFile: LocalizationJsonFile | undefined;

  constructor(
    readonly availableLocales: string[],
    readonly filePaths: string[],
    readonly getJsonFileContentFn: GetJsonFileFn,
    readonly getCurrentLocalesFn: GetCurrentLocalesFn,
  ) {
    if (availableLocales.length !== filePaths.length) {
      throw new Error('Mismatched availableLocales and filePaths');
    }
  }

  withCurrentLocales(getCurrentLocales: GetCurrentLocalesFn): InlineUnparsedLocalizableStringResolver {
    return new InlineUnparsedLocalizableStringResolver(
      this.availableLocales,
      this.filePaths,
      this.getJsonFileContentFn,
      getCurrentLocales,
    );
  }

  resolveUnparsedLocalizableString(key: string): string | undefined {
    const message = this.resolveLocalizationJsonFile()[key]?.defaultMessage;
    if (!message) {
      return undefined;
    }

    return convertStringParametersToIndexes(message);
  }

  private resolveLocalizationJsonFile(): LocalizationJsonFile {
    if (!this.localizationJsonFile) {
      const currentLocales = this.getCurrentLocalesFn();
      const index = resolveLocale(currentLocales, this.availableLocales.map(Locale.parse));
      if (index < 0) {
        throw new Error(
          `Could not resolve locale with current locale [${currentLocales.join(
            ', ',
          )}] and available locales [${this.availableLocales.join(', ')}]`,
        );
      }

      const filePath = this.filePaths[index];
      const jsonFile = this.getJsonFileContentFn(filePath);
      if (!jsonFile) {
        throw new Error(`Could not resolve localization JSON file at ${filePath}`);
      }
      this.localizationJsonFile = jsonFile;
    }

    return this.localizationJsonFile;
  }
}
