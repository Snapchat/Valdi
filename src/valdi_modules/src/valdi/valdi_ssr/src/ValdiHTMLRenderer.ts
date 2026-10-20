import { ComponentPrototype } from 'valdi_core/src/ComponentPrototype';
import type { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import { Renderer } from 'valdi_core/src/Renderer';
import type { ValdiWebRendererDelegate } from 'web_renderer/src/ValdiWebRendererDelegate';
import type { IValdiHTMLRendererListener } from './IValdiHTMLRendererListener';
import { RenderMutationCoordinator } from './RenderMutationCoordinator';
import { type ServerElement, type ServerShadowRoot } from './dom/ServerDOM';
import { createServerDOMHost, releaseServerDOMHost } from './dom/ServerDOMEnvironment';
import { serializeServerHTMLRoot } from './serialization/ServerHTMLSerializer';

declare const require: (id: string) => any;

let rendererIdSequence = 0;

// Must match ValdiWebRenderer's root element allowlist.
const ROOT_ELEMENT_TYPES = [
  'view',
  'label',
  'layout',
  'scroll',
  'image',
  'textfield',
  'textview',
  'spinner',
  'custom-view',
  'video',
  'shape',
];

// The web renderer's views probe `document` when their modules load, so they can only be
// required once the server DOM is installed. ValdiWebRenderer itself is avoided because
// importing it bootstraps the browser runtime over the native one.
function makeWebRendererDelegate(root: ServerShadowRoot): ValdiWebRendererDelegate {
  const { ValdiWebRendererDelegate: DelegateCtor } = require('web_renderer/src/ValdiWebRendererDelegate');
  const delegate: ValdiWebRendererDelegate = new DelegateCtor(root as unknown as ShadowRoot);
  const onElementCreated = delegate.onElementCreated.bind(delegate);
  delegate.onElementCreated = (id, viewClass) => onElementCreated(id, webViewClass(viewClass));
  return delegate;
}

// Mirrors jsx.registerNativeElement: on a native runtime, elements resolve to iOS or Android
// view classes, but the web renderer only knows the element names.
const NATIVE_VIEW_CLASSES: ReadonlyArray<[string, string, string]> = [
  ['layout', 'Layout', 'Layout'],
  ['view', 'SCValdiView', 'com.snap.valdi.views.ValdiView'],
  ['label', 'SCValdiLabel', 'com.snap.valdi.views.ValdiTextView'],
  ['scroll', 'SCValdiScrollView', 'com.snap.valdi.views.ValdiScrollView'],
  ['image', 'SCValdiImageView', 'com.snap.valdi.views.ValdiImageView'],
  ['video', 'SCValdiVideoView', 'com.snap.valdi.views.ValdiVideoView'],
  ['spinner', 'SCValdiSpinnerView', 'com.snap.valdi.views.ValdiSpinnerView'],
  ['textfield', 'SCValdiTextField', 'com.snap.valdi.views.ValdiEditText'],
  ['textview', 'SCValdiTextView', 'com.snap.valdi.views.ValdiEditTextMultiline'],
  ['shape', 'SCValdiShapeView', 'com.snap.valdi.views.ShapeView'],
];

const WEB_VIEW_CLASS_BY_NATIVE_CLASS = new Map<string, string>();
for (const [elementName, iosClass, androidClass] of NATIVE_VIEW_CLASSES) {
  WEB_VIEW_CLASS_BY_NATIVE_CLASS.set(iosClass, elementName);
  // Several elements share an Android class; the first registration (the plain one) wins.
  if (!WEB_VIEW_CLASS_BY_NATIVE_CLASS.has(androidClass)) {
    WEB_VIEW_CLASS_BY_NATIVE_CLASS.set(androidClass, elementName);
  }
}

function webViewClass(viewClass: string): string {
  return WEB_VIEW_CLASS_BY_NATIVE_CLASS.get(viewClass) ?? viewClass;
}

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
  private readonly renderer: Renderer;
  private readonly delegate: ValdiWebRendererDelegate;
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
    const shadowRoot = this.host.attachShadow({ mode: 'open' });
    this.delegate = makeWebRendererDelegate(shadowRoot);
    const renderer = new Renderer(this.markerName, ROOT_ELEMENT_TYPES, this.delegate);
    this.delegate.setAttributeDelegate({
      updateAttribute: (elementId, attributeName, attributeValue) =>
        renderer.attributeUpdatedExternally(elementId, attributeName, attributeValue),
    });
    this.renderer = renderer;
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
    this.delegate.onDestroyed();
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
      throw new Error('ValdiHTMLRenderer host lost its shadow root');
    }
    return shadowRoot;
  }
}
