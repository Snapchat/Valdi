/** @ExportModel */
export interface Request {
  left: number;
  right: number;
}

/** @ExportFunction */
export function calculate(request: Request, complete: (result: number) => void): void {
  complete(request.left + request.right);
}

/** @ExportFunction */
export function answer(): number {
  return 42;
}
