import { RequireFunc } from 'valdi_core/src/IModuleLoader';
import { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import { ComponentPrototype } from 'valdi_core/src/ComponentPrototype';
declare const require: RequireFunc;

// Require this to get the globals to run and setup env
require('./ValdiWebRuntime');

declare const moduleLoader: any;

var customRequire = moduleLoader.resolveRequire('web_renderer/src/ValdiWebRenderer.ts');


const { Renderer } = customRequire('valdi_core/src/Renderer');
const rendererDelegate = customRequire('./ValdiWebRendererDelegate');
type UpdateAttributeDelegate = typeof rendererDelegate.UpdateAttributeDelegate;
type ValdiWebRendererDelegateType = typeof rendererDelegate.ValdiWebRendererDelegate;
const ValdiWebRendererDelegate = rendererDelegate.ValdiWebRendererDelegate;


export class ValdiWebRenderer extends Renderer implements UpdateAttributeDelegate {
  delegate: ValdiWebRendererDelegateType;

  constructor(private htmlRoot: HTMLElement | ShadowRoot) {
    const delegate = new ValdiWebRendererDelegate(htmlRoot);
    super('valdi-web-renderer', ['view', 'label', 'layout', 'scroll', 'image', 'textfield', 'spinner'], delegate);
    delegate.setAttributeDelegate(this);
    this.delegate = delegate;
  }
  updateAttribute(elementId: number, attributeName: string, attributeValue: any) {
    super.attributeUpdatedExternally(elementId, attributeName, attributeValue);
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