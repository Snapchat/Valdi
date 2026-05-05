import { StringCache } from 'valdi_core/src/StringCache';
import { StringMap } from 'coreutils/src/StringMap';
import { IRenderedComponentHolder } from 'valdi_tsx/src/IRenderedComponentHolder';
import { IRenderedElementHolder } from 'valdi_tsx/src/IRenderedElementHolder';
import { AnyRenderFunction } from './AnyRenderFunction';
import { isDevBuild } from './BuildType';
import { resolveComponentConstructor } from './ComponentPath';
import { ComponentPrototype } from './ComponentPrototype';
import { ValdiRuntime } from './ValdiRuntime';
import { ComponentConstructor, IComponent } from './IComponent';
import { RequireFunc } from './IModuleLoader';
import { IRenderedElement } from './IRenderedElement';
import { IRenderedVirtualNode } from './IRenderedVirtualNode';
import { IRenderer } from './IRenderer';
import { JSXRendererDelegate } from './JSXRendererDelegate';
import { DeferredNodePrototype, NodePrototype, ViewClass } from './NodePrototype';
import * as RendererModule from './Renderer';
import { Renderer } from './Renderer';
import { RendererFactory } from './RendererFactory';
import { Style } from './Style';
import { CustomMessageHandler } from './debugging/CustomMessageHandler';
import {
  DaemonClientManager,
  IDaemonClientManagerListener,
  ReceivedDaemonClientMessage,
} from './debugging/DaemonClientManager';
import { DaemonClientMessageType, DumpHeapRequest, Messages } from './debugging/Messages';
import { toError } from './utils/ErrorUtils';
import { PropertyList, removeProperty } from './utils/PropertyList';

/**
 * TypeScript to Native interactions
 */

declare const runtime: ValdiRuntime;

class DummyComponent implements IComponent {
  viewModel: any;

  get renderer(): IRenderer {
    return undefined!;
  }

  onCreate(): void {}
  onRender(): void {}
  onDestroy(): void {}
  onViewModelUpdate(oldViewModel: any): void {}
}

const getRenderer = RendererModule.getRenderer;

const ALLOWED_ROOT_ELEMENT_TYPES: string[] | undefined = isDevBuild() ? ['view', 'layout'] : undefined;

class JSXModule implements IDaemonClientManagerListener, RendererFactory {
  private currentPlatform: number;
  private resolvedViewClasses: StringMap<ViewClass> = {};
  private messageHandlers: CustomMessageHandler[] = [];
  private stringCache: StringCache;
  private attributeCache: StringCache;
  private injectedAttributeCache: StringCache;
  readonly daemonClientManager: DaemonClientManager;

  constructor() {
    this.currentPlatform = runtime.getCurrentPlatform();
    if (this.currentPlatform !== 1 && this.currentPlatform !== 2 && this.currentPlatform !== 3 && this.currentPlatform !== 4) {
      throw Error(`Unrecognized platform type ${this.currentPlatform.toString()}`);
    }

    this.stringCache = new StringCache(runtime.internString);
    this.attributeCache = new StringCache(runtime.getAttributeId);
    this.injectedAttributeCache = new StringCache((str: string) => this.attributeCache.get(str.substr(1)));
    this.daemonClientManager = new DaemonClientManager();

    this.daemonClientManager.addListener(this);
  }

  // JSX render callbacks

  beginRenderCustomView(
    previousPrototype: NodePrototype | undefined,
    iosClassName: string,
    androidClassName: string,
    key?: string,
  ): NodePrototype {
    let prototype = previousPrototype;
    if (!prototype) {
      prototype = this.makeNodePrototype('custom-view', ['iosClass', iosClassName, 'androidClass', androidClassName]);
    }
    this.beginRender(prototype, key);
    return prototype;
  }

  getCurrentComponentInstance(): IComponent | undefined {
    return getRenderer().getCurrentComponentInstance();
  }

  beginRender(prototype: NodePrototype, key?: string): void {
    getRenderer().beginElement(prototype, key);
  }

  beginRenderIfNeeded(prototype: NodePrototype, key?: string): boolean {
    return getRenderer().beginElementIfNeeded(prototype, key);
  }

  endRender() {
    getRenderer().endElement();
  }

  setAttributeBool(name: string, value: boolean | undefined) {
    getRenderer().setAttributeBool(name, value);
  }

  setAttributeNumber(name: string, value: number | undefined) {
    getRenderer().setAttributeNumber(name, value);
  }

  setAttributeStyle<T>(name: string, value: Style<T> | undefined) {
    getRenderer().setAttributeStyle(name, value);
  }

  setAttributeString(name: string, value: string | undefined) {
    getRenderer().setAttributeString(name, value);
  }

  setAttributeFunction(name: string, value: () => void | undefined) {
    getRenderer().setAttributeFunction(name, value);
  }

  setAttributeRef(ref: IRenderedElementHolder<any> | undefined) {
    getRenderer().setAttributeRef(ref);
  }

  setAttribute(name: string, value: any): void {
    getRenderer().setAttribute(name, value);
  }

  hasAttribute(name: string): boolean {
    return getRenderer().hasAttribute(name);
  }

  setAttributes(attributes: PropertyList) {
    getRenderer().setAttributes(attributes);
  }

  beginComponent<T extends IComponent>(
    ctr: ComponentConstructor<T>,
    prototype: ComponentPrototype,
    key?: string,
    componentRef?: IRenderedComponentHolder<T, IRenderedVirtualNode>,
  ) {
    getRenderer().beginComponent(ctr, prototype, key, componentRef);
  }

  /**
   * @deprecated needed for VUE support only
   */
  beginComponentFromPath<T extends IComponent>(
    require: RequireFunc,
    componentPath: string | undefined,
    key?: string,
    componentRef?: IRenderedComponentHolder<T, IRenderedVirtualNode>,
  ) {
    if (componentPath) {
      const ctr = resolveComponentConstructor<T>(require, componentPath);
      getRenderer().beginComponentWithoutPrototype(ctr, key, componentRef);
    } else {
      getRenderer().beginComponentWithoutPrototype(DummyComponent, key, undefined);
    }
  }

  /**
   * @deprecated needed for VUE support only
   */
  beginComponentWithoutPrototype<T extends IComponent>(
    ctr: ComponentConstructor<T>,
    key?: string,
    componentRef?: IRenderedComponentHolder<T, IRenderedVirtualNode>,
  ) {
    getRenderer().beginComponentWithoutPrototype(ctr, key, componentRef);
  }

  hasContext(): boolean {
    return getRenderer().hasComponentContext();
  }

  setContext(context: any): void {
    getRenderer().setComponentContext(context);
  }

  setComponentOnCreateObserver(observer?: (instance: any) => void) {
    getRenderer().setComponentOnCreateObserver(observer);
  }

  setComponentOnDestroyObserver(observer?: (instance: any) => void) {
    getRenderer().setComponentOnDestroyObserver(observer);
  }

  endComponent() {
    getRenderer().endComponent();
  }

  renderFnComponent<ViewModel>(renderFn: (viewModel: ViewModel) => void, viewModel: ViewModel) {
    renderFn(viewModel);
  }

  hasViewModelProperty(name: string): boolean {
    return getRenderer().hasViewModelProperty(name);
  }

  setViewModelProperty(name: string, value: any) {
    getRenderer().setViewModelProperty(name, value);
  }

  setViewModelProperties(properties?: StringMap<any>) {
    getRenderer().setViewModelProperties(properties);
  }

  setViewModelFull(viewModel: any) {
    getRenderer().setViewModelFull(viewModel);
  }

  hasInjectedAttribute(name: string): boolean {
    return getRenderer().hasInjectedAttribute(name);
  }

  setInjectedAttribute(name: string, value: any) {
    getRenderer().setInjectedAttribute(name, value);
  }

  /**
   * Render the given slot name of the component.
   * An ElementRef can be passed if the component wants to capture
   * which elements were rendered inside.
   * @param name
   * @param component
   * @param ref
   */
  renderNamedSlot<T = any>(name: string, component: IComponent, ref?: IRenderedElementHolder<T>, slotKey?: string) {
    getRenderer().renderNamedSlot(name, component, ref, slotKey);
  }

  setNamedSlot(name: string, renderFunc: AnyRenderFunction | undefined) {
    getRenderer().setNamedSlot(name, renderFunc);
  }

  renderUnnamedSlot<T = any>(component: IComponent, ref?: IRenderedElementHolder<T>, slotKey?: string) {
    getRenderer().renderUnnamedSlot(component, ref, slotKey);
  }

  setUnnamedSlot(renderFunc: AnyRenderFunction | undefined) {
    getRenderer().setUnnamedSlot(renderFunc);
  }

  setNamedSlots(namedSlots: StringMap<AnyRenderFunction | undefined>) {
    getRenderer().setNamedSlots(namedSlots);
  }

  willEvaluate(desc: string, expression: string, line: number, column: number) {}

  // Factory methods

  makeNodePrototype(className: string, attributes?: PropertyList): NodePrototype {
    if (className === 'custom-view') {
      let androidClass: string | undefined;
      let iosClass: string | undefined;
      let macosClass: string | undefined;
      if (attributes) {
        androidClass = removeProperty(attributes, 'androidClass');
        iosClass = removeProperty(attributes, 'iosClass');
        macosClass = removeProperty(attributes, 'macosClass');
        // webClass is left in attributes so WebValdiCustomView receives it via changeAttribute
      }

      // 1 = Android, 2 = iOS, 3 = MacOS, 4 = Web
      if (this.currentPlatform === 4) {
        return new NodePrototype(className, 'custom-view', attributes);
      }
      if (this.currentPlatform === 2) {
        if (iosClass) {
          return new NodePrototype(className, iosClass, attributes);
        } else {
          return new DeferredNodePrototype(className, attributes);
        }
      }
      if (this.currentPlatform === 3) {
        if (macosClass) {
          return new NodePrototype(className, macosClass, attributes);
        } else if (iosClass) {
          // macOS falls through to iOS class names
          return new NodePrototype(className, iosClass, attributes);
        } else {
          return new DeferredNodePrototype(className, attributes);
        }
      }
      if (androidClass) {
        return new NodePrototype(className, androidClass, attributes);
      } else {
        return new DeferredNodePrototype(className, attributes);
      }
    }

    const viewClass = this.resolvedViewClasses[className];
    if (!viewClass) {
      throw Error(`No view class mapping registered for class ${className}`);
    }

    // TODO(simon): NodePrototype native registration to avoid marshalling
    // const native = runtime.createNodePrototype(viewClass, attributes);

    return new NodePrototype(className, viewClass, attributes);
  }

  makeComponentPrototype(attributes?: PropertyList) {
    return ComponentPrototype.instanceWithNewId(attributes);
  }

  makeRenderer(treeId: string): Renderer {
    return this.makeRendererWithAllowedRootElementTypes(ALLOWED_ROOT_ELEMENT_TYPES, treeId);
  }

  makeRendererWithAllowedRootElementTypes(allowedRootElementTypes: string[] | undefined, treeId: string): Renderer {
    const useTopDownMoveOrder = typeof runtime.useTopDownMoveOrder === 'boolean' ? runtime.useTopDownMoveOrder : false;
    return new Renderer(
      treeId,
      allowedRootElementTypes,
      new JSXRendererDelegate(treeId, this.stringCache, this.attributeCache, this.injectedAttributeCache),
      useTopDownMoveOrder,
    );
  }

  // Native elements

  registerNativeElement(className: string, iosClass: string, androidClass: string): void {
    let viewClassName: string;
    if (this.currentPlatform === 2 || this.currentPlatform === 3) {
      // macOS (3) falls through to iOS class names
      viewClassName = iosClass;
    } else if (this.currentPlatform === 1) {
      viewClassName = androidClass;
    } else {
      viewClassName = className;
    }

    // TODO(simon): ViewClass native registration to avoid marshalling

    // const viewClass = runtime.getViewClass(viewClassName);
    this.resolvedViewClasses[className] = viewClassName;
  }

  // Component registration

  /**
   * Evaluates the given function, and returns all the elements
   * which were rendered during that call.
   */
  render(renderFunc: () => (() => void) | undefined): IRenderedElement[] {
    if (!renderFunc) {
      return [];
    }

    return getRenderer().render(renderFunc);
  }

  onDaemonClientEvent(eventType: number, daemonClient: any, payload?: any): void {
    this.daemonClientManager.onEvent(eventType, daemonClient, payload);
  }

  addCustomMessageHandler(messageHandler: CustomMessageHandler) {
    this.messageHandlers.push(messageHandler);
  }

  removeCustomMessageHandler(messageHandler: CustomMessageHandler) {
    const idx = this.messageHandlers.indexOf(messageHandler);
    if (idx >= 0) {
      this.messageHandlers.splice(idx, 1);
    }
  }

  // IDaemonClientManagerListener

  onMessage(message: ReceivedDaemonClientMessage) {
    if (message.message.type === DaemonClientMessageType.CUSTOM_REQUEST) {
      const body = message.message.body;
      for (const messageHandler of this.messageHandlers) {
        const promise = messageHandler.messageReceived(body.identifier, body.data);
        if (promise) {
          // Found a handler
          promise
            .then(data => {
              message.respond(requestId => Messages.customMessageResponse(requestId, { handled: true, data }));
            })
            .catch(err => {
              message.respond(requestId => Messages.errorResponse(requestId, err));
            });
          return;
        }
      }

      message.respond(requestId => Messages.customMessageResponse(requestId, { handled: false, data: undefined }));
    } else if (message.message.type === DaemonClientMessageType.DUMP_HEAP_REQUEST) {
      const dumpHeapRequest = (message.message as DumpHeapRequest).body;
      if (!runtime.dumpHeap) {
        message.respond(requestId => Messages.errorResponse(requestId, `Runtime does not support dumping heap`));
        return;
      }

      const doDumpHeap = () => {
        const memoryStatistics = runtime.dumpMemoryStatistics();

        let response: (requestId: string) => string;
        try {
          const dumpedHeap = runtime.dumpHeap!();
          const heapDumpJSON = runtime.bytesToString(dumpedHeap);
          response = requestId =>
            Messages.dumpHeapResponse(requestId, {
              memoryUsageBytes: memoryStatistics.memoryUsageBytes,
              heapDumpJSON,
            });
        } catch (err: unknown) {
          response = requestId => Messages.errorResponse(requestId, toError(err));
        }
        message.respond(response);
      };

      if (dumpHeapRequest.performGC) {
        runtime.performGC();
        setTimeout(doDumpHeap);
      } else {
        doDumpHeap();
      }
    }
  }
}

export const jsx = new JSXModule();

jsx.registerNativeElement('layout', 'Layout', 'Layout');
jsx.registerNativeElement('view', 'SCValdiView', 'com.snap.valdi.views.ValdiView');
jsx.registerNativeElement('label', 'SCValdiLabel', 'com.snap.valdi.views.ValdiTextView');
jsx.registerNativeElement('scroll', 'SCValdiScrollView', 'com.snap.valdi.views.ValdiScrollView');
jsx.registerNativeElement('image', 'SCValdiImageView', 'com.snap.valdi.views.ValdiImageView');
jsx.registerNativeElement('video', 'SCValdiVideoView', 'com.snap.valdi.views.ValdiVideoView');
jsx.registerNativeElement('button', 'UIButton', 'android.widget.Button');
jsx.registerNativeElement('spinner', 'SCValdiSpinnerView', 'com.snap.valdi.views.ValdiSpinnerView');
jsx.registerNativeElement('blur', 'SCValdiBlurView', 'com.snap.valdi.views.ValdiView');
jsx.registerNativeElement('textfield', 'SCValdiTextField', 'com.snap.valdi.views.ValdiEditText');
jsx.registerNativeElement('textview', 'SCValdiTextView', 'com.snap.valdi.views.ValdiEditTextMultiline');
jsx.registerNativeElement('shape', 'SCValdiShapeView', 'com.snap.valdi.views.ShapeView');
jsx.registerNativeElement(
  'animatedimage',
  'SCValdiAnimatedContentView',
  'com.snap.valdi.views.AnimatedImageView',
);
