import type { RequireFunc } from './IModuleLoader';

declare global {
  const require: RequireFunc;
}

export interface LazyImport<TModule> {
  readonly get: TModule;
}

class LazyImportImpl<TModule> implements LazyImport<TModule> {
  private didLoad = false;
  private module: TModule | undefined;

  constructor(private readonly loader: () => TModule) {}

  get get(): TModule {
    if (!this.didLoad) {
      this.module = this.loader();
      this.didLoad = true;
    }

    return this.module as TModule;
  }
}

/*
 * Creates a synchronous, cached lazy module reference.
 *
 * Pass a loader callback so relative paths resolve from the call site, and use
 * a type-only import to keep the result strongly typed without emitting an
 * eager runtime import:
 *
 * const Symbolicator = lazyImport<typeof import('../Symbolicator')>(() => require('../Symbolicator', true));
 * Symbolicator.get.symbolicate(error);
 */
export function lazyImport<TModule>(loader: () => TModule): LazyImport<TModule> {
  return new LazyImportImpl<TModule>(loader);
}
