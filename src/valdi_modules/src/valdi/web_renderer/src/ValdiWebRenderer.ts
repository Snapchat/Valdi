import type { ComponentPrototype } from 'valdi_core/src/ComponentPrototype';
import type { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import type { RequireFunc } from 'valdi_core/src/IModuleLoader';
import { Renderer } from 'valdi_core/src/Renderer';
import { getValdiRuntime } from 'valdi_core/src/ValdiRuntimeProvider';
import { ValdiWebRendererDelegate } from './ValdiWebRendererDelegate';
import { createIsolatedWebRendererRoot, registerWebRendererLayoutRoot } from './WebRendererRoot';
import type { WebRendererLayoutRegistration } from './WebRendererRoot';
import type { AttributeUpdatedExternallyDelegate } from './core/ElementClass';
import { COLOR_PALETTE_MANAGER } from './core/Palette';
import { ViewNodeTree } from './core/ViewNodeTree';
import { WebDebuggerBridge } from './debug/WebDebuggerBridge';
import { WebNavigationHost } from './navigation/WebNavigator';
import { isValdiWebTracingEnabled } from './tracing/ValdiWebTracing';
import { configureValdiWebTracingFromLocation } from './tracing/WebTracingConfiguration';

export { createViewFactory } from './ViewFactory';

declare const require: RequireFunc;

type WebRuntime = {
  requireByComponent(componentPath: string): ComponentConstructor<IComponent> | undefined;
};

// Resolved per call: on web the host may install the runtime after this module evaluates.
function getWebRuntime(): WebRuntime {
  return getValdiRuntime() as unknown as WebRuntime;
}

function resolveRegisteredComponent(componentPath: string): ComponentConstructor<IComponent> {
  const constructor = getWebRuntime().requireByComponent(componentPath);
  if (!constructor) {
    throw new Error(`Could not resolve component ${componentPath}`);
  }
  return constructor;
}

// Collapsed web packages generate narrow registries for runtime-only lookups.
try {
  require('../../_navigation_registry');
  require('../../_worker_registry');
} catch {
  // Non-collapsed test environments do not provide generated registries.
}

// Collapsed web packages generate browser worker factories at this path.
// Keep this host-only: worker entries import ValdiWebRuntime, and loading the
// factories from there would make worker chunks discover worker entries again.
try {
  require('../../_web_worker_factories');
} catch {
  // Optional: only collapsed web packages generate worker factories.
}

let CONTEXT_ID_SEQUENCE = 0;

function makeContextId(contextIdentifierPrefix: string | undefined): string {
  const contextIdSuffix = (++CONTEXT_ID_SEQUENCE).toString();
  return contextIdentifierPrefix === undefined ? contextIdSuffix : `${contextIdentifierPrefix}-${contextIdSuffix}`;
}

export class ValdiWebRenderer extends Renderer implements AttributeUpdatedExternallyDelegate {
  delegate: InstanceType<typeof ValdiWebRendererDelegate>;
  private readonly debuggerBridge: InstanceType<typeof WebDebuggerBridge>;
  private readonly isolatedRoot: HTMLElement;
  private readonly layoutRegistration: WebRendererLayoutRegistration;
  private navigationHost?: WebNavigationHost;

  constructor(htmlRoot: HTMLElement | ShadowRoot, contextIdentifierPrefix?: string) {
    configureValdiWebTracingFromLocation(typeof window === 'undefined' ? undefined : window.location?.search);
    const isolatedRoot = createIsolatedWebRendererRoot(htmlRoot);
    const viewNodeTree = new ViewNodeTree(COLOR_PALETTE_MANAGER);
    const delegate = new ValdiWebRendererDelegate(isolatedRoot, viewNodeTree);
    viewNodeTree.setPostLayoutScheduler((callback: () => void) => delegate.onNextLayoutComplete(callback));
    super(
      makeContextId(contextIdentifierPrefix),
      [
        'view',
        'label',
        'layout',
        'scroll',
        'image',
        'animatedimage',
        'textfield',
        'textview',
        'spinner',
        'custom-view',
        'video',
        'shape',
        'blur',
        'webview',
      ],
      delegate,
      undefined,
      isValdiWebTracingEnabled(),
    );
    delegate.setAttributeUpdatedExternallyDelegate(this);
    this.delegate = delegate;
    this.isolatedRoot = isolatedRoot;
    this.layoutRegistration = registerWebRendererLayoutRoot(this.contextId, isolatedRoot);
    ViewNodeTree.register(this.contextId, viewNodeTree);
    this.debuggerBridge = new WebDebuggerBridge(isolatedRoot, delegate, this, viewNodeTree);
  }

  onAttributeUpdatedExternally(elementId: number, attributeName: string, attributeValue: unknown): void {
    super.attributeUpdatedExternally(elementId, attributeName, attributeValue);
  }

  /**
   * Tears the renderer down, giving mounted components their onDestroy hooks.
   * Kept from the v1 API for hosts that own a renderer directly.
   */
  destroy(): void {
    this.renderRoot(() => {});
    this.onDestroy();
  }

  override onDestroy(): void {
    this.navigationHost?.destroy();
    this.navigationHost = undefined;
    this.debuggerBridge.destroy();
    ViewNodeTree.unregister(this.contextId);
    this.layoutRegistration.dispose();
    super.onDestroy();
  }

  renderRootComponent<T extends IComponent<ViewModel, Context>, ViewModel = any, Context = any>(
    ctr: ComponentConstructor<T>,
    prototype: ComponentPrototype,
    viewModel: ViewModel,
    context: Context,
  ): void {
    const sourceContext = context && typeof context === 'object' ? context : {};
    const existingNavigator = (sourceContext as { navigator?: unknown }).navigator;
    if (existingNavigator !== undefined) {
      super.renderRootComponent(ctr, prototype, viewModel, context);
      return;
    }

    if (!this.navigationHost) {
      this.navigationHost = new WebNavigationHost({
        createRenderer: (root, contextIdentifierPrefix) => new ValdiWebRenderer(root, contextIdentifierPrefix),
        resolveComponent: resolveRegisteredComponent,
        root: this.isolatedRoot,
      });
    }
    const componentContext = { ...sourceContext, navigator: this.navigationHost.rootNavigator } as Context;
    super.renderRootComponent(ctr, prototype, viewModel, componentContext);
  }
}
