const TOUCH_AREA_STYLES = `
  [data-touch-ext]::before {
    content: '';
    position: absolute;
    top: calc(-1 * var(--touch-ext-top, 0px));
    right: calc(-1 * var(--touch-ext-right, 0px));
    bottom: calc(-1 * var(--touch-ext-bottom, 0px));
    left: calc(-1 * var(--touch-ext-left, 0px));
    pointer-events: auto;
  }
`;

const STYLE_ID = 'valdi-touch-area-styles';

export function injectTouchAreaStyles(root: Document | ShadowRoot) {
  const container = root instanceof Document ? root.head : root;
  if (container.querySelector(`#${STYLE_ID}`)) return;

  const style = document.createElement('style');
  style.id = STYLE_ID;
  style.textContent = TOUCH_AREA_STYLES;
  container.appendChild(style);
}
