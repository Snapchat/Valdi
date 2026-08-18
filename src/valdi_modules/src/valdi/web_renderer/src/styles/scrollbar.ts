const SCROLLBAR_STYLES = `
  .hide-v-scrollbar::-webkit-scrollbar:vertical {
    display: none;
    width: 0;
  }

  .hide-h-scrollbar::-webkit-scrollbar:horizontal {
    display: none;
    height: 0;
  }
`;

const STYLE_ID = 'valdi-scrollbar-styles';

export function injectScrollbarStyles(root: Document | ShadowRoot): void {
  const container =
    typeof Document !== 'undefined' && root instanceof Document ? root.head : root;
  if (container.querySelector(`#${STYLE_ID}`)) {
    return;
  }

  const style = document.createElement('style');
  style.id = STYLE_ID;
  style.textContent = SCROLLBAR_STYLES;
  container.appendChild(style);
}
