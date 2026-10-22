import type { IRenderedVirtualNode } from 'valdi_core/src/IRenderedVirtualNode';
import { getNodeTag } from 'valdi_core/src/utils/RenderedVirtualNodeUtils';
import { debugStringify } from 'valdi_core/src/utils/StringUtils';

import type { IntegrationTestRenderedNode } from './IntegrationTestTypes';

export function toIntegrationTestRenderedNode(node: IRenderedVirtualNode): IntegrationTestRenderedNode {
  const attributes: { [name: string]: string } = {};
  const element = node.element;
  if (element) {
    for (const attributeName of element.getAttributeNames()) {
      const key = String(attributeName);
      if (key === 'ref') {
        continue;
      }
      attributes[key] = debugStringify(element.getAttribute(attributeName), 4, false);
    }
  }

  return {
    tag: getNodeTag(node),
    attributes,
    children: node.children.map(child => toIntegrationTestRenderedNode(child)),
  };
}
