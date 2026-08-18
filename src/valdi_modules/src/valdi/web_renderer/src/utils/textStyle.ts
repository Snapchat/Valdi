import { AttributeApplierContext } from '../core/ElementClass';
import { applyCssColorOpacity } from './cssColor';
import { isPlainCssNumber, readPreviousWhitespaceSeparatedToken } from './cssScanner';

function readTrailingNumberToken(
  value: string,
  endIndex: number,
): { value: number; startIndex: number } | undefined {
  const token = readPreviousWhitespaceSeparatedToken(value, endIndex);
  if (!token || !isPlainCssNumber(token.token)) {
    return undefined;
  }
  return {
    value: Number(token.token),
    startIndex: token.startIndex,
  };
}

export function textShadowCssValue(
  value: string,
  context: AttributeApplierContext,
): string | undefined {
  const offsetY = readTrailingNumberToken(value, value.length);
  const offsetX = offsetY ? readTrailingNumberToken(value, offsetY.startIndex) : undefined;
  const opacity = offsetX ? readTrailingNumberToken(value, offsetX.startIndex) : undefined;
  const radius = opacity ? readTrailingNumberToken(value, opacity.startIndex) : undefined;
  if (!offsetY || !offsetX || !opacity || !radius) {
    return undefined;
  }

  const color = value.slice(0, radius.startIndex).trim();
  if (color.length === 0) {
    return undefined;
  }
  const finalColor = applyCssColorOpacity(context.resolveColor(color), String(opacity.value));
  return `${offsetX.value}px ${offsetY.value}px ${radius.value}px ${finalColor}`;
}
