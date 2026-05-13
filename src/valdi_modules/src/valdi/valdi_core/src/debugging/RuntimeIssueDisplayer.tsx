import { Label } from 'valdi_tsx/src/NativeTemplateElements';
import { Component } from '../Component';
import { getVirtualNodeDataFromRootToChild } from '../IRenderedVirtualNodeData';
import { Style } from '../Style';
import { when } from '../utils/When';
import { VirtualNodePathDisplayer } from './VirtualNodePathDisplayer';

export interface RuntimeIssue {
  isError: boolean;
  message: string;
  nodeId?: number;
}

export interface RuntimeIssueViewModel {
  issue: RuntimeIssue;
}

const baseTextStyle = new Style<Label>({
  textAlign: 'center',
  numberOfLines: 0,
});

const errorTypeStyle = baseTextStyle.extend({
  font: 'Montserrat-Bold 16',
  marginBottom: 8,
});

const titleStyle = baseTextStyle.extend({
  font: 'Montserrat-SemiBold 12',
});

const genericTextStyle = baseTextStyle.extend({
  font: 'Montserrat-Medium 12',
});

export class RuntimeIssueDisplayer extends Component<RuntimeIssueViewModel> {
  onRender() {
    const issue = this.viewModel.issue;

    <layout>
      <layout>
        <label
          value={issue.isError ? 'Error' : 'Warning'}
          color={issue.isError ? 'red' : 'orange'}
          style={errorTypeStyle}
        />
        <label value={issue.message} style={titleStyle} />
      </layout>
      <layout marginTop={20}>
        {when(issue.nodeId, nodeId => this.renderNodePath(nodeId))}
        {when(!issue.nodeId, () => this.renderGenericIssue())}
      </layout>
    </layout>;
  }

  private renderNodePath(nodeId: number) {
    const element = this.renderer.getElementForId(nodeId);
    if (!element) {
      this.renderIssue(`Could not resolve RenderedElement for id ${nodeId}`);
      return;
    }

    const pair = getVirtualNodeDataFromRootToChild(element.getVirtualNode());

    <VirtualNodePathDisplayer node={pair.root} focusedNode={pair.child} />;
  }

  private renderGenericIssue() {
    this.renderIssue(`No further informations available to display.
    If you feel we should be able to provide more data here, please reach out to us on Slack on #valdi!
    `);
  }

  private renderIssue(message: string) {
    <label style={genericTextStyle} value={message} />;
  }
}
