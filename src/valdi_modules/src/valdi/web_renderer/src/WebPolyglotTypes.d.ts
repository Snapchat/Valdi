export type WebPolyglotRegistryModule = {
  registerWebPolyglotViewClass?: (className: string, factory: (container: HTMLElement) => void) => void;
};

export type WebPolyglotRuntimeModule = {
  registerWebPolyglotViewClassOrThrow?: (
    fallbackModulePath: string,
    className: string,
    factory: (container: HTMLElement) => void,
  ) => void;
  tryRegisterWebPolyglotViewClass?: (
    fallbackModulePath: string,
    className: string,
    factory: (container: HTMLElement) => void,
  ) => boolean;
};

export type WebPolyglotModuleLoader = {
  resolveRequire: (modulePath: string) => (moduleId: string) => unknown;
};

export type GlobalWithModulePath = typeof globalThis & { module?: { path?: string } };
export type GlobalWithModuleLoader = typeof globalThis & { moduleLoader: WebPolyglotModuleLoader };
