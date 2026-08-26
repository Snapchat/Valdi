import { ServerElement, ServerNode, ServerShadowRoot, ServerText } from '../dom/ServerDOM';
import { scopeServerCSS } from './ServerCSSScoper';

const BOOLEAN_ATTRIBUTES: Record<string, true> = {
  allowfullscreen: true,
  async: true,
  autofocus: true,
  autoplay: true,
  checked: true,
  controls: true,
  default: true,
  defer: true,
  disabled: true,
  formnovalidate: true,
  hidden: true,
  inert: true,
  ismap: true,
  itemscope: true,
  loop: true,
  multiple: true,
  muted: true,
  nomodule: true,
  novalidate: true,
  open: true,
  playsinline: true,
  readonly: true,
  required: true,
  reversed: true,
  selected: true,
};

const UNSAFE_ATTRIBUTE_NAMES: Record<string, true> = { srcdoc: true };
const UNSAFE_ELEMENTS: Record<string, true> = { embed: true, iframe: true, object: true, script: true };
const URL_ATTRIBUTE_NAMES: Record<string, true> = {
  action: true,
  formaction: true,
  href: true,
  poster: true,
  src: true,
};
const VOID_ELEMENTS: Record<string, true> = {
  area: true,
  base: true,
  br: true,
  col: true,
  embed: true,
  hr: true,
  img: true,
  input: true,
  link: true,
  meta: true,
  param: true,
  source: true,
  track: true,
  wbr: true,
};

function escapeText(value: string): string {
  return value.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

function escapeAttribute(value: string): string {
  return escapeText(value).replace(/"/g, '&quot;');
}

function escapeStyleText(value: string): string {
  return value.replace(/<\/style/gi, match => `\\3C ${match.slice(1)}`);
}

function cssPropertyName(name: string): string {
  if (name.startsWith('--') || name.includes('-')) {
    return name;
  }
  if (name === 'cssFloat') {
    return 'float';
  }
  const kebabName = name.replace(/[A-Z]/g, character => `-${character.toLowerCase()}`);
  if (kebabName.startsWith('webkit-')) {
    return `-${kebabName}`;
  }
  if (kebabName.startsWith('moz-')) {
    return `-${kebabName}`;
  }
  return kebabName;
}

function serializeStyle(element: ServerElement): string | undefined {
  const entries = element.style.entries
    .filter(entry => entry[1] !== '')
    .map(entry => [cssPropertyName(entry[0]), entry[1]] as const)
    .sort((left, right) => left[0].localeCompare(right[0]));
  if (!entries.length) {
    return undefined;
  }
  return entries.map(entry => `${entry[0]}: ${entry[1]}`).join('; ');
}

function isSafeAttribute(name: string, value: string): boolean {
  const normalizedName = name.toLowerCase();
  if (normalizedName.startsWith('on') || UNSAFE_ATTRIBUTE_NAMES[normalizedName] === true) {
    return false;
  }
  if (URL_ATTRIBUTE_NAMES[normalizedName] === true && /^\s*javascript:/i.test(value)) {
    return false;
  }
  return true;
}

function serializeAttributes(element: ServerElement): string {
  const attributeMap = new Map(element.attributeEntries);
  const style = serializeStyle(element);
  if (style !== undefined) {
    attributeMap.set('style', style);
  }
  const entries = Array.from(attributeMap.entries())
    .filter(entry => isSafeAttribute(entry[0], entry[1]))
    .sort((left, right) => left[0].localeCompare(right[0]));
  let output = '';
  for (let index = 0; index < entries.length; index++) {
    const [name, value] = entries[index];
    if (BOOLEAN_ATTRIBUTES[name.toLowerCase()] === true && value === '') {
      output += ` ${name}`;
    } else {
      output += ` ${name}="${escapeAttribute(value)}"`;
    }
  }
  return output;
}

function serializeChildren(node: ServerNode): string {
  let output = '';
  for (const child of node.childNodes) {
    output += serializeNode(child);
  }
  return output;
}

function serializeNode(node: ServerNode): string {
  if (node instanceof ServerText) {
    return escapeText(node.textContent);
  }
  if (!(node instanceof ServerElement)) {
    return serializeChildren(node);
  }
  const tagName = node.localName;
  if (UNSAFE_ELEMENTS[tagName] === true) {
    return `<span data-valdi-unsupported="${tagName}"></span>`;
  }
  const openingTag = `<${tagName}${serializeAttributes(node)}>`;
  if (VOID_ELEMENTS[tagName] === true) {
    return openingTag;
  }
  return `${openingTag}${serializeChildren(node)}</${tagName}>`;
}

function serializeShadowChild(node: ServerNode, scopeSelectorValue: string): string {
  if (node instanceof ServerElement && node.localName === 'style') {
    const css = scopeServerCSS(node.textContent, scopeSelectorValue);
    return `<style${serializeAttributes(node)}>${escapeStyleText(css)}</style>`;
  }
  return serializeNode(node);
}

export function serializeServerHTMLRoot(shadowRoot: ServerShadowRoot, rootId: string): string {
  const escapedRootId = escapeAttribute(rootId);
  const scopeSelectorValue = `[data-valdi-html-root="${escapedRootId}"]`;
  let children = '';
  for (const child of shadowRoot.childNodes) {
    children += serializeShadowChild(child, scopeSelectorValue);
  }
  return `<div data-valdi-html-root="${escapedRootId}">${children}</div>`;
}
