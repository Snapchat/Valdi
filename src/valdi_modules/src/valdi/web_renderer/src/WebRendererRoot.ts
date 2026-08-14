const ISOLATED_ROOT_STYLES: Record<string, string> = {
  all: 'initial',
  color: 'black',
  display: 'block',
  height: '100%',
  width: '100%',
};

const ISOLATED_ELEMENT_STYLES = '* { box-sizing: border-box; }';

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
