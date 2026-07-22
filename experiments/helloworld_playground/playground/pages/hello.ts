import { ComponentPrototype } from 'hello_experiment_npm/src/valdi_core/src/ComponentPrototype';
import { App } from 'hello_experiment_npm/src/hello_world/src/HelloWorldApp';
import { ValdiWebRenderer } from '../setup';

export function render(appContainer: HTMLElement) {
  const shadowRoot = appContainer.attachShadow({ mode: 'open' });
  const renderer = new ValdiWebRenderer(shadowRoot);
  renderer.renderRootComponent(
    App as any,
    ComponentPrototype.instanceWithNewId(),
    {},
    {},
  );
}
