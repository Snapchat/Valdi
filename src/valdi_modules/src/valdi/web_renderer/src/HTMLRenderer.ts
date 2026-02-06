import { WebValdiLayout } from './views/WebValdiLayout';
import { WebValdiLabel } from './views/WebValdiLabel';
import { WebValdiScroll } from './views/WebValdiScroll';
import { WebValdiView } from './views/WebValdiView';
import { WebValdiTextField, registerTextFieldElements } from './views/WebValdiTextField';
import { WebValdiImage } from './views/WebValdiImage';
import { WebValdiSpinner } from './views/WebValdiSpinner';
import { UpdateAttributeDelegate } from './ValdiWebRendererDelegate';

export const nodesRef = new Map<number, WebValdiLayout>();
export let rootNode: WebValdiLayout | null = null;

export function registerElements() {
  registerTextFieldElements();
}

function initViewClass(viewClass: string, id: number, attributeDelegate?: UpdateAttributeDelegate): WebValdiLayout {
  switch (viewClass) {
    case 'label':
      return new WebValdiLabel(id, attributeDelegate);
    case 'layout':
      return new WebValdiLayout(id, attributeDelegate);
    case 'view':
      return new WebValdiView(id, attributeDelegate);
    case 'image':
      return new WebValdiImage(id, attributeDelegate);
    case 'spinner':
      return new WebValdiSpinner(id, attributeDelegate);
    case 'SCValdiDatePicker':
      return new WebValdiView(id, attributeDelegate);
    case 'scroll':
      return new WebValdiScroll(id, attributeDelegate);
    case 'textfield':
      return new WebValdiTextField(id, attributeDelegate);

    default:
      throw new Error(`Unknown viewClass: ${viewClass}`);
  }
}

export function createElement(id: number, viewClass: string, attributeDelegate?: UpdateAttributeDelegate) {
  const element = initViewClass(viewClass, id, attributeDelegate);

  nodesRef.set(id, element);
  return element;
}

export function destroyElement(id: number) {
  const element = nodesRef.get(id);
  if (element) {
    element.destroy();
    nodesRef.delete(id);
  }
}

export function makeElementRoot(id: number, root: HTMLElement | ShadowRoot) {
  const element = nodesRef.get(id);
  if (!element) {
    throw new Error(`makeElementRoot: element is missing, id: ${id}`);
  }

  element.makeRoot(root);
}

export function moveElement(id: number, parentId: number, parentIndex: number) {
  const element = nodesRef.get(id);
  const parent = nodesRef.get(parentId);

  if (!element || !parent) {
    throw new Error(`moveElement: element or parent is missing, id: ${id}, parentId: ${parentId}`);
  }

  element.move(parent, parentIndex);
}

export function changeAttributeOnElement(id: number, attributeName: string, attributeValue: any) {
  const element = nodesRef.get(id);
  if (!element) {
    throw new Error(`changeAttributeOnElement: element is missing, id: ${id}`);
  }

  // Stripping $ prefix for injected attributes to match native behavior.
  // Injected attributes like $width, $height should become width, height respectively.
  const actualAttributeName = attributeName.startsWith('$') ? attributeName.substring(1) : attributeName;

  element.changeAttribute(actualAttributeName, attributeValue);
}

export function setAllElementsAttributeDelegate(attributeDelegate?: UpdateAttributeDelegate) {
  for (const [key, value] of nodesRef) {
    value.setAttributeDelegate(attributeDelegate);
  }
}