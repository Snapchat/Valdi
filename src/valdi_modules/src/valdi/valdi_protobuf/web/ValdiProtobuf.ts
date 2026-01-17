import type { ValdiProtobufModule } from './src_symlink/ValdiProtobuf';

// Re-export types needed by headless modules
export { FieldType } from './src_symlink/ValdiProtobuf';

// Re-export @protobuf-ts/runtime types for tests (web/ is excluded from Valdi compiler checks)
export { ScalarType, RepeatType } from '@protobuf-ts/runtime';
export type {
  ValdiProtobufModule,
  IField,
  ILoadMessageResult,
  INativeMessageArena,
  INativeMessageFactory,
  INativeNamespaceEntries,
  INativeMessageIndex,
  NativeFieldValues,
} from './src_symlink/ValdiProtobuf';

/** Webpack's require.context typing (used by Bazel/webpack builds) */
interface WebpackRequireContext {
  <T = any>(path: string): T;
  keys(): string[];
  resolve(path: string): string;
  id: string;
}

interface WebpackRequire {
  context(path: string, recursive: boolean, regExp: RegExp): WebpackRequireContext;
}
declare const require: WebpackRequire;


// Lazily obtain a webpack context, without ever touching bare `require` at top level
function getWebpackContext(): WebpackRequireContext | undefined {
  return require.context('../../../src', true, /\.protodecl\.js$/);
}

/**
 * Loads the compiled protodecl JS (bytes) and returns the raw buffer.
 * The VALDIPRO header (if present) is stripped by DescriptorDatabase.
 *
 * @param path e.g. "proto.protodecl" (without ".js")
 */
function loadFn(path: string): Uint8Array {
  const context = getWebpackContext();
  if (!context) {
    throw new Error(
      "require.context is unavailable. Ensure your bundler provides it or swap in a custom loader."
    );
  }

  // NOTE: your regex is /\.protodecl\.js$/, so `path` should include `.protodecl`
  // ex: "proto.protodecl" -> "./proto.protodecl.js"
  const mod: any = context(`./${path}.js`);
  return (mod?.default ?? mod) as Uint8Array;
}

/* ---------- Headless module wiring ---------- */

import { HeadlessValdiProtobufModule } from './headless/HeadlessValdiProtobufModule';

const moduleInstance: ValdiProtobufModule = new HeadlessValdiProtobufModule(loadFn);

/* Re-export as named functions (mirrors the original CommonJS export) */
export const createArena = moduleInstance.createArena.bind(moduleInstance);
export const loadMessages = moduleInstance.loadMessages.bind(moduleInstance);
export const parseAndLoadMessages = moduleInstance.parseAndLoadMessages?.bind(moduleInstance);
export const getFieldsForMessageDescriptor =
  moduleInstance.getFieldsForMessageDescriptor.bind(moduleInstance);
export const getNamespaceEntries = moduleInstance.getNamespaceEntries.bind(moduleInstance);
export const createMessage = moduleInstance.createMessage.bind(moduleInstance);
export const decodeMessage = moduleInstance.decodeMessage.bind(moduleInstance);
export const decodeMessageAsync = moduleInstance.decodeMessageAsync.bind(moduleInstance);
export const decodeMessageDebugJSONAsync =
  moduleInstance.decodeMessageDebugJSONAsync.bind(moduleInstance);
export const encodeMessage = moduleInstance.encodeMessage.bind(moduleInstance);
export const encodeMessageAsync = moduleInstance.encodeMessageAsync.bind(moduleInstance);
export const batchEncodeMessageAsync = moduleInstance.batchEncodeMessageAsync.bind(moduleInstance);
export const encodeMessageToJSON = moduleInstance.encodeMessageToJSON.bind(moduleInstance);
export const setMessageField = moduleInstance.setMessageField.bind(moduleInstance);
export const getMessageFields = moduleInstance.getMessageFields.bind(moduleInstance);
export const copyMessage = moduleInstance.copyMessage.bind(moduleInstance);

/* Optionally export the instance (handy for testing / DI) */
export const valdiProtobufModule: ValdiProtobufModule = moduleInstance;
