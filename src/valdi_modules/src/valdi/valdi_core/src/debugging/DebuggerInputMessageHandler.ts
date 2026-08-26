import { TouchEventState } from 'valdi_tsx/src/GestureEvents';
import type { ElementFrame } from 'valdi_tsx/src/Geometry';
import type { IRenderedElement } from '../IRenderedElement';
import type { IRenderedVirtualNode } from '../IRenderedVirtualNode';
import type { IRenderer } from '../IRenderer';
import type { CustomMessageHandler } from './CustomMessageHandler';

const DEBUGGER_INPUT_IDENTIFIER = 'ValdiDebuggerInput';
const DEBUGGER_INPUT_CONTRACT_VERSION = 1;
const MAX_DEBUGGER_INPUT_TRAVERSAL_NODES = 20000;

export enum DebuggerInputType {
  Capabilities = 'capabilities',
  Query = 'query',
  Tap = 'tap',
  Focus = 'focus',
  Text = 'text',
  Key = 'key',
  Scroll = 'scroll',
}

const SUPPORTED_DEBUGGER_INPUT_TYPES: ReadonlyArray<DebuggerInputType> = [
  DebuggerInputType.Capabilities,
  DebuggerInputType.Query,
  DebuggerInputType.Tap,
  DebuggerInputType.Focus,
  DebuggerInputType.Text,
  DebuggerInputType.Key,
  DebuggerInputType.Scroll,
];
const SUPPORTED_DEBUGGER_INPUT_TYPE_SET: ReadonlySet<string> = new Set<string>(SUPPORTED_DEBUGGER_INPUT_TYPES);
const DEBUGGER_INPUT_SELECTOR_FIELDS: ReadonlySet<string> = new Set<string>(['elementId', 'accessibilityId', 'tag']);
const DEBUGGER_INPUT_COMMON_FIELDS: ReadonlyArray<string> = [
  'type',
  'contextId',
  'elementId',
  'accessibilityId',
  'selector',
];
const DEBUGGER_INPUT_FIELDS_BY_TYPE: ReadonlyMap<DebuggerInputType, ReadonlySet<string>> = new Map([
  [DebuggerInputType.Capabilities, new Set<string>(['type', 'contextId'])],
  [DebuggerInputType.Query, new Set<string>(DEBUGGER_INPUT_COMMON_FIELDS)],
  [DebuggerInputType.Tap, new Set<string>([...DEBUGGER_INPUT_COMMON_FIELDS, 'x', 'y'])],
  [DebuggerInputType.Focus, new Set<string>([...DEBUGGER_INPUT_COMMON_FIELDS, 'focused'])],
  [
    DebuggerInputType.Text,
    new Set<string>([...DEBUGGER_INPUT_COMMON_FIELDS, 'text', 'value', 'selectionStart', 'selectionEnd']),
  ],
  [DebuggerInputType.Key, new Set<string>([...DEBUGGER_INPUT_COMMON_FIELDS, 'key', 'selectionStart', 'selectionEnd'])],
  [DebuggerInputType.Scroll, new Set<string>([...DEBUGGER_INPUT_COMMON_FIELDS, 'x', 'y', 'deltaX', 'deltaY'])],
]);

interface DebuggerInputSelector {
  accessibilityId?: string;
  elementId?: number;
  tag?: string;
}

interface DebuggerInputRequest {
  type?: string;
  contextId?: string;
  elementId?: number;
  accessibilityId?: string;
  selector?: string | DebuggerInputSelector;
  focused?: boolean;
  text?: string;
  value?: string;
  key?: string;
  selectionStart?: number;
  selectionEnd?: number;
  x?: number;
  y?: number;
  deltaX?: number;
  deltaY?: number;
}

interface EditTextEvent {
  text: string;
  selectionStart: number;
  selectionEnd: number;
}

interface ElementMatch {
  element: IRenderedElement;
}

interface ElementCollection {
  elements: ElementMatch[];
}

interface ElementPosition {
  x: number;
  y: number;
}

interface TapTarget {
  element: IRenderedElement;
  callback: AttributeCallback;
}

interface ParentChainEntry {
  element: IRenderedElement;
  parent: IRenderedElement | undefined;
}

interface ParentFacts {
  parent: IRenderedElement | undefined;
  absolutePosition: ElementPosition;
  descendantOrigin: ElementPosition;
  blocker: InteractionBlocker | undefined;
  tapTarget: TapTarget | undefined;
  textInput: IRenderedElement | undefined;
  scrollElement: IRenderedElement | undefined;
}

interface SegmenterPart {
  index: number;
}

interface SegmenterLike {
  segment(value: string): Iterable<SegmenterPart>;
}

interface SegmenterConstructor {
  new (locales: undefined, options: { granularity: string }): SegmenterLike;
}

enum InteractionBlockReason {
  Disabled,
  TouchDisabled,
}

interface InteractionBlocker {
  element: IRenderedElement;
  reason: InteractionBlockReason;
}

export interface DebuggerElementDescriptor {
  elementId: number;
  parentElementId?: number;
  tag: string;
  accessibilityId?: string;
  accessibilityCategory?: string;
  accessibilityNavigation?: string;
  accessibilityLabel?: string;
  accessibilityHint?: string;
  accessibilityValue?: string;
  selected: boolean;
  enabled: boolean;
  focused: boolean;
  frame: ElementFrame;
  absoluteFrame: ElementFrame;
  actions: string[];
}

interface DebuggerInputResult {
  contractVersion: number;
  handled: boolean;
  type: string;
  contextId?: string;
  elementId?: number;
  accessibilityId?: string;
  action?: string;
  actionElementId?: number;
  message?: string;
  value?: string;
  selectionStart?: number;
  selectionEnd?: number;
  contentOffsetX?: number;
  contentOffsetY?: number;
  elements?: DebuggerElementDescriptor[];
  supportedTypes?: string[];
  selectorForms?: string[];
}

type AttributeCallback = (event: any) => any;

function asFiniteNumber(value: unknown, fallback: number): number {
  return typeof value === 'number' && Number.isFinite(value) ? value : fallback;
}

function asString(value: unknown): string | undefined {
  return typeof value === 'string' ? value : undefined;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function validateOptionalStringField(
  values: Record<string, unknown>,
  fieldName: string,
  displayName: string,
  requireNonEmpty: boolean,
): string | undefined {
  const value = values[fieldName];
  if (value === undefined) {
    return undefined;
  }
  if (typeof value !== 'string') {
    return `${displayName} must be a string.`;
  }
  if (requireNonEmpty && !value.length) {
    return `${displayName} must not be empty.`;
  }
  if (containsLoneSurrogate(value)) {
    return `${displayName} must contain valid Unicode.`;
  }
  return undefined;
}

function validateOptionalNumberField(
  values: Record<string, unknown>,
  fieldName: string,
  displayName: string,
  requireInteger: boolean,
): string | undefined {
  const value = values[fieldName];
  if (value === undefined) {
    return undefined;
  }
  if (typeof value !== 'number' || !Number.isFinite(value) || (requireInteger && !Number.isInteger(value))) {
    return `${displayName} must be a finite ${requireInteger ? 'integer' : 'number'}.`;
  }
  return undefined;
}

function validateSelector(request: DebuggerInputRequest): string | undefined {
  const values = request as Record<string, unknown>;
  const selectorCount = [values['elementId'], values['accessibilityId'], values['selector']].filter(
    value => value !== undefined,
  ).length;
  if (selectorCount > 1) {
    return 'Use only one of elementId, accessibilityId, or selector.';
  }

  const elementIdError = validateOptionalNumberField(values, 'elementId', 'elementId', true);
  if (elementIdError) {
    return elementIdError;
  }
  const accessibilityIdError = validateOptionalStringField(values, 'accessibilityId', 'accessibilityId', true);
  if (accessibilityIdError) {
    return accessibilityIdError;
  }

  const selector = values['selector'];
  if (selector === undefined) {
    return undefined;
  }
  if (typeof selector === 'string') {
    return validateOptionalStringField(values, 'selector', 'selector', true);
  }
  if (!isRecord(selector)) {
    return 'selector must be a string or an object.';
  }
  const unknownField = Object.keys(selector)
    .sort()
    .find(fieldName => !DEBUGGER_INPUT_SELECTOR_FIELDS.has(fieldName));
  if (unknownField) {
    return `Unsupported selector field '${unknownField}'.`;
  }
  if (!Object.keys(selector).some(fieldName => selector[fieldName] !== undefined)) {
    return 'selector object must include elementId, accessibilityId, or tag.';
  }

  return (
    validateOptionalNumberField(selector, 'elementId', 'selector.elementId', true) ??
    validateOptionalStringField(selector, 'accessibilityId', 'selector.accessibilityId', true) ??
    validateOptionalStringField(selector, 'tag', 'selector.tag', true)
  );
}

function validateSelection(request: DebuggerInputRequest): string | undefined {
  const values = request as Record<string, unknown>;
  return (
    validateOptionalNumberField(values, 'selectionStart', 'selectionStart', true) ??
    validateOptionalNumberField(values, 'selectionEnd', 'selectionEnd', true)
  );
}

function validateCoordinates(request: DebuggerInputRequest): string | undefined {
  const values = request as Record<string, unknown>;
  return validateOptionalNumberField(values, 'x', 'x', false) ?? validateOptionalNumberField(values, 'y', 'y', false);
}

function validateRequest(request: DebuggerInputRequest): string | undefined {
  const values = request as Record<string, unknown>;
  if (typeof values['type'] !== 'string') {
    return 'Debugger input type must be a string.';
  }
  if (!SUPPORTED_DEBUGGER_INPUT_TYPE_SET.has(values['type'])) {
    return `Unsupported debugger input type '${values['type']}'.`;
  }
  const type = values['type'] as DebuggerInputType;
  const supportedFields = DEBUGGER_INPUT_FIELDS_BY_TYPE.get(type)!;
  const unknownField = Object.keys(values)
    .sort()
    .find(fieldName => !supportedFields.has(fieldName));
  if (unknownField) {
    return `Field '${unknownField}' is not supported for ${type} input.`;
  }

  const contextIdError = validateOptionalStringField(values, 'contextId', 'contextId', true);
  if (contextIdError) {
    return contextIdError;
  }
  const selectorError = validateSelector(request);
  if (selectorError) {
    return selectorError;
  }

  if (values['focused'] !== undefined && typeof values['focused'] !== 'boolean') {
    return 'focused must be a boolean.';
  }
  const textError = validateOptionalStringField(values, 'text', 'text', false);
  if (textError) {
    return textError;
  }
  const valueError = validateOptionalStringField(values, 'value', 'value', false);
  if (valueError) {
    return valueError;
  }
  const keyError = validateOptionalStringField(values, 'key', 'key', true);
  if (keyError) {
    return keyError;
  }
  const selectionError = validateSelection(request);
  if (selectionError) {
    return selectionError;
  }
  const coordinateError = validateCoordinates(request);
  if (coordinateError) {
    return coordinateError;
  }
  const deltaError =
    validateOptionalNumberField(values, 'deltaX', 'deltaX', false) ??
    validateOptionalNumberField(values, 'deltaY', 'deltaY', false);
  if (deltaError) {
    return deltaError;
  }

  if (request.type === DebuggerInputType.Text && values['text'] === undefined && values['value'] === undefined) {
    return 'Text input requires a string text or value.';
  }
  if (request.type === DebuggerInputType.Text && values['text'] !== undefined && values['value'] !== undefined) {
    return 'Use only one of text or value.';
  }
  if (request.type === DebuggerInputType.Key && values['key'] === undefined) {
    return 'Key input requires a string key.';
  }
  return undefined;
}

function getAttributeCallback(element: IRenderedElement, name: string): AttributeCallback | undefined {
  const value = element.getAttribute(name);
  return typeof value === 'function' ? value : undefined;
}

class DebuggerTraversalFailure extends Error {}

class DebuggerInputTraversal {
  private remainingWork = MAX_DEBUGGER_INPUT_TRAVERSAL_NODES;
  private readonly admittedVirtualNodes = new Set<IRenderedVirtualNode>();
  private readonly representedElements = new Set<IRenderedElement>();
  private readonly admittedParentElements = new Set<IRenderedElement>();
  private readonly parentFacts = new Map<IRenderedElement, ParentFacts>();

  collectElements(renderer: IRenderer): ElementMatch[] {
    const root = renderer.getRootVirtualNode();
    if (!root) {
      return [];
    }
    this.reserveWork(1);
    this.admitVirtualNode(root);
    const elements: ElementMatch[] = [];
    const pending: IRenderedVirtualNode[] = [root];
    while (pending.length) {
      const node = pending.pop()!;
      const childCount = node.children.length;
      this.reserveWork(childCount);
      if (node.element) {
        elements.push({ element: node.element });
      }
      for (let index = childCount - 1; index >= 0; index -= 1) {
        const child = node.children[index]!;
        this.admitVirtualNode(child);
        pending.push(child);
      }
    }
    return elements;
  }

  factsFor(element: IRenderedElement): ParentFacts {
    const existingFacts = this.parentFacts.get(element);
    if (existingFacts) {
      return existingFacts;
    }

    const chain: ParentChainEntry[] = [];
    const chainElements = new Set<IRenderedElement>();
    let current: IRenderedElement | undefined = element;
    let inheritedFacts: ParentFacts | undefined;
    while (current) {
      const cachedFacts = this.parentFacts.get(current);
      if (cachedFacts) {
        inheritedFacts = cachedFacts;
        break;
      }
      if (chainElements.has(current)) {
        throw new DebuggerTraversalFailure('Debugger input element ancestry contains a cycle.');
      }
      chainElements.add(current);
      this.admitParentElement(current);
      const parent: IRenderedElement | undefined = current.parent;
      chain.push({ element: current, parent });
      current = parent;
    }

    for (let index = chain.length - 1; index >= 0; index -= 1) {
      const entry = chain[index]!;
      inheritedFacts = this.makeParentFacts(entry, inheritedFacts);
      this.parentFacts.set(entry.element, inheritedFacts);
    }
    return this.parentFacts.get(element)!;
  }

  private reserveWork(work: number): void {
    if (work > this.remainingWork) {
      throw new DebuggerTraversalFailure(
        `Debugger input traversal exceeds the ${MAX_DEBUGGER_INPUT_TRAVERSAL_NODES}-node work limit.`,
      );
    }
    this.remainingWork -= work;
  }

  private admitVirtualNode(node: IRenderedVirtualNode): void {
    if (this.admittedVirtualNodes.has(node)) {
      throw new DebuggerTraversalFailure('Debugger input render tree contains a cycle or repeated node.');
    }
    this.admittedVirtualNodes.add(node);
    if (node.element) {
      this.representedElements.add(node.element);
    }
  }

  private admitParentElement(element: IRenderedElement): void {
    if (this.representedElements.has(element) || this.admittedParentElements.has(element)) {
      return;
    }
    this.reserveWork(1);
    this.admittedParentElements.add(element);
  }

  private makeParentFacts(entry: ParentChainEntry, parentFacts: ParentFacts | undefined): ParentFacts {
    const { element, parent } = entry;
    const frame = element.frame;
    const hasFrame = frame !== undefined;
    const parentOrigin = parentFacts?.descendantOrigin;
    const absolutePosition = hasFrame
      ? {
          x:
            frame.x +
            asFiniteNumber(element.getAttribute('translationX'), 0) +
            (parentOrigin === undefined ? 0 : parentOrigin.x),
          y:
            frame.y +
            asFiniteNumber(element.getAttribute('translationY'), 0) +
            (parentOrigin === undefined ? 0 : parentOrigin.y),
        }
      : { x: 0, y: 0 };
    const ownBlocker = interactionBlockerForElement(element);
    const onTap = getAttributeCallback(element, 'onTap');
    return {
      parent,
      absolutePosition,
      descendantOrigin: hasFrame
        ? {
            x: absolutePosition.x - asFiniteNumber(element.getAttribute('contentOffsetX'), 0),
            y: absolutePosition.y - asFiniteNumber(element.getAttribute('contentOffsetY'), 0),
          }
        : { x: 0, y: 0 },
      blocker: ownBlocker ?? parentFacts?.blocker,
      tapTarget: onTap ? { element, callback: onTap } : parentFacts?.tapTarget,
      textInput: element.tag === 'textfield' || element.tag === 'textview' ? element : parentFacts?.textInput,
      scrollElement: isScrollElement(element) ? element : parentFacts?.scrollElement,
    };
  }
}

function selectorFromRequest(request: DebuggerInputRequest): DebuggerInputSelector | undefined {
  const selector = request.selector;
  if (typeof selector === 'object' && selector !== null) {
    return selector;
  }
  if (typeof selector === 'string') {
    const accessibilityAttributeMatch = selector.match(/^\[accessibilityId=(?:"([^"]+)"|'([^']+)')\]$/);
    if (accessibilityAttributeMatch) {
      return { accessibilityId: accessibilityAttributeMatch[1] ?? accessibilityAttributeMatch[2] };
    }
    return { accessibilityId: selector.startsWith('#') ? selector.slice(1) : selector };
  }
  if (request.accessibilityId !== undefined) {
    return { accessibilityId: request.accessibilityId };
  }
  if (request.elementId !== undefined) {
    return { elementId: request.elementId };
  }
  return undefined;
}

function matchesSelector(element: IRenderedElement, selector: DebuggerInputSelector): boolean {
  if (selector.elementId !== undefined && element.id !== selector.elementId) {
    return false;
  }
  if (selector.accessibilityId !== undefined && element.getAttribute('accessibilityId') !== selector.accessibilityId) {
    return false;
  }
  return selector.tag === undefined || element.tag === selector.tag;
}

function getElementsForSelector(
  renderer: IRenderer,
  selector: DebuggerInputSelector | undefined,
  traversal: DebuggerInputTraversal,
): ElementCollection {
  if (selector?.elementId !== undefined) {
    const element = renderer.getElementForId(selector.elementId);
    return {
      elements: element && matchesSelector(element, selector) ? [{ element }] : [],
    };
  }

  const elements = traversal.collectElements(renderer);
  if (!selector) {
    return { elements };
  }
  return {
    elements: elements.filter(match => matchesSelector(match.element, selector)),
  };
}

function actionsForElement(element: IRenderedElement): string[] {
  const actions: string[] = [];
  if (getAttributeCallback(element, 'onTap')) {
    actions.push(DebuggerInputType.Tap);
  }
  if (element.tag === 'textfield' || element.tag === 'textview') {
    actions.push(DebuggerInputType.Focus, DebuggerInputType.Text, DebuggerInputType.Key);
  }
  if (isScrollElement(element)) {
    actions.push(DebuggerInputType.Scroll);
  }
  return actions;
}

function describeElement(element: IRenderedElement, facts: ParentFacts): DebuggerElementDescriptor {
  const absolutePosition = facts.absolutePosition;
  const value = asString(element.getAttribute('value'));
  return {
    elementId: element.id,
    parentElementId: facts.parent?.id,
    tag: element.tag,
    accessibilityId: asString(element.getAttribute('accessibilityId')),
    accessibilityCategory: asString(element.getAttribute('accessibilityCategory')),
    accessibilityNavigation: asString(element.getAttribute('accessibilityNavigation')),
    accessibilityLabel: asString(element.getAttribute('accessibilityLabel')),
    accessibilityHint: asString(element.getAttribute('accessibilityHint')),
    accessibilityValue: asString(element.getAttribute('accessibilityValue')) ?? value,
    selected: element.getAttribute('accessibilityStateSelected') === true,
    enabled: facts.blocker === undefined,
    focused: element.getAttribute('focused') === true,
    frame: element.frame,
    absoluteFrame: {
      x: absolutePosition.x,
      y: absolutePosition.y,
      width: element.frame.width,
      height: element.frame.height,
    },
    actions: actionsForElement(element),
  };
}

function makeResult(request: DebuggerInputRequest, handled: boolean): DebuggerInputResult {
  return {
    contractVersion: DEBUGGER_INPUT_CONTRACT_VERSION,
    handled,
    type: typeof request.type === 'string' ? request.type : '',
    contextId: typeof request.contextId === 'string' ? request.contextId : undefined,
  };
}

function interactionBlockerForElement(element: IRenderedElement): InteractionBlocker | undefined {
  if (element.getAttribute('enabled') === false || element.getAttribute('accessibilityStateDisabled') === true) {
    return { element, reason: InteractionBlockReason.Disabled };
  }
  if (element.getAttribute('touchEnabled') === false) {
    return { element, reason: InteractionBlockReason.TouchDisabled };
  }
  return undefined;
}

function isScrollElement(element: IRenderedElement): boolean {
  return (
    element.tag === 'scroll' ||
    element.getAttribute('contentOffsetX') !== undefined ||
    element.getAttribute('contentOffsetY') !== undefined
  );
}

function makeInteractionBlockedResult(
  request: DebuggerInputRequest,
  selectedElement: IRenderedElement,
  actionElement: IRenderedElement,
  blocker: InteractionBlocker,
): DebuggerInputResult {
  const message =
    blocker.reason === InteractionBlockReason.TouchDisabled
      ? `Element ${blocker.element.id} has touchEnabled=false.`
      : `Element ${blocker.element.id} is disabled.`;
  return {
    ...makeResult(request, false),
    elementId: selectedElement.id,
    actionElementId: actionElement.id,
    message,
  };
}

function makeEditTextEvent(element: IRenderedElement, request: DebuggerInputRequest, text: string): EditTextEvent {
  const selection = element.getAttribute('selection');
  const selectionStartFallback = Array.isArray(selection) ? asFiniteNumber(selection[0], text.length) : text.length;
  const selectionEndFallback = Array.isArray(selection) ? asFiniteNumber(selection[1], text.length) : text.length;
  let selectionStart = Math.max(
    0,
    Math.min(text.length, Math.trunc(asFiniteNumber(request.selectionStart, selectionStartFallback))),
  );
  let selectionEnd = Math.max(
    selectionStart,
    Math.min(text.length, Math.trunc(asFiniteNumber(request.selectionEnd, selectionEndFallback))),
  );

  if (selectionStart === selectionEnd) {
    selectionStart = boundaryAtOrBefore(text, selectionStart);
    selectionEnd = selectionStart;
  } else {
    selectionStart = boundaryAtOrBefore(text, selectionStart);
    selectionEnd = boundaryAtOrAfter(text, selectionEnd);
  }
  return { text, selectionStart, selectionEnd };
}

function isHighSurrogate(codeUnit: number): boolean {
  return codeUnit >= 0xd800 && codeUnit <= 0xdbff;
}

function isLowSurrogate(codeUnit: number): boolean {
  return codeUnit >= 0xdc00 && codeUnit <= 0xdfff;
}

function containsLoneSurrogate(value: string): boolean {
  for (let index = 0; index < value.length; index += 1) {
    const codeUnit = value.charCodeAt(index);
    if (isHighSurrogate(codeUnit)) {
      if (index + 1 >= value.length || !isLowSurrogate(value.charCodeAt(index + 1))) {
        return true;
      }
      index += 1;
    } else if (isLowSurrogate(codeUnit)) {
      return true;
    }
  }
  return false;
}

function isCombiningCodePoint(codePoint: number): boolean {
  return (
    (codePoint >= 0x0300 && codePoint <= 0x036f) ||
    (codePoint >= 0x0483 && codePoint <= 0x0489) ||
    (codePoint >= 0x0591 && codePoint <= 0x05bd) ||
    codePoint === 0x05bf ||
    (codePoint >= 0x05c1 && codePoint <= 0x05c2) ||
    (codePoint >= 0x0610 && codePoint <= 0x061a) ||
    (codePoint >= 0x064b && codePoint <= 0x065f) ||
    (codePoint >= 0x0670 && codePoint <= 0x0670) ||
    (codePoint >= 0x06d6 && codePoint <= 0x06ed) ||
    (codePoint >= 0x1ab0 && codePoint <= 0x1aff) ||
    (codePoint >= 0x1dc0 && codePoint <= 0x1dff) ||
    (codePoint >= 0x20d0 && codePoint <= 0x20ff) ||
    (codePoint >= 0xfe20 && codePoint <= 0xfe2f)
  );
}

function isGraphemeExtension(codePoint: number): boolean {
  return (
    isCombiningCodePoint(codePoint) ||
    (codePoint >= 0xfe00 && codePoint <= 0xfe0f) ||
    (codePoint >= 0xe0100 && codePoint <= 0xe01ef) ||
    (codePoint >= 0x1f3fb && codePoint <= 0x1f3ff) ||
    codePoint === 0x20e3
  );
}

function isRegionalIndicator(codePoint: number): boolean {
  return codePoint >= 0x1f1e6 && codePoint <= 0x1f1ff;
}

function visitFallbackGraphemeBoundaries(value: string, visitor: (index: number) => boolean): void {
  if (!visitor(0) || !value.length) {
    return;
  }

  let previousCodePoint = value.codePointAt(0)!;
  let currentIndex = previousCodePoint > 0xffff ? 2 : 1;
  let regionalIndicatorCount = isRegionalIndicator(previousCodePoint) ? 1 : 0;
  while (currentIndex < value.length) {
    const currentCodePoint = value.codePointAt(currentIndex)!;
    const joinsPrevious =
      (previousCodePoint === 0x000d && currentCodePoint === 0x000a) ||
      isGraphemeExtension(currentCodePoint) ||
      previousCodePoint === 0x200d ||
      currentCodePoint === 0x200d ||
      (isRegionalIndicator(previousCodePoint) &&
        isRegionalIndicator(currentCodePoint) &&
        regionalIndicatorCount % 2 === 1);
    if (!joinsPrevious && !visitor(currentIndex)) {
      return;
    }
    if (isRegionalIndicator(currentCodePoint)) {
      regionalIndicatorCount = isRegionalIndicator(previousCodePoint) ? regionalIndicatorCount + 1 : 1;
    } else if (!isGraphemeExtension(currentCodePoint)) {
      regionalIndicatorCount = 0;
    }
    previousCodePoint = currentCodePoint;
    currentIndex += currentCodePoint > 0xffff ? 2 : 1;
  }
  visitor(value.length);
}

function visitGraphemeBoundaries(value: string, visitor: (index: number) => boolean): void {
  const segmenterConstructor =
    typeof Intl === 'undefined' ? undefined : (Intl as unknown as { Segmenter?: SegmenterConstructor }).Segmenter;
  if (segmenterConstructor) {
    try {
      const segmenter = new segmenterConstructor(undefined, { granularity: 'grapheme' });
      let lastBoundary = -1;
      for (const part of segmenter.segment(value)) {
        lastBoundary = part.index;
        if (!visitor(part.index)) {
          return;
        }
      }
      if (lastBoundary < 0 && !visitor(0)) {
        return;
      }
      if (lastBoundary < 0) {
        lastBoundary = 0;
      }
      if (lastBoundary !== value.length) {
        visitor(value.length);
      }
      return;
    } catch {
      // Older Valdi runtimes may expose Intl without Segmenter support for grapheme granularity.
    }
  }
  visitFallbackGraphemeBoundaries(value, visitor);
}

function boundaryAtOrBefore(value: string, index: number): number {
  let candidate = 0;
  visitGraphemeBoundaries(value, boundary => {
    if (boundary > index) {
      return false;
    }
    candidate = boundary;
    return true;
  });
  return candidate;
}

function boundaryAtOrAfter(value: string, index: number): number {
  let candidate = value.length;
  visitGraphemeBoundaries(value, boundary => {
    if (boundary >= index) {
      candidate = boundary;
      return false;
    }
    return true;
  });
  return candidate;
}

function previousGraphemeBoundary(value: string, index: number): number {
  let previousBoundary = 0;
  visitGraphemeBoundaries(value, boundary => {
    if (boundary >= index) {
      return false;
    }
    previousBoundary = boundary;
    return true;
  });
  return previousBoundary;
}

function nextGraphemeBoundary(value: string, index: number): number {
  return boundaryAtOrAfter(value, index + 1);
}

function isSinglePrintableGrapheme(key: string): boolean {
  if (!key.length || containsLoneSurrogate(key)) {
    return false;
  }
  let boundaryCount = 0;
  visitGraphemeBoundaries(key, () => {
    boundaryCount += 1;
    return boundaryCount <= 2;
  });
  if (boundaryCount !== 2) {
    return false;
  }
  let index = 0;
  while (index < key.length) {
    const codePoint = key.codePointAt(index)!;
    if (codePoint < 0x20 || (codePoint >= 0x7f && codePoint <= 0x9f)) {
      return false;
    }
    index += codePoint > 0xffff ? 2 : 1;
  }
  return true;
}

function closesWhenReturnKeyPressed(element: IRenderedElement): boolean {
  const configuredValue = element.getAttribute('closesWhenReturnKeyPressed');
  return typeof configuredValue === 'boolean' ? configuredValue : element.tag === 'textfield';
}

function applyText(
  element: IRenderedElement,
  request: DebuggerInputRequest,
  requestedText: string,
): DebuggerInputResult {
  let event = makeEditTextEvent(element, request, requestedText);
  const onWillChange = getAttributeCallback(element, 'onWillChange');
  if (onWillChange) {
    const replacement = onWillChange(event);
    if (isRecord(replacement) && replacement['text'] !== undefined) {
      if (typeof replacement['text'] !== 'string' || containsLoneSurrogate(replacement['text'])) {
        return {
          ...makeResult(request, false),
          elementId: element.id,
          actionElementId: element.id,
          message: `Element ${element.id} returned invalid Unicode text from onWillChange.`,
        };
      }
      const replacementRequest = replacement as DebuggerInputRequest;
      const replacementSelectionError = validateSelection(replacementRequest);
      if (replacementSelectionError) {
        return {
          ...makeResult(request, false),
          elementId: element.id,
          actionElementId: element.id,
          message: `Element ${element.id} returned invalid onWillChange selection: ${replacementSelectionError}`,
        };
      }
      event = makeEditTextEvent(element, replacementRequest, replacement['text']);
    }
  }

  element.setAttributes({
    value: event.text,
    selection: [event.selectionStart, event.selectionEnd],
  });
  const onChange = getAttributeCallback(element, 'onChange');
  if (onChange) {
    onChange(event);
  }

  return {
    ...makeResult(request, true),
    elementId: element.id,
    accessibilityId: asString(element.getAttribute('accessibilityId')),
    action: 'onChange',
    actionElementId: element.id,
    value: event.text,
    selectionStart: event.selectionStart,
    selectionEnd: event.selectionEnd,
  };
}

function applyKey(element: IRenderedElement, request: DebuggerInputRequest): DebuggerInputResult {
  const key = request.key as string;
  const currentText = asString(element.getAttribute('value')) ?? '';
  if (containsLoneSurrogate(currentText)) {
    return {
      ...makeResult(request, false),
      elementId: element.id,
      actionElementId: element.id,
      message: `Element ${element.id} contains invalid Unicode text.`,
    };
  }
  const currentEvent = makeEditTextEvent(element, request, currentText);

  if (key === 'Enter' || key === 'Return') {
    const onReturn = getAttributeCallback(element, 'onReturn');
    let finalEvent = currentEvent;
    let insertedNewline = false;
    if (element.tag === 'textview' && element.getAttribute('ignoreNewlines') !== true) {
      const nextText =
        currentText.slice(0, currentEvent.selectionStart) + '\n' + currentText.slice(currentEvent.selectionEnd);
      const caret = currentEvent.selectionStart + 1;
      const textResult = applyText(element, { ...request, selectionStart: caret, selectionEnd: caret }, nextText);
      if (!textResult.handled) {
        return textResult;
      }
      finalEvent = {
        text: textResult.value ?? nextText,
        selectionStart: textResult.selectionStart ?? caret,
        selectionEnd: textResult.selectionEnd ?? caret,
      };
      insertedNewline = true;
    }
    const closesOnReturn = closesWhenReturnKeyPressed(element);
    if (closesOnReturn) {
      element.setAttribute('focused', false);
    }
    if (onReturn) {
      onReturn(finalEvent);
    }
    return {
      ...makeResult(request, true),
      elementId: element.id,
      accessibilityId: asString(element.getAttribute('accessibilityId')),
      action: onReturn ? 'onReturn' : insertedNewline ? 'onChange' : closesOnReturn ? 'focused' : 'return',
      actionElementId: element.id,
      value: finalEvent.text,
      selectionStart: finalEvent.selectionStart,
      selectionEnd: finalEvent.selectionEnd,
    };
  }

  if (key === 'Escape') {
    element.setAttribute('focused', false);
    return {
      ...makeResult(request, true),
      elementId: element.id,
      accessibilityId: asString(element.getAttribute('accessibilityId')),
      action: 'focused',
      actionElementId: element.id,
      value: currentText,
    };
  }

  if (key === 'Backspace' || key === 'Delete') {
    const onWillDelete = getAttributeCallback(element, 'onWillDelete');
    if (onWillDelete) {
      onWillDelete(currentEvent);
    }

    let start = currentEvent.selectionStart;
    let end = currentEvent.selectionEnd;
    if (start === end) {
      if (key === 'Backspace' && start > 0) {
        start = previousGraphemeBoundary(currentText, start);
      } else if (key === 'Delete' && end < currentText.length) {
        end = nextGraphemeBoundary(currentText, end);
      }
    }
    const nextText = currentText.slice(0, start) + currentText.slice(end);
    return applyText(element, { ...request, selectionStart: start, selectionEnd: start }, nextText);
  }

  if (isSinglePrintableGrapheme(key)) {
    const nextText =
      currentText.slice(0, currentEvent.selectionStart) + key + currentText.slice(currentEvent.selectionEnd);
    const caret = currentEvent.selectionStart + key.length;
    return applyText(element, { ...request, selectionStart: caret, selectionEnd: caret }, nextText);
  }

  return {
    ...makeResult(request, false),
    elementId: element.id,
    message: `Unsupported key '${key}'.`,
  };
}

export class DebuggerInputMessageHandler implements CustomMessageHandler {
  constructor(private readonly getRendererForContextId: (contextId: string) => IRenderer | undefined) {}

  messageReceived(identifier: string, body: any): Promise<any> | undefined {
    if (identifier !== DEBUGGER_INPUT_IDENTIFIER) {
      return undefined;
    }
    if (!isRecord(body)) {
      return Promise.resolve({
        ...makeResult({}, false),
        message: 'Debugger input request must be an object.',
      });
    }
    const request = body as DebuggerInputRequest;
    return Promise.resolve().then(() => this.handle(request));
  }

  private handle(request: DebuggerInputRequest): DebuggerInputResult {
    const validationError = validateRequest(request);
    if (validationError) {
      return {
        ...makeResult(request, false),
        message: validationError,
      };
    }

    if (request.type === DebuggerInputType.Capabilities) {
      return {
        ...makeResult(request, true),
        action: DebuggerInputType.Capabilities,
        supportedTypes: SUPPORTED_DEBUGGER_INPUT_TYPES.slice(),
        selectorForms: [
          'elementId',
          'accessibilityId',
          'selector: "#accessibilityId"',
          'selector: "[accessibilityId=\\"accessibilityId\\"]"',
          'selector: { elementId?, accessibilityId?, tag? }',
        ],
      };
    }

    if (!request.contextId) {
      return {
        ...makeResult(request, false),
        message: 'A contextId is required.',
      };
    }
    const renderer = this.getRendererForContextId(request.contextId);
    if (!renderer) {
      return {
        ...makeResult(request, false),
        message: `No Valdi renderer found for context ${request.contextId}.`,
      };
    }

    const traversal = new DebuggerInputTraversal();
    try {
      const selector = selectorFromRequest(request);
      const collection = getElementsForSelector(renderer, selector, traversal);
      const matches = collection.elements;

      if (request.type === DebuggerInputType.Query) {
        return {
          ...makeResult(request, true),
          action: DebuggerInputType.Query,
          elements: matches.map(match => describeElement(match.element, traversal.factsFor(match.element))),
        };
      }

      if (!selector) {
        return {
          ...makeResult(request, false),
          message: 'An elementId, accessibilityId, or selector is required.',
        };
      }
      if (!matches.length) {
        return {
          ...makeResult(request, false),
          message: 'No element matched the debugger input selector.',
        };
      }
      if (matches.length > 1) {
        return {
          ...makeResult(request, false),
          message: `Debugger input selector matched ${matches.length} elements; use a unique accessibilityId or elementId.`,
          elements: matches.map(match => describeElement(match.element, traversal.factsFor(match.element))),
        };
      }

      const selectedElement = matches[0]!.element;
      const selectedFacts = traversal.factsFor(selectedElement);
      if (request.type === DebuggerInputType.Tap) {
        const tapTarget = selectedFacts.tapTarget;
        if (!tapTarget) {
          return {
            ...makeResult(request, false),
            elementId: selectedElement.id,
            message: `Element ${selectedElement.id} and its ancestors do not expose onTap.`,
          };
        }
        const blocker = selectedFacts.blocker;
        if (blocker) {
          return makeInteractionBlockedResult(request, selectedElement, tapTarget.element, blocker);
        }
        const position = traversal.factsFor(tapTarget.element).absolutePosition;
        const absoluteX = request.x === undefined ? position.x + tapTarget.element.frame.width / 2 : request.x;
        const absoluteY = request.y === undefined ? position.y + tapTarget.element.frame.height / 2 : request.y;
        const localX = absoluteX - position.x;
        const localY = absoluteY - position.y;
        tapTarget.callback({
          state: TouchEventState.Ended,
          x: localX,
          y: localY,
          absoluteX: position.x + localX,
          absoluteY: position.y + localY,
          pointerCount: 1,
          pointerLocations: [{ pointerId: 0, x: localX, y: localY }],
          eventTime: Date.now() / 1000,
        });
        return {
          ...makeResult(request, true),
          elementId: selectedElement.id,
          accessibilityId: asString(selectedElement.getAttribute('accessibilityId')),
          action: 'onTap',
          actionElementId: tapTarget.element.id,
        };
      }

      if (request.type === DebuggerInputType.Focus) {
        const textInput = selectedFacts.textInput;
        if (!textInput) {
          return {
            ...makeResult(request, false),
            elementId: selectedElement.id,
            message: `Element ${selectedElement.id} is not a text input.`,
          };
        }
        const blocker = selectedFacts.blocker;
        if (blocker) {
          return makeInteractionBlockedResult(request, selectedElement, textInput, blocker);
        }
        const focused = request.focused !== false;
        textInput.setAttribute('focused', focused);
        return {
          ...makeResult(request, true),
          elementId: textInput.id,
          accessibilityId: asString(textInput.getAttribute('accessibilityId')),
          action: 'focused',
          actionElementId: textInput.id,
        };
      }

      if (request.type === DebuggerInputType.Text) {
        const textInput = selectedFacts.textInput;
        if (!textInput) {
          return {
            ...makeResult(request, false),
            elementId: selectedElement.id,
            message: `Element ${selectedElement.id} is not a text input.`,
          };
        }
        const blocker = selectedFacts.blocker;
        if (blocker) {
          return makeInteractionBlockedResult(request, selectedElement, textInput, blocker);
        }
        if (textInput.getAttribute('editable') === false) {
          return {
            ...makeResult(request, false),
            elementId: selectedElement.id,
            actionElementId: textInput.id,
            message: `Element ${textInput.id} is not editable.`,
          };
        }
        textInput.setAttribute('focused', true);
        return applyText(textInput, request, (request.text ?? request.value) as string);
      }

      if (request.type === DebuggerInputType.Key) {
        const textInput = selectedFacts.textInput;
        if (!textInput) {
          return {
            ...makeResult(request, false),
            elementId: selectedElement.id,
            message: `Element ${selectedElement.id} is not a text input.`,
          };
        }
        const blocker = selectedFacts.blocker;
        if (blocker) {
          return makeInteractionBlockedResult(request, selectedElement, textInput, blocker);
        }
        if (textInput.getAttribute('editable') === false) {
          return {
            ...makeResult(request, false),
            elementId: selectedElement.id,
            actionElementId: textInput.id,
            message: `Element ${textInput.id} is not editable.`,
          };
        }
        return applyKey(textInput, request);
      }

      if (request.type === DebuggerInputType.Scroll) {
        const scrollElement = selectedFacts.scrollElement;
        if (!scrollElement) {
          return {
            ...makeResult(request, false),
            elementId: selectedElement.id,
            message: `Element ${selectedElement.id} is not in a scroll container.`,
          };
        }
        const blocker = selectedFacts.blocker;
        if (blocker) {
          return makeInteractionBlockedResult(request, selectedElement, scrollElement, blocker);
        }
        const contentOffsetX = asFiniteNumber(scrollElement.getAttribute('contentOffsetX'), 0) + (request.deltaX ?? 0);
        const contentOffsetY = asFiniteNumber(scrollElement.getAttribute('contentOffsetY'), 0) + (request.deltaY ?? 0);
        scrollElement.setAttributes({
          contentOffsetAnimated: false,
          contentOffsetX,
          contentOffsetY,
        });
        return {
          ...makeResult(request, true),
          elementId: selectedElement.id,
          accessibilityId: asString(selectedElement.getAttribute('accessibilityId')),
          action: 'contentOffset',
          actionElementId: scrollElement.id,
          contentOffsetX,
          contentOffsetY,
        };
      }

      return {
        ...makeResult(request, false),
        elementId: selectedElement.id,
        message: `Unsupported debugger input type '${request.type ?? ''}'.`,
      };
    } catch (error) {
      if (error instanceof DebuggerTraversalFailure) {
        return {
          ...makeResult(request, false),
          message: error.message,
        };
      }
      throw error;
    }
  }
}
