import { RequireFunc } from './IModuleLoader';

declare global {
  const require: RequireFunc;
}

export interface LazyImport<TModule> {
  readonly get: TModule;
}

class LazyImportImpl<TModule> implements LazyImport<TModule> {
  private didLoad = false;
  private module: TModule | undefined;

  constructor(private readonly requireFunc: RequireFunc, private readonly path: string) {}

  get get(): TModule {
    if (!this.didLoad) {
      this.module = this.requireFunc(this.path, true) as TModule;
      this.didLoad = true;
    }

    return this.module as TModule;
  }
}

/*
 * Creates a synchronous, cached lazy module reference.
 *
 * Pass the current module's `require` so relative paths resolve from the call
 * site, and use a type-only import to keep the result strongly typed without
 * emitting an eager runtime import:
 *
 * const Symbolicator = lazyImport<typeof import('../Symbolicator')>(require, '../Symbolicator');
 * Symbolicator.get.symbolicate(error);
 */
export function lazyImport<TModule>(requireFunc: RequireFunc, path: string): LazyImport<TModule> {
  return new LazyImportImpl<TModule>(requireFunc, path);
}
