import { getRenderer } from './Renderer';

export function remember<T>(factory: () => T, ...keys: unknown[]): T {
  return getRenderer().remember(factory, keys);
}
