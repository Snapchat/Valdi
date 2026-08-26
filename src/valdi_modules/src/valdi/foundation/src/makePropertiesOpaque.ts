import { getValdiRuntime } from 'valdi_core/src/ValdiRuntimeProvider';

const runtime = getValdiRuntime();

export interface WithOpaqueProperties<T> {}

/**
 * Iterate over every entry of the passed-in object and call runtime.makeOpaque on it.
 */
export function makePropertiesOpaque<T>(obj: T): WithOpaqueProperties<T> {
  const result = {} as any;
  for (const key in obj) {
    result[key] = runtime.makeOpaque(obj[key]);
  }
  return result;
}
