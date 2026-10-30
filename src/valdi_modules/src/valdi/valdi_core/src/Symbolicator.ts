import {
  GetOrCreateSourceMapFunc,
  SymbolicatedStack,
  symbolicateStackWithFrames,
} from 'source_map/src/StackSymbolicator';
import { getModuleLoader } from './ModuleLoaderGlobal';
import { bundleNameFromStackFrames } from './StackAttribution';
import { SymbolicatedError } from './SymbolicatedError';

let symbolicateStackFunction:
  | ((stack: string, getOrCreateSourceMapFunc: GetOrCreateSourceMapFunc) => SymbolicatedStack)
  | undefined;
try {
  symbolicateStackFunction = symbolicateStackWithFrames;
} catch (err: any) {
  console.log(`symbolicateStackFunction error: ${err.message}`);
}

/**
 * Parse and symbolicate an error stack such that lines reported
 * within the stack are mapped to the original TypeScript files.
 */
export function symbolicateErrorStack(stack: string): string {
  if (!symbolicateStackFunction) {
    return stack;
  }

  return symbolicateStackFunction(stack, getModuleLoader().getOrCreateSourceMap).stack;
}

export function symbolicate(error: Error | SymbolicatedError): SymbolicatedError {
  if (!symbolicateStackFunction || !error.stack) {
    return error;
  }

  if ((error as SymbolicatedError).originalStack) {
    return error;
  }

  const symbolicated = symbolicateStackFunction(error.stack, getModuleLoader().getOrCreateSourceMap);

  const newError = new (error as any).constructor(error.message) as SymbolicatedError;
  newError.stack = symbolicated.stack;
  newError.originalStack = error.stack;
  const bundleName = bundleNameFromStackFrames(symbolicated.frames);
  if (bundleName) {
    newError.stackBundleName = bundleName;
  }

  return newError;
}
