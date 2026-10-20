// Index of the parenthesis that closes the one at `open`, or -1.
function findClosingParenthesis(value: string, open: number): number {
  let depth = 0;
  for (let index = open; index < value.length; index++) {
    if (value[index] === '(') {
      depth++;
    } else if (value[index] === ')') {
      depth--;
      if (depth === 0) {
        return index;
      }
    }
  }
  return -1;
}

function scopeSelector(selector: string, scopeSelector: string): string {
  const trimmedSelector = selector.trim();
  if (!trimmedSelector.startsWith(':host')) {
    return `${scopeSelector} ${trimmedSelector}`;
  }
  let rest = trimmedSelector.slice(':host'.length);
  // `:host(<compound>)` matches the host itself when it matches <compound>.
  if (rest.startsWith('(')) {
    const close = findClosingParenthesis(rest, 0);
    if (close < 0) {
      return `${scopeSelector} ${trimmedSelector}`;
    }
    rest = `${rest.slice(1, close)}${rest.slice(close + 1)}`;
  } else if (rest !== '' && !/^[\s:.[>+~]/.test(rest)) {
    // Something like `:hostname` is not the :host pseudo-class.
    return `${scopeSelector} ${trimmedSelector}`;
  }
  return `${scopeSelector}${rest}`;
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
