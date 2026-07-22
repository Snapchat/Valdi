import { getModuleLoader } from 'valdi_core/src/ModuleLoaderGlobal';

export function registerModuleFactory(name: string, module: any): void {
  getModuleLoader().registerModule(name, () => module);
}

export function mockNativeModules() {
  registerModuleFactory('valdi_core/src/ApplicationBridge', {
    observeEnteredBackground: () => {
      return {
        cancel: () => {},
      };
    },
    observeEnteredForeground: () => {
      return {
        cancel: () => {},
      };
    },
    observeKeyboardHeight: () => {
      return {
        cancel: () => {},
      };
    },
  });

  registerModuleFactory('valdi_core/src/Strings', {
    getLocalizedString(_0: string, key: string): string {
      return key;
    },
  });

  registerModuleFactory('Cof', {
    getString(key: string, fallback: string, callback: (v: string) => void): void {
      callback(fallback);
    },
    getBool(key: string, fallback: boolean, callback: (v: boolean) => void): void {
      callback(fallback);
    },
    getFloat(key: string, fallback: number, callback: (v: number) => void): void {
      callback(fallback);
    },
    getInt(key: string, fallback: number, callback: (v: number) => void): void {
      callback(fallback);
    },
    getStringSync(key: string, fallback: string): string {
      return fallback;
    },
    getBoolSync(key: string, fallback: boolean): boolean {
      return fallback;
    },
    getFloatSync(key: string, fallback: number): number {
      return fallback;
    },
    getIntSync(key: string, fallback: number): number {
      return fallback;
    },
  });
  // TODO(simon): Provide a concrete impl and remove this
  const drawingModule: any = {
    getFont(specs: any): any {
      return {
        measureText(text: any, maxWidth?: any, maxHeight?: any, maxLines?: any) {
          return { width: 0, height: 0 };
        },
      };
    },
    registerFont(fontName: any, weight: any, style: any, bytes: any) {
      // no-op
    },
  };
  registerModuleFactory('Drawing', drawingModule);

  const mockFormatterFractional = (value: number, digits: number): string => {
    if (digits < 0) {
      return value.toString();
    } else {
      return value.toFixed(digits);
    }
  };

  const mockFormatterBigNumbers = (value: number, digits: number): string => {
    const fractional = mockFormatterFractional(value, digits);
    const split = fractional.split('.');
    const integer = split[0] ? split[0] : '0';
    const rest = split[1] ? '.' + split[1] : '';
    const parts = [];
    for (let i = integer.length; i > 0; i -= 3) {
      parts.push(integer.substring(Math.max(0, i - 3), i));
    }
    parts.reverse();
    return parts.join(',') + rest;
  };

  const mockFormatterCurrenciesSymbols: { [code: string]: string } = {
    USD: '$',
    CAD: 'CA$',
    AUD: 'A$',
    EUR: '€',
    GBP: '£',
    JPY: '¥',
    CNY: 'CN¥',
    INR: '₹',
    ILS: '₪',
    RUB: '₽',
    TRY: '₺',
    KRW: '₩',
  };
  const mockFormattercurrenciesDigits: { [code: string]: number } = {
    JPY: 0,
    KRW: 0,
  };
  const mockFormatterCurrency = (value: number, currencyCode: string): string => {
    const symbol = mockFormatterCurrenciesSymbols[currencyCode] ?? currencyCode + ' ';
    const digits = mockFormattercurrenciesDigits[currencyCode] ?? 2;
    const count = mockFormatterBigNumbers(value, digits);
    return symbol + count;
  };

  registerModuleFactory('NumberFormatting', {
    formatNumber(value: number, fractionalDigits: number): string {
      return mockFormatterBigNumbers(value, fractionalDigits);
    },
    formatNumberWithCurrency(value: number, currencyCode: string): string {
      return mockFormatterCurrency(value, currencyCode);
    },
  });
}
