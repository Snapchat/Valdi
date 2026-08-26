import { ComponentPrototype } from 'valdi_core/src/ComponentPrototype';
import type { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import type {
  INavigator,
  INavigatorPageConfig,
  INavigatorPageVisibility,
  JSOnlyINavigator,
} from 'valdi_navigation/src/INavigator';

const HIDDEN_PAGE_VISIBILITY = 0 as INavigatorPageVisibility;
const VISIBLE_PAGE_VISIBILITY = 1 as INavigatorPageVisibility;
const PAGE_ANIMATION_DURATION_MS = 180;
const DIALOG_CLASS_NAME = 'valdi-web-navigation-dialog';
const SHEET_CLASS_NAME = 'valdi-web-navigation-sheet';

const PAGE_STYLES: Record<string, string> = {
  height: '100%',
  inset: '0',
  position: 'absolute',
  width: '100%',
};

const DIALOG_STYLES: Record<string, string> = {
  background: 'transparent',
  border: '0',
  borderRadius: '20px',
  boxSizing: 'border-box',
  height: 'min(720px, calc(100vh - 48px))',
  margin: 'auto',
  maxHeight: 'calc(100vh - 48px)',
  maxWidth: 'calc(100vw - 32px)',
  outline: 'none',
  overflow: 'hidden',
  padding: '0',
  width: 'min(520px, calc(100vw - 32px))',
};

const SHEET_STYLES: Record<string, string> = {
  height: 'fit-content',
  margin: 'auto auto 24px',
  width: 'min(480px, calc(100vw - 32px))',
};

const DIALOG_HOST_STYLES: Record<string, string> = {
  height: '100%',
  overflow: 'hidden',
  position: 'relative',
  width: '100%',
};

const NAVIGATION_STYLES = `
.${DIALOG_CLASS_NAME}::backdrop {
  background: rgba(0, 0, 0, 0.36);
}
`;

enum WebNavigationPresentation {
  Page,
  Dialog,
  Sheet,
}

export interface WebNavigationRenderer {
  onDestroy(): void;
  renderRoot(render: () => void): void;
  renderRootComponent(
    constructor: ComponentConstructor<IComponent>,
    prototype: ComponentPrototype,
    viewModel: unknown,
    context: unknown,
  ): void;
}

export interface WebNavigationHostOptions {
  readonly createRenderer: (root: HTMLElement, contextIdentifierPrefix: string) => WebNavigationRenderer;
  readonly resolveComponent: (componentPath: string) => ComponentConstructor<IComponent>;
  readonly root: HTMLElement;
}

interface WebNavigationPageEntry {
  readonly container: HTMLElement;
  readonly navigator: WebNavigator;
  readonly presentation: WebNavigationPresentation;
  renderer?: WebNavigationRenderer;
  stack: WebNavigationStack;
}

interface WebNavigationStack {
  readonly dialog?: HTMLDialogElement;
  readonly entries: WebNavigationPageEntry[];
  readonly mount: HTMLElement | ShadowRoot;
}

interface DialogListeners {
  readonly cancel: (event: Event) => void;
  readonly click: (event: MouseEvent) => void;
}

export class WebNavigationHost {
  readonly rootNavigator: WebNavigator;

  private readonly createRenderer: WebNavigationHostOptions['createRenderer'];
  private readonly resolveComponent: WebNavigationHostOptions['resolveComponent'];
  private readonly overlayRoot: HTMLElement | ShadowRoot;
  private readonly stacks: WebNavigationStack[];
  private readonly dialogListeners = new Map<HTMLDialogElement, DialogListeners>();
  private navigationStyle?: HTMLStyleElement;
  private contextSequence = 0;
  private destroyed = false;

  constructor(options: WebNavigationHostOptions) {
    this.createRenderer = options.createRenderer;
    this.resolveComponent = options.resolveComponent;
    this.overlayRoot = (options.root.parentNode as HTMLElement | ShadowRoot | null) ?? options.root;

    const rootStack: WebNavigationStack = { entries: [], mount: this.overlayRoot };
    const rootEntry: WebNavigationPageEntry = {
      container: options.root,
      navigator: undefined as unknown as WebNavigator,
      presentation: WebNavigationPresentation.Page,
      stack: rootStack,
    };
    const rootNavigator = new WebNavigator(this, rootEntry);
    (rootEntry as { navigator: WebNavigator }).navigator = rootNavigator;
    rootStack.entries.push(rootEntry);
    this.stacks = [rootStack];
    this.rootNavigator = rootNavigator;

    if (typeof document !== 'undefined') {
      document.addEventListener('keydown', this.handleKeyDown, true);
    }
  }

  destroy(): void {
    if (this.destroyed) {
      return;
    }
    this.destroyed = true;
    if (typeof document !== 'undefined') {
      document.removeEventListener('keydown', this.handleKeyDown, true);
    }
    this.removePresentationStacks(1);
    const rootStack = this.stacks[0];
    if (rootStack) {
      this.removePagesAfter(rootStack, 0);
      rootStack.entries[0]?.navigator.destroy();
    }
    this.navigationStyle?.remove();
    this.navigationStyle = undefined;
  }

  push(source: WebNavigationPageEntry, page: INavigatorPageConfig, animated: boolean): void {
    if (!this.isActiveEntry(source)) {
      return;
    }
    const entry = this.createPage(source.stack, page, WebNavigationPresentation.Page);
    source.stack.entries.push(entry);
    this.updateVisibility();
    this.animatePresentation(entry.container, entry.presentation, animated);
  }

  present(source: WebNavigationPageEntry, page: INavigatorPageConfig, animated: boolean): void {
    if (!this.isActiveEntry(source)) {
      return;
    }

    const presentation = page.isPartiallyHiding ? WebNavigationPresentation.Sheet : WebNavigationPresentation.Dialog;
    const dialog = this.createDialog(presentation, page.platformNavigationTitle);
    const stack: WebNavigationStack = { dialog, entries: [], mount: dialog };
    this.stacks.push(stack);

    try {
      const entry = this.createPage(stack, page, presentation);
      stack.entries.push(entry);
      this.updateVisibility();
      this.showDialog(dialog);
      this.animatePresentation(dialog, presentation, animated);
    } catch (error) {
      this.removePresentationStacks(this.stacks.length - 1);
      this.updateVisibility();
      throw error;
    }
  }

  pop(source: WebNavigationPageEntry, _animated: boolean): void {
    if (!this.isActiveEntry(source)) {
      return;
    }
    const stack = source.stack;
    if (stack.entries.length > 1) {
      this.removePagesAfter(stack, stack.entries.length - 2);
      this.updateVisibility();
      return;
    }
    if (stack.dialog) {
      this.removePresentationStacks(this.stacks.indexOf(stack));
      this.updateVisibility();
    }
  }

  popToRoot(source: WebNavigationPageEntry, _animated: boolean): void {
    if (!this.isEntryInActiveStack(source)) {
      return;
    }
    this.removePagesAfter(source.stack, 0);
    this.updateVisibility();
  }

  popToSelf(source: WebNavigationPageEntry, _animated: boolean): void {
    if (!this.isEntryInActiveStack(source)) {
      return;
    }
    const index = source.stack.entries.indexOf(source);
    if (index >= 0) {
      this.removePagesAfter(source.stack, index);
      this.updateVisibility();
    }
  }

  dismiss(source: WebNavigationPageEntry, _animated: boolean): void {
    const stackIndex = this.stacks.indexOf(source.stack);
    if (stackIndex < 0) {
      return;
    }

    if (stackIndex === 0) {
      if (this.stacks.length > 1) {
        this.removePresentationStacks(this.stacks.length - 1);
        this.updateVisibility();
      }
      return;
    }

    this.removePresentationStacks(stackIndex);
    this.updateVisibility();
  }

  handleInteractiveDismissal(): boolean {
    const active = this.currentEntry();
    if (!active || active.navigator.isDismissalDisabled) {
      return false;
    }

    const observer = active.navigator.backButtonObserver;
    if (observer) {
      observer();
      return true;
    }

    if (active.stack.entries.length > 1 || active.stack.dialog) {
      this.pop(active, true);
      return true;
    }
    return false;
  }

  isVisible(entry: WebNavigationPageEntry): boolean {
    return this.currentEntry() === entry;
  }

  removeHiddenEntry(entry: WebNavigationPageEntry): void {
    if (this.isVisible(entry)) {
      return;
    }
    const stackIndex = this.stacks.indexOf(entry.stack);
    if (stackIndex < 0) {
      return;
    }
    if (entry.stack.entries[0] === entry && entry.stack.dialog) {
      this.removePresentationStacks(stackIndex);
    } else {
      const entryIndex = entry.stack.entries.indexOf(entry);
      if (entryIndex > 0) {
        this.removePagesAfter(entry.stack, entryIndex - 1);
      }
    }
    this.updateVisibility();
  }

  private createPage(
    stack: WebNavigationStack,
    page: INavigatorPageConfig,
    presentation: WebNavigationPresentation,
  ): WebNavigationPageEntry {
    const constructor = this.resolveComponent(page.componentPath);
    const container = document.createElement('div');
    Object.assign(container.style, stack.dialog ? DIALOG_HOST_STYLES : PAGE_STYLES);
    stack.mount.appendChild(container);

    const entry: WebNavigationPageEntry = {
      container,
      navigator: undefined as unknown as WebNavigator,
      presentation,
      stack,
    };
    const navigator = new WebNavigator(this, entry);
    (entry as { navigator: WebNavigator }).navigator = navigator;

    try {
      const renderer = this.createRenderer(container, `navigation-${++this.contextSequence}`);
      entry.renderer = renderer;
      const componentContext = this.contextWithNavigator(page.componentContext, navigator);
      renderer.renderRootComponent(
        constructor,
        ComponentPrototype.instanceWithNewId(),
        page.componentViewModel,
        componentContext,
      );
      return entry;
    } catch (error) {
      navigator.destroy();
      this.destroyRenderer(entry.renderer);
      container.remove();
      throw error;
    }
  }

  private contextWithNavigator(value: unknown, navigator: WebNavigator): Record<string, unknown> {
    const source = value && typeof value === 'object' ? value : {};
    return { ...source, navigator };
  }

  private createDialog(presentation: WebNavigationPresentation, title: string | undefined): HTMLDialogElement {
    this.ensureNavigationStyles();
    const dialog = document.createElement('dialog');
    dialog.className =
      presentation === WebNavigationPresentation.Sheet ? `${DIALOG_CLASS_NAME} ${SHEET_CLASS_NAME}` : DIALOG_CLASS_NAME;
    dialog.setAttribute('aria-label', title || (presentation === WebNavigationPresentation.Sheet ? 'Sheet' : 'Dialog'));
    dialog.setAttribute('aria-modal', 'true');
    Object.assign(dialog.style, DIALOG_STYLES);
    if (presentation === WebNavigationPresentation.Sheet) {
      Object.assign(dialog.style, SHEET_STYLES);
    }

    const listeners: DialogListeners = {
      cancel: event => {
        event.preventDefault();
        this.handleInteractiveDismissal();
      },
      click: event => {
        if (event.target === dialog) {
          this.handleInteractiveDismissal();
        }
      },
    };
    dialog.addEventListener('cancel', listeners.cancel);
    dialog.addEventListener('click', listeners.click);
    this.dialogListeners.set(dialog, listeners);
    this.overlayRoot.appendChild(dialog);
    return dialog;
  }

  private ensureNavigationStyles(): void {
    if (this.navigationStyle) {
      return;
    }
    const style = document.createElement('style');
    style.textContent = NAVIGATION_STYLES;
    this.overlayRoot.appendChild(style);
    this.navigationStyle = style;
  }

  private showDialog(dialog: HTMLDialogElement): void {
    if (typeof dialog.showModal === 'function') {
      dialog.showModal();
      return;
    }
    dialog.setAttribute('open', '');
  }

  private animatePresentation(element: HTMLElement, presentation: WebNavigationPresentation, animated: boolean): void {
    if (!animated || typeof element.animate !== 'function') {
      return;
    }

    const initialTransform =
      presentation === WebNavigationPresentation.Page
        ? 'translateX(24px)'
        : presentation === WebNavigationPresentation.Sheet
          ? 'translateY(24px)'
          : 'translateY(8px) scale(0.98)';
    element.animate(
      [
        { opacity: 0, transform: initialTransform },
        { opacity: 1, transform: 'none' },
      ],
      { duration: PAGE_ANIMATION_DURATION_MS, easing: 'ease-out' },
    );
  }

  private removePresentationStacks(firstIndex: number): void {
    while (this.stacks.length > firstIndex) {
      const stack = this.stacks.pop();
      if (!stack) {
        return;
      }
      this.removePagesAfter(stack, -1);
      const dialog = stack.dialog;
      if (dialog) {
        const listeners = this.dialogListeners.get(dialog);
        if (listeners) {
          dialog.removeEventListener('cancel', listeners.cancel);
          dialog.removeEventListener('click', listeners.click);
          this.dialogListeners.delete(dialog);
        }
        if (dialog.open && typeof dialog.close === 'function') {
          dialog.close();
        }
        dialog.remove();
      }
    }
  }

  private removePagesAfter(stack: WebNavigationStack, keepIndex: number): void {
    while (stack.entries.length > keepIndex + 1) {
      const entry = stack.entries.pop();
      if (!entry) {
        return;
      }
      entry.navigator.destroy();
      this.destroyRenderer(entry.renderer);
      entry.container.remove();
    }
  }

  private destroyRenderer(renderer: WebNavigationRenderer | undefined): void {
    if (!renderer) {
      return;
    }
    // Renderer.onDestroy only releases its delegate. Render an empty root first
    // so presented components receive their normal onDestroy lifecycle hooks.
    renderer.renderRoot(() => {});
    renderer.onDestroy();
  }

  private updateVisibility(): void {
    const active = this.currentEntry();
    for (const stack of this.stacks) {
      const top = stack.entries[stack.entries.length - 1];
      for (const entry of stack.entries) {
        const isStackTop = entry === top;
        entry.container.style.display = isStackTop ? '' : 'none';
        entry.container.setAttribute('aria-hidden', entry === active ? 'false' : 'true');
        entry.navigator.updateVisibility(entry === active);
      }
    }
  }

  private currentEntry(): WebNavigationPageEntry | undefined {
    const stack = this.stacks[this.stacks.length - 1];
    return stack?.entries[stack.entries.length - 1];
  }

  private isActiveEntry(entry: WebNavigationPageEntry): boolean {
    return !this.destroyed && this.currentEntry() === entry;
  }

  private isEntryInActiveStack(entry: WebNavigationPageEntry): boolean {
    return (
      !this.destroyed && this.stacks[this.stacks.length - 1] === entry.stack && entry.stack.entries.includes(entry)
    );
  }

  private readonly handleKeyDown = (event: KeyboardEvent): void => {
    if (event.key !== 'Escape' || event.defaultPrevented) {
      return;
    }
    if (this.handleInteractiveDismissal()) {
      event.preventDefault();
    }
  };
}

export class WebNavigator implements INavigator, JSOnlyINavigator {
  readonly __shouldDisableMakeOpaque = true;

  private visibilityObserver?: (visibility: INavigatorPageVisibility) => void;
  private delayedPopMs?: number;
  private delayedPopTimer?: ReturnType<typeof setTimeout>;
  private visibility = VISIBLE_PAGE_VISIBILITY;
  private destroyed = false;
  backButtonObserver?: () => void;
  isDismissalDisabled = false;

  constructor(
    private readonly host: WebNavigationHost,
    private readonly entry: WebNavigationPageEntry,
  ) {}

  pushComponent(page: INavigatorPageConfig, animated: boolean): void {
    if (!this.destroyed) {
      this.host.push(this.entry, page, animated);
    }
  }

  pop(animated: boolean): void {
    if (!this.destroyed) {
      this.host.pop(this.entry, animated);
    }
  }

  popToRoot(animated: boolean): void {
    if (!this.destroyed) {
      this.host.popToRoot(this.entry, animated);
    }
  }

  popToSelf(animated: boolean): void {
    if (!this.destroyed) {
      this.host.popToSelf(this.entry, animated);
    }
  }

  presentComponent(page: INavigatorPageConfig, animated: boolean): void {
    if (!this.destroyed) {
      this.host.present(this.entry, page, animated);
    }
  }

  dismiss(animated: boolean): void {
    if (!this.destroyed) {
      this.host.dismiss(this.entry, animated);
    }
  }

  forceDisableDismissalGesture(forceDisable: boolean): void {
    if (!this.destroyed) {
      this.isDismissalDisabled = forceDisable;
    }
  }

  setBackButtonObserver(observer: (() => void) | undefined): void {
    if (!this.destroyed) {
      this.backButtonObserver = observer;
    }
  }

  setOnPausePopAfterDelay(delayMs: number | undefined): void {
    if (this.destroyed) {
      return;
    }
    this.delayedPopMs = delayMs !== undefined && Number.isFinite(delayMs) && delayMs >= 0 ? delayMs : undefined;
    this.updateDelayedPop();
  }

  setPageVisibilityObserver(observer: ((visibility: INavigatorPageVisibility) => void) | undefined): void {
    if (this.destroyed) {
      return;
    }
    this.visibilityObserver = observer;
    observer?.(this.visibility);
  }

  updateVisibility(visible: boolean): void {
    const nextVisibility = visible ? VISIBLE_PAGE_VISIBILITY : HIDDEN_PAGE_VISIBILITY;
    if (nextVisibility === this.visibility) {
      return;
    }
    this.visibility = nextVisibility;
    this.visibilityObserver?.(nextVisibility);
    this.updateDelayedPop();
  }

  destroy(): void {
    if (this.destroyed) {
      return;
    }
    this.destroyed = true;
    this.cancelDelayedPop();
    this.backButtonObserver = undefined;
    this.visibilityObserver = undefined;
  }

  private updateDelayedPop(): void {
    this.cancelDelayedPop();
    if (this.visibility !== HIDDEN_PAGE_VISIBILITY || this.delayedPopMs === undefined) {
      return;
    }
    this.delayedPopTimer = setTimeout(() => {
      this.delayedPopTimer = undefined;
      if (!this.destroyed && !this.host.isVisible(this.entry)) {
        this.host.removeHiddenEntry(this.entry);
      }
    }, this.delayedPopMs);
  }

  private cancelDelayedPop(): void {
    if (this.delayedPopTimer !== undefined) {
      clearTimeout(this.delayedPopTimer);
      this.delayedPopTimer = undefined;
    }
  }
}
