import { StringMap } from 'coreutils/src/StringMap';
import { StringSet } from 'coreutils/src/StringSet';
import { registerLogMetadataProvider } from './BugReporter';
import { ComponentPath, parseComponentPath, resolveComponentConstructor } from './ComponentPath';
import { ComponentRef } from './ComponentRef';
import { makeEntryPointRenderFunction } from './EntryPointComponent';
import { EntryPointRenderFunction } from './EntryPointRenderFunction';
import { IComponent } from './IComponent';
import { IEntryPointComponent } from './IEntryPointComponent';
import { RequireFunc } from './IModuleLoader';
import { fromRenderedVirtualNodeWithOptions } from './IRenderedVirtualNodeData';
import { IRootComponentsManager } from './IRootComponentsManager';
import { getModuleLoader } from './ModuleLoaderGlobal';
import { withLocalNativeRefs } from './NativeReferences';
import { Renderer } from './Renderer';
import { RendererFactory } from './RendererFactory';
import {
  DaemonClientManager,
  IDaemonClientManagerListener,
  ReceivedDaemonClientMessage,
} from './debugging/DaemonClientManager';
import { DebugLevel, SubmitDebugMessageFunc } from './debugging/DebugMessage';
import { DaemonClientMessageType, Messages, RemoteValdiContext } from './debugging/Messages';
import { NativeAppearanceDebugSettings } from './debugging/NativeAppearanceDebugSettings';
import { trace } from './utils/Trace';

interface StashedRootComponentHandle {
  contextId: string;
  componentPath: ComponentPath;
  componentContext: any;
  initialViewModel: any;
  latestViewModel: any;
}

export interface RootComponentHandle {
  contextId: string;
  renderer: Renderer;
  componentPath: ComponentPath;
  componentContext: any;
  initialViewModel: any;
  latestViewModel: any;
  rootRef: ComponentRef<IEntryPointComponent>;
  disposeFunction: () => void;
  renderFunction: EntryPointRenderFunction;
}

const requireWithoutDependencies: RequireFunc = (path, disableProxy) => {
  return getModuleLoader().load(path, disableProxy, true);
};

export class RootComponentsManager implements IRootComponentsManager, IDaemonClientManagerListener {
  readonly rootComponents: StringMap<RootComponentHandle> = {};

  private reloadedContextIds?: string[];
  private nativeAppearanceDebugSettings?: NativeAppearanceDebugSettings;

  constructor(
    readonly rendererFactory: RendererFactory,
    readonly daemonClientManager: DaemonClientManager | undefined,
    readonly submitDebugMessage: SubmitDebugMessageFunc,
  ) {
    if (daemonClientManager) {
      daemonClientManager.addListener(this);
      this.nativeAppearanceDebugSettings = new NativeAppearanceDebugSettings();
    }
  }

  stashData(): any {
    const handles: StashedRootComponentHandle[] = [];
    for (const contextId in this.rootComponents) {
      const rootComponentHandle = this.rootComponents[contextId]!;

      handles.push({
        contextId: rootComponentHandle.contextId,
        componentPath: rootComponentHandle.componentPath,
        componentContext: rootComponentHandle.componentContext,
        initialViewModel: rootComponentHandle.initialViewModel,
        latestViewModel: rootComponentHandle.latestViewModel,
      });

      withLocalNativeRefs(contextId, () => {
        rootComponentHandle.disposeFunction();

        try {
          rootComponentHandle.renderer.renderRoot(() => {});
        } catch (err: any) {
          console.error(`Error while disposing component before hot reloading:`, err);
        }
      });
    }

    if (this.daemonClientManager) {
      this.daemonClientManager.removeListener(this);
    }
    this.nativeAppearanceDebugSettings?.dispose();
    this.nativeAppearanceDebugSettings = undefined;

    return handles;
  }

  restoreData(data: any) {
    const handles = data as StashedRootComponentHandle[];
    if (!Array.isArray(handles)) {
      return;
    }

    for (const handle of handles) {
      if (this.rootComponents[handle.contextId]) {
        continue;
      }

      withLocalNativeRefs(handle.contextId, () => {
        this.createWithComponentPath(
          handle.contextId,
          handle.componentPath,
          handle.initialViewModel,
          handle.componentContext,
        );

        if (handle.latestViewModel !== handle.initialViewModel) {
          this.render(handle.contextId, handle.latestViewModel);
        }

        this.onHotReloaded(handle.contextId);
      });
    }
  }

  private createHandle(
    contextId: string,
    componentPath: ComponentPath,
    onHotReloadSubscription: (() => void) | undefined,
    renderFunction: EntryPointRenderFunction,
    viewModel: any,
    componentContext: any,
  ): RootComponentHandle {
    const renderer = this.rendererFactory.makeRenderer(contextId);
    const removeAppearanceObserver = this.nativeAppearanceDebugSettings?.addRenderer(renderer);

    const observerDisposer = registerLogMetadataProvider('Valdi Runtime', renderer.dumpLogMetadata.bind(renderer));

    const disposeFunction = () => {
      if (onHotReloadSubscription) {
        onHotReloadSubscription();
      }
      removeAppearanceObserver?.();
      observerDisposer();
      renderer.delegate.onDestroyed();
    };

    const rootRef = new ComponentRef<IEntryPointComponent>();

    const handle: RootComponentHandle = {
      contextId,
      renderer,
      componentPath,
      componentContext,
      initialViewModel: viewModel,
      latestViewModel: viewModel,
      rootRef,
      disposeFunction,
      renderFunction,
    };

    this.rootComponents[contextId] = handle;

    return handle;
  }

  private renderRoot(handle: RootComponentHandle, isInitialRender: boolean) {
    handle.renderer.renderRoot(() => {
      const traceName = isInitialRender
        ? `createRoot.${handle.componentPath.symbolName}`
        : `updateRoot.${handle.componentPath.symbolName}`;
      const viewModel = isInitialRender ? handle.initialViewModel : handle.latestViewModel;
      trace(traceName, () => {
        handle.renderFunction(handle.rootRef, viewModel, handle.componentContext);
      });
    });
  }

  private makeRenderFunction(componentPath: ComponentPath): EntryPointRenderFunction {
    return makeEntryPointRenderFunction(
      () => resolveComponentConstructor(requireWithoutDependencies, componentPath),
      !!this.daemonClientManager,
      this.daemonClientManager,
    );
  }

  private onHotReloaded(contextId: string) {
    let reloadedContextIds = this.reloadedContextIds;
    if (!reloadedContextIds) {
      reloadedContextIds = [];
      this.reloadedContextIds = reloadedContextIds;
      setTimeout(() => {
        this.showHotReloadedMessage();
      });
    }

    reloadedContextIds.push(contextId);
  }

  private showHotReloadedMessage() {
    const reloadedContextIds = this.reloadedContextIds;
    if (!reloadedContextIds) {
      return;
    }
    this.reloadedContextIds = undefined;

    const allReloadedComponentPaths: StringSet = {};
    for (const contextId of reloadedContextIds) {
      const handle = this.rootComponents[contextId];
      if (!handle) {
        continue;
      }

      const componentPath = `${handle.componentPath.symbolName}@${handle.componentPath.filePath}`;
      allReloadedComponentPaths[componentPath] = true;
    }

    this.submitDebugMessage(
      DebugLevel.DEBUG,
      `Reloaded ${reloadedContextIds.length} from ${Object.keys(allReloadedComponentPaths).join(', ')}`,
    );
  }

  private hotReload(contextId: string) {
    const handle = this.rootComponents[contextId];
    if (!handle) {
      return;
    }

    withLocalNativeRefs(contextId, () => {
      // Render once empty, so that we can destroy the whole tree
      handle.renderer.renderRoot(() => {});

      // Update the render function
      handle.renderFunction = this.makeRenderFunction(handle.componentPath);

      this.renderRoot(handle, true);

      if (handle.latestViewModel !== handle.initialViewModel) {
        this.renderRoot(handle, false);
      }

      this.onHotReloaded(contextId);
    });
  }

  getComponentForContextId(contextId: string): IComponent | undefined {
    const handle = this.rootComponents[contextId];
    if (!handle) {
      return undefined;
    }
    return handle.renderer.getRootComponent();
  }

  createWithRenderFunction(
    contextId: string,
    componentPath: ComponentPath,
    renderFunction: EntryPointRenderFunction,
    viewModel: any,
    componentContext: any,
  ): void {
    const handle = this.createHandle(contextId, componentPath, undefined, renderFunction, viewModel, componentContext);
    this.renderRoot(handle, true);
  }

  createWithComponentPath(contextId: string, componentPath: ComponentPath, viewModel: any, componentContext: any) {
    const onHotReloadSubscription = getModuleLoader().onHotReload(module, componentPath.filePath, () => {
      this.hotReload(contextId);
    });
    const renderFunction = this.makeRenderFunction(componentPath);

    const handle = this.createHandle(
      contextId,
      componentPath,
      onHotReloadSubscription,
      renderFunction,
      viewModel,
      componentContext,
    );

    this.renderRoot(handle, true);
  }

  create(contextId: string, componentPath: string, viewModel: any, componentContext: any): void {
    const parsedComponentPath = parseComponentPath(componentPath);
    this.createWithComponentPath(contextId, parsedComponentPath, viewModel, componentContext);
  }

  // TODO: rename to something like renderWithNewViewModel
  // (to reflect the fact that this gets callde by the runtime when the root view's
  // view model gets set).
  render(contextId: string, viewModel: any): void {
    const handle = this.rootComponents[contextId];
    if (!handle) {
      return;
    }
    handle.latestViewModel = viewModel;

    this.renderRoot(handle, false);
  }

  destroy(contextId: string): void {
    const handle = this.rootComponents[contextId];
    if (!handle) {
      return;
    }

    trace(`destroyRoot.${handle.componentPath.symbolName}`, () => {
      delete this.rootComponents[contextId];
      handle.disposeFunction();

      handle.renderer.renderRoot(() => {});
    });
  }

  attributeChanged(contextId: string, nodeId: number, attributeName: string, attributeValue: any): void {
    const handle = this.rootComponents[contextId];
    if (!handle) {
      return;
    }

    handle.renderer.attributeUpdatedExternally(nodeId, attributeName, attributeValue);
  }

  callComponentFunction(contextId: string, functionName: string, parameters: any[]): any {
    const rootComponent = this.rootComponents[contextId]?.rootRef.single();
    if (!rootComponent) {
      throw new Error(`Could not resolve component for id ${contextId}`);
    }

    return rootComponent.forwardCall(functionName, parameters);
  }

  onRuntimeIssue(contextId: string, isError: boolean, message: string): void {
    const handle = this.rootComponents[contextId];
    if (!handle) {
      return;
    }

    handle.renderer.onRuntimeIssue(isError, message);
  }

  onMessage(message: ReceivedDaemonClientMessage): void {
    if (message.message.type === DaemonClientMessageType.LIST_CONTEXTS_REQUEST) {
      const contexts: RemoteValdiContext[] = [];

      for (const treeId in this.rootComponents) {
        const registeredRenderer = this.rootComponents[treeId]!;
        const entryPointComponent = registeredRenderer.rootRef.single();
        const rootComponentName = entryPointComponent?.rootComponent?.constructor.name ?? '<undefined>';

        contexts.push({
          id: treeId,
          rootComponentName,
        });
      }

      message.respond(requestId => Messages.listContextsResponse(requestId, contexts));
    } else if (message.message.type === DaemonClientMessageType.GET_CONTEXT_TREE_REQUEST) {
      const contextId = message.message.body.id;
      const registeredRenderer = this.rootComponents[contextId];
      if (!registeredRenderer) {
        message.respond(requestId => Messages.errorResponse(requestId, `No context with id ${contextId}`));
        return;
      }

      const rootNode = registeredRenderer.renderer.getRootVirtualNode();
      if (!rootNode) {
        message.respond(requestId => Messages.errorResponse(requestId, `No root node`));
        return;
      }

      const rootNodeData = fromRenderedVirtualNodeWithOptions(rootNode, {
        includeAttributes: true,
        includeComponentData: message.message.body.includeComponentData === true,
        onCreate: undefined,
      });
      message.respond(requestId => Messages.getContextTreeResponse(requestId, rootNodeData));
    } else if (message.message.type === DaemonClientMessageType.TAKE_ELEMENT_SNAPSHOT_REQUEST) {
      const contextId = message.message.body.contextId;
      const registeredRenderer = this.rootComponents[contextId];
      if (!registeredRenderer) {
        message.respond(requestId => Messages.errorResponse(requestId, `No context with id ${contextId}`));
        return;
      }

      const elementId = message.message.body.elementId;

      const element = registeredRenderer.renderer.getElementForId(elementId);
      if (!element) {
        message.respond(requestId => Messages.errorResponse(requestId, `No node with id ${elementId}`));
        return;
      }

      // eslint-disable-next-line @typescript-eslint/no-floating-promises
      element.takeSnapshot().then(snapshot => {
        message.respond(requestId => Messages.takeElementSnapshotResponse(requestId, snapshot ?? ''));
      });
    }
  }
}
