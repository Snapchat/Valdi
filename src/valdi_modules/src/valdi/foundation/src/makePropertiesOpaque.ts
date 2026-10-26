import { getValdiRuntime } from 'valdi_core/src/ValdiRuntimeProvider';

export interface WithOpaqueProperties<T> {}

/**
 * Iterate over every entry of the passed-in object and call runtime.makeOpaque on it.
 */
export function makePropertiesOpaque<T>(obj: T): WithOpaqueProperties<T> {
  const result = {} as any;
  for (const key in obj) {
    result[key] = getValdiRuntime().makeOpaque(obj[key]);
  }
  return result;
}
