import { ComponentPath } from 'valdi_core/src/ComponentPath';
import { ComponentContext, LegacyVueComponent } from 'valdi_core/src/Valdi';
import { getValdiRuntime } from 'valdi_core/src/ValdiRuntimeProvider';
import { makeEntryPointRenderFunction } from 'valdi_core/src/EntryPointComponent';
import { EntryPointRenderFunction } from 'valdi_core/src/EntryPointRenderFunction';
import { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import { IEntryPointComponent } from 'valdi_core/src/IEntryPointComponent';
import { jsx } from 'valdi_core/src/JSXBootstrap';
import { RendererFactory } from 'valdi_core/src/RendererFactory';
import { RootComponentsManager } from 'valdi_core/src/RootComponentsManager';
import { DaemonClientManager } from 'valdi_core/src/debugging/DaemonClientManager';
import { SubmitDebugMessageFunc } from 'valdi_core/src/debugging/DebugMessage';
import { StringMap } from 'coreutils/src/StringMap';

const runtime = getValdiRuntime();

export interface Rectangle {
  x: number;
  y: number;
  width: number;
  height: number;
}

export interface RenderedElementId {
  id: string;
  documentPath: string;
}

export interface RenderedElement {
  ids: RenderedElementId[];
  viewClass: string;
  nodeTag: string;
  frame: Rectangle;
  children: RenderedElement[];
  attributes: { [name: string]: any };
}

export interface SimplifiedRenderedElement {
  viewClass?: string;
  nodeTag?: string;
  frame?: Rectangle;
  children?: SimplifiedRenderedElement[];
  attributes?: { [name: string]: any };
}

export type RenderedElementProperty = 'viewClass' | 'nodeTag' | 'frame' | 'attributes';

export interface ValdiStandalone {
  exit(code: number): void;
  debuggerEnabled: boolean;

  getRenderedElement(
    contextId: string,
    callback: (renderedElement: RenderedElement | undefined, error: string | undefined) => void,
  ): void;
  destroyAllComponents(): void;

  arguments: string[];
}

declare global {
  const valdiStandalone: ValdiStandalone;
}

export function getStandaloneRuntime(): ValdiStandalone {
  return valdiStandalone;
}

function visitRenderedElement(
  element: RenderedElement,
  depth: number,
  callback: (element: RenderedElement, depth: number) => void,
) {
  callback(element, depth);
  for (const child of element.children) {
    visitRenderedElement(child, depth + 1, callback);
  }
}

function searchRenderedElement(
  element: RenderedElement,
  predicate: (element: RenderedElement) => boolean,
): RenderedNode[] {
  const out: RenderedNode[] = [];
  visitRenderedElement(element, 0, childElement => {
    if (predicate(childElement)) {
      out.push(new RenderedNode(childElement));
    }
  });
  return out;
}

function outputXMLAttributeValue(value: any): string {
  if (value === undefined) {
    return '<undefined>';
  } else if (value === null) {
    return '<null>';
  } else if (typeof value === 'function') {
    return '<function>';
  } else if (Array.isArray(value)) {
    const values = value.map(outputXMLAttributeValue).join(', ');
    return `[${values}]`;
  } else {
    return value.toString();
  }
}

function outputXML(element: RenderedElement, indent: string, output: string[]) {
  output.push(indent);
  output.push('<');
  output.push(element.nodeTag);

  for (const attributeName in element.attributes) {
    const attributeValue = outputXMLAttributeValue(element.attributes[attributeName]);

    output.push(' ');
    output.push(attributeName);
    output.push('="');
    output.push(attributeValue.toString());
    output.push('"');
  }

  if (element.children.length) {
    output.push('>\n');
    const childIndent = indent + '  ';

    for (const child of element.children) {
      outputXML(child, childIndent, output);
    }

    output.push(indent);
    output.push('</');
    output.push(element.nodeTag);
    output.push('>\n');
  } else {
    output.push('/>\n');
  }
}

export class RenderedNode {
  private element: RenderedElement;

  constructor(element: RenderedElement) {
    this.element = element;
  }

  getNodeTag(): string {
    return this.element.nodeTag;
  }

  getViewClass(): string {
    return this.element.viewClass;
  }

  getAttribute(name: string): any {
    return this.element.attributes[name];
  }

  getAttributes(): StringMap<any> {
    return this.element.attributes;
  }

  getChildCount(): number {
    return this.element.children.length;
  }

  getChild(index: number): RenderedNode {
    return new RenderedNode(this.element.children[index]);
  }

  findForId(id: string): RenderedNode[] {
    return searchRenderedElement(this.element, element => {
      for (const elementId of element.ids) {
        if (elementId.id === id) {
          return true;
        }
      }

      return false;
    });
  }

  findForViewClass(viewClass: string): RenderedNode[] {
    return searchRenderedElement(this.element, element => {
      return element.viewClass === viewClass;
    });
  }

  findForNodeType(nodeType: string): RenderedNode[] {
    return searchRenderedElement(this.element, element => {
      return element.nodeTag === nodeType;
    });
  }

  findForAttribute(name: string, value: any): RenderedNode[] {
    return searchRenderedElement(this.element, element => {
      return element.attributes[name] === value;
    });
  }

  /**
   * Returns a simplified representation of the tree by only including
   * the specified properties. This allows to simplify deep comparisons.
   *
   * @param include which properties to include
   */
  simplify(include?: RenderedElementProperty[]): SimplifiedRenderedElement {
    const out: SimplifiedRenderedElement = {};

    if (!include) {
      include = ['viewClass', 'nodeTag', 'frame', 'attributes'];
    }

    for (const prop of include) {
      if (prop === 'viewClass') {
        out.viewClass = this.element.viewClass;
      } else if (prop === 'nodeTag') {
        out.nodeTag = this.element.nodeTag;
      } else if (prop === 'frame') {
        out.frame = this.element.frame;
      } else if (prop === 'attributes') {
        out.attributes = this.element.attributes;
      }
    }

    if (this.getChildCount() > 0) {
      const children: SimplifiedRenderedElement[] = [];
      for (let index = 0; index < this.getChildCount(); index++) {
        const child = this.getChild(index);
        children.push(child.simplify(include));
      }
      out.children = children;
    }

    return out;
  }

  toXML(): string {
    const allStrings: string[] = [];
    outputXML(this.element, '', allStrings);

    return allStrings.join('');
  }
}

class OverridenRootComponentsManager extends RootComponentsManager {
  constructor(
    rendererFactory: RendererFactory,
    daemonClientManager: DaemonClientManager | undefined,
    submitDebugMessage: SubmitDebugMessageFunc,
    readonly renderFunction: EntryPointRenderFunction,
    readonly componentPath: ComponentPath,
    readonly initialViewModel: any,
    readonly componentContext: any,
  ) {
    super(rendererFactory, daemonClientManager, submitDebugMessage);
  }

  create(componentId: string, componentPath: string, viewModel: any, componentContext: any): void {
    this.createWithRenderFunction(
      componentId,
      this.componentPath,
      this.renderFunction,
      this.initialViewModel,
      this.componentContext,
    );
  }
}

export class InstrumentedComponentBase<T extends IComponent, ViewModelType, ContextType> {
  componentId: string = '';
  private component: T;

  private readonly rootComponentsManager: RootComponentsManager;
  private onRenderPromises?: (() => void)[];

  constructor(
    componentClass: ComponentConstructor<T>,
    viewModel: ViewModelType | undefined,
    componentContext: ContextType | undefined,
    useErrorBoundary: boolean,
  ) {
    this.rootComponentsManager = new OverridenRootComponentsManager(
      { makeRenderer: treeId => jsx.makeRendererWithAllowedRootElementTypes(undefined, treeId) },
      undefined,
      runtime.submitDebugMessage,
      makeEntryPointRenderFunction(() => componentClass, useErrorBoundary, undefined),
      {
        filePath: '<none>',
        symbolName: componentClass.name,
      },
      viewModel,
      componentContext,
    );

    const contextId = runtime.createContext(this.rootComponentsManager);

    const component = (this.rootComponentsManager.getComponentForContextId(contextId) as IEntryPointComponent)
      ?.rootComponent;
    if (!component) {
      throw Error('Could not resolve component after context was created');
    }

    this.componentId = contextId;
    this.component = component as T;

    const onRender = this.component.onRender.bind(this.component);
    this.component.onRender = () => {
      onRender();
      setTimeout(() => {
        this.componentDidRender();
      }, 0);
    };
  }

  getComponent(): T {
    return this.component;
  }

  getRenderedNode(): Promise<RenderedNode> {
    return new Promise<RenderedNode>((resolve, reject) => {
      valdiStandalone.getRenderedElement(this.componentId!, (renderedNode, error) => {
        if (error) {
          reject(new Error(error));
        } else {
          resolve(new RenderedNode(renderedNode!));
        }
      });
    });
  }

  waitForNextRender(): Promise<void> {
    let resolved = false;
    let resolveFn: (() => void) | undefined;

    if (!this.onRenderPromises) {
      this.onRenderPromises = [];
    }

    this.onRenderPromises.push(() => {
      resolved = true;
      if (resolveFn) {
        resolveFn();
      }
    });

    return new Promise(resolve => {
      resolveFn = resolve;
      if (resolved) {
        resolve();
      }
    });
  }

  destroy() {
    runtime.destroyContext(this.componentId);
  }

  setViewModel(viewModel: ViewModelType) {
    this.rootComponentsManager.render(this.componentId, viewModel);
  }

  setLayoutSpecs(width: number, height: number, rtl?: boolean): void {
    runtime.setLayoutSpecs(this.componentId, width, height, !!rtl);
  }

  waitForNextLayout(): Promise<void> {
    return new Promise(resolve => {
      runtime.performSyncWithMainThread(resolve);
    });
  }

  private componentDidRender() {
    const onRenderPromises = this.onRenderPromises;
    this.onRenderPromises = undefined;
    if (onRenderPromises) {
      onRenderPromises.forEach(cb => cb());
    }
  }
}

export class InstrumentedComponent<
  T extends LegacyVueComponent<ViewModelType, any, ContextType>,
  ViewModelType extends {},
  ContextType extends {},
> extends InstrumentedComponentBase<T, ViewModelType, ContextType> {
  static create<
    T extends LegacyVueComponent<ViewModelType, any, ContextType>,
    ViewModelType extends object,
    ContextType extends ComponentContext,
  >(
    componentClass: ComponentConstructor<T>,
    viewModel: ViewModelType,
    context: ContextType,
    useErrorBoundary: boolean,
  ): InstrumentedComponent<T, ViewModelType, ContextType> {
    return new InstrumentedComponent(componentClass, viewModel, context, useErrorBoundary);
  }
}
