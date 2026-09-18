import { WebValdiLayout } from './views/WebValdiLayout';
import { WebValdiLabel } from './views/WebValdiLabel';
import { WebValdiScroll } from './views/WebValdiScroll';
import { WebValdiView } from './views/WebValdiView';
import { WebValdiCustomView } from './views/WebValdiCustomView';
import { WebValdiTextField, registerTextFieldElements } from './views/WebValdiTextField';
import { WebValdiTextView } from './views/WebValdiTextView';
import { WebValdiImage } from './views/WebValdiImage';
import { WebValdiSpinner } from './views/WebValdiSpinner';
import { WebValdiVideo } from './views/WebValdiVideo';
import { WebValdiShape } from './views/WebValdiShape';
import { UpdateAttributeDelegate } from './ValdiWebRendererDelegate';

export type NodesRef = Map<number, WebValdiLayout>;

// Each renderer (i.e. each navigation page) owns its own node map. A single
// shared map keyed by element id let a pushed page clobber the page underneath
// at colliding ids (id counters restart at 1 per renderer) and corrupt view
// reuse on the next re-render (github.com/Snapchat/Valdi#115). Owning the map on
// the delegate instance also makes lookups consistent if this module is bundled
// twice, which the old globalThis shim was working around.
export function createNodesRef(): NodesRef {
  return new Map<number, WebValdiLayout>();
}

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
    case 'textview':
    case 'SCValdiTextView':
      return new WebValdiTextView(id, attributeDelegate);
    case 'custom-view':
      return new WebValdiCustomView(id, attributeDelegate);
    case 'video':
    case 'SCValdiVideoView':
      return new WebValdiVideo(id, attributeDelegate);
    case 'shape':
    case 'SCValdiShapeView':
      return new WebValdiShape(id, attributeDelegate);

    default:
      throw new Error(`Unknown viewClass: ${viewClass}`);
  }
}

export function createElement(nodes: NodesRef, id: number, viewClass: string, attributeDelegate?: UpdateAttributeDelegate) {
  const element = initViewClass(viewClass, id, attributeDelegate);
  nodes.set(id, element);
  return element;
}

export function destroyElement(nodes: NodesRef, id: number) {
  const element = nodes.get(id);
  if (element) {
    element.destroy();
    nodes.delete(id);
  }
}

export function makeElementRoot(nodes: NodesRef, id: number, root: HTMLElement | ShadowRoot) {
  const element = nodes.get(id);
  if (!element) {
    throw new Error(`makeElementRoot: element is missing, id: ${id}`);
  }

  element.makeRoot(root);
}

export function moveElement(nodes: NodesRef, id: number, parentId: number, parentIndex: number) {
  const element = nodes.get(id);
  const parent = nodes.get(parentId);

  if (!element || !parent) {
    throw new Error(`moveElement: element or parent is missing, id: ${id}, parentId: ${parentId}`);
  }

  element.move(parent, parentIndex);
}

export function changeAttributeOnElement(nodes: NodesRef, id: number, attributeName: string, attributeValue: any) {
  const element = nodes.get(id);
  if (!element) {
    throw new Error(`changeAttributeOnElement: element is missing, id: ${id}`);
  }
  const actualAttributeName = attributeName.startsWith('$') ? attributeName.substring(1) : attributeName;

  element.changeAttribute(actualAttributeName, attributeValue);
}

export function setAllElementsAttributeDelegate(nodes: NodesRef, attributeDelegate?: UpdateAttributeDelegate) {
  for (const [, value] of nodes) {
    value.setAttributeDelegate(attributeDelegate);
  }
}
