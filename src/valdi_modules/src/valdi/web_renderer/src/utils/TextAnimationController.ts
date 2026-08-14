import {
  isTextAnimationAttachmentSpan,
  NormalizedTextAnimationTransform,
  textAnimationTransformForSpan,
} from './TextAnimationTypes';

const MAX_PART_PATTERN_CACHE_SIZE = 64;
const DEFAULT_FLUSH_MULTIPLIER = 20;

interface TextAnimationPart {
  element: HTMLElement;
  partIndex: number;
  transform: NormalizedTextAnimationTransform;
}

interface TextAnimationInstance {
  active: boolean;
  element: HTMLElement;
  key: string;
  partIndex: number;
  progress: number;
  scheduledStartTime: number;
  startTime?: number;
  timelineKey: string;
  transform: NormalizedTextAnimationTransform;
}

interface OriginalAnimationStyle {
  display: string;
  opacity: string;
  transform: string;
  transformOrigin: string;
  willChange: string;
}

type TextAnimationFrameCallback = (time: number) => void;

const ORIGINAL_STYLES_BY_ELEMENT = new WeakMap<HTMLElement, OriginalAnimationStyle>();
const PART_PATTERN_CACHE = new Map<string, RegExp>();

interface TextAnimationTimelineState {
  compressedNewAnimationStartTimesByStartDelay?: Map<number, number>;
  existingAnimationStartTime?: number;
  minimumNewAnimationStartDelay: number;
  newAnimationStartTime?: number;
  newAnimationReferenceTime?: number;
  newAnimationStartDelays: Set<number>;
}

export interface TextAnimationControllerRegistry {
  hasTextAnimationGroup(element: HTMLElement): boolean;
  nearestTextAnimationGroup(element: HTMLElement): TextAnimationGroupController | undefined;
  textAnimationParticipantForElement(element: HTMLElement): TextAnimationParticipant | undefined;
}

export function easeOutTextAnimationProgress(progress: number): number {
  const clampedProgress = Math.max(0, Math.min(progress, 1));
  return 1 - Math.pow(1 - clampedProgress, 3);
}

class TextAnimationTimeline {
  private readonly timelineStates = new Map<string, TextAnimationTimelineState>();
  private flushDurationThresholdMillis: number | undefined;
  private flushMultiplier = DEFAULT_FLUSH_MULTIPLIER;

  resetFrameState(): void {
    this.timelineStates.clear();
  }

  setFlushDurationThreshold(flushDurationThreshold: number | undefined): void {
    this.flushDurationThresholdMillis =
      flushDurationThreshold === undefined ? undefined : Math.max(flushDurationThreshold * 1000, 0);
  }

  setFlushMultiplier(flushMultiplier: number | undefined): void {
    this.flushMultiplier = flushMultiplier === undefined ? DEFAULT_FLUSH_MULTIPLIER : Math.max(flushMultiplier, 0);
  }

  recordExistingAnimationScheduledStartTime(timelineKey: string, scheduledStartTime: number): void {
    const timelineState = this.timelineStateForKey(timelineKey);
    const existingAnimationStartTime = Math.max(
      timelineState.existingAnimationStartTime ?? Number.NEGATIVE_INFINITY,
      scheduledStartTime,
    );
    if (existingAnimationStartTime !== timelineState.existingAnimationStartTime) {
      timelineState.existingAnimationStartTime = existingAnimationStartTime;
      timelineState.compressedNewAnimationStartTimesByStartDelay = undefined;
    }
  }

  recordNewAnimationStartDelay(timelineKey: string, startDelay: number): void {
    const timelineState = this.timelineStateForKey(timelineKey);
    timelineState.minimumNewAnimationStartDelay = Math.min(timelineState.minimumNewAnimationStartDelay, startDelay);
    if (!timelineState.newAnimationStartDelays.has(startDelay)) {
      timelineState.newAnimationStartDelays.add(startDelay);
      timelineState.compressedNewAnimationStartTimesByStartDelay = undefined;
    }
  }

  startTimeForNewAnimation(timelineKey: string, currentTime: number, timeOffset: number, startDelay: number): number {
    const timelineState = this.timelineStateForKey(timelineKey);
    this.recordNewAnimationStartDelay(timelineKey, startDelay);
    if (this.flushDurationThresholdMillis !== undefined) {
      return this.compressedStartTimeForNewAnimation(timelineState, currentTime, timeOffset, startDelay);
    }

    if (timelineState.newAnimationStartTime === undefined) {
      const firstScheduledStartTime =
        timelineState.existingAnimationStartTime === undefined
          ? currentTime
          : Math.max(currentTime, timelineState.existingAnimationStartTime + timeOffset);
      timelineState.newAnimationStartTime = firstScheduledStartTime - timelineState.minimumNewAnimationStartDelay;
    }
    return timelineState.newAnimationStartTime;
  }

  private compressedStartTimeForNewAnimation(
    timelineState: TextAnimationTimelineState,
    currentTime: number,
    timeOffset: number,
    startDelay: number,
  ): number {
    if (timelineState.newAnimationReferenceTime === undefined) {
      timelineState.newAnimationReferenceTime = currentTime;
    }

    const referenceTime = timelineState.newAnimationReferenceTime;
    const compressedStartTimesByStartDelay =
      timelineState.compressedNewAnimationStartTimesByStartDelay ??
      this.compressedStartTimesByStartDelay(timelineState, referenceTime, timeOffset);
    timelineState.compressedNewAnimationStartTimesByStartDelay = compressedStartTimesByStartDelay;
    return compressedStartTimesByStartDelay.get(startDelay) ?? referenceTime;
  }

  private compressedStartTimesByStartDelay(
    timelineState: TextAnimationTimelineState,
    referenceTime: number,
    timeOffset: number,
  ): Map<number, number> {
    let previousScheduledStartTime = referenceTime;
    let previousDelay = 0;
    let hasPreviousDelay = false;
    const startTimesByStartDelay = new Map<number, number>();

    if (timelineState.existingAnimationStartTime !== undefined) {
      previousScheduledStartTime = timelineState.existingAnimationStartTime;
      previousScheduledStartTime = Math.max(
        referenceTime,
        previousScheduledStartTime + this.effectiveDelta(timeOffset, previousScheduledStartTime, referenceTime),
      );
    }

    const sortedStartDelays = Array.from(timelineState.newAnimationStartDelays).sort((left, right) => left - right);
    for (let i = 0; i < sortedStartDelays.length; i++) {
      const delay = sortedStartDelays[i];
      if (timelineState.existingAnimationStartTime === undefined && !hasPreviousDelay) {
        previousScheduledStartTime = referenceTime;
      } else if (hasPreviousDelay) {
        const normalDelta = Math.max(delay - previousDelay, 0);
        previousScheduledStartTime = Math.max(
          referenceTime,
          previousScheduledStartTime + this.effectiveDelta(normalDelta, previousScheduledStartTime, referenceTime),
        );
      }
      previousDelay = delay;
      hasPreviousDelay = true;
      startTimesByStartDelay.set(delay, previousScheduledStartTime - delay);
    }

    return startTimesByStartDelay;
  }

  private effectiveDelta(normalDelta: number, previousScheduledStartTime: number, currentTime: number): number {
    const threshold = this.flushDurationThresholdMillis;
    if (threshold === undefined || normalDelta <= 0) {
      return normalDelta;
    }

    const lead = previousScheduledStartTime - currentTime;
    if (lead <= threshold) {
      return normalDelta;
    }

    const lagSeconds = (lead - threshold) / 1000;
    return normalDelta / (1 + lagSeconds * this.flushMultiplier);
  }

  private timelineStateForKey(timelineKey: string): TextAnimationTimelineState {
    let timelineState = this.timelineStates.get(timelineKey);
    if (!timelineState) {
      timelineState = {
        minimumNewAnimationStartDelay: Number.POSITIVE_INFINITY,
        newAnimationStartDelays: new Set(),
      };
      this.timelineStates.set(timelineKey, timelineState);
    }
    return timelineState;
  }
}

export class TextAnimationGroupController {
  private readonly timeline = new TextAnimationTimeline();
  private frameRequest: number | undefined;

  constructor(
    readonly element: HTMLElement,
    private readonly registry: TextAnimationControllerRegistry,
  ) {}

  setFlushDurationThreshold(flushDurationThreshold: number | undefined): void {
    this.timeline.setFlushDurationThreshold(flushDurationThreshold);
  }

  setFlushMultiplier(flushMultiplier: number | undefined): void {
    this.timeline.setFlushMultiplier(flushMultiplier);
  }

  startFrameLoopIfNeeded(): void {
    if (this.frameRequest !== undefined) {
      return;
    }
    this.frameRequest = requestTextAnimationFrame(time => this.runFrame(time));
  }

  destroy(): void {
    this.cancelFrameLoop();
  }

  private runFrame(currentTime: number): void {
    this.frameRequest = undefined;
    const participants = this.collectOrderedParticipants();
    this.timeline.resetFrameState();

    let basePartIndex = 0;
    for (let i = 0; i < participants.length; i++) {
      const participant = participants[i];
      participant.setBasePartIndex(basePartIndex);
      basePartIndex += participant.textAnimationPartCount;
      participant.prepareFrame(this.timeline);
    }

    let hasActiveAnimations = false;
    for (let i = 0; i < participants.length; i++) {
      hasActiveAnimations = participants[i].updatePreparedFrame(currentTime, this.timeline) || hasActiveAnimations;
    }

    if (hasActiveAnimations) {
      this.frameRequest = requestTextAnimationFrame(time => this.runFrame(time));
    }
  }

  private cancelFrameLoop(): void {
    if (this.frameRequest === undefined) {
      return;
    }
    cancelTextAnimationFrame(this.frameRequest);
    this.frameRequest = undefined;
  }

  private collectOrderedParticipants(): TextAnimationParticipant[] {
    const participants: TextAnimationParticipant[] = [];
    const childNodes = this.element.childNodes;
    for (let i = 0; i < childNodes.length; i++) {
      this.collectParticipantsInElement(childNodes.item(i), participants);
    }
    return participants;
  }

  private collectParticipantsInElement(node: Node | null, output: TextAnimationParticipant[]): void {
    const element = nodeAsHTMLElement(node);
    if (!element) {
      return;
    }
    if (this.registry.hasTextAnimationGroup(element)) {
      return;
    }

    const participant = this.registry.textAnimationParticipantForElement(element);
    if (participant && participant.hasTextAnimationParts()) {
      output.push(participant);
    }

    const childNodes = element.childNodes;
    for (let i = 0; i < childNodes.length; i++) {
      this.collectParticipantsInElement(childNodes.item(i), output);
    }
  }
}

export class TextAnimationParticipant {
  private readonly animations = new Map<string, TextAnimationInstance>();
  private readonly localTimeline = new TextAnimationTimeline();
  private frameRequest: number | undefined;
  private hasTextAnimationPartDefinitions = false;
  private basePartIndex = 0;

  constructor(
    readonly ownerElement: HTMLElement,
    private readonly registry: TextAnimationControllerRegistry,
  ) {}

  get textAnimationPartCount(): number {
    return this.hasTextAnimationPartDefinitions ? this.animations.size : 0;
  }

  setContainer(container: HTMLElement): void {
    const parts = buildTextAnimationParts(container);
    const activeKeys = new Set<string>();
    this.hasTextAnimationPartDefinitions = parts.length > 0;

    for (let i = 0; i < parts.length; i++) {
      const part = parts[i];
      const key = keyFor(part.partIndex, part.transform);
      activeKeys.add(key);

      let animation = this.animations.get(key);
      if (!animation) {
        animation = {
          active: true,
          element: part.element,
          key,
          partIndex: part.partIndex,
          progress: 0,
          scheduledStartTime: 0,
          timelineKey: timelineKeyFor(part.transform),
          transform: part.transform,
        };
        this.animations.set(key, animation);
      } else {
        animation.element = part.element;
        animation.partIndex = part.partIndex;
        animation.timelineKey = timelineKeyFor(part.transform);
        animation.transform = part.transform;
      }
    }

    for (const [key, animation] of this.animations) {
      if (!activeKeys.has(key)) {
        restoreAnimationStyle(animation.element);
        this.animations.delete(key);
      }
    }
  }

  hasTextAnimationParts(): boolean {
    return this.hasTextAnimationPartDefinitions;
  }

  setBasePartIndex(basePartIndex: number): void {
    this.basePartIndex = basePartIndex;
  }

  startFrameLoopIfNeeded(): void {
    const group = this.registry.nearestTextAnimationGroup(this.ownerElement);
    if (group) {
      this.cancelFrameLoop();
      this.applyFrame(currentTimeMillis());
      group.startFrameLoopIfNeeded();
      return;
    }

    this.cancelFrameLoop();
    this.runFrame(currentTimeMillis());
  }

  prepareFrame(timeline: TextAnimationTimeline): void {
    for (const animation of this.animations.values()) {
      if (animation.startTime === undefined || !animation.active) {
        if (animation.startTime === undefined) {
          timeline.recordNewAnimationStartDelay(
            animation.timelineKey,
            delayMillisFor(animation.transform, this.basePartIndex),
          );
        }
        continue;
      }
      timeline.recordExistingAnimationScheduledStartTime(animation.timelineKey, animation.scheduledStartTime);
    }
  }

  updatePreparedFrame(currentTime: number, timeline: TextAnimationTimeline): boolean {
    this.schedulePendingAnimations(currentTime, timeline);
    return this.applyFrame(currentTime);
  }

  destroy(): void {
    this.cancelFrameLoop();
    for (const animation of this.animations.values()) {
      restoreAnimationStyle(animation.element);
    }
    this.animations.clear();
    this.hasTextAnimationPartDefinitions = false;
  }

  private runFrame(currentTime: number): void {
    this.frameRequest = undefined;
    const group = this.registry.nearestTextAnimationGroup(this.ownerElement);
    if (group) {
      group.startFrameLoopIfNeeded();
      return;
    }

    this.localTimeline.resetFrameState();
    this.prepareFrame(this.localTimeline);
    const hasActiveAnimations = this.updatePreparedFrame(currentTime, this.localTimeline);
    if (hasActiveAnimations) {
      this.frameRequest = requestTextAnimationFrame(time => this.runFrame(time));
    }
  }

  private cancelFrameLoop(): void {
    if (this.frameRequest === undefined) {
      return;
    }
    cancelTextAnimationFrame(this.frameRequest);
    this.frameRequest = undefined;
  }

  private schedulePendingAnimations(currentTime: number, timeline: TextAnimationTimeline): void {
    for (const animation of this.animations.values()) {
      if (animation.startTime !== undefined) {
        continue;
      }
      const timeOffset = timeOffsetMillisFor(animation.transform);
      const startDelay = delayMillisFor(animation.transform, this.basePartIndex);
      const startTime = timeline.startTimeForNewAnimation(animation.timelineKey, currentTime, timeOffset, startDelay);
      animation.startTime = startTime;
      animation.scheduledStartTime = startTime + startDelay;
      animation.progress = 0;
      animation.active = true;
    }
  }

  private applyFrame(currentTime: number): boolean {
    let hasActiveAnimations = false;
    for (const animation of this.animations.values()) {
      const progress = progressForAnimation(animation, currentTime, this.basePartIndex);
      animation.progress = progress;
      if (progress >= 1) {
        animation.active = false;
        restoreAnimationStyle(animation.element);
        continue;
      }

      animation.active = true;
      applyAnimationStyle(animation.element, animation.transform, easeOutTextAnimationProgress(progress));
      hasActiveAnimations = true;
    }
    return hasActiveAnimations;
  }
}

function buildTextAnimationParts(container: HTMLElement): TextAnimationPart[] {
  const parts: TextAnimationPart[] = [];
  const partIndexesByGroup: number[] = [];
  const childNodes = container.childNodes;
  for (let i = 0; i < childNodes.length; i++) {
    const span = nodeAsSpan(childNodes.item(i));
    const transform = span ? textAnimationTransformForSpan(span) : undefined;
    if (!span || !transform || isNoOpStartTransform(transform)) {
      continue;
    }
    appendPartsForSpan(span, transform, partIndexesByGroup, parts);
  }
  return parts;
}

function appendPartsForSpan(
  span: HTMLSpanElement,
  transform: NormalizedTextAnimationTransform,
  partIndexesByGroup: number[],
  output: TextAnimationPart[],
): void {
  const partPattern = transform.partPattern;
  if (isTextAnimationAttachmentSpan(span) || !partPattern) {
    appendWholePart(span, transform, partIndexesByGroup, output);
    return;
  }

  const pattern = compiledPartPattern(partPattern);
  if (!pattern) {
    return;
  }

  const text = span.textContent ?? '';
  const segments: HTMLSpanElement[] = [];
  let previousEnd = 0;
  let matched = false;
  pattern.lastIndex = 0;

  while (true) {
    const match = pattern.exec(text);
    if (!match) {
      break;
    }
    const matchStart = match.index;
    const matchEnd = matchStart + match[0].length;
    if (matchEnd > matchStart) {
      appendTextSegment(text, previousEnd, matchStart, segments);
      const animatedSegment = createTextSegment(text.slice(matchStart, matchEnd));
      appendWholePart(animatedSegment, transform, partIndexesByGroup, output);
      segments.push(animatedSegment);
      previousEnd = matchEnd;
      matched = true;
    }
    if (pattern.lastIndex === matchStart) {
      pattern.lastIndex++;
    }
  }

  if (!matched) {
    return;
  }

  appendTextSegment(text, previousEnd, text.length, segments);
  span.replaceChildren(...segments);
}

function appendWholePart(
  element: HTMLElement,
  transform: NormalizedTextAnimationTransform,
  partIndexesByGroup: number[],
  output: TextAnimationPart[],
): void {
  const groupIndex = transform.groupIndex;
  const partIndexInGroup = partIndexesByGroup[groupIndex] ?? 0;
  partIndexesByGroup[groupIndex] = partIndexInGroup + 1;
  output.push({
    element,
    partIndex: output.length,
    transform: {
      ...transform,
      partIndexInGroup,
    },
  });
}

function appendTextSegment(text: string, start: number, end: number, output: HTMLSpanElement[]): void {
  if (end <= start) {
    return;
  }
  output.push(createTextSegment(text.slice(start, end)));
}

function createTextSegment(text: string): HTMLSpanElement {
  const span = document.createElement('span');
  span.textContent = text;
  return span;
}

function compiledPartPattern(partPattern: string): RegExp | undefined {
  const cachedPattern = PART_PATTERN_CACHE.get(partPattern);
  if (cachedPattern) {
    return cachedPattern;
  }

  let pattern: RegExp;
  try {
    pattern = new RegExp(partPattern, 'g');
  } catch (error) {
    console.error(`Invalid text animation partPattern: ${partPattern}`, error);
    return undefined;
  }

  if (PART_PATTERN_CACHE.size > MAX_PART_PATTERN_CACHE_SIZE) {
    PART_PATTERN_CACHE.clear();
  }
  PART_PATTERN_CACHE.set(partPattern, pattern);
  return pattern;
}

function applyAnimationStyle(
  element: HTMLElement,
  transform: NormalizedTextAnimationTransform,
  easedProgress: number,
): void {
  ensureOriginalAnimationStyle(element);
  if (!element.style.display || element.style.display === 'inline') {
    element.style.display = 'inline-block';
  }
  element.style.transformOrigin = 'center center';
  element.style.willChange = 'opacity, transform';
  element.style.opacity = String(transform.opacity + (1 - transform.opacity) * easedProgress);

  const translationX = transform.translationX * (1 - easedProgress);
  const translationY = transform.translationY * (1 - easedProgress);
  const scale = transform.scale + (1 - transform.scale) * easedProgress;
  const transforms: string[] = [];
  if (translationX !== 0) {
    transforms.push(`translateX(${translationX}px)`);
  }
  if (translationY !== 0) {
    transforms.push(`translateY(${translationY}px)`);
  }
  if (scale !== 1) {
    transforms.push(`scale(${scale})`);
  }
  element.style.transform = transforms.join(' ');
}

function ensureOriginalAnimationStyle(element: HTMLElement): void {
  if (ORIGINAL_STYLES_BY_ELEMENT.has(element)) {
    return;
  }
  ORIGINAL_STYLES_BY_ELEMENT.set(element, {
    display: element.style.display ?? '',
    opacity: element.style.opacity ?? '',
    transform: element.style.transform ?? '',
    transformOrigin: element.style.transformOrigin ?? '',
    willChange: element.style.willChange ?? '',
  });
}

function restoreAnimationStyle(element: HTMLElement): void {
  const originalStyle = ORIGINAL_STYLES_BY_ELEMENT.get(element);
  if (!originalStyle) {
    return;
  }
  element.style.display = originalStyle.display;
  element.style.opacity = originalStyle.opacity;
  element.style.transform = originalStyle.transform;
  element.style.transformOrigin = originalStyle.transformOrigin;
  element.style.willChange = originalStyle.willChange;
  ORIGINAL_STYLES_BY_ELEMENT.delete(element);
}

function progressForAnimation(animation: TextAnimationInstance, currentTime: number, basePartIndex: number): number {
  if (animation.startTime === undefined) {
    return 0;
  }
  const delayedElapsed = currentTime - animation.startTime - delayMillisFor(animation.transform, basePartIndex);
  const duration = durationMillisFor(animation.transform);
  if (duration === 0) {
    return delayedElapsed >= 0 ? 1 : 0;
  }
  if (delayedElapsed <= 0) {
    return 0;
  }
  return Math.max(0, Math.min(delayedElapsed / duration, 1));
}

function delayMillisFor(transform: NormalizedTextAnimationTransform, basePartIndex: number): number {
  const delaySeconds = transform.timeOffsetBetweenParts * (basePartIndex + transform.partIndexInGroup);
  return Math.max(delaySeconds * 1000, 0);
}

function timeOffsetMillisFor(transform: NormalizedTextAnimationTransform): number {
  return Math.max(transform.timeOffsetBetweenParts * 1000, 0);
}

function durationMillisFor(transform: NormalizedTextAnimationTransform): number {
  return Math.max(transform.duration * 1000, 0);
}

function isNoOpStartTransform(transform: NormalizedTextAnimationTransform): boolean {
  return (
    transform.translationX === 0 &&
    transform.translationY === 0 &&
    transform.scale === 1 &&
    transform.opacity === 1
  );
}

function keyFor(partIndex: number, transform: NormalizedTextAnimationTransform): string {
  return transform.key === undefined ? String(partIndex) : `${transform.key}:${partIndex}`;
}

function timelineKeyFor(transform: NormalizedTextAnimationTransform): string {
  return transform.key ?? `group:${transform.groupIndex}`;
}

function nodeAsSpan(node: Node | null): HTMLSpanElement | undefined {
  const element = nodeAsHTMLElement(node);
  if (!element || element.tagName !== 'SPAN') {
    return undefined;
  }
  return element as HTMLSpanElement;
}

function nodeAsHTMLElement(node: Node | null): HTMLElement | undefined {
  const element = node as HTMLElement | null;
  return element && typeof element === 'object' && element.style !== undefined ? element : undefined;
}

function requestTextAnimationFrame(callback: TextAnimationFrameCallback): number {
  const requestAnimationFrameFn = (globalThis as { requestAnimationFrame?: TextAnimationFrameCallbackScheduler })
    .requestAnimationFrame;
  if (typeof requestAnimationFrameFn === 'function') {
    return requestAnimationFrameFn(callback);
  }
  return setTimeout(() => callback(currentTimeMillis()), 16);
}

function cancelTextAnimationFrame(handle: number): void {
  const cancelAnimationFrameFn = (globalThis as { cancelAnimationFrame?: (handle: number) => void })
    .cancelAnimationFrame;
  if (typeof cancelAnimationFrameFn === 'function') {
    cancelAnimationFrameFn(handle);
    return;
  }
  clearTimeout(handle);
}

interface TextAnimationFrameCallbackScheduler {
  (callback: TextAnimationFrameCallback): number;
}

function currentTimeMillis(): number {
  return performance.now();
}
