import type { RequireFunc } from 'valdi_core/src/IModuleLoader';
import type { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import type { ComponentPrototype } from 'valdi_core/src/ComponentPrototype';
import type { Renderer as RendererType } from 'valdi_core/src/Renderer';
import type { AttributeUpdatedExternallyDelegate } from './core/ElementClass';
import { getValdiRuntime } from 'valdi_core/src/ValdiRuntimeProvider';
import { isValdiWebTracingEnabled } from './tracing/ValdiWebTracing';

declare const require: RequireFunc;

getValdiRuntime();

// Collapsed web packages generate browser worker factories at this path.
// Keep this host-only: worker entries import ValdiWebRuntime, and loading the
// factories from there would make worker chunks discover worker entries again.
try {
  require('../../_web_worker_factories');
} catch {}

declare const moduleLoader: any;

const customRequire = moduleLoader.resolveRequire('web_renderer/src/ValdiWebRenderer.ts');

const { Renderer } = customRequire('valdi_core/src/Renderer') as { Renderer: typeof RendererType };
const rendererDelegate = customRequire('./ValdiWebRendererDelegate') as typeof import('./ValdiWebRendererDelegate');
const rendererCore = customRequire('./core/ViewNodeTree') as typeof import('./core/ViewNodeTree');
const paletteCore = customRequire('./core/Palette') as typeof import('./core/Palette');
const debuggerCore = customRequire('./debug/WebDebuggerBridge') as typeof import('./debug/WebDebuggerBridge');
const rootCore = customRequire('./WebRendererRoot') as typeof import('./WebRendererRoot');
const ValdiWebRendererDelegate = rendererDelegate.ValdiWebRendererDelegate;
const ViewNodeTree = rendererCore.ViewNodeTree;
const COLOR_PALETTE_MANAGER = paletteCore.COLOR_PALETTE_MANAGER;
const WebDebuggerBridge = debuggerCore.WebDebuggerBridge;
const createIsolatedWebRendererRoot = rootCore.createIsolatedWebRendererRoot;

let CONTEXT_ID_SEQUENCE = 0;

function makeContextId(contextIdentifierPrefix: string | undefined): string {
  const contextIdSuffix = (++CONTEXT_ID_SEQUENCE).toString();
  return contextIdentifierPrefix === undefined ? contextIdSuffix : `${contextIdentifierPrefix}-${contextIdSuffix}`;
}

export class ValdiWebRenderer extends Renderer implements AttributeUpdatedExternallyDelegate {
  delegate: InstanceType<typeof ValdiWebRendererDelegate>;
  private readonly debuggerBridge: InstanceType<typeof WebDebuggerBridge>;

  constructor(htmlRoot: HTMLElement | ShadowRoot, contextIdentifierPrefix?: string) {
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
    this.debuggerBridge = new WebDebuggerBridge(isolatedRoot, viewNodeTree);
  }

  onAttributeUpdatedExternally(elementId: number, attributeName: string, attributeValue: unknown): void {
    super.attributeUpdatedExternally(elementId, attributeName, attributeValue);
  }

  setComponentContext(context: any): void {
    super.setComponentContext(context);
    super.setViewModelProperty('context', context);
  }

  override onDestroy(): void {
    this.debuggerBridge.destroy();
    super.onDestroy();
  }

  renderRootComponent<T extends IComponent<ViewModel, Context>, ViewModel = any, Context = any>(
    ctr: ComponentConstructor<T>,
    prototype: ComponentPrototype,
    viewModel: ViewModel,
    context: Context,
  ): void {
    super.renderRootComponent(ctr, prototype, viewModel, context);
  }
}
