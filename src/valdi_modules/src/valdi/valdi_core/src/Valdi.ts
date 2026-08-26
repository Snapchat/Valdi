import { ElementFrame } from 'valdi_tsx/src/Geometry';
import { IRenderedElementHolder } from 'valdi_tsx/src/IRenderedElementHolder';
import { Layout } from 'valdi_tsx/src/NativeTemplateElements';
import { NativeView } from 'valdi_tsx/src/NativeView';
import { AnimationOptions } from './AnimationOptions';
import { ComponentKey } from './ComponentKey';
import { parseComponentPath } from './ComponentPath';
import { ValdiRuntime, NativeViewNodeInfo } from './ValdiRuntime';
import { ElementRef } from './ElementRef';
import { ComponentConstructor, IComponent } from './IComponent';
import { RequireFunc } from './IModuleLoader';
import { IRenderedElement } from './IRenderedElement';
import { IRenderer } from './IRenderer';
import { IRootComponentsManager } from './IRootComponentsManager';
import { jsx } from './JSXBootstrap';
import { RootComponentsManager } from './RootComponentsManager';
import { Style } from './Style';
import { DaemonClientManager } from './debugging/DaemonClientManager';
import { DebuggerInputMessageHandler } from './debugging/DebuggerInputMessageHandler';
import { mergePartial } from './utils/PartialUtils';

export interface Attributes {
  [attribute: string]: any;
}

export interface Element {
  /**
   * If the Element has not a 'for-each' attribute, this property
   * will be set and will contain the attributes rendered from JS.
   */
  attributes: Attributes;
}

export interface Elements {
  [id: string]: Element;
}

export interface Actions {
  [actionName: string]: (...args: any[]) => void;
}

// Returns a formatted string of the form: "the $0 jumped over the $1"
export function formatParameterizedString(format: string, ...formatArgs: any[]): string {
  return format.replace(/\$(\d+)/g, (_, is) => {
    const index = parseInt(is, 10);
    if (index >= formatArgs.length) {
      throw 'not enough args provided for string: ' + format;
    }
    return formatArgs[index];
  });
}

declare const runtime: ValdiRuntime;

export interface ActionCaller {
  callAction(actionName: string, parameters: any[]): void;
}

/**
 * Checks whether a given Valdi module is loaded.
 * This will return true if the module is available for
 * use without having to make an asynchronous network request.
 * @param module the Valdi module to query
 */
export function isModuleLoaded(module: string): boolean {
  return runtime.isModuleLoaded(module);
}

/**
 * Asynchronously loads the given Valdi module, calls the completion
 * handler when it's done. The completion handler might be called immediately
 * if the module is already loaded.
 * @param module the Valdi module to query
 * @param completion the callback which will be called when loading has completed.
 */
export function loadModule(module: string, completion: (error?: string) => void): void {
  runtime.loadModule(module, completion);
}

/**
 * Retrieves the content as a Uint8Array of a file entry at the given path
 * inside the given Valdi module.
 */
export function getModuleFileEntryAsBytes(module: string, path: string): Uint8Array {
  return runtime.getModuleEntry(module, path, false) as Uint8Array;
}

/**
 * Retrieves the content as a string of a file entry at the given path
 * inside the given Valdi module.
 */
export function getModuleFileEntryAsString(module: string, path: string): string {
  return runtime.getModuleEntry(module, path, true) as string;
}

/**
 * Retrieves all the JS files that are available within the given Valdi module name
 */
export function getModuleJsPaths(module: string): string[] {
  return runtime.getModuleJsPaths(module);
}

interface NodeIdPathEntry {
  id?: string;
  forEachKey?: string;
}

export function parseNodeIdPath(nodeIdPath: string): NodeIdPathEntry[] {
  const paths = new Array<NodeIdPathEntry>();
  const elements = nodeIdPath.split('.');
  let trailingId: string | undefined = undefined;
  for (const element of elements) {
    const bracketStartIdx = element.indexOf('[');
    if (bracketStartIdx > 0) {
      const bracketEndIdx = element.indexOf(']');
      if (bracketEndIdx === -1 || bracketStartIdx > bracketEndIdx) {
        throw new Error("Invalid nodeIdPath: '" + nodeIdPath + "'");
      }
      const id = element.substr(0, bracketStartIdx);
      paths.push({
        id,
      });
      const forEachKey = element.substr(bracketStartIdx + 1, bracketEndIdx - bracketStartIdx - 1);
      paths.push({
        forEachKey,
      });
      trailingId = id;
    } else {
      paths.push({
        id: element,
      });
      trailingId = undefined;
    }
  }

  // Automatically resolves labels[1] to labels[1].labels
  if (trailingId) {
    paths.push({
      id: trailingId,
    });
  }

  return paths;
}

interface BundleAndValdiPath {
  // DEPRECATED: Use componentPath instead
  valdiPath?: string;
  componentPath?: string;
  bundleName?: string;
}

interface CreateComponentRequest extends BundleAndValdiPath {
  viewModel?: any;
  context?: any;
}

export type UpdateElementsFunction = (elements: Elements) => void;

/**
 * Unique object shared across all components for a given component tree.
 * It can be provided by native code, and altered by any component in the tree to
 * inject services.
 */
export interface ComponentContext {}

export type ElementsSpecs = string[];

export interface ComponentClass {
  renderTemplate: () => void;
  componentPath: string;
  elements: ElementsSpecs;
}

declare const require: RequireFunc;

function makeActions(actionCaller: ActionCaller): Actions {
  return new Proxy({} as Actions, {
    get(target, key): any {
      let value = target[key];

      if (!value) {
        value = (...args: any[]) => {
          actionCaller.callAction(key, args);
        };
        target[key] = value;
      }
      return value;
    },
  }) as Actions;
}

export type ElementRefs<T> = { [customId: string]: ElementRef<T> };

export interface InjectedAttributes {
  $ref?: IRenderedElementHolder<Layout>;
  $onTap?: () => void;
  $accessibilityId?: string;
  $marginBottom?: number;
  $style?: Style<Layout>;
}

export class LegacyVueComponent<
  ViewModelType extends object = any,
  StateType extends object = object,
  ContextType extends ComponentContext = ComponentContext,
> implements ActionCaller, IComponent
{
  viewModel: ViewModelType & ComponentKey & InjectedAttributes;

  state?: StateType;
  context: ContextType;

  get componentId(): string {
    return this.renderer.contextId;
  }

  get componentKey(): string {
    return this.renderer.getComponentKey(this);
  }

  get parent(): IComponent | undefined {
    return this.renderer.getComponentParent(this);
  }

  get children(): IComponent[] {
    return this.renderer.getComponentChildren(this);
  }

  get rootElements(): IRenderedElement[] {
    return this.renderer.getComponentRootElements(this, false);
  }

  get elementIdWithinParent(): string | undefined {
    for (const rootElement of this.rootElements) {
      const value = rootElement.getAttribute('$id');
      if (value) {
        return value;
      }
    }
    return undefined;
  }

  readonly renderer: IRenderer;

  get componentClass(): ComponentClass {
    return this.constructor as any as ComponentClass;
  }

  private _actions?: Actions;
  get actions(): Actions {
    if (!this._actions) {
      this._actions = makeActions(this);
    }
    return this._actions;
  }

  private _elements?: Elements;
  get elements(): Elements {
    if (!this._elements) {
      const elements: Elements = {};
      for (const elementId of this.componentClass.elements) {
        const elementRef = this.elementRefs[elementId]!;

        const attributesProxy = new Proxy(elementRef, {
          get(target, key) {
            return target.getAppliedAttribute(key);
          },
          set(target, attribute, attributeValue): boolean {
            target.setAttribute(attribute, attributeValue);
            return true;
          },
        });
        elements[elementId] = { attributes: attributesProxy };
      }
      this._elements = elements;
    }
    return this._elements;
  }

  private _elementRefs?: ElementRefs<any>;
  get elementRefs(): ElementRefs<any> {
    if (!this._elementRefs) {
      this._elementRefs = {};
    }
    return this._elementRefs;
  }

  private animationOptions?: AnimationOptions;

  constructor(renderer: IRenderer, viewModel: any, context: any) {
    this.renderer = renderer;
    this.viewModel = viewModel;

    this.context = context;

    const componentClass = this.componentClass;
    if (!componentClass.renderTemplate) {
      // Automatically import the generated JS
      require(parseComponentPath(componentClass.componentPath).filePath, true, true);
    }

    if (componentClass.elements.length) {
      const elementRefs = this.elementRefs;
      for (const elementId of componentClass.elements) {
        elementRefs[elementId] = new ElementRef();
      }
    }
  }

  /**
   * Render the template (resolve all dynamic attributes in the template)
   * and send all changes to the runtime.
   */
  render(): void {
    this.renderer.renderComponent(this, undefined);
  }

  onRender() {
    const animationOptions = this.animationOptions;
    if (animationOptions) {
      this.animationOptions = undefined;
      this.renderer.animate(animationOptions, this.onRender.bind(this));
    } else {
      this.componentClass.renderTemplate.apply(this);
    }
  }

  /**
   * Animate the view tree using the given animation options.
   * The animations closure will be called
   */
  animate(options: AnimationOptions, animations: () => void): Promise<void> {
    return new Promise(resolve => {
      this.renderer.animate(
        {
          ...options,
          completion: () => {
            options.completion?.(false);
            resolve();
          },
        },
        () => {
          animations();
          this.renderer.renderComponent(this, undefined);
        },
      );
    });
  }

  private stringIdsToElementIds(elementIds: string[]): number[] {
    const out: number[] = [];

    for (const elementId of elementIds) {
      const ref = this.elementRefs[elementId];
      if (ref) {
        for (const realElementId of ref.all()) {
          out.push(realElementId.id);
        }
      }
    }
    return out;
  }

  private callRuntimeElementIdMethodFromStringIds<T>(
    stringIds: Array<string>,
    runtimeMethod: (contextId: string, elementId: number, callback: (result: T | undefined) => void) => void,
    callback: (result: Array<T>) => void,
  ): void {
    this.renderer.onRenderComplete(() => {
      this.callRuntimeElementIdMethod(this.stringIdsToElementIds(stringIds), runtimeMethod, callback);
    });
  }

  private callRuntimeElementIdMethod<T>(
    elementIds: Array<number>,
    runtimeMethod: (contextId: string, elementId: number, callback: (result: T | undefined) => void) => void,
    callback: (result: Array<T>) => void,
  ): void {
    let callbackCount = 0;
    const output: (T | undefined)[] = [];

    for (let i = 0; i < elementIds.length; i++) {
      const elementId = elementIds[i];
      output.push(undefined);

      runtimeMethod(this.componentId, elementId, result => {
        output[i] = result;
        callbackCount++;
        if (callbackCount === elementIds.length) {
          callback(output as Array<T>);
        }
      });
    }
  }

  /**
   * Returns the view frames for the given elements.
   * It is more efficient and often more practical to use the `on-layout` attribute,
   * which will call the given function whenever the frame of the element changes.
   * @param elementIds
   * @param callback
   */
  getElementFrames(elementIds: Array<string>, callback: (frames: Array<ElementFrame>) => void): void {
    this.callRuntimeElementIdMethodFromStringIds(elementIds, runtime.getFrameForElementId, callback);
  }

  getElementFramesFromRawElementIds(elementIds: Array<number>, callback: (frames: Array<ElementFrame>) => void): void {
    this.callRuntimeElementIdMethod(elementIds, runtime.getFrameForElementId, callback);
  }

  /**
   * Returns the native view objects for the given elements. These views are only useful when passed to native.
   * @param elementIds
   * @param callback
   */
  getElementNativeViews(elementIds: Array<string>, callback: (views: Array<NativeView>) => void): void {
    this.callRuntimeElementIdMethodFromStringIds(elementIds, runtime.getNativeViewForElementId, callback);
  }

  /**
   * Returns the recursive layout debug info of the given elementIds in HTML format.
   * This will give all layout attributes applied to the given elementIds and their children,
   * and the calculated position and dimensions for them.
   * Giving the root element id will return the whole tree.
   * @param elementIds
   * @param callback
   */
  getElementLayoutDebugInfos(elementIds: Array<string>, callback: (layoutInfos: Array<string>) => void): void {
    this.callRuntimeElementIdMethodFromStringIds(elementIds, runtime.getLayoutDebugInfo, callback);
  }

  /**
   * Returns the recursive native debug infos of the given elementIds.
   * This will give all applied attributes, which includes CSS attributes, static attributes,
   * dynamic attributes and attributes applied manually from JS.
   * @param elementIds
   * @param callback
   */
  getElementNativeDebugInfos(
    elementIds: Array<string>,
    callback: (nativeDebugInfos: Array<NativeViewNodeInfo>) => void,
  ): void {
    this.callRuntimeElementIdMethodFromStringIds(elementIds, runtime.getViewNodeDebugInfo, callback);
  }

  /**
   * Called whenever the view model has changed on this component
   * @param previousViewModel
   */
  onViewModelUpdate(previousViewModel?: ViewModelType): void {}

  /**
   * Called after a state has changed on this component.
   * @param previousState
   */
  onStateUpdate(previousState?: StateType) {}

  /**
   * Called when the component is created.
   */
  onCreate() {}

  /**
   * Called wen the component was destroyed
   */
  onDestroy() {}

  /**
   * Call the action with the given name and pass
   * the given parameters.
   */
  callAction(actionName: string, parameters: any[]) {
    runtime.postMessage(this.componentId, 'callAction', {
      action: actionName,
      parameters,
    });
  }

  /**
   * setState applies the given partial state or updater to the internal state and requests
   * a re-render. You may pass in an optional
   * callback that is invoked after the state has been applied and the component updated.
   */
  setState(updater: Partial<StateType>, callback?: () => void) {
    if (this.isDestroyed()) {
      return;
    }

    const newState = mergePartial(updater, this.state);
    if (newState) {
      this.state = newState;
      this.render();
    }

    if (callback) {
      callback();
    }
  }

  /**
   * Registers a function which will be called right after the component is destroyed.
   * @param disposable the callback to call after the component is destroyed
   */
  registerDisposable(disposable: () => void): void {
    this.renderer.registerComponentDisposable(this, disposable);
  }

  /**
   * Set an AnimationOptions to use for the next render.
   * You can use this method as part of an onViewModelUpdate callback
   *
   * @param animationOptions
   */
  setAnimationOptionsForNextRender(animationOptions: AnimationOptions | undefined): void {
    this.animationOptions = animationOptions;
  }

  /**
   * Changes the state by merging the given partial state.
   * Will synchronously re-render the component if the state changes.
   * Any elements mutation that happens within that setState call will be animated.

   * @return A promise which will be resolved when the animation finishes.
   */
  setStateAnimated(state: Partial<StateType>, animationOptions: AnimationOptions): Promise<void> {
    return this.animate(animationOptions, () => {
      this.setState(state);
    });
  }

  /**
   * Returns whether the Component was destroyed.
   */
  isDestroyed(): boolean {
    return !this.renderer.isComponentAlive(this);
  }

  static resolveBundleAndPath(componentPath: string, bundleName?: string): BundleAndValdiPath {
    const bundleSeparatorIdx = componentPath.indexOf(':');
    if (bundleSeparatorIdx != -1) {
      bundleName = componentPath.substring(0, bundleSeparatorIdx);
      componentPath = componentPath.substring(bundleSeparatorIdx + 1);
    }

    return { bundleName: bundleName, valdiPath: componentPath };
  }

  static componentPath: string;

  static setup<T extends IComponent>(
    componentClass: ComponentConstructor<T>,
    elements: ElementsSpecs,
    renderTemplate: () => void,
  ) {
    const resolvedClass = componentClass as any as ComponentClass;
    resolvedClass.renderTemplate = renderTemplate;
    resolvedClass.elements = elements;
  }
}

/**
 * StatefulComponent manages lifecycle and rendering with its own state. By default,
 * this component is re-rendered after every setState call, so consider using
 * PureComponent instead. See #setState method for more info.
 */
// Deprecated for backward-compatibility, use Component
export class LegacyVueStatefulComponent<
  ViewModelType extends object = object,
  StateType extends object = object,
  ContextType extends ComponentContext = ComponentContext,
> extends LegacyVueComponent<ViewModelType, StateType, ContextType> {}

/**
 * PureComponent is a component that implements shouldComponentUpdate with a shallow
 * view model and state comparison.
 */
// Deprecated for backward-compatibility, use Component
export class LegacyVuePureComponent<
  ViewModelType extends object = object,
  StateType extends object = object,
  ContextType extends ComponentContext = ComponentContext,
> extends LegacyVueComponent<ViewModelType, StateType, ContextType> {}

interface NavigationPushInfo {
  animated?: boolean;
  newNavigationTitle?: string;
}

export interface NavigationPush extends CreateComponentRequest, NavigationPushInfo {}

interface NavigationPresentInfo extends NavigationPushInfo {
  wrapInNavigationController: boolean;
}

interface NavigationPresent extends CreateComponentRequest, NavigationPresentInfo {}

interface TypedNavigationRequest<
  ViewModelType extends object,
  ContextType extends ComponentContext,
  T extends LegacyVueComponent<ViewModelType, any, ContextType>,
> {
  componentClass: ComponentConstructor<T>;
  viewModel?: ViewModelType;
  context?: ContextType;
}

interface PushComponentRequest<
  ViewModelType extends object,
  ContextType extends ComponentContext,
  T extends LegacyVueComponent<ViewModelType, any, ContextType>,
> extends TypedNavigationRequest<ViewModelType, ContextType, T>,
    NavigationPushInfo {}

interface PresentComponentRequest<
  ViewModelType extends object,
  ContextType extends ComponentContext,
  T extends LegacyVueComponent<ViewModelType, any, ContextType>,
> extends TypedNavigationRequest<ViewModelType, ContextType, T>,
    NavigationPresentInfo {}

function makeCreateRequest<
  ViewModelType extends object,
  ContextType extends ComponentContext,
  T extends LegacyVueComponent<ViewModelType, any, ContextType>,
>(request: TypedNavigationRequest<ViewModelType, ContextType, T>): CreateComponentRequest {
  const valdiPath = (request.componentClass as any).componentPath;
  const resolvedBundleAndPath = LegacyVueComponent.resolveBundleAndPath(valdiPath);
  return {
    valdiPath: resolvedBundleAndPath.valdiPath,
    bundleName: resolvedBundleAndPath.bundleName,
    viewModel: request.viewModel,
    context: request.context,
  };
}

export class Navigation {
  private actionCaller: ActionCaller;

  constructor(actionCaller: ActionCaller) {
    this.actionCaller = actionCaller;
  }

  push(navigationPush: NavigationPush): void {
    this.actionCaller.callAction('push', [navigationPush]);
  }

  pushTyped<
    ViewModelType extends object,
    ContextType extends ComponentContext,
    T extends LegacyVueComponent<ViewModelType, any, ContextType>,
  >(navigationPush: PushComponentRequest<ViewModelType, ContextType, T>): void {
    const request = makeCreateRequest(navigationPush);

    this.push({
      ...request,
      animated: navigationPush.animated,
      newNavigationTitle: navigationPush.newNavigationTitle,
    });
  }

  present(navigationPresent: NavigationPresent): void {
    this.actionCaller.callAction('present', [navigationPresent]);
  }

  presentTyped<
    ViewModelType extends object,
    ContextType extends ComponentContext,
    T extends LegacyVueComponent<ViewModelType, any, ContextType>,
  >(navigationPresent: PresentComponentRequest<ViewModelType, ContextType, T>): void {
    const request = makeCreateRequest(navigationPresent);
    this.present({
      ...request,
      animated: navigationPresent.animated,
      newNavigationTitle: navigationPresent.newNavigationTitle,
      wrapInNavigationController: navigationPresent.wrapInNavigationController,
    });
  }

  back(animated: boolean = true) {
    this.actionCaller.callAction('back', [
      {
        animated,
      },
    ]);
  }

  popToSelf(animated: boolean = true) {
    this.actionCaller.callAction('popToSelf', [
      {
        animated,
      },
    ]);
  }

  dismiss(animated: boolean = true) {
    this.actionCaller.callAction('dismiss', [
      {
        animated,
      },
    ]);
  }
}

/**
 * A ViewControllerComponent is a subclass of Component that exposes
 * methods that are tied to a UIViewController on iOS and ViewController
 * on Android. For those functions to work, the Component must live within
 * a ValdiViewController in native
 * .
 */
export class ViewControllerComponent<
  ViewModelType extends object = any,
  StateType extends object = object,
  ContextType extends ComponentContext = any,
> extends LegacyVueComponent<ViewModelType, StateType, ContextType> {
  navigation = new Navigation(this);

  setNavigationTitle(navigationTitle: string) {
    this.callAction('setNavigationTitle', [
      {
        title: navigationTitle,
      },
    ]);
  }
}

export function performSyncWithMainThread(func: () => void): void {
  runtime.performSyncWithMainThread(func);
}

interface ProxyHandler<T> {
  get?(target: T, property: string): any;
  set?(target: T, property: string, value: any): boolean;
}

declare class Proxy<T> {
  constructor(target: T, handler: ProxyHandler<T>);
}

let lastRootComponentsManager: RootComponentsManager | undefined;
let debuggerInputMessageHandler: DebuggerInputMessageHandler | undefined;

export function getLastRootComponentsManager(): RootComponentsManager | undefined {
  return lastRootComponentsManager;
}

export function makeRootComponentsManager(): IRootComponentsManager {
  if (debuggerInputMessageHandler) {
    jsx.removeCustomMessageHandler(debuggerInputMessageHandler);
    debuggerInputMessageHandler = undefined;
  }
  lastRootComponentsManager = new RootComponentsManager(
    jsx,
    runtime.isDebugEnabled ? jsx.daemonClientManager : undefined,
    runtime.submitDebugMessage,
  );
  if (runtime.isDebugEnabled) {
    const rootComponentsManager = lastRootComponentsManager;
    debuggerInputMessageHandler = new DebuggerInputMessageHandler(
      contextId => rootComponentsManager.rootComponents[contextId]?.renderer,
    );
    jsx.addCustomMessageHandler(debuggerInputMessageHandler);
  }
  return lastRootComponentsManager;
}

export function onDaemonClientEvent(eventType: number, daemonClient: any, payload?: any): void {
  jsx.onDaemonClientEvent(eventType, daemonClient, payload);
}

export function getDaemonClientManager(): DaemonClientManager {
  return jsx.daemonClientManager;
}
