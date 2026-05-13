import { RequireFunc } from 'valdi_core/src/IModuleLoader';
declare const require: RequireFunc;

export function djinniImport<T>(moduleName: string): T {
  const m = require(moduleName) as T;
  return m;
}
