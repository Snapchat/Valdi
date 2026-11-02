import { getLocalizedString } from '../Strings';
import { IUnparsedLocalizableStringResolver } from './IUnparsedLocalizableStringResolver';

export class ExternalUnparsedLocalizableStringResolver implements IUnparsedLocalizableStringResolver {
  constructor(readonly module: string) {}

  resolveUnparsedLocalizableString(key: string): string | undefined {
    return getLocalizedString(this.module, key);
  }
}
