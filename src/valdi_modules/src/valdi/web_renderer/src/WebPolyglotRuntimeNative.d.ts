export function registerWebPolyglotViewClassOrThrow(
  fallbackModulePath: string,
  className: string,
  factory: (container: HTMLElement) => void,
): void;

export function tryRegisterWebPolyglotViewClass(
  fallbackModulePath: string,
  className: string,
  factory: (container: HTMLElement) => void,
): boolean;
