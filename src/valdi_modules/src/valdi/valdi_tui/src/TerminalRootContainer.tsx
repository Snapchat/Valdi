import { Component } from 'valdi_core/src/Component';
import { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import { jsx } from 'valdi_core/src/JSXBootstrap';

/**
 * Builds the component `render()` mounts: a column container that hosts the caller's root. The
 * root view of a context is a plain view, so a root component whose own element is a `<Text>` or a
 * `<Static>` would have nowhere to put its content without this container.
 */
export function makeTerminalRootContainer<ViewModel extends object, Context extends object>(
  rootClass: ComponentConstructor<IComponent<ViewModel, Context>>,
): ComponentConstructor<IComponent<ViewModel, Context>> {
  const rootPrototype = jsx.makeComponentPrototype();

  class TerminalRootContainer extends Component<ViewModel, Context> {
    onRender(): void {
      <view flexDirection="column">{this.renderRoot()}</view>;
    }

    private renderRoot(): void {
      // The same post-processed calls the entry point component uses, because the TSX syntax has no way
      // to hand over a whole view model and the context of a component picked at runtime.
      jsx.beginComponent(rootClass, rootPrototype, undefined, undefined);
      jsx.setContext(this.context);
      jsx.setViewModelFull(this.viewModel);
      jsx.endComponent();
    }
  }
  return TerminalRootContainer as unknown as ComponentConstructor<IComponent<ViewModel, Context>>;
}
