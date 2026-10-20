const HTML_NAMESPACE = 'http://www.w3.org/1999/xhtml';

// Lightweight DOM support used only by the server-side HTML renderer.

import { createServerStyleDeclaration, type ServerStyleDeclaration } from './ServerStyleDeclaration';

interface ServerEventListenerObject {
  handleEvent(event: Event): void;
}

type ServerEventListener = EventListenerOrEventListenerObject | ServerEventListenerObject;

export interface ServerDOMMutationTracker {
  markMutation(): void;
}

class ServerNodeList<T extends ServerNode> {
  constructor(private readonly nodes: T[]) {}

  get length(): number {
    return this.nodes.length;
  }

  item(index: number): T | null {
    return this.nodes[index] ?? null;
  }

  [Symbol.iterator](): Iterator<T> {
    return this.nodes[Symbol.iterator]();
  }
}

class ServerHTMLCollection extends ServerNodeList<ServerElement> {}

export abstract class ServerNode {
  readonly childNodes: ServerNodeList<ServerNode>;
  parentNode: ServerNode | null = null;
  protected readonly mutableChildNodes: ServerNode[] = [];
  protected mutationTracker: ServerDOMMutationTracker | undefined;

  abstract readonly nodeType: number;
  abstract readonly nodeName: string;
  abstract cloneNode(deep?: boolean): ServerNode;

  constructor(readonly ownerDocument: ServerDocument) {
    this.childNodes = new ServerNodeList(this.mutableChildNodes);
  }

  get firstChild(): ServerNode | null {
    return this.mutableChildNodes[0] ?? null;
  }

  get lastChild(): ServerNode | null {
    return this.mutableChildNodes[this.mutableChildNodes.length - 1] ?? null;
  }

  get parentElement(): ServerElement | null {
    return this.parentNode instanceof ServerElement ? this.parentNode : null;
  }

  get textContent(): string {
    let result = '';
    for (let index = 0; index < this.mutableChildNodes.length; index++) {
      result += this.mutableChildNodes[index].textContent;
    }
    return result;
  }

  set textContent(value: string) {
    if (value === '') {
      this.replaceChildren();
    } else {
      this.replaceChildren(this.ownerDocument.createTextNode(value));
    }
  }

  appendChild<T extends ServerNode>(child: T): T {
    return this.insertBefore(child, null);
  }

  insertBefore<T extends ServerNode>(child: T, before: ServerNode | null): T {
    if (child.contains(this as unknown as Node)) {
      throw new Error('Cannot insert a node into itself or one of its descendants');
    }
    child.parentNode?.removeChild(child);
    const targetIndex = before === null ? this.mutableChildNodes.length : this.mutableChildNodes.indexOf(before);
    if (targetIndex < 0) {
      throw new Error('The reference node is not a child of this node');
    }
    child.parentNode = this;
    child.setMutationTracker(this.mutationTracker);
    this.mutableChildNodes.splice(targetIndex, 0, child);
    this.didMutate();
    return child;
  }

  removeChild<T extends ServerNode>(child: T): T {
    const index = this.mutableChildNodes.indexOf(child);
    if (index < 0) {
      throw new Error('The node to remove is not a child of this node');
    }
    this.mutableChildNodes.splice(index, 1);
    child.parentNode = null;
    child.setMutationTracker(undefined);
    this.didMutate();
    return child;
  }

  replaceChildren(...children: Array<ServerNode | string>): void {
    const normalizedChildren: ServerNode[] = [];
    for (let index = 0; index < children.length; index++) {
      const child = children[index];
      normalizedChildren.push(typeof child === 'string' ? this.ownerDocument.createTextNode(child) : child);
    }
    for (let index = 0; index < this.mutableChildNodes.length; index++) {
      const child = this.mutableChildNodes[index];
      child.parentNode = null;
      child.setMutationTracker(undefined);
    }
    this.mutableChildNodes.length = 0;
    for (let index = 0; index < normalizedChildren.length; index++) {
      const child = normalizedChildren[index];
      child.parentNode?.removeChild(child);
      child.parentNode = this;
      child.setMutationTracker(this.mutationTracker);
      this.mutableChildNodes.push(child);
    }
    this.didMutate();
  }

  remove(): void {
    this.parentNode?.removeChild(this);
  }

  contains(node: Node | null): boolean {
    if (node === (this as unknown as Node)) {
      return true;
    }
    for (let index = 0; index < this.mutableChildNodes.length; index++) {
      if (this.mutableChildNodes[index].contains(node)) {
        return true;
      }
    }
    return false;
  }

  getRootNode(): ServerNode {
    let root: ServerNode = this;
    while (root.parentNode) {
      root = root.parentNode;
    }
    return root;
  }

  setMutationTracker(tracker: ServerDOMMutationTracker | undefined): void {
    this.mutationTracker = tracker;
    for (let index = 0; index < this.mutableChildNodes.length; index++) {
      this.mutableChildNodes[index].setMutationTracker(tracker);
    }
  }

  protected didMutate(): void {
    this.mutationTracker?.markMutation();
  }
}

export class ServerText extends ServerNode {
  readonly nodeType = 3;
  readonly nodeName = '#text';
  private value: string;

  constructor(ownerDocument: ServerDocument, value: string) {
    super(ownerDocument);
    this.value = value;
  }

  override get textContent(): string {
    return this.value;
  }

  override set textContent(value: string) {
    if (this.value === value) {
      return;
    }
    this.value = value;
    this.didMutate();
  }

  cloneNode(_deep?: boolean): ServerText {
    return new ServerText(this.ownerDocument, this.value);
  }
}

export interface ServerAttribute {
  readonly name: string;
  readonly value: string;
}

class ServerNamedNodeMap {
  constructor(private readonly element: ServerElement) {}

  get length(): number {
    return this.element.attributeEntries.length;
  }

  item(index: number): ServerAttribute | null {
    const entry = this.element.attributeEntries[index];
    return entry ? { name: entry[0], value: entry[1] } : null;
  }
}

class ServerClassList {
  constructor(private readonly element: ServerElement) {}

  add(...names: string[]): void {
    const values = this.values;
    for (let index = 0; index < names.length; index++) {
      if (names[index]) {
        values.add(names[index]);
      }
    }
    this.element.className = Array.from(values).join(' ');
  }

  contains(name: string): boolean {
    return this.values.has(name);
  }

  remove(...names: string[]): void {
    const values = this.values;
    for (let index = 0; index < names.length; index++) {
      values.delete(names[index]);
    }
    this.element.className = Array.from(values).join(' ');
  }

  toggle(name: string, force?: boolean): boolean {
    const values = this.values;
    const shouldAdd = force ?? !values.has(name);
    if (shouldAdd) {
      values.add(name);
    } else {
      values.delete(name);
    }
    this.element.className = Array.from(values).join(' ');
    return shouldAdd;
  }

  private get values(): Set<string> {
    return new Set(this.element.className.split(/\s+/).filter(Boolean));
  }
}

const STRING_REFLECTED_PROPERTIES: Readonly<Record<string, string>> = {
  accept: 'accept',
  alt: 'alt',
  autocomplete: 'autocomplete',
  contentEditable: 'contenteditable',
  crossOrigin: 'crossorigin',
  name: 'name',
  placeholder: 'placeholder',
  poster: 'poster',
  src: 'src',
  type: 'type',
  value: 'value',
};

const NUMBER_REFLECTED_PROPERTIES: Readonly<Record<string, string>> = {
  cols: 'cols',
  height: 'height',
  maxLength: 'maxlength',
  rows: 'rows',
  tabIndex: 'tabindex',
  width: 'width',
};

const BOOLEAN_REFLECTED_PROPERTIES: Readonly<Record<string, string>> = {
  autoplay: 'autoplay',
  checked: 'checked',
  controls: 'controls',
  disabled: 'disabled',
  loop: 'loop',
  multiple: 'multiple',
  muted: 'muted',
  readOnly: 'readonly',
};

function defineReflectedProperties(element: ServerElement): void {
  for (const propertyName of Object.keys(STRING_REFLECTED_PROPERTIES)) {
    const attributeName = STRING_REFLECTED_PROPERTIES[propertyName];
    Object.defineProperty(element, propertyName, {
      configurable: true,
      enumerable: true,
      get: () => element.getAttribute(attributeName) ?? '',
      set: value => element.setAttribute(attributeName, String(value ?? '')),
    });
  }
  for (const propertyName of Object.keys(NUMBER_REFLECTED_PROPERTIES)) {
    const attributeName = NUMBER_REFLECTED_PROPERTIES[propertyName];
    Object.defineProperty(element, propertyName, {
      configurable: true,
      enumerable: true,
      get: () => Number(element.getAttribute(attributeName) ?? 0),
      set: value => element.setAttribute(attributeName, String(value)),
    });
  }
  for (const propertyName of Object.keys(BOOLEAN_REFLECTED_PROPERTIES)) {
    const attributeName = BOOLEAN_REFLECTED_PROPERTIES[propertyName];
    Object.defineProperty(element, propertyName, {
      configurable: true,
      enumerable: true,
      get: () => element.hasAttribute(attributeName),
      set: value => {
        if (value) {
          element.setAttribute(attributeName, '');
        } else {
          element.removeAttribute(attributeName);
        }
      },
    });
  }
}

function makeCanvasContext(): CanvasRenderingContext2D {
  const context: Record<string, unknown> = {
    clearRect(): void {},
    drawImage(): void {},
    getImageData(): ImageData {
      return { data: new Uint8ClampedArray(0), colorSpace: 'srgb', height: 0, width: 0 } as ImageData;
    },
    putImageData(): void {},
    restore(): void {},
    rotate(): void {},
    save(): void {},
    scale(): void {},
    setTransform(): void {},
    translate(): void {},
  };
  return context as unknown as CanvasRenderingContext2D;
}

export class ServerElement extends ServerNode {
  readonly nodeType = 1;
  readonly nodeName: string;
  readonly tagName: string;
  readonly localName: string;
  readonly attributes: ServerNamedNodeMap;
  readonly classList: ServerClassList;
  readonly children: ServerHTMLCollection;
  readonly namespaceURI: string;
  readonly style: ServerStyleDeclaration;
  shadowRoot: ServerShadowRoot | null = null;
  naturalHeight = 1;
  naturalWidth = 1;
  playbackRate = 1;
  selectionEnd: number | null = null;
  selectionStart: number | null = null;
  scrollLeft = 0;
  scrollTop = 0;
  private readonly attributeValues = new Map<string, string>();
  private readonly eventListeners = new Map<string, ServerEventListener[]>();
  private readonly mutableChildren: ServerElement[] = [];
  private canvasContext: CanvasRenderingContext2D | undefined;

  constructor(ownerDocument: ServerDocument, tagName: string, namespaceURI: string) {
    super(ownerDocument);
    this.localName = tagName.toLowerCase();
    this.tagName = namespaceURI === HTML_NAMESPACE ? tagName.toUpperCase() : tagName;
    this.nodeName = this.tagName;
    this.namespaceURI = namespaceURI;
    this.attributes = new ServerNamedNodeMap(this);
    this.classList = new ServerClassList(this);
    this.children = new ServerHTMLCollection(this.mutableChildren);
    this.style = createServerStyleDeclaration(() => this.didMutate());
    defineReflectedProperties(this);
  }

  get attributeEntries(): Array<[string, string]> {
    return Array.from(this.attributeValues.entries());
  }

  get childElementCount(): number {
    return this.mutableChildren.length;
  }

  get id(): string {
    return this.getAttribute('id') ?? '';
  }

  set id(value: string) {
    this.setAttribute('id', value);
  }

  get className(): string {
    return this.getAttribute('class') ?? '';
  }

  set className(value: string) {
    if (value) {
      this.setAttribute('class', value);
    } else {
      this.removeAttribute('class');
    }
  }

  get clientHeight(): number {
    return this.offsetHeight;
  }

  get clientWidth(): number {
    return this.offsetWidth;
  }

  get offsetHeight(): number {
    return this.pixelStyleValue('height');
  }

  get offsetLeft(): number {
    return this.pixelStyleValue('left');
  }

  get offsetParent(): ServerElement | null {
    return this.parentElement;
  }

  get offsetTop(): number {
    return this.pixelStyleValue('top');
  }

  get offsetWidth(): number {
    return this.pixelStyleValue('width');
  }

  get scrollHeight(): number {
    return this.offsetHeight;
  }

  get scrollWidth(): number {
    return this.offsetWidth;
  }

  override appendChild<T extends ServerNode>(child: T): T {
    const result = super.appendChild(child);
    this.refreshElementChildren();
    return result;
  }

  override insertBefore<T extends ServerNode>(child: T, before: ServerNode | null): T {
    const result = super.insertBefore(child, before);
    this.refreshElementChildren();
    return result;
  }

  override removeChild<T extends ServerNode>(child: T): T {
    const result = super.removeChild(child);
    this.refreshElementChildren();
    return result;
  }

  override replaceChildren(...children: Array<ServerNode | string>): void {
    super.replaceChildren(...children);
    this.refreshElementChildren();
  }

  setAttribute(name: string, value: string): void {
    const normalizedName = this.namespaceURI === HTML_NAMESPACE ? name.toLowerCase() : name;
    const normalizedValue = String(value);
    if (this.attributeValues.get(normalizedName) === normalizedValue) {
      return;
    }
    this.attributeValues.set(normalizedName, normalizedValue);
    this.didMutate();
  }

  getAttribute(name: string): string | null {
    const normalizedName = this.namespaceURI === HTML_NAMESPACE ? name.toLowerCase() : name;
    return this.attributeValues.get(normalizedName) ?? null;
  }

  hasAttribute(name: string): boolean {
    const normalizedName = this.namespaceURI === HTML_NAMESPACE ? name.toLowerCase() : name;
    return this.attributeValues.has(normalizedName);
  }

  removeAttribute(name: string): void {
    const normalizedName = this.namespaceURI === HTML_NAMESPACE ? name.toLowerCase() : name;
    if (this.attributeValues.delete(normalizedName)) {
      this.didMutate();
    }
  }

  attachShadow(_init: ShadowRootInit): ServerShadowRoot {
    if (this.shadowRoot) {
      throw new Error('A shadow root is already attached');
    }
    this.shadowRoot = new ServerShadowRoot(this.ownerDocument, this);
    this.shadowRoot.setMutationTracker(this.mutationTracker);
    this.didMutate();
    return this.shadowRoot;
  }

  override setMutationTracker(tracker: ServerDOMMutationTracker | undefined): void {
    super.setMutationTracker(tracker);
    this.shadowRoot?.setMutationTracker(tracker);
  }

  querySelector(selector: string): ServerElement | null {
    return this.querySelectorAll(selector)[0] ?? null;
  }

  querySelectorAll(selector: string): ServerElement[] {
    const matches: ServerElement[] = [];
    this.collectSelectorMatches(selector, matches);
    return matches;
  }

  addEventListener(type: string, listener: ServerEventListener | null, _options?: unknown): void {
    if (!listener) {
      return;
    }
    const listeners = this.eventListeners.get(type) ?? [];
    listeners.push(listener);
    this.eventListeners.set(type, listeners);
  }

  removeEventListener(type: string, listener: ServerEventListener | null, _options?: unknown): void {
    if (!listener) {
      return;
    }
    const listeners = this.eventListeners.get(type);
    const index = listeners?.indexOf(listener) ?? -1;
    if (index >= 0) {
      listeners!.splice(index, 1);
    }
  }

  dispatchEvent(event: Event): boolean {
    const listeners = this.eventListeners.get(event.type) ?? [];
    for (let index = 0; index < listeners.length; index++) {
      const listener = listeners[index];
      if (typeof listener === 'function') {
        listener(event);
      } else {
        listener.handleEvent(event);
      }
    }
    return !event.defaultPrevented;
  }

  focus(): void {
    const root = this.getRootNode();
    if (root instanceof ServerShadowRoot) {
      root.activeElement = this;
    }
    this.ownerDocument.activeElement = this;
  }

  blur(): void {
    const root = this.getRootNode();
    if (root instanceof ServerShadowRoot && root.activeElement === this) {
      root.activeElement = null;
    }
    if (this.ownerDocument.activeElement === this) {
      this.ownerDocument.activeElement = null;
    }
  }

  select(): void {
    this.setSelectionRange(0, this.getAttribute('value')?.length ?? 0);
  }

  setSelectionRange(start: number, end: number): void {
    this.selectionStart = start;
    this.selectionEnd = end;
  }

  scrollTo(options: ScrollToOptions): void {
    if (options.left !== undefined) {
      this.scrollLeft = options.left;
    }
    if (options.top !== undefined) {
      this.scrollTop = options.top;
    }
  }

  getBoundingClientRect(): DOMRect {
    const left = this.offsetLeft;
    const top = this.offsetTop;
    const width = this.offsetWidth;
    const height = this.offsetHeight;
    return {
      bottom: top + height,
      height,
      left,
      right: left + width,
      top,
      width,
      x: left,
      y: top,
      toJSON(): object {
        return { height, left, top, width };
      },
    };
  }

  getContext(contextId: string): CanvasRenderingContext2D | null {
    if (this.localName !== 'canvas' || contextId !== '2d') {
      return null;
    }
    this.canvasContext ??= makeCanvasContext();
    return this.canvasContext;
  }

  getTotalLength(): number {
    return 0;
  }

  play(): Promise<void> {
    return Promise.resolve();
  }

  pause(): void {}

  load(): void {}

  showModal(): void {
    this.setAttribute('open', '');
  }

  close(): void {
    this.removeAttribute('open');
  }

  cloneNode(deep?: boolean): ServerElement {
    const clone = new ServerElement(this.ownerDocument, this.localName, this.namespaceURI);
    for (const [name, value] of this.attributeValues) {
      clone.attributeValues.set(name, value);
    }
    const clonedStyle = this.style.entries;
    for (let index = 0; index < clonedStyle.length; index++) {
      clone.style.setProperty(clonedStyle[index][0], clonedStyle[index][1]);
    }
    clone.naturalHeight = this.naturalHeight;
    clone.naturalWidth = this.naturalWidth;
    clone.playbackRate = this.playbackRate;
    if (deep) {
      for (let index = 0; index < this.mutableChildNodes.length; index++) {
        clone.appendChild(this.mutableChildNodes[index].cloneNode(true));
      }
    }
    return clone;
  }

  private refreshElementChildren(): void {
    this.mutableChildren.length = 0;
    for (let index = 0; index < this.mutableChildNodes.length; index++) {
      const child = this.mutableChildNodes[index];
      if (child instanceof ServerElement) {
        this.mutableChildren.push(child);
      }
    }
  }

  private collectSelectorMatches(selector: string, matches: ServerElement[]): void {
    for (let index = 0; index < this.mutableChildren.length; index++) {
      const child = this.mutableChildren[index];
      if (child.matchesSimpleSelector(selector)) {
        matches.push(child);
      }
      child.collectSelectorMatches(selector, matches);
    }
  }

  private matchesSimpleSelector(selector: string): boolean {
    if (selector.startsWith('#')) {
      return this.id === selector.slice(1);
    }
    return this.localName === selector.toLowerCase();
  }

  private pixelStyleValue(name: string): number {
    const value = this.style.getPropertyValue(name);
    return value.endsWith('px') ? Number.parseFloat(value) || 0 : 0;
  }
}

export class ServerShadowRoot extends ServerNode {
  readonly nodeType = 11;
  readonly nodeName = '#document-fragment';
  activeElement: ServerElement | null = null;

  constructor(
    ownerDocument: ServerDocument,
    readonly host: ServerElement,
  ) {
    super(ownerDocument);
  }

  querySelector(selector: string): ServerElement | null {
    return this.querySelectorAll(selector)[0] ?? null;
  }

  querySelectorAll(selector: string): ServerElement[] {
    const matches: ServerElement[] = [];
    for (let index = 0; index < this.mutableChildNodes.length; index++) {
      const child = this.mutableChildNodes[index];
      if (!(child instanceof ServerElement)) {
        continue;
      }
      if ((selector.startsWith('#') && child.id === selector.slice(1)) || child.localName === selector.toLowerCase()) {
        matches.push(child);
      }
      matches.push(...child.querySelectorAll(selector));
    }
    return matches;
  }

  cloneNode(deep?: boolean): ServerShadowRoot {
    const cloneHost = this.host.cloneNode(false);
    const clone = new ServerShadowRoot(this.ownerDocument, cloneHost);
    if (deep) {
      for (let index = 0; index < this.mutableChildNodes.length; index++) {
        clone.appendChild(this.mutableChildNodes[index].cloneNode(true));
      }
    }
    return clone;
  }
}

export class ServerImageElement extends ServerElement {
  onabort: (() => void) | null = null;
  onerror: (() => void) | null = null;
  onload: (() => void) | null = null;

  constructor(ownerDocument: ServerDocument) {
    super(ownerDocument, 'img', HTML_NAMESPACE);
    Object.defineProperty(this, 'src', {
      configurable: true,
      enumerable: true,
      get: () => this.getAttribute('src') ?? '',
      set: value => {
        this.setAttribute('src', String(value));
        if (value) {
          Promise.resolve().then(() => this.onload?.());
        }
      },
    });
  }

  override cloneNode(deep?: boolean): ServerImageElement {
    const clone = new ServerImageElement(this.ownerDocument);
    const entries = this.attributeEntries;
    for (let index = 0; index < entries.length; index++) {
      clone.setAttribute(entries[index][0], entries[index][1]);
    }
    const styles = this.style.entries;
    for (let index = 0; index < styles.length; index++) {
      clone.style.setProperty(styles[index][0], styles[index][1]);
    }
    if (deep) {
      for (const child of this.childNodes) {
        clone.appendChild(child.cloneNode(true));
      }
    }
    return clone;
  }
}

export class ServerDocument extends ServerNode {
  readonly nodeType = 9;
  readonly nodeName = '#document';
  readonly body: ServerElement;
  readonly head: ServerElement;
  activeElement: ServerElement | null = null;
  dir = 'ltr';
  title = '';
  private readonly eventListeners = new Map<string, ServerEventListener[]>();

  constructor() {
    super(undefined as unknown as ServerDocument);
    (this as { ownerDocument: ServerDocument }).ownerDocument = this;
    this.head = new ServerElement(this, 'head', HTML_NAMESPACE);
    this.body = new ServerElement(this, 'body', HTML_NAMESPACE);
    this.mutableChildNodes.push(this.head, this.body);
    this.head.parentNode = this;
    this.body.parentNode = this;
  }

  createElement(tagName: string): ServerElement {
    return new ServerElement(this, tagName, HTML_NAMESPACE);
  }

  createElementNS(namespaceURI: string, qualifiedName: string): ServerElement {
    return new ServerElement(this, qualifiedName, namespaceURI);
  }

  createTextNode(value: string): ServerText {
    return new ServerText(this, value);
  }

  querySelector(selector: string): ServerElement | null {
    if (this.head.localName === selector.toLowerCase()) {
      return this.head;
    }
    if (this.body.localName === selector.toLowerCase()) {
      return this.body;
    }
    return this.head.querySelector(selector) ?? this.body.querySelector(selector);
  }

  addEventListener(type: string, listener: ServerEventListener | null, _options?: unknown): void {
    if (!listener) {
      return;
    }
    const listeners = this.eventListeners.get(type) ?? [];
    listeners.push(listener);
    this.eventListeners.set(type, listeners);
  }

  removeEventListener(type: string, listener: ServerEventListener | null, _options?: unknown): void {
    if (!listener) {
      return;
    }
    const listeners = this.eventListeners.get(type);
    const index = listeners?.indexOf(listener) ?? -1;
    if (index >= 0) {
      listeners!.splice(index, 1);
    }
  }

  createRange(): Range {
    return {
      setStart(): void {},
      setEnd(): void {},
    } as unknown as Range;
  }

  getSelection(): Selection | null {
    return null;
  }

  cloneNode(deep?: boolean): ServerDocument {
    const clone = new ServerDocument();
    if (deep) {
      clone.head.replaceChildren(...Array.from(this.head.childNodes).map(child => child.cloneNode(true)));
      clone.body.replaceChildren(...Array.from(this.body.childNodes).map(child => child.cloneNode(true)));
    }
    return clone;
  }
}
