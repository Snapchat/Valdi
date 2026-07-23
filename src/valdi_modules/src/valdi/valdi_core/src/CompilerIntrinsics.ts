import { AnyRenderFunction } from 'valdi_core/src/AnyRenderFunction';
import { ValdiRuntime } from './ValdiRuntime';

declare global {
  /** Placeholder for a native API version allocated automatically when a change merges. */
  const __PLACEHOLDER__: number;
}

declare const runtime: ValdiRuntime;
const PLACEHOLDER_VERSION = Number.MAX_SAFE_INTEGER;

(globalThis as typeof globalThis & { __PLACEHOLDER__: number }).__PLACEHOLDER__ = PLACEHOLDER_VERSION;

type GetTypeOfChildren<TViewModel> = TViewModel extends { children: any } ? TViewModel['children'] : never;

/**
 * Compiler annotation to mark that a render function should be provided
 * as the slot of a component through its "children" view model property.
 * This function can only be called in TSX.
 * @param value the render function that should be passed as the view model "children" property
 */
export declare function $slot<T extends AnyRenderFunction>(value: T | undefined): T | undefined;

/**
 * Compiler annotation to mark that an object should be treated as a named slots,
 * and provided to the component through its "children" view model property.
 * This function can only be called in TSX.
 * @param value the named slots object that should be passed as the view model "children" property.
 */
export declare function $namedSlots<TNamedSlots>(value: GetTypeOfChildren<TNamedSlots>): GetTypeOfChildren<TNamedSlots>;

/**
 * Compiler intrinsic to guard access to APIs annotated with @Version.
 * The compiler recognizes numeric literals and __PLACEHOLDER__, and treats the
 * guarded block as safe for declarations introduced at that version or lower.
 */
export function isVersionAtLeast(version: number): boolean {
  return version === PLACEHOLDER_VERSION || runtime.apiVersion >= version;
}
