import 'jasmine/src/jasmine';
import { IComponent } from '../src/IComponent';
import { IRenderedVirtualNode } from '../src/IRenderedVirtualNode';
import { IRenderedVirtualNodeData } from '../src/IRenderedVirtualNodeData';
import { RootComponentHandle, RootComponentsManager } from '../src/RootComponentsManager';
import { RendererFactory } from '../src/RendererFactory';
import { ReceivedDaemonClientMessage } from '../src/debugging/DaemonClientManager';
import {
  DaemonClientMessage,
  DaemonClientMessageType,
  GetContextTreeBody,
  Messages,
  PerformanceTraceStatusBody,
} from '../src/debugging/Messages';

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
  let tracingEnabled = false;
  manager.rootComponents['context'] = {
    contextId: 'context',
    componentPath: { filePath: 'test', symbolName: 'TestRoot' },
    disposeFunction: () => {},
    renderer: {
      getRootVirtualNode: () => rootNode,
      renderRoot: (render: () => void) => render(),
      isTracingEnabled: () => tracingEnabled,
      setTracingEnabled: (enabled: boolean) => {
        tracingEnabled = enabled;
      },
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

function sendDebuggerMessage(manager: RootComponentsManager, requestJson: string): DaemonClientMessage {
  const request = Messages.parse(1, requestJson);
  let responseJson: string | undefined;
  const receivedMessage = {
    message: request,
    respond: (makeMessage: (requestId: string) => string) => {
      responseJson = makeMessage(request.requestId);
    },
  } as ReceivedDaemonClientMessage;

  manager.onMessage(receivedMessage);
  if (responseJson === undefined) {
    throw new Error('RootComponentsManager did not respond to the debugger request.');
  }
  return Messages.parse(1, responseJson);
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

describe('RootComponentsManager performance tracing', () => {
  it('reports an idle trace recorder through the daemon protocol', () => {
    const manager = createManager(createComponentNode());
    const response = sendDebuggerMessage(manager, Messages.performanceTraceStatusRequest('trace-status', {}));
    const body = response.body as PerformanceTraceStatusBody;

    expect(response.type).toBe(DaemonClientMessageType.PERFORMANCE_TRACE_STATUS_RESPONSE);
    expect(body.recording).toBeFalse();
    expect(body.rendererTracingEnabled).toBeFalse();
  });

  it('returns a protocol error when the requested renderer context does not exist', () => {
    const manager = createManager(createComponentNode());
    const response = sendDebuggerMessage(
      manager,
      Messages.performanceTraceStartRequest('trace-start', { contextId: 'missing' }),
    );

    expect(response.type).toBe(DaemonClientMessageType.ERROR_RESPONSE);
    expect((response.body as { message: string }).message).toContain('No Valdi renderer found for context missing');
  });

  it('binds stop requests to the context that started the recording', () => {
    const manager = createManager(createComponentNode());
    const startResponse = sendDebuggerMessage(
      manager,
      Messages.performanceTraceStartRequest('trace-start', { contextId: 'context' }),
    );
    const wrongStopResponse = sendDebuggerMessage(
      manager,
      Messages.performanceTraceStopRequest('trace-stop-wrong', { contextId: 'other' }),
    );
    const statusResponse = sendDebuggerMessage(manager, Messages.performanceTraceStatusRequest('trace-status', {}));
    const stopResponse = sendDebuggerMessage(
      manager,
      Messages.performanceTraceStopRequest('trace-stop', { contextId: 'context' }),
    );

    expect(startResponse.type).toBe(DaemonClientMessageType.PERFORMANCE_TRACE_START_RESPONSE);
    expect((startResponse.body as PerformanceTraceStatusBody).contextId).toBe('context');
    expect(wrongStopResponse.type).toBe(DaemonClientMessageType.ERROR_RESPONSE);
    expect((wrongStopResponse.body as { message: string }).message).toContain(
      'Valdi performance trace recording belongs to context context, not other',
    );
    expect((statusResponse.body as PerformanceTraceStatusBody).contextId).toBe('context');
    expect(stopResponse.type).toBe(DaemonClientMessageType.PERFORMANCE_TRACE_STOP_RESPONSE);
  });

  it('aborts an active recording when its root context is destroyed', () => {
    const manager = createManager(createComponentNode());
    sendDebuggerMessage(manager, Messages.performanceTraceStartRequest('trace-start', { contextId: 'context' }));

    manager.destroy('context');
    const statusResponse = sendDebuggerMessage(manager, Messages.performanceTraceStatusRequest('trace-status', {}));

    expect((statusResponse.body as PerformanceTraceStatusBody).recording).toBeFalse();
  });
});
