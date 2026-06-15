import { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import { ComponentPrototype } from 'valdi_core/src/ComponentPrototype';
import { Renderer } from 'valdi_core/src/Renderer';
import { UpdateAttributeDelegate, ValdiWebRendererDelegate } from './ValdiWebRendererDelegate';

declare const require: (id: string) => any;

// Bootstrap the runtime (sets up globals, moduleLoader, etc.)
require('./ValdiWebRuntime');

export class ValdiWebRenderer extends Renderer implements UpdateAttributeDelegate {
  delegate: InstanceType<typeof ValdiWebRendererDelegate>;

  constructor(htmlRoot: HTMLElement | ShadowRoot) {
    const delegate = new ValdiWebRendererDelegate(htmlRoot);
    super('valdi-web-renderer', ['view', 'label', 'layout', 'scroll', 'image', 'textfield', 'textview', 'spinner', 'custom-view', 'video', 'shape'], delegate);
    delegate.setAttributeDelegate(this);
    this.delegate = delegate;
  }
  updateAttribute(elementId: number, attributeName: string, attributeValue: any) {
    super.attributeUpdatedExternally(elementId, attributeName, attributeValue);
  }

  destroy() {
    this.delegate.onDestroyed();
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
