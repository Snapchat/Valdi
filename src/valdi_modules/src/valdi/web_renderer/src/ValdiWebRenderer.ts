import { Renderer } from 'valdi_core/src/Renderer';
import { UpdateAttributeDelegate, ValdiWebRendererDelegate } from './ValdiWebRendererDelegate';
import { WebDebuggerBridge } from './debug/WebDebuggerBridge';

declare const require: (id: string) => any;

// Bootstrap the runtime (sets up globals, moduleLoader, etc.)
require('./ValdiWebRuntime');

export class ValdiWebRenderer extends Renderer implements UpdateAttributeDelegate {
  delegate: InstanceType<typeof ValdiWebRendererDelegate>;
  private readonly debuggerBridge: WebDebuggerBridge;

  constructor(htmlRoot: HTMLElement | ShadowRoot) {
    const delegate = new ValdiWebRendererDelegate(htmlRoot);
    super('valdi-web-renderer', ['view', 'label', 'layout', 'scroll', 'image', 'textfield', 'textview', 'spinner', 'custom-view', 'video', 'shape'], delegate);
    delegate.setAttributeDelegate(this);
    this.delegate = delegate;
    this.debuggerBridge = new WebDebuggerBridge(htmlRoot, delegate, this);
  }
  updateAttribute(elementId: number, attributeName: string, attributeValue: any) {
    super.attributeUpdatedExternally(elementId, attributeName, attributeValue);
  }

  destroy() {
    this.debuggerBridge.destroy();
    this.delegate.onDestroyed();
  }
}
