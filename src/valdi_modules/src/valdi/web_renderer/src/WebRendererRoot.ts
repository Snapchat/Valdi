const ISOLATED_ROOT_STYLES: Record<string, string> = {
  all: 'initial',
  color: 'black',
  display: 'block',
  fontFamily: 'inherit',
  height: '100%',
  MozOsxFontSmoothing: 'inherit',
  webkitFontSmoothing: 'inherit',
  width: '100%',
};

const ISOLATED_ELEMENT_STYLES = `
* {
  box-sizing: border-box;
}

/* contenteditable elements do not render their placeholder attribute. */
[data-valdi-empty='true'][placeholder]::before {
  color: var(--valdi-textview-placeholder-color, currentColor);
  content: attr(placeholder);
  pointer-events: none;
}
`;

export interface WebRendererLayoutRegistration {
  dispose(): void;
}

const layoutRoots = new Map<string, HTMLElement>();

function getOrCreateShadowRoot(htmlRoot: HTMLElement | ShadowRoot): ShadowRoot {
  if (typeof ShadowRoot !== 'undefined' && htmlRoot instanceof ShadowRoot) {
    return htmlRoot;
  }
  const host = htmlRoot as HTMLElement;
  return host.shadowRoot ?? host.attachShadow({ mode: 'open' });
}

export function createIsolatedWebRendererRoot(htmlRoot: HTMLElement | ShadowRoot): HTMLElement {
  const shadowRoot = getOrCreateShadowRoot(htmlRoot);
  const style = document.createElement('style');
  style.textContent = ISOLATED_ELEMENT_STYLES;
  const root = document.createElement('div');
  Object.assign(root.style, ISOLATED_ROOT_STYLES);
  shadowRoot.replaceChildren(style, root);
  return root;
}

export function registerWebRendererLayoutRoot(contextId: string, root: HTMLElement): WebRendererLayoutRegistration {
  layoutRoots.set(contextId, root);

  let disposed = false;
  return {
    dispose(): void {
      if (disposed) {
        return;
      }
      disposed = true;
      if (layoutRoots.get(contextId) === root) {
        layoutRoots.delete(contextId);
      }
    },
  };
}

export function setWebRendererLayoutDirection(contextId: string, rightToLeft: boolean): void {
  const root = layoutRoots.get(contextId);
  if (root !== undefined) {
    root.style.direction = rightToLeft ? 'rtl' : 'ltr';
  }
}
