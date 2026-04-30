import { ValdiRuntime } from "valdi_core/src/ValdiRuntime";

type NativeRef = unknown;

interface ChildContext {
  contextId: string;
  // Expose this to keep the NativeRef alive
  getNativeRef(): NativeRef;
}

declare const runtime: ValdiRuntime;

export function createChildContext(makeRef: () => NativeRef): ChildContext {
  const contextId = runtime.createContext();

  runtime.pushCurrentContext(contextId);
  const nativeRef = makeRef();
  runtime.popCurrentContext();

  return {
    contextId,
    getNativeRef() {
      return nativeRef;
    },
  }
}