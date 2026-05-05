import { ValdiRuntime } from './ValdiRuntime';
import { IComponent } from './IComponent';

declare const runtime: ValdiRuntime;

/**
 * By default, bridged objects from native are disposed after onDestroy()
 * is called on the root component. If you need to use a bridged object
 * for potentially longer than the lifetime of your component, you can
 * call this method to ensure the native refs won't be disposed. You should
 * call the returned disposable once you are done with the instance. It uses
 * a ref counting mecanism internally, so you can call protect as many times
 * as you want as long as they get balanced by the same number of unprotect
 * calls eventually.
 */
export function protectNativeRefs(component: IComponent): () => void {
  return protectNativeRefsForContextId(component.renderer.contextId);
}

/**
 * By default, bridged objects from native are disposed after onDestroy()
 * is called on the root component. If you need to use a bridged object
 * for potentially longer that the lifetime of your component, you can
 * call this method to ensure the native refs won't be disposed. You should
 * call the returned disposable once you are done with the instance. It uses
 * a ref counting mecanism internally, so you can call protect as many times
 * as you want as long as they get balanced by the same number of unprotect
 * calls eventually.
 */
export function protectNativeRefsForContextId(contextId: string): () => void {
  return runtime.protectNativeRefs(contextId);
}

/**
 * By default, bridged objects from native are disposed after onDestroy()
 * is called on the root component. If you need to use a bridged object
 * for potentially longer that the lifetime of your component, you can
 * call this method to ensure the native refs won't be disposed. You should
 * call the returned disposable once you are done with the instance. It uses
 * a ref counting mechanism internally, so you can call protect as many times
 * as you want as long as they get balanced by the same number of unprotect
 * calls eventually.
 */
export function protectNativeRefsForCurrentContextId(): () => void {
  return runtime.protectNativeRefs(0);
}

/**
 * By default, bridged objects from native are disposed after onDestroy()
 * is called on the root component. If you need a bridged object to last the
 * lifetime of a Promise's resolution, use this function to keep the current
 * context alive until the promise is resolved or rejected.
 */
export function asyncWithProtectedRefsForCurrentContextId<T>(asyncAction: () => Promise<T>): Promise<T> {
  const dispose = protectNativeRefsForCurrentContextId();
  try {
    return asyncAction().finally(() => dispose());
  } catch (e) {
    dispose();
    throw e;
  }
}

/**
 * Like asyncWithProtectedRefsForCurrentContextId, but uses an explicit context
 * id rather than the current context. Prefer this when the promise may be
 * created or chained inside an async callback where the "current context" is
 * not guaranteed to be the component's context (e.g. inside a .then() handler).
 */
export function asyncWithProtectedRefsForContextId<T>(contextId: string, asyncAction: () => Promise<T>): Promise<T> {
  const dispose = protectNativeRefsForContextId(contextId);
  try {
    return asyncAction().finally(() => dispose());
  } catch (e) {
    dispose();
    throw e;
  }
}

/**
 * Evaluate the given block with a global native refs scope.
 * Any native references emitted while the block is evaluated
 * will be associated with the global native references, which
 * aren't destroyed when a root component is destroyed.
 */
export function withGlobalNativeRefs<T>(cb: () => T): T {
  runtime.pushCurrentContext(undefined);
  try {
    return cb();
  } finally {
    runtime.popCurrentContext();
  }
}

/**
 * Evaluate the given block with a local native refs scope tied
 * to the given context id. Any native references emitted while
 * the block is evaluated will be associated with the native
 * references on the given context.
 */
export function withLocalNativeRefs<T>(contextId: string, cb: () => T): T {
  runtime.pushCurrentContext(contextId);
  try {
    return cb();
  } finally {
    runtime.popCurrentContext();
  }
}

/**
 * Given an async function, returns a new async function with native references that will be protected
 * for the duration of the async execution.
 * 
 * This is a convenience for:
 * 
 * ```ts
 * async function someAsyncFunction() {
 *  const dispose = protectNativeRefsForCurrentContextId();
 *  try {
 *    // do async work
 *    await someWork();
 *    // do more async work
 *    await someMoreWork()
 *  } finally {
 *    dispose();
 *  }
 * }
 * ```
 * 
 * This can be written as:
 * 
 * ```ts
 * const someAsyncFunction = withCurrentContextProtected(async () => {
 *  // do async work
 *  await someWork();
 *  // do more async work
 *  await someMoreWork();
 * })
 * ```
 */
export function withCurrentContextProtected<Args extends unknown[], R>(asyncAction: (...args: Args) => Promise<R>) {
  return (...args: Args): Promise<R> => asyncWithProtectedRefsForCurrentContextId(() => asyncAction(...args));
}

/**
 * a NativeReferencesContext holds the references that are exchanged between
 * TypeScript and Platform. When the NativeReferencesContext is disposed(),
 * all the references are removed, which prevents Platform call the exported
 * TypeScript functions, and prevents TypeScript to call proxied functions
 * provided from Platform. Every Valdi root view gets a NativeReferencesContext
 * by default, which is disposed when the root view gets destroyed.
 * A NativeReferencesContext is represented as Valdi::Context in C++,
 * SCValdiContext in Objective-C, and ValdiContext in Kotlin.
 */
export class NativeReferencesContext {
  constructor(readonly contextId: string) {}

  /**
   * Destroy the Context, which will release all the retained references.
   */
  destroy() {
    runtime.destroyContext(this.contextId);
  }

  /**
   * Make the context as current in the given callback function.
   * For the duration of the callback, any interactions between TypeScript
   * and Platform that result in references being created will end up being
   * associated with this context. For example, if TypeScript code passes
   * "function A" to Kotlin while the makeCurrent() callback function is called,
   * a reference for "function A" will be created and associated with this context.
   * That reference prevents "function A" to be garbage collected while it might
   * be in use by Platform. The reference will be removed as soon as either Platform
   * releases its own reference to "function A", or when the context is explicitly
   * destroyed through a destroy() call.
   */
  makeCurrent(cb: () => void) {
    withLocalNativeRefs(this.contextId, cb);
  }

  /**
   * Create a new NativeReferencesContext. The context
   * will have to be explicitly destroyed at some point using the
   * destroy() method.
   */
  static create(): NativeReferencesContext {
    return new NativeReferencesContext(runtime.createContext());
  }

  /**
   * Returns the NativeReferencesContext that is associated with the given component.
   * Valdi components in a tree are all associated to the same context.
   * The returned context should NOT be destroyed as it will be already destroyed
   * when the root component gets destroyed.
   */
  static fromComponent(component: IComponent): NativeReferencesContext {
    return new NativeReferencesContext(component.renderer.contextId);
  }
}
