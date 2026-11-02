/**
 * Web stub for valdi_test/test/JSXTestUtils.
 *
 * Surface mirrors the real test harness so any compiled require() of
 * 'valdi_test/test/JSXTestUtils' resolves to a module with the expected
 * exports. Implementations are no-ops — real test harness depends on
 * jasmine + valdi_standalone InstrumentedComponentBase + JSX renderer
 * machinery, none of which ship in the web npm package.
 *
 * Loose `any` typing keeps the stub free of valdi_core/valdi_standalone
 * imports; callers of the real module retain strict types via the test/
 * sources at native build time.
 */

export class InstrumentedComponentJSX<T = any, ViewModelType = any, ContextType = any> {
  // Phantom fields so TypeScript treats the generic params as used.
  private _t?: T;
  private _v?: ViewModelType;
  private _c?: ContextType;

  static create<T = any, ViewModelType = any, ContextType = any>(
    _componentConstructor: any,
    _viewModel?: ViewModelType,
    _context?: ContextType,
  ): InstrumentedComponentJSX<T, ViewModelType, ContextType> {
    return new InstrumentedComponentJSX<T, ViewModelType, ContextType>();
  }

  getComponent(): any {
    return undefined;
  }
  getRenderedNode(): any {
    return undefined;
  }
  setViewModel(_viewModel: any): void {}
  setLayoutSpecs(_width: number, _height: number, _rtl?: boolean): void {}
  waitForNextLayout(): Promise<void> {
    return Promise.resolve();
  }
  destroy(): void {}
}

export function createComponent<T = any, ViewModel = any, Context = any>(
  componentConstructor: any,
  viewModel?: ViewModel,
  context?: Context,
): InstrumentedComponentJSX<T, ViewModel, Context> {
  return InstrumentedComponentJSX.create<T, ViewModel, Context>(componentConstructor, viewModel, context);
}

export interface IComponentTestDriver {
  render(cb: () => void): any[];
  renderComponent<T = any>(componentConstructor: any, viewModel: any, context: any): T;
  renderComponentWithService<T = any>(
    componentConstructor: any,
    viewModel: any,
    context: any,
    providerConstructor: any,
    providerValue: any,
  ): T;
  renderComponentWithServices<T = any>(
    componentConstructor: any,
    viewModel: any,
    context: any,
    outerProviderConstructor: any,
    outerProviderValue: any,
    innerProviderConstructor: any,
    innerProviderValue: any,
  ): T;
  performLayout(params: { width: number; height: number; rtl?: boolean }): Promise<void>;
}

const noopDriver: IComponentTestDriver = {
  render(_cb) {
    return [];
  },
  renderComponent(_ctor, _vm, _ctx) {
    return undefined as any;
  },
  renderComponentWithService(_ctor, _vm, _ctx, _pCtor, _pVal) {
    return undefined as any;
  },
  renderComponentWithServices(_ctor, _vm, _ctx, _oCtor, _oVal, _iCtor, _iVal) {
    return undefined as any;
  },
  performLayout(_params) {
    return Promise.resolve();
  },
};

export function makeComponentTest(_cb: (driver: IComponentTestDriver) => Promise<void>): () => Promise<void> {
  return async () => {};
}

export function valdiIt(_expectation: string, _assertion: (driver: IComponentTestDriver) => Promise<void>): void {}
export function fvaldiIt(_expectation: string, _assertion: (driver: IComponentTestDriver) => Promise<void>): void {}
export function xvaldiIt(_expectation: string, _assertion: (driver: IComponentTestDriver) => Promise<void>): void {}

export function withValdiRenderer(_assertion: (driver: IComponentTestDriver) => Promise<void>): () => Promise<void> {
  return async () => {};
}

// Exported so the noop driver instance is reachable for callers that want to
// poke at the shape without going through makeComponentTest.
export const _noopComponentTestDriver: IComponentTestDriver = noopDriver;
