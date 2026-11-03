import { $slot } from 'valdi_core/src/CompilerIntrinsics';
import { Component } from 'valdi_core/src/Component';
import { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import { IRenderedVirtualNode } from 'valdi_core/src/IRenderedVirtualNode';
import { ProviderConstructor, ProviderValue } from 'valdi_core/src/provider/ProviderComponent';
import { RendererError } from 'valdi_core/src/utils/RendererError';
import { InstrumentedComponentBase } from 'valdi_standalone/src/ValdiStandalone';
import 'jasmine/src/jasmine';

export class InstrumentedComponentJSX<
  T extends IComponent,
  ViewModelType extends {},
  ContextType extends {},
> extends InstrumentedComponentBase<T, ViewModelType, ContextType> {
  static create<T extends IComponent, ViewModelType extends object, ContextType extends object>(
    componentConstructor: ComponentConstructor<T>,
    viewModel: ViewModelType | undefined,
    context: ContextType | undefined,
  ): InstrumentedComponentJSX<T, ViewModelType, ContextType> {
    return new InstrumentedComponentJSX(componentConstructor, viewModel, context, false);
  }
}

/**
 * @deprecated
 * @see valdiIt
 */
export function createComponent<
  T extends IComponent,
  ViewModel extends object = object,
  Context extends object = object,
>(
  componentConstructor: ComponentConstructor<T>,
  viewModel?: ViewModel,
  context?: Context,
): InstrumentedComponentJSX<T, ViewModel, Context> {
  return InstrumentedComponentJSX.create(componentConstructor, viewModel, context);
}

interface ViewModel {
  renderCb?: () => void;
}

class TestComponentContainer extends Component<ViewModel> {
  onRender() {
    if (this.viewModel.renderCb) {
      this.viewModel.renderCb();
    }
  }
}

export interface IComponentTestDriver {
  /**
   * Perform a render by evaluating the given render function.
   * Returns the nodes that were rendered as part of the render.
   */
  render(cb: () => void): IRenderedVirtualNode[];

  /**
   * Performs a render by rendering the given component with the given
   * viewModel and context. Returns the rendered component instance.
   */
  renderComponent<T extends IComponent, ViewModelType extends object, ContextType extends object>(
    componentConstructor: ComponentConstructor<T>,
    viewModel: ViewModelType | undefined,
    context: ContextType | undefined,
  ): T;

  /**
   * Performs a render by wrapping the given component with the given
   * service provider and by rendering it with the given viewModel and context.
   * Returns the rendered component instance.
   */
  renderComponentWithService<
    T extends IComponent,
    ViewModelType extends object,
    ContextType extends object,
    ServiceProviderType extends object,
  >(
    componentConstructor: ComponentConstructor<T>,
    viewModel: ViewModelType | undefined,
    context: ContextType | undefined,
    providerConstructor: ProviderConstructor<ServiceProviderType>,
    providerValue: ProviderValue<ServiceProviderType>,
  ): T;

  /**
   * Performs a render by wrapping the given component with the given
   * service providers and by rendering it with the given viewModel and context.
   * Example:
   * <outer>
   *  <inner>
   *   <component />
   *  </inner>
   * </outer>
   * Returns the rendered component instance.
   */
  renderComponentWithServices<
    T extends IComponent,
    ViewModelType extends object,
    ContextType extends object,
    ServiceProviderTypeOuter extends object,
    ServiceProviderTypeInner extends object,
  >(
    componentConstructor: ComponentConstructor<T>,
    viewModel: ViewModelType | undefined,
    context: ContextType | undefined,
    outerProviderConstructor: ProviderConstructor<ServiceProviderTypeOuter>,
    outerProviderValue: ProviderValue<ServiceProviderTypeOuter>,
    innerProviderConstructor: ProviderConstructor<ServiceProviderTypeInner>,
    innerProviderValue: ProviderValue<ServiceProviderTypeInner>,
  ): T;

  /**
   * Perform the layout with the given width and height which represents
   * the size of the main container.
   */
  performLayout(params: { width: number; height: number; rtl?: boolean }): Promise<void>;
}

interface ErrorCatcherViewNodel {
  children?: () => void;
  onInterceptedError(error: Error): void;
}

class ErrorCatcher extends Component<ErrorCatcherViewNodel> {
  onRender(): void {
    this.viewModel.children?.();
  }

  onError(error: Error) {
    this.viewModel.onInterceptedError(error);
  }
}

class ComponentTestDriverImpl implements IComponentTestDriver {
  public errors: Error[] = [];

  constructor(readonly instrumentedComponent: InstrumentedComponentJSX<TestComponentContainer, ViewModel, {}>) {}

  render(cb: () => void): IRenderedVirtualNode[] {
    const wrappedCb = () => {
      // eslint-disable-next-line @snap/valdi/jsx-no-lambda -- test driver wraps each render callback once; the handler does not need a stable identity
      <ErrorCatcher onInterceptedError={error => this.errors.push(error)}>{$slot(cb)}</ErrorCatcher>;
    };
    this.instrumentedComponent.setViewModel({ renderCb: wrappedCb });

    const component = this.instrumentedComponent.getComponent();
    const errorCatcherNode = component.renderer.getComponentVirtualNode(component).children[0];
    return errorCatcherNode.children;
  }

  renderComponent<T extends IComponent<any, any>, ViewModelType extends object, ContextType extends object>(
    componentConstructor: ComponentConstructor<T>,
    viewModel: ViewModelType | undefined,
    context: ContextType | undefined,
  ): T {
    const nodes = this.render(() => {
      const Ctor = componentConstructor;
      <Ctor context={context} {...viewModel} />;
    });

    return nodes[0].component as T;
  }

  renderComponentWithService<
    T extends IComponent<any, any>,
    ViewModelType extends object,
    ContextType extends object,
    ServiceProviderType extends object,
  >(
    componentConstructor: ComponentConstructor<T>,
    viewModel: ViewModelType | undefined,
    context: ContextType | undefined,
    providerConstructor: ProviderConstructor<ServiceProviderType>,
    providerValue: ProviderValue<ServiceProviderType>,
  ): T {
    const nodes = this.render(() => {
      const Ctor = componentConstructor;
      const Ptor = providerConstructor;
      <Ptor value={providerValue}>
        <Ctor context={context} {...viewModel} />;
      </Ptor>;
    });

    return nodes[0].children[0].children[0].component as T;
  }

  renderComponentWithServices<
    T extends IComponent,
    ViewModelType extends object,
    ContextType extends object,
    ServiceProviderTypeOuter extends object,
    ServiceProviderTypeInner extends object,
  >(
    componentConstructor: ComponentConstructor<T>,
    viewModel: ViewModelType | undefined,
    context: ContextType | undefined,
    outerProviderConstructor: ProviderConstructor<ServiceProviderTypeOuter>,
    outerProviderValue: ProviderValue<ServiceProviderTypeOuter>,
    innerProviderConstructor: ProviderConstructor<ServiceProviderTypeInner>,
    innerProviderValue: ProviderValue<ServiceProviderTypeInner>,
  ): T {
    const nodes = this.render(() => {
      const Ctor = componentConstructor;
      const PtorOuter = outerProviderConstructor;
      const PtorInner = innerProviderConstructor;
      <PtorOuter value={outerProviderValue}>
        <PtorInner value={innerProviderValue}>
          <Ctor context={context} {...viewModel} />
        </PtorInner>
      </PtorOuter>;
    });

    return nodes[0].children[0].children[0].component as T;
  }

  performLayout(params: { width: number; height: number; rtl?: boolean }): Promise<void> {
    this.instrumentedComponent.setLayoutSpecs(params.width, params.height, params.rtl);
    return this.instrumentedComponent.waitForNextLayout();
  }
}

export function makeComponentTest(cb: (driver: IComponentTestDriver) => Promise<void>): () => Promise<void> {
  return async () => {
    const component = InstrumentedComponentJSX.create(TestComponentContainer, {}, {});

    const driver = new ComponentTestDriverImpl(component);
    try {
      await cb(driver);
      if (driver.errors.length > 0) {
        const firstError = driver.errors[0];
        if (firstError instanceof RendererError) {
          throw firstError.sourceError;
        } else {
          throw firstError;
        }
      }
    } finally {
      component.destroy();
    }
  };
}

export function valdiIt(expectation: string, assertion: (driver: IComponentTestDriver) => Promise<void>) {
  it(expectation, makeComponentTest(assertion));
}

export function fvaldiIt(expectation: string, assertion: (driver: IComponentTestDriver) => Promise<void>) {
  fit(expectation, makeComponentTest(assertion));
}

export function xvaldiIt(expectation: string, assertion: (driver: IComponentTestDriver) => Promise<void>) {
  xit(expectation, makeComponentTest(assertion));
}

export function withValdiRenderer(assertion: (driver: IComponentTestDriver) => Promise<void>): () => Promise<void> {
  return makeComponentTest(assertion);
}
