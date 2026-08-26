import { IModuleLoader } from './IModuleLoader';

export function getModuleLoader(): IModuleLoader {
  return (globalThis as any).moduleLoader as IModuleLoader;
}
