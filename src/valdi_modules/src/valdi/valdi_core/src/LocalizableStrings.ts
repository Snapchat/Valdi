import { IUnparsedLocalizableStringResolver } from './localization/IUnparsedLocalizableStringResolver';
import { ExternalUnparsedLocalizableStringResolver } from './localization/ExternalUnparsedLocalizableStringResolver';
import { LocalizableStringsModule, LocalizableStringsModuleExports } from './localization/LocalizableStringsModule';
import { LocalizableStringParameterType } from './localization/LocalizableString';
import {
  GetCurrentLocalesFn,
  InlineUnparsedLocalizableStringResolver,
} from './localization/InlineUnparsedLocalizableStringResolver';
import { getValdiRuntime } from './ValdiRuntimeProvider';
import { getCurrentLocales } from './localization/LocaleResolver';

const runtime = getValdiRuntime();

export function overrideLocales(exports: any, getCurrentLocales: GetCurrentLocalesFn): void {
  const module = LocalizableStringsModule.fromExports(exports);
  const inlineStringResolver = module.stringResolver;
  if (!(inlineStringResolver instanceof InlineUnparsedLocalizableStringResolver)) {
    throw new Error('Locales can only overriden on strings using the inline localization mode');
  }

  module.stringResolver = inlineStringResolver.withCurrentLocales(getCurrentLocales);
}

export function removeOverrideLocales(exports: any) {
  overrideLocales(exports, getCurrentLocales);
}

export function buildModuleWithResolver(
  unparsedLocalizableStringsResolver: IUnparsedLocalizableStringResolver,
  strings: string[],
  parameterTypes: (LocalizableStringParameterType[] | undefined)[],
): LocalizableStringsModuleExports {
  const localizableStringsModule = new LocalizableStringsModule(unparsedLocalizableStringsResolver);

  const length = strings.length;
  for (let index = 0; index < length; index++) {
    localizableStringsModule.add(strings[index], parameterTypes[index]);
  }

  return localizableStringsModule.exports;
}

/**
 * Builds a LocalizableStringsModuleExports using localizable strings that are found
 * within the .valdimodule of the given module.
 */
export function buildInlineModule(
  module: string,
  strings: string[],
  parameterTypes: (LocalizableStringParameterType[] | undefined)[],
  availableLanguages: string[],
  filePaths: string[],
) {
  return buildModuleWithResolver(
    new InlineUnparsedLocalizableStringResolver(
      availableLanguages,
      filePaths,
      filePath => {
        // Check for bundler-preloaded JSON (web builds register lazy thunks via __valdiPreloadedStrings)
        const preloaded = (globalThis as any).__valdiPreloadedStrings?.[module]?.[filePath];
        if (typeof preloaded === 'function') return preloaded();
        return JSON.parse(runtime.getModuleEntry(module, filePath, true) as string);
      },
      getCurrentLocales,
    ),
    strings,
    parameterTypes,
  );
}

/**
 * Builds a LocalizableStringsModuleExports using localizable strings that are provided
 * externally from Localizable.strings or strings.xml files.
 */
export function buildExternalModule(
  module: string,
  strings: string[],
  parameterTypes: (LocalizableStringParameterType[] | undefined)[],
): LocalizableStringsModuleExports {
  return buildModuleWithResolver(new ExternalUnparsedLocalizableStringResolver(module), strings, parameterTypes);
}

/**
 * @deprecated Legacy buildModule function
 */
export function buildModule(
  module: string,
  strings: string[],
  parameterTypes: (LocalizableStringParameterType[] | undefined)[],
): LocalizableStringsModuleExports {
  return buildExternalModule(module, strings, parameterTypes);
}
