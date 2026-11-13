import { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import { RenderInstance, RenderOptions, render as renderTerminal } from '../src/TerminalRenderer';

const mounted: { unmount(): void }[] = [];

/**
 * `render()` that remembers the instance, so {@link unmountAll} can release it. A failing
 * expectation then cannot leave a keep-alive timer, a patched console or an active application
 * behind for the next spec.
 */
export function render<
  T extends IComponent<ViewModel, Context>,
  ViewModel extends object = {},
  Context extends object = {},
>(
  componentClass: ComponentConstructor<T>,
  viewModel: ViewModel = {} as ViewModel,
  options: RenderOptions = {},
  componentContext: Context = {} as Context,
): RenderInstance<ViewModel> {
  const instance = renderTerminal(componentClass, viewModel, options, componentContext);
  mounted.push(instance);
  return instance;
}

/** Unmounts every instance rendered with {@link render}. Register it with `afterEach`. */
export function unmountAll(): void {
  for (const instance of mounted.splice(0)) {
    instance.unmount();
  }
}
