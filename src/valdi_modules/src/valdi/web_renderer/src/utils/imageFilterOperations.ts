import { ParsedCssColor } from './cssColor';

const FILTER_TYPE_BLUR = 1;
const FILTER_TYPE_COLOR_MATRIX = 2;

export type ImageFilterOperation =
  | {
      type: 'blur';
      radius: number;
    }
  | {
      type: 'colorMatrix';
      matrix: number[];
    };

function parseFilterNumbers(value: unknown): number[] | undefined {
  if (Array.isArray(value)) {
    const numbers = value.map(Number);
    return numbers.every(Number.isFinite) ? numbers : undefined;
  }
  if (typeof value !== 'string') {
    return undefined;
  }
  const trimmed = value.trim();
  if (trimmed.length === 0) {
    return [];
  }
  let parsed: unknown;
  try {
    parsed = trimmed.startsWith('[') ? JSON.parse(trimmed) : trimmed.split(',');
  } catch (_error) {
    return undefined;
  }
  if (!Array.isArray(parsed)) {
    return undefined;
  }
  const numbers = parsed.map(Number);
  return numbers.every(Number.isFinite) ? numbers : undefined;
}

export function parseImageFilterOperations(value: unknown): ImageFilterOperation[] | undefined {
  const numbers = parseFilterNumbers(value);
  if (!numbers) {
    return undefined;
  }

  const operations: ImageFilterOperation[] = [];
  let index = 0;
  while (index < numbers.length) {
    const type = numbers[index++];
    if (type === FILTER_TYPE_BLUR) {
      const radius = numbers[index++];
      if (!Number.isFinite(radius)) {
        return undefined;
      }
      operations.push({ type: 'blur', radius });
      continue;
    }
    if (type === FILTER_TYPE_COLOR_MATRIX) {
      const matrix = numbers.slice(index, index + 20);
      if (matrix.length !== 20 || !matrix.every(Number.isFinite)) {
        return undefined;
      }
      operations.push({ type: 'colorMatrix', matrix });
      index += 20;
      continue;
    }
    return undefined;
  }

  return operations;
}

function clampImageDataChannel(value: number): number {
  return Math.max(0, Math.min(255, Math.round(value)));
}

export function applyColorMatrixToImageData(imageData: ImageData, matrix: number[]): void {
  const data = imageData.data;
  for (let i = 0; i < data.length; i += 4) {
    const r = data[i] / 255;
    const g = data[i + 1] / 255;
    const b = data[i + 2] / 255;
    const a = data[i + 3] / 255;
    data[i] = clampImageDataChannel((matrix[0] * r + matrix[1] * g + matrix[2] * b + matrix[3] * a + matrix[4]) * 255);
    data[i + 1] = clampImageDataChannel((matrix[5] * r + matrix[6] * g + matrix[7] * b + matrix[8] * a + matrix[9]) * 255);
    data[i + 2] = clampImageDataChannel(
      (matrix[10] * r + matrix[11] * g + matrix[12] * b + matrix[13] * a + matrix[14]) * 255,
    );
    data[i + 3] = clampImageDataChannel(
      (matrix[15] * r + matrix[16] * g + matrix[17] * b + matrix[18] * a + matrix[19]) * 255,
    );
  }
}

export function applyTintToImageData(imageData: ImageData, tint: ParsedCssColor): void {
  const data = imageData.data;
  for (let i = 0; i < data.length; i += 4) {
    if (data[i + 3] === 0) {
      continue;
    }
    data[i] = tint.r;
    data[i + 1] = tint.g;
    data[i + 2] = tint.b;
    data[i + 3] = clampImageDataChannel(data[i + 3] * tint.a);
  }
}
