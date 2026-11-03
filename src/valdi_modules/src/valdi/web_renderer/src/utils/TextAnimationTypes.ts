import type { AttributedTextAnimationTransform } from 'valdi_tsx/src/AttributedText';

export interface NormalizedTextAnimationTransform {
  key?: string;
  translationX: number;
  translationY: number;
  scale: number;
  opacity: number;
  duration: number;
  timeOffsetBetweenParts: number;
  groupIndex: number;
  partIndexInGroup: number;
  partPattern?: string;
}

const TEXT_ANIMATION_TRANSFORMS_BY_SPAN = new WeakMap<HTMLSpanElement, NormalizedTextAnimationTransform>();
const TEXT_ANIMATION_ATTACHMENT_SPANS = new WeakSet<HTMLSpanElement>();

export function setTextAnimationTransform(span: HTMLSpanElement, transform: NormalizedTextAnimationTransform): void {
  TEXT_ANIMATION_TRANSFORMS_BY_SPAN.set(span, transform);
}

export function textAnimationTransformForSpan(span: HTMLSpanElement): NormalizedTextAnimationTransform | undefined {
  return TEXT_ANIMATION_TRANSFORMS_BY_SPAN.get(span);
}

export function markTextAnimationAttachmentSpan(span: HTMLSpanElement): void {
  TEXT_ANIMATION_ATTACHMENT_SPANS.add(span);
}

export function isTextAnimationAttachmentSpan(span: HTMLSpanElement): boolean {
  return TEXT_ANIMATION_ATTACHMENT_SPANS.has(span);
}

export function normalizeTextAnimationTransform(
  value: unknown,
  groupIndex: number,
): NormalizedTextAnimationTransform | undefined {
  if (!value || typeof value !== 'object') {
    return undefined;
  }

  const transform = value as AttributedTextAnimationTransform;
  return {
    key: typeof transform.key === 'string' ? transform.key : undefined,
    translationX: numberOrDefault(transform.translationX, 0),
    translationY: numberOrDefault(transform.translationY, 0),
    scale: numberOrDefault(transform.scale, 1),
    opacity: numberOrDefault(transform.opacity, 1),
    duration: numberOrDefault(transform.duration, 0.35),
    timeOffsetBetweenParts: numberOrDefault(transform.timeOffsetBetweenParts, 0),
    groupIndex,
    partIndexInGroup: 0,
    partPattern: typeof transform.partPattern === 'string' ? transform.partPattern : undefined,
  };
}

function numberOrDefault(value: unknown, defaultValue: number): number {
  return typeof value === 'number' && Number.isFinite(value) ? value : defaultValue;
}
