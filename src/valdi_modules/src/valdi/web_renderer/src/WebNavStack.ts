import { INavigatorPageConfig } from 'valdi_navigation/src/INavigator';
import { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import { ComponentPrototype } from 'valdi_core/src/ComponentPrototype';
import { ValdiWebRenderer } from './ValdiWebRenderer';
import { WebNavigator } from './WebNavigator';
import { RouteRegistry } from './RouteRegistry';

interface WebNavPage {
  pageDiv: HTMLDivElement;
  renderer: ValdiWebRenderer | undefined;
  navigator: WebNavigator;
  componentPath: string;
  viewModel: any;
  context: any;
  isModal: boolean;
}

interface HistoryState {
  depth: number;
  paths: string[];
}

const ANIMATION_DURATION_MS = 250;

export type WebNavStackBuildUrl = (
  paths: string[],
  routeRegistry: RouteRegistry | undefined,
  basePath: string,
) => string;

export interface WebNavStackOptions {
  routeRegistry?: RouteRegistry;
  basePath?: string;
  /** Override URL generation from the current stack's component paths. */
  buildUrl?: WebNavStackBuildUrl;
}

export class WebNavStack {
  private stack: WebNavPage[] = [];
  private container: HTMLElement;
  private routeRegistry: RouteRegistry | undefined;
  private basePath: string;
  private buildUrlFn: WebNavStackBuildUrl | undefined;
  // Sync guard: when set, the next popstate matching this depth is suppressed.
  private expectedDepth: number | null = null;
  private boundOnPopState: (e: PopStateEvent) => void;
  private initialPathname: string;
  private suppressHistory = false;

  constructor(container: HTMLElement, options?: WebNavStackOptions) {
    this.container = container;
    this.container.style.position = 'relative';
    this.container.style.overflow = 'hidden';
    this.container.style.width = '100%';
    this.container.style.height = '100%';
    this.routeRegistry = options?.routeRegistry;
    this.basePath = (options?.basePath ?? '').replace(/\/+$/, '');
    this.buildUrlFn = options?.buildUrl;
    this.initialPathname = window.location.pathname;

    this.boundOnPopState = this.onPopState.bind(this);
    window.addEventListener('popstate', this.boundOnPopState);
  }

  push(page: INavigatorPageConfig, animated: boolean, isModal: boolean): WebNavigator {
    const pageIndex = this.stack.length;
    const navigator = new WebNavigator(this, pageIndex);

    const pageDiv = document.createElement('div');
    pageDiv.style.cssText =
      'position:absolute;inset:0;background:white;overflow:hidden;display:flex;flex-direction:column;min-height:0;';

    const runtime = (globalThis as any).runtime;
    const ctor = runtime?.requireByComponent?.(page.componentPath);
    if (!ctor) {
      console.error('WebNavStack: component not found:', page.componentPath);
      return navigator;
    }

    const renderer = new ValdiWebRenderer(pageDiv);
    const contextWithNav = { ...(page.componentContext ?? {}), navigator };
    renderer.renderRootComponent(
      ctor,
      ComponentPrototype.instanceWithNewId(),
      page.componentViewModel ?? {},
      contextWithNav,
    );

    this.container.appendChild(pageDiv);

    const entry: WebNavPage = {
      pageDiv,
      renderer,
      navigator,
      componentPath: page.componentPath,
      viewModel: page.componentViewModel,
      context: page.componentContext,
      isModal,
    };
    this.stack.push(entry);

    this.updateVisibility();

    if (animated && this.stack.length > 1) {
      this.animateIn(pageDiv, isModal);
    }

    if (!this.suppressHistory && this.expectedDepth === null) {
      this.syncHistory();
    }

    return navigator;
  }

  /**
   * Push a component by constructor rather than by componentPath.
   * Useful at the app entry point where you already have the class reference.
   * A synthetic componentPath is stored for history state tracking.
   */
  pushWithConstructor<T extends IComponent<ViewModel, Context>, ViewModel, Context>(
    ctor: ComponentConstructor<T, ViewModel, Context>,
    viewModel: ViewModel,
    context: Context,
    animated: boolean,
    isModal: boolean,
  ): WebNavigator {
    const pageIndex = this.stack.length;
    const navigator = new WebNavigator(this, pageIndex);

    const pageDiv = document.createElement('div');
    pageDiv.style.cssText =
      'position:absolute;inset:0;background:white;overflow:hidden;display:flex;flex-direction:column;min-height:0;';

    const renderer = new ValdiWebRenderer(pageDiv);
    const contextWithNav = { ...(context ?? {} as any), navigator };
    renderer.renderRootComponent(
      ctor,
      ComponentPrototype.instanceWithNewId(),
      viewModel ?? {} as any,
      contextWithNav,
    );

    this.container.appendChild(pageDiv);

    const componentPath = (ctor as any).componentPath ?? ctor.name ?? `anonymous-${pageIndex}`;
    const entry: WebNavPage = {
      pageDiv,
      renderer,
      navigator,
      componentPath,
      viewModel,
      context,
      isModal,
    };
    this.stack.push(entry);

    this.updateVisibility();

    if (animated && this.stack.length > 1) {
      this.animateIn(pageDiv, isModal);
    }

    if (!this.suppressHistory && this.expectedDepth === null) {
      this.syncHistory();
    }

    return navigator;
  }

  /**
   * Push a page rendered by a plain DOM callback instead of a Valdi component.
   * Useful for non-Valdi content or lightweight demos.
   */
  pushDOM(
    componentPath: string,
    renderFn: (container: HTMLElement, navigator: WebNavigator) => void,
    animated: boolean,
    isModal: boolean,
  ): WebNavigator {
    const pageIndex = this.stack.length;
    const navigator = new WebNavigator(this, pageIndex);

    const pageDiv = document.createElement('div');
    pageDiv.style.cssText =
      'position:absolute;inset:0;overflow:hidden;display:flex;flex-direction:column;min-height:0;';
    const contentDiv = document.createElement('div');
    contentDiv.style.cssText = 'width:100%;height:100%;min-height:0;flex:1;display:flex;flex-direction:column;';
    pageDiv.appendChild(contentDiv);
    renderFn(contentDiv, navigator);

    this.container.appendChild(pageDiv);

    const entry: WebNavPage = {
      pageDiv,
      renderer: undefined,
      navigator,
      componentPath,
      viewModel: undefined,
      context: undefined,
      isModal,
    };
    this.stack.push(entry);

    this.updateVisibility();

    if (animated && this.stack.length > 1) {
      this.animateIn(pageDiv, isModal);
    }

    if (!this.suppressHistory && this.expectedDepth === null) {
      this.syncHistory();
    }

    return navigator;
  }

  popTo(targetIndex: number, animated: boolean): void {
    if (targetIndex < 0 || targetIndex >= this.stack.length - 1) {
      return;
    }

    const entriesToRemove = this.stack.slice(targetIndex + 1);
    if (entriesToRemove.length === 0) {
      return;
    }

    const topEntry = entriesToRemove[entriesToRemove.length - 1];

    // Reveal the page we're popping back to.
    const targetEntry = this.stack[targetIndex];
    targetEntry.pageDiv.style.visibility = 'visible';
    targetEntry.pageDiv.style.pointerEvents = 'auto';

    // Update stack and history synchronously to prevent race conditions
    // with pushes or pops that arrive during the animation window.
    this.stack = this.stack.slice(0, targetIndex + 1);
    this.updateVisibility();

    if (this.expectedDepth === null) {
      const delta = entriesToRemove.length;
      this.expectedDepth = this.stack.length;
      history.go(-delta);
    } else if (this.stack.length === this.expectedDepth) {
      this.expectedDepth = null;
    }

    const cleanup = (): void => {
      for (const entry of entriesToRemove) {
        entry.renderer?.destroy();
        entry.pageDiv.remove();
      }
    };

    if (animated) {
      this.animateOut(topEntry.pageDiv, topEntry.isModal, cleanup);
    } else {
      cleanup();
    }
  }

  dismissFrom(navigatorIndex: number, animated: boolean): void {
    let modalIndex = Math.min(navigatorIndex, this.stack.length - 1);
    while (modalIndex > 0 && !this.stack[modalIndex].isModal) {
      modalIndex--;
    }
    if (modalIndex > 0) {
      this.popTo(modalIndex - 1, animated);
    }
  }

  /**
   * Parse the current URL and return an initial stack if the route registry
   * can resolve every segment. Returns null if deep linking is not possible.
   */
  getInitialStackFromUrl(): Array<{ componentPath: string; viewModel: any; context: any }> | null {
    if (!this.routeRegistry) {
      return null;
    }

    let path = this.initialPathname;
    if (this.basePath) {
      if (path === this.basePath) {
        path = '/';
      } else if (path.startsWith(this.basePath + '/')) {
        path = path.slice(this.basePath.length);
      }
    }
    if (!path || path === '/') {
      return null;
    }

    const segments = path.split('/').filter(s => s.length > 0);
    if (segments.length === 0) {
      return null;
    }

    const entries: Array<{ componentPath: string; viewModel: any; context: any }> = [];
    for (const segment of segments) {
      const route = this.routeRegistry.entryForSegment(segment);
      if (!route) {
        return null;
      }
      entries.push({
        componentPath: route.componentPath,
        viewModel: route.defaultViewModel?.() ?? {},
        context: route.defaultContext?.() ?? {},
      });
    }
    return entries;
  }

  /**
   * Restore the navigation stack from the current URL without creating
   * individual browser history entries for each page. After all pages are
   * pushed, a single replaceState sets the correct depth so that browser
   * back walks through the stack one page at a time.
   */
  restoreFromUrl(): boolean {
    const entries = this.getInitialStackFromUrl();
    if (!entries || entries.length === 0) {
      return false;
    }

    this.suppressHistory = true;
    for (const entry of entries) {
      this.push(
        { componentPath: entry.componentPath, componentViewModel: entry.viewModel, componentContext: entry.context },
        false,
        false,
      );
    }
    this.suppressHistory = false;

    const paths = this.stack.map(e => e.componentPath);
    const existingState = history.state as HistoryState | null;
    const isRefresh = existingState != null && typeof existingState.depth === 'number';

    if (isRefresh) {
      // On refresh the pre-existing history entries still sit behind us.
      // Just stamp the current entry so our state matches the stack.
      history.replaceState(this.makeState(paths), '', this.buildUrl(paths));
    } else {
      // Fresh deep link — no prior entries exist. Back-fill one entry per
      // stack level so browser back walks through the stack.
      history.replaceState(this.makeState(paths.slice(0, 1)), '', this.buildUrl(paths.slice(0, 1)));
      for (let i = 2; i <= paths.length; i++) {
        const sub = paths.slice(0, i);
        history.pushState(this.makeState(sub), '', this.buildUrl(sub));
      }
    }
    return true;
  }

  destroy(): void {
    window.removeEventListener('popstate', this.boundOnPopState);
    for (const entry of this.stack) {
      entry.renderer?.destroy();
      entry.pageDiv.remove();
    }
    this.stack = [];
  }

  // -- History API listener -------------------------------------------------

  private onPopState(e: PopStateEvent): void {
    const state = e.state as HistoryState | null;
    if (!state || typeof state.depth !== 'number') {
      return;
    }

    const targetDepth = state.depth;

    // If we triggered this popstate ourselves, consume and return.
    if (this.expectedDepth !== null) {
      if (targetDepth === this.expectedDepth) {
        this.expectedDepth = null;
      }
      return;
    }

    const currentDepth = this.stack.length;

    if (targetDepth < currentDepth) {
      // Browser back — pop Valdi stack to match.
      const targetIndex = Math.max(targetDepth - 1, 0);
      if (targetIndex >= currentDepth - 1) {
        return;
      }
      this.expectedDepth = targetIndex + 1;
      this.popTo(targetIndex, true);
    } else if (targetDepth > currentDepth && state.paths) {
      // Browser forward — re-push pages from stored component paths.
      this.expectedDepth = targetDepth;
      const pathsToPush = state.paths.slice(currentDepth);
      for (const componentPath of pathsToPush) {
        const segment = this.routeRegistry?.segmentForComponentPath(componentPath);
        const route = segment ? this.routeRegistry?.entryForSegment(segment) : undefined;
        this.push(
          {
            componentPath,
            componentViewModel: route?.defaultViewModel?.() ?? {},
            componentContext: route?.defaultContext?.() ?? {},
          },
          false,
          false,
        );
      }
      this.expectedDepth = null;
    }
  }

  // -- Animation helpers ----------------------------------------------------

  private animateIn(pageDiv: HTMLDivElement, isModal: boolean): void {
    const axis = isModal ? 'Y' : 'X';
    pageDiv.style.transform = `translate${axis}(100%)`;
    requestAnimationFrame(() => {
      pageDiv.style.transition = `transform ${ANIMATION_DURATION_MS}ms ease-out`;
      pageDiv.style.transform = 'translate(0)';
      pageDiv.addEventListener(
        'transitionend',
        () => {
          pageDiv.style.transition = '';
        },
        { once: true },
      );
    });
  }

  private animateOut(
    pageDiv: HTMLDivElement,
    isModal: boolean,
    onComplete: () => void,
  ): void {
    const axis = isModal ? 'Y' : 'X';
    pageDiv.style.transition = `transform ${ANIMATION_DURATION_MS}ms ease-out`;
    pageDiv.style.transform = `translate${axis}(100%)`;

    let finished = false;
    const finish = (): void => {
      if (finished) return;
      finished = true;
      pageDiv.removeEventListener('transitionend', finish);
      onComplete();
    };
    pageDiv.addEventListener('transitionend', finish);
    setTimeout(finish, ANIMATION_DURATION_MS + 50);
  }

  // -- Internal helpers -----------------------------------------------------

  private updateVisibility(): void {
    for (let i = 0; i < this.stack.length; i++) {
      const entry = this.stack[i];
      const isTop = i === this.stack.length - 1;
      entry.pageDiv.style.visibility = isTop ? 'visible' : 'hidden';
      entry.pageDiv.style.pointerEvents = isTop ? 'auto' : 'none';
    }
  }

  private syncHistory(): void {
    const paths = this.stack.map(e => e.componentPath);
    if (this.stack.length <= 1) {
      history.replaceState(this.makeState(paths), '', this.buildUrl(paths));
    } else {
      history.pushState(this.makeState(paths), '', this.buildUrl(paths));
    }
  }

  private buildUrl(paths: string[]): string {
    if (this.buildUrlFn) {
      return this.buildUrlFn(paths, this.routeRegistry, this.basePath);
    }
    const base = this.basePath || '';
    if (!this.routeRegistry || paths.length === 0) {
      return base || '/';
    }

    const segments = paths.map(p => {
      const segment = this.routeRegistry!.segmentForComponentPath(p);
      return segment ?? sanitizeComponentPath(p);
    });
    return base + '/' + segments.join('/');
  }

  private makeState(paths: string[]): HistoryState {
    return { depth: paths.length, paths };
  }
}

function sanitizeComponentPath(componentPath: string): string {
  const atIndex = componentPath.indexOf('@');
  const name = atIndex >= 0 ? componentPath.substring(0, atIndex) : componentPath;
  return name.toLowerCase().replace(/[^a-z0-9]+/g, '-');
}
