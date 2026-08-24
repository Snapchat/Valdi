import { StringMap } from 'coreutils/src/StringMap';
import { ElementFrame } from 'valdi_tsx/src/Geometry';
import { IComponent } from './IComponent';
import { IRenderedVirtualNode } from './IRenderedVirtualNode';
import { getNodeTag } from './utils/RenderedVirtualNodeUtils';
import { debugStringify } from './utils/StringUtils';

const MAX_COMPONENT_DEBUG_DATA_CHARACTERS_PER_FIELD = 65_536;
const MAX_COMPONENT_DEBUG_DATA_CHARACTERS_PER_TREE = 262_144;
const COMPONENT_DEBUG_DATA_TRUNCATION_MARKER = '\n... <truncated>';

export interface IRenderedElementData {
  readonly id: number;
  readonly frame: ElementFrame;
  readonly attributes?: StringMap<string>;
}

export interface IRenderedComponentData {
  /**
   * Debug-stringified component input.
   */
  readonly viewModel?: string;

  /**
   * Debug-stringified StatefulComponent state when present.
   */
  readonly state?: string;

  /**
   * Whether one or more component debug values were omitted by the snapshot
   * character budget.
   */
  readonly debugDataOmitted?: boolean;
}

interface IDebuggableComponent extends IComponent {
  readonly state?: unknown;
}

interface ComponentDebugDataBudget {
  remainingCharacters: number;
}

interface ComponentDebugString {
  value: string | undefined;
  omitted: boolean;
}

interface BoundedDebugWriter {
  value: string;
  readonly characterLimit: number;
  truncated: boolean;
}

interface BoundedComponentDebugKeys {
  readonly keys: string[];
  readonly omitted: boolean;
}

const ACCESSOR_DEBUG_VALUE = {};
const MAX_COMPONENT_DEBUG_DEPTH = 4;
const MAX_COMPONENT_DEBUG_COLLECTION_ITEMS = 50;
const MAP_SIZE_GETTER = Object.getOwnPropertyDescriptor(Map.prototype, 'size')?.get;
const SET_SIZE_GETTER = Object.getOwnPropertyDescriptor(Set.prototype, 'size')?.get;

export interface RenderedVirtualNodeDataOptions {
  readonly includeAttributes: boolean;
  readonly includeComponentData: boolean;
  readonly onCreate: ((node: IRenderedVirtualNode, data: IRenderedVirtualNodeData) => void) | undefined;
}

function appendComponentDebugText(writer: BoundedDebugWriter, text: string): void {
  if (writer.truncated || text.length === 0) {
    return;
  }

  if (writer.value.length + text.length <= writer.characterLimit) {
    writer.value += text;
    return;
  }

  const marker = COMPONENT_DEBUG_DATA_TRUNCATION_MARKER.slice(0, writer.characterLimit);
  const prefixLength = Math.max(0, writer.characterLimit - marker.length);
  if (writer.value.length > prefixLength) {
    writer.value = writer.value.slice(0, prefixLength);
  } else if (writer.value.length < prefixLength) {
    writer.value += text.slice(0, prefixLength - writer.value.length);
  }
  writer.value += marker;
  writer.truncated = true;
}

function componentDebugIndent(depth: number): string {
  return `\n${'  '.repeat(depth)}`;
}

function readComponentDebugProperty(object: object, key: string): unknown {
  const descriptor = Object.getOwnPropertyDescriptor(object, key);
  if (!descriptor) {
    return undefined;
  }
  if (descriptor.get || descriptor.set) {
    return ACCESSOR_DEBUG_VALUE;
  }
  return descriptor.value;
}

function collectBoundedComponentDebugKeys(object: object): BoundedComponentDebugKeys {
  const keys: string[] = [];
  for (const key in object) {
    const descriptor = Object.getOwnPropertyDescriptor(object, key);
    if (!descriptor) break;
    if (!descriptor.enumerable) continue;
    if (keys.length >= MAX_COMPONENT_DEBUG_COLLECTION_ITEMS) return { keys, omitted: true };
    keys.push(key);
  }
  return { keys, omitted: false };
}

function readComponentDebugMapSize(map: Map<unknown, unknown>): number {
  return MAP_SIZE_GETTER ? Number(MAP_SIZE_GETTER.call(map)) : 0;
}

function readComponentDebugSetSize(set: Set<unknown>): number {
  return SET_SIZE_GETTER ? Number(SET_SIZE_GETTER.call(set)) : 0;
}

function writeComponentDebugArray(
  array: unknown[],
  writer: BoundedDebugWriter,
  visited: Set<object>,
  depth: number,
): void {
  const lengthDescriptor = Object.getOwnPropertyDescriptor(array, 'length');
  const length = typeof lengthDescriptor?.value === 'number' ? lengthDescriptor.value : 0;
  if (length === 0) {
    appendComponentDebugText(writer, '[]');
    return;
  }

  appendComponentDebugText(writer, `[${componentDebugIndent(depth + 1)}`);
  const itemCount = Math.min(length, MAX_COMPONENT_DEBUG_COLLECTION_ITEMS);
  for (let index = 0; index < itemCount && !writer.truncated; index++) {
    if (index > 0) {
      appendComponentDebugText(writer, `,${componentDebugIndent(depth + 1)}`);
    }
    const item = readComponentDebugProperty(array, String(index));
    writeComponentDebugValue(item, writer, visited, depth + 1);
  }
  if (length > itemCount && !writer.truncated) {
    appendComponentDebugText(writer, `,${componentDebugIndent(depth + 1)}... ${length - itemCount} more item(s) ...`);
  }
  appendComponentDebugText(writer, `${componentDebugIndent(depth)}]`);
}

function writeComponentDebugMap(
  map: Map<unknown, unknown>,
  writer: BoundedDebugWriter,
  visited: Set<object>,
  depth: number,
): void {
  appendComponentDebugText(writer, 'Map{');
  const size = readComponentDebugMapSize(map);
  let itemCount = 0;
  for (const [key, value] of Map.prototype.entries.call(map) as IterableIterator<[unknown, unknown]>) {
    if (itemCount >= MAX_COMPONENT_DEBUG_COLLECTION_ITEMS || writer.truncated) {
      break;
    }
    appendComponentDebugText(writer, componentDebugIndent(depth + 1));
    writeComponentDebugValue(key, writer, visited, depth + 1);
    appendComponentDebugText(writer, ': ');
    writeComponentDebugValue(value, writer, visited, depth + 1);
    itemCount += 1;
    if (itemCount < size) {
      appendComponentDebugText(writer, ',');
    }
  }
  if (size > itemCount && !writer.truncated) {
    appendComponentDebugText(writer, `${componentDebugIndent(depth + 1)}... ${size - itemCount} more item(s) ...`);
  }
  if (size > 0) {
    appendComponentDebugText(writer, componentDebugIndent(depth));
  }
  appendComponentDebugText(writer, '}');
}

function writeComponentDebugSet(
  set: Set<unknown>,
  writer: BoundedDebugWriter,
  visited: Set<object>,
  depth: number,
): void {
  appendComponentDebugText(writer, 'Set(');
  const size = readComponentDebugSetSize(set);
  let itemCount = 0;
  for (const value of Set.prototype.values.call(set) as IterableIterator<unknown>) {
    if (itemCount >= MAX_COMPONENT_DEBUG_COLLECTION_ITEMS || writer.truncated) {
      break;
    }
    appendComponentDebugText(writer, componentDebugIndent(depth + 1));
    writeComponentDebugValue(value, writer, visited, depth + 1);
    itemCount += 1;
    if (itemCount < size) {
      appendComponentDebugText(writer, ',');
    }
  }
  if (size > itemCount && !writer.truncated) {
    appendComponentDebugText(writer, `${componentDebugIndent(depth + 1)}... ${size - itemCount} more item(s) ...`);
  }
  if (size > 0) {
    appendComponentDebugText(writer, componentDebugIndent(depth));
  }
  appendComponentDebugText(writer, ')');
}

function writeComponentDebugObject(
  object: object,
  writer: BoundedDebugWriter,
  visited: Set<object>,
  depth: number,
): void {
  const boundedKeys = collectBoundedComponentDebugKeys(object);
  if (boundedKeys.keys.length === 0 && !boundedKeys.omitted) {
    appendComponentDebugText(writer, '{}');
    return;
  }

  appendComponentDebugText(writer, `{${componentDebugIndent(depth + 1)}`);
  for (let index = 0; index < boundedKeys.keys.length && !writer.truncated; index++) {
    const key = boundedKeys.keys[index];
    if (key === undefined) {
      continue;
    }
    if (index > 0) {
      appendComponentDebugText(writer, `,${componentDebugIndent(depth + 1)}`);
    }
    appendComponentDebugText(writer, `${key}: `);
    const propertyValue = readComponentDebugProperty(object, key);
    writeComponentDebugValue(propertyValue, writer, visited, depth + 1);
  }
  if (boundedKeys.omitted && !writer.truncated) {
    const prefix = boundedKeys.keys.length > 0 ? ',' : '';
    appendComponentDebugText(writer, `${prefix}${componentDebugIndent(depth + 1)}... more properties ...`);
  }
  appendComponentDebugText(writer, `${componentDebugIndent(depth)}}`);
}

function writeComponentDebugValue(
  value: unknown,
  writer: BoundedDebugWriter,
  visited: Set<object>,
  depth: number,
): void {
  if (writer.truncated) {
    return;
  }
  if (depth >= MAX_COMPONENT_DEBUG_DEPTH) {
    appendComponentDebugText(writer, '...');
    return;
  }
  if (value === ACCESSOR_DEBUG_VALUE) {
    appendComponentDebugText(writer, '<accessor/>');
    return;
  }
  if (value === undefined) {
    appendComponentDebugText(writer, 'undefined');
    return;
  }
  if (value === null) {
    appendComponentDebugText(writer, 'null');
    return;
  }

  const valueType = typeof value;
  if (valueType === 'string') {
    if (depth > 0) {
      appendComponentDebugText(writer, '"');
    }
    appendComponentDebugText(writer, value as string);
    if (depth > 0) {
      appendComponentDebugText(writer, '"');
    }
    return;
  }
  if (valueType === 'number' || valueType === 'boolean' || valueType === 'bigint' || valueType === 'symbol') {
    appendComponentDebugText(writer, String(value));
    return;
  }
  if (valueType === 'function') {
    let functionName = '';
    try {
      functionName = String((value as { readonly name?: unknown }).name ?? '');
    } catch {
      // Proxied functions may reject property reads. The function is still represented without invoking it.
    }
    appendComponentDebugText(writer, `<function${functionName ? ` ${functionName}` : ''}/>`);
    return;
  }

  const object = value as object;
  if (visited.has(object)) {
    appendComponentDebugText(writer, '<circular object/>');
    return;
  }
  visited.add(object);

  try {
    if (Array.isArray(object)) {
      writeComponentDebugArray(object, writer, visited, depth);
    } else if (typeof ArrayBuffer !== 'undefined' && ArrayBuffer.isView(object)) {
      appendComponentDebugText(writer, '<array buffer view/>');
    } else if (object instanceof Map) {
      writeComponentDebugMap(object, writer, visited, depth);
    } else if (object instanceof Set) {
      writeComponentDebugSet(object, writer, visited, depth);
    } else if (object instanceof Date) {
      appendComponentDebugText(writer, `<date ${Date.prototype.toISOString.call(object)}/>`);
    } else if (object instanceof Error) {
      const message = readComponentDebugProperty(object, 'message');
      appendComponentDebugText(writer, '<error');
      if (message !== undefined && message !== ACCESSOR_DEBUG_VALUE) {
        appendComponentDebugText(writer, ' ');
        writeComponentDebugValue(message, writer, visited, depth + 1);
      }
      appendComponentDebugText(writer, '/>');
    } else {
      writeComponentDebugObject(object, writer, visited, depth);
    }
  } catch {
    appendComponentDebugText(writer, '<failure/>');
  }
}

function stringifyComponentDebugData(value: unknown, budget: ComponentDebugDataBudget): ComponentDebugString {
  const availableCharacters = Math.min(MAX_COMPONENT_DEBUG_DATA_CHARACTERS_PER_FIELD, budget.remainingCharacters);
  if (availableCharacters <= 0) {
    return { value: undefined, omitted: true };
  }

  const writer: BoundedDebugWriter = {
    value: '',
    characterLimit: availableCharacters,
    truncated: false,
  };
  writeComponentDebugValue(value, writer, new Set<object>(), 0);
  budget.remainingCharacters -= writer.value.length;
  return { value: writer.value, omitted: writer.truncated };
}

export interface IRenderedVirtualNodeData {
  /**
   * The key, which was either automatically generated by
   * the renderer or manually provided.
   */
  readonly key: string;

  readonly tag: string;

  readonly element: IRenderedElementData | undefined;
  readonly component: IRenderedComponentData | undefined;
  readonly children: IRenderedVirtualNodeData[] | undefined;
}

export function getPathToNode(parent: IRenderedVirtualNodeData, child: IRenderedVirtualNodeData): number[] | undefined {
  if (parent === child) {
    return [];
  }

  if (parent.children) {
    let index = 0;
    for (const childOfParent of parent.children) {
      if (childOfParent === child) {
        return [index];
      }

      const path = getPathToNode(childOfParent, child);
      if (path) {
        path.unshift(index);
        return path;
      }

      index++;
    }
  }

  return undefined;
}

export function fromRenderedVirtualNode(
  node: IRenderedVirtualNode,
  includeAttributes: boolean,
  onCreate?: (node: IRenderedVirtualNode, data: IRenderedVirtualNodeData) => void,
): IRenderedVirtualNodeData {
  return fromRenderedVirtualNodeWithOptions(node, {
    includeAttributes,
    includeComponentData: false,
    onCreate,
  });
}

export function fromRenderedVirtualNodeWithOptions(
  node: IRenderedVirtualNode,
  options: RenderedVirtualNodeDataOptions,
): IRenderedVirtualNodeData {
  const componentDebugDataBudget: ComponentDebugDataBudget = {
    remainingCharacters: MAX_COMPONENT_DEBUG_DATA_CHARACTERS_PER_TREE,
  };
  return fromRenderedVirtualNodeWithBudget(node, options, componentDebugDataBudget);
}

function fromRenderedVirtualNodeWithBudget(
  node: IRenderedVirtualNode,
  options: RenderedVirtualNodeDataOptions,
  componentDebugDataBudget: ComponentDebugDataBudget,
): IRenderedVirtualNodeData {
  let children: IRenderedVirtualNodeData[] | undefined;
  let element: IRenderedElementData | undefined;
  let component: IRenderedComponentData | undefined;

  if (node.component) {
    if (options.includeComponentData) {
      const debuggableComponent = node.component as IDebuggableComponent;
      const componentViewModel = readComponentDebugProperty(debuggableComponent, 'viewModel');
      const componentState = readComponentDebugProperty(debuggableComponent, 'state');
      const viewModel = stringifyComponentDebugData(componentViewModel, componentDebugDataBudget);
      const state =
        componentState === undefined
          ? undefined
          : stringifyComponentDebugData(componentState, componentDebugDataBudget);
      const debugDataOmitted = viewModel.omitted || state?.omitted === true;
      component = {
        ...(viewModel.value === undefined ? {} : { viewModel: viewModel.value }),
        ...(state?.value === undefined ? {} : { state: state.value }),
        ...(debugDataOmitted ? { debugDataOmitted: true } : {}),
      };
    } else {
      component = {};
    }
  }

  if (node.children.length) {
    children = [];
    for (const child of node.children) {
      children.push(fromRenderedVirtualNodeWithBudget(child, options, componentDebugDataBudget));
    }
  }

  // Preserve the existing post-order attribute reads while reserving the
  // component debug-data budget from the root downward.
  if (node.element) {
    const renderedElement = node.element;
    let attributes: StringMap<string> | undefined;

    if (options.includeAttributes) {
      attributes = {};
      for (const attributeName of renderedElement.getAttributeNames()) {
        const attributeValue = renderedElement.getAttribute(attributeName);
        attributes[attributeName] = debugStringify(attributeValue, 3, true);
      }
    }

    element = {
      id: renderedElement.id,
      frame: renderedElement.frame,
      attributes,
    };
  }

  const tag = getNodeTag(node);

  const data = {
    key: node.key,
    tag,
    element,
    component,
    children,
  };

  if (options.onCreate) {
    options.onCreate(node, data);
  }

  return data;
}

export interface RenderedVirtualNodeDataFromRoot {
  root: IRenderedVirtualNodeData;
  child: IRenderedVirtualNodeData;
}

export function getVirtualNodeDataFromRootToChild(child: IRenderedVirtualNode): RenderedVirtualNodeDataFromRoot {
  let childData: IRenderedVirtualNodeData | undefined;

  let root = child;
  while (root.parent) {
    root = root.parent;
  }

  const rootData = fromRenderedVirtualNode(root, false, (node, data) => {
    if (node === child) {
      childData = data;
    }
  });

  return {
    root: rootData,
    child: childData!,
  };
}
