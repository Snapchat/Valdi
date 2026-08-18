function scopeSelector(selector: string, scopeSelector: string): string {
  const trimmedSelector = selector.trim();
  if (trimmedSelector === ':host') {
    return scopeSelector;
  }
  if (trimmedSelector.startsWith(':host(')) {
    return `${scopeSelector}${trimmedSelector.slice(5)}`;
  }
  return `${scopeSelector} ${trimmedSelector}`;
}

// The server DOM flattens the renderer's shadow root into a normal element, so
// each ordinary style rule needs the SSR root selector prepended to it. This
// expression matches only the flat rules emitted by WebRendererRoot:
//   1. (^|}) captures the start of the stylesheet or the previous rule's end.
//   2. \s* consumes the whitespace between rules.
//   3. ([^}{]+) captures the selector list up to its opening `{`.
// At-rules are recognized explicitly in the replacement callback and retained.
const FLAT_STYLE_RULE = /(^|})\s*([^}{]+)\{/g;

export function scopeServerCSS(css: string, rootSelector: string): string {
  const withoutComments = css.replace(/\/\*[\s\S]*?\*\//g, '');
  return withoutComments.replace(FLAT_STYLE_RULE, (match, boundary: string, selectors: string) => {
    if (selectors.trimStart().startsWith('@')) {
      return match;
    }
    const scopedSelectors = selectors
      .split(',')
      .map(selector => scopeSelector(selector, rootSelector))
      .join(', ');
    return `${boundary}\n${scopedSelectors} {`;
  });
}
