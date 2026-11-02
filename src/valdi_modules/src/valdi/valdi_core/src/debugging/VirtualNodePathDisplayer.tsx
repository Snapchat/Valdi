import { StringMap } from 'coreutils/src/StringMap';
import { AttributedTextAttributes } from 'valdi_tsx/src/AttributedText';
import { Component } from '../Component';
import { getPathToNode, IRenderedVirtualNodeData } from '../IRenderedVirtualNodeData';
import { AttributedTextBuilder } from '../utils/AttributedTextBuilder';
import { colors, styles } from './Styles';

interface ViewModel {
  node: IRenderedVirtualNodeData;
  focusedNode?: IRenderedVirtualNodeData;
}

const headerStyle = styles.infoText.extend({
  marginBottom: 8,
});

export class VirtualNodePathDisplayer extends Component<ViewModel> {
  onRender() {
    let title: string;

    if (this.viewModel.focusedNode) {
      if (this.viewModel.focusedNode.component) {
        title = `Path from root to Component '${this.viewModel.focusedNode.tag}':`;
      } else {
        title = `Path from root to Element '${this.viewModel.focusedNode.tag}' with id ${
          this.viewModel.focusedNode.element!.id
        }:`;
      }
    } else {
      title = 'Path from root';
    }

    <layout>
      <label style={headerStyle} color="black" value={title} />
      <view style={styles.console}>
        <scroll horizontal>
          <layout>{this.renderNodePaths()}</layout>
        </scroll>
      </view>
    </layout>;
  }

  renderNodePaths() {
    let nodePaths: number[] | undefined;

    if (this.viewModel.focusedNode) {
      nodePaths = getPathToNode(this.viewModel.node, this.viewModel.focusedNode);
    }

    this.renderNode(this.viewModel.node, 0, nodePaths);
  }

  private renderChildrenCount(label: string, count: number, indent: string) {
    if (!count) {
      return;
    }

    const childText = count === 1 ? 'child' : 'children';

    this.appendComment(indent, `${count} ${childText} ${label}`);
  }

  private appendComment(indent: string, text: string) {
    <label style={styles.bodyText} color={colors.comment} value={`${indent}{ /* ${text} */ }`} />;
  }

  private appendAttributes(builder: AttributedTextBuilder, attributes: StringMap<string>) {
    for (const attributeName in attributes) {
      builder.append(' ');
      builder.append(attributeName, { color: colors.attributeName });
      builder.append('=', { color: colors.expression });

      let attributeValue = attributes[attributeName]!;
      attributeValue = attributeValue.replace(/\n/g, '');

      if (Number.isNaN(parseInt(attributeValue))) {
        attributeValue = `'${attributeValue}'`;
      }

      builder.append(attributeValue, { color: colors.attributeValue });
    }
  }

  private renderNode(node: IRenderedVirtualNodeData, depth: number, nodePaths: number[] | undefined) {
    let indent = '';
    for (let i = 0; i < depth; i++) {
      indent += '  ';
    }

    const tagStyle: AttributedTextAttributes = node.component
      ? { color: colors.tagComponent }
      : { color: colors.tagElement };

    const isFocused = node === this.viewModel.focusedNode;

    if (isFocused) {
      this.appendComment(indent, '↓↓↓');
    }

    const shouldRenderChildren =
      node.children && node !== this.viewModel.focusedNode && (!nodePaths || nodePaths.length);

    if (!shouldRenderChildren) {
      const textBuilder = new AttributedTextBuilder()
        .append(indent)
        .append('<', { color: colors.tagBracket })
        .append(node.tag, tagStyle);

      if (node.element && node.element.attributes) {
        this.appendAttributes(textBuilder, node.element.attributes);
      }

      const text = textBuilder.append('/>', { color: colors.tagBracket }).build();

      if (isFocused) {
        <label style={styles.bodyStrongText} value={text} />;
      } else {
        <label style={styles.bodyText} value={text} />;
      }
    } else {
      const openingTagBuilder = new AttributedTextBuilder()
        .append(indent)
        .append('<', { color: colors.tagBracket })
        .append(node.tag, tagStyle);

      if (node.element && node.element.attributes) {
        this.appendAttributes(openingTagBuilder, node.element.attributes);
      }

      const openingTag = openingTagBuilder.append('>', { color: colors.tagBracket }).build();
      const closingTag = new AttributedTextBuilder()
        .append(indent)
        .append('</', { color: colors.tagBracket })
        .append(node.tag, tagStyle)
        .append('>', { color: colors.tagBracket })
        .build();

      <label style={styles.bodyText} value={openingTag} />;
      {
        this.renderNodeChildren(node, depth + 1, indent, nodePaths);
      }
      <label style={styles.bodyText} value={closingTag} />;
    }
  }

  private renderNodeChildren(
    node: IRenderedVirtualNodeData,
    newDepth: number,
    indent: string,
    nodePaths: number[] | undefined,
  ) {
    const children = node.children;
    if (!children) {
      return;
    }

    if (nodePaths) {
      const index = nodePaths.shift();
      if (index === undefined) {
        return;
      }

      const childrenBefore = index;
      const childrenAfter = children.length - index - 1;
      this.renderChildrenCount('before', childrenBefore, indent);
      this.renderNode(children[index], newDepth, nodePaths);
      this.renderChildrenCount('after', childrenAfter, indent);
    } else {
      for (const child of children) {
        this.renderNode(child, newDepth, nodePaths);
      }
    }
  }
}
