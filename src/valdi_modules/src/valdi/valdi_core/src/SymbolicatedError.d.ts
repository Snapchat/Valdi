export interface SymbolicatedError extends Error {
  originalStack?: string;
  /** Bundle of the innermost feature frame in `stack`, when the stack names one. */
  stackBundleName?: string;
}
