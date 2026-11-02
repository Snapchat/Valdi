/**
 * @ExportModel
 * Wrapper to use with Observables to avoid emitting / handling null values.
 */
export interface Optional<T> {
  readonly data: T | undefined;
}

export const EMPTY_OPTIONAL: Readonly<Optional<any>> = { data: undefined };
