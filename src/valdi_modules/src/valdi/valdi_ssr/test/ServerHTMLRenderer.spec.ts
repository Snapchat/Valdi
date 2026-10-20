import 'jasmine/src/jasmine';
import { type ServerDOMMutationTracker } from '../src/dom/ServerDOM';
import { createServerDOMHost, releaseServerDOMHost } from '../src/dom/ServerDOMEnvironment';
import { scopeServerCSS } from '../src/serialization/ServerCSSScoper';
import { serializeServerHTMLRoot } from '../src/serialization/ServerHTMLSerializer';

describe('server HTML renderer DOM', () => {
  it('serializes escaped content and scoped shadow-root styles', () => {
    let mutationCount = 0;
    const mutationTracker: ServerDOMMutationTracker = {
      markMutation(): void {
        mutationCount++;
      },
    };
    const host = createServerDOMHost(mutationTracker);
    try {
      const shadowRoot = host.attachShadow({ mode: 'open' });
      const style = host.ownerDocument.createElement('style');
      style.textContent = '* { box-sizing: border-box; }';
      const content = host.ownerDocument.createElement('div');
      content.setAttribute('title', '"quoted" & value');
      content.style.setProperty('backgroundColor', 'red');
      content.textContent = '<first & value>';
      shadowRoot.replaceChildren(style, content);

      const html = serializeServerHTMLRoot(shadowRoot, 'test-root');
      expect(html).toContain('data-valdi-html-root="test-root"');
      expect(html).toContain('[data-valdi-html-root="test-root"] *');
      expect(html).toContain('background-color: red');
      expect(html).toContain('title="&quot;quoted&quot; &amp; value"');
      expect(html).toContain('&lt;first &amp; value&gt;');
      expect(mutationCount).toBeGreaterThan(0);
    } finally {
      releaseServerDOMHost();
    }
  });

  it('omits executable elements and event attributes', () => {
    const host = createServerDOMHost({ markMutation(): void {} });
    try {
      const shadowRoot = host.attachShadow({ mode: 'open' });
      const script = host.ownerDocument.createElement('script');
      script.textContent = 'globalThis.compromised = true';
      const content = host.ownerDocument.createElement('div');
      content.setAttribute('onclick', 'globalThis.compromised = true');
      content.setAttribute('srcdoc', '<script>bad()</script>');
      shadowRoot.replaceChildren(script, content);

      const html = serializeServerHTMLRoot(shadowRoot, 'safe-root');
      expect(html).toContain('data-valdi-unsupported="script"');
      expect(html).not.toContain('globalThis.compromised');
      expect(html).not.toContain('onclick');
      expect(html).not.toContain('srcdoc');
    } finally {
      releaseServerDOMHost();
    }
  });

  it('escapes style raw-text terminators', () => {
    const host = createServerDOMHost({ markMutation(): void {} });
    try {
      const shadowRoot = host.attachShadow({ mode: 'open' });
      const style = host.ownerDocument.createElement('style');
      const dynamicColor = '</StYlE><script>globalThis.compromised = true</script>';
      style.textContent = `.field::placeholder { color: ${dynamicColor}; }`;
      shadowRoot.replaceChildren(style);

      const html = serializeServerHTMLRoot(shadowRoot, 'safe-style-root');
      expect(html).toContain('\\3C /StYlE><script>');
      expect(html.toLowerCase()).not.toContain('</style><script>');
    } finally {
      releaseServerDOMHost();
    }
  });
  it('drops script URLs hidden behind control characters', () => {
    const host = createServerDOMHost({ markMutation(): void {} });
    try {
      const shadowRoot = host.attachShadow({ mode: 'open' });
      const links = ['\u0001javascript:alert(1)', ' java\tscript:alert(1)', 'VBScript:msgbox(1)'].map(href => {
        const link = host.ownerDocument.createElement('a');
        link.setAttribute('href', href);
        return link;
      });
      const safe = host.ownerDocument.createElement('a');
      safe.setAttribute('href', '/docs');
      shadowRoot.replaceChildren(...links, safe);

      const html = serializeServerHTMLRoot(shadowRoot, 'url-root');
      expect(html.toLowerCase()).not.toContain('script:');
      expect(html).toContain('href="/docs"');
    } finally {
      releaseServerDOMHost();
    }
  });

  it('refuses tag and attribute names that would break out of the markup', () => {
    const host = createServerDOMHost({ markMutation(): void {} });
    try {
      const shadowRoot = host.attachShadow({ mode: 'open' });
      const badTag = host.ownerDocument.createElement('script src="http://evil"');
      const badAttribute = host.ownerDocument.createElement('div');
      badAttribute.setAttribute('"><script>alert(1)</script>', 'x');
      badAttribute.setAttribute('title', 'kept');
      shadowRoot.replaceChildren(badTag, badAttribute);

      const html = serializeServerHTMLRoot(shadowRoot, 'name-root');
      expect(html).not.toContain('<script');
      expect(html).toContain('data-valdi-unsupported="script src=&quot;http://evil&quot;"');
      expect(html).toContain('title="kept"');
    } finally {
      releaseServerDOMHost();
    }
  });
});

describe('server CSS scoper', () => {
  const root = '[data-valdi-html-root="r"]';

  it('scopes :host forms onto the root', () => {
    expect(scopeServerCSS(':host { color: red; }', root)).toContain(`${root} {`);
    expect(scopeServerCSS(':host(.dark) { color: red; }', root)).toContain(`${root}.dark {`);
    expect(scopeServerCSS(':host(:not(.a)) .b { color: red; }', root)).toContain(`${root}:not(.a) .b {`);
    expect(scopeServerCSS(':host:hover { color: red; }', root)).toContain(`${root}:hover {`);
    expect(scopeServerCSS(':host .child { color: red; }', root)).toContain(`${root} .child {`);
  });

  it('scopes ordinary selectors as descendants of the root', () => {
    expect(scopeServerCSS('.a, .b { color: red; }', root)).toContain(`${root} .a, ${root} .b {`);
  });
});
