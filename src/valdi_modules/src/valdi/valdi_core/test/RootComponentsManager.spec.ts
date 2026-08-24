import 'jasmine/src/jasmine';
import { IComponent } from '../src/IComponent';
import { IRenderedVirtualNode } from '../src/IRenderedVirtualNode';
import { IRenderedVirtualNodeData } from '../src/IRenderedVirtualNodeData';
import { RootComponentHandle, RootComponentsManager } from '../src/RootComponentsManager';
import { RendererFactory } from '../src/RendererFactory';
import { ReceivedDaemonClientMessage } from '../src/debugging/DaemonClientManager';
import { GetContextTreeBody, Messages } from '../src/debugging/Messages';

function createComponentNode(): IRenderedVirtualNode {
  const component = {
    viewModel: { title: 'Sensitive input' },
    state: { token: 'sensitive-state' },
  } as unknown as IComponent;

  return {
    key: 'component',
    uniqueId: 'component',
    parent: undefined,
    element: undefined,
    component,
    children: [],
    parentIndex: 0,
    renderer: component.renderer,
  };
}

function createManager(rootNode: IRenderedVirtualNode): RootComponentsManager {
  const manager = new RootComponentsManager({} as RendererFactory, undefined, () => {});
  manager.rootComponents['context'] = {
    renderer: {
      getRootVirtualNode: () => rootNode,
    },
  } as RootComponentHandle;
  return manager;
}

function requestContextTree(
  manager: RootComponentsManager,
  includeComponentData: boolean | undefined,
): IRenderedVirtualNodeData {
  const body: GetContextTreeBody = { id: 'context' };
  if (includeComponentData !== undefined) {
    body.includeComponentData = includeComponentData;
  }

  const request = Messages.parse(1, Messages.getContextTreeRequest('request-1', body));
  let responseJson: string | undefined;
  const receivedMessage = {
    message: request,
    respond: (makeMessage: (requestId: string) => string) => {
      responseJson = makeMessage(request.requestId);
    },
  } as ReceivedDaemonClientMessage;

  manager.onMessage(receivedMessage);
  if (responseJson === undefined) {
    throw new Error('RootComponentsManager did not respond to the tree request.');
  }

  return Messages.parse(1, responseJson).body as IRenderedVirtualNodeData;
}

describe('RootComponentsManager debugger tree serialization', () => {
  it('does not expose component data to existing tree clients by default', () => {
    const data = requestContextTree(createManager(createComponentNode()), undefined);

    expect(data.component).toEqual({});
  });

  it('exposes component data only when the request explicitly opts in', () => {
    const data = requestContextTree(createManager(createComponentNode()), true);

    expect(data.component?.viewModel).toContain('Sensitive input');
    expect(data.component?.state).toContain('sensitive-state');
  });
});
