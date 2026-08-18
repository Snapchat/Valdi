import { ComponentPrototype } from 'valdi_core/src/ComponentPrototype';
import type { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import { ValdiWebRenderer } from 'web_renderer/src/ValdiWebRenderer';
import type { IValdiHTMLRendererListener } from './IValdiHTMLRendererListener';
import { RenderMutationCoordinator } from './RenderMutationCoordinator';
import { type ServerElement, type ServerShadowRoot } from './dom/ServerDOM';
import { createServerDOMHost, releaseServerDOMHost } from './dom/ServerDOMEnvironment';
import { serializeServerHTMLRoot } from './serialization/ServerHTMLSerializer';

let rendererIdSequence = 0;

export type { IValdiHTMLRendererListener } from './IValdiHTMLRendererListener';

export class ValdiHTMLRenderer<
  ViewModel = any,
  Context = any,
  T extends IComponent<ViewModel, Context> = IComponent<ViewModel, Context>,
> {
  private readonly componentPrototype = ComponentPrototype.instanceWithNewId();
  private readonly host: ServerElement;
  private readonly markerName: string;
  private readonly mutationCoordinator: RenderMutationCoordinator;
  private readonly renderer: ValdiWebRenderer;
  private destroyed = false;
  private hasEmittedInitialHtml = false;
  private html = '';

  constructor(
    private readonly ctor: ComponentConstructor<T, ViewModel, Context>,
    private readonly componentContext: Context,
    private readonly listener: IValdiHTMLRendererListener,
  ) {
    const rendererId = (++rendererIdSequence).toString();
    this.markerName = `valdi-html-${rendererId}`;
    this.mutationCoordinator = new RenderMutationCoordinator();
    this.host = createServerDOMHost(this.mutationCoordinator);
    this.renderer = new ValdiWebRenderer(this.host as unknown as HTMLElement, this.markerName);
    this.renderer.setEventListener(this.mutationCoordinator);
    this.mutationCoordinator.setCommitCallback(() => this.commitHtmlUpdate(true));
    this.mutationCoordinator.start();
  }

  async render(viewModel: ViewModel): Promise<void> {
    if (this.destroyed) {
      throw new Error('Cannot render with a destroyed ValdiHTMLRenderer');
    }
    this.renderer.renderRootComponent(this.ctor, this.componentPrototype, viewModel, this.componentContext);
    if (!this.hasEmittedInitialHtml) {
      this.commitHtmlUpdate(false);
    }
  }

  currentHtml(): string {
    return this.html;
  }

  destroy(): void {
    if (this.destroyed) {
      return;
    }
    this.destroyed = true;
    this.mutationCoordinator.stop();
    this.renderer.onDestroy();
    releaseServerDOMHost();
  }

  private commitHtmlUpdate(notify: boolean): void {
    const content = serializeServerHTMLRoot(this.getShadowRoot(), this.markerName);
    if (!this.hasEmittedInitialHtml) {
      this.hasEmittedInitialHtml = true;
      this.html = `<?start name="${this.markerName}">${content}<?end>`;
    } else {
      this.html = `<template for="${this.markerName}"><?start name="${this.markerName}">${content}<?end></template>`;
    }
    if (notify) {
      this.listener.hasUpdatedHtml();
    }
  }

  private getShadowRoot(): ServerShadowRoot {
    const shadowRoot = this.host.shadowRoot;
    if (!shadowRoot) {
      throw new Error('ValdiWebRenderer did not create its isolated root');
    }
    return shadowRoot;
  }
}
