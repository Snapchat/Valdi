import fs from 'node:fs';
import path from 'node:path';
import { CliError } from '../core/errors';
import { isLoopbackHost } from './loopbackHost';
import { OWL_DEVTOOLS_TARGET_NONCE_PROPERTY } from './owlCdpClient';

const DEVTOOLS_PAGE = `<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <script src="./devtools.js"></script>
  </head>
</html>
`;

const DEVTOOLS_SCRIPT = `chrome.devtools.panels.create('Valdi', '', 'panel.html');
`;

const PANEL_PAGE = `<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>Valdi Debugger</title>
    <link rel="stylesheet" href="./panel.css" />
  </head>
  <body>
    <iframe id="valdi-debugger" title="Valdi Debugger" referrerpolicy="no-referrer"></iframe>
    <script src="./panel.js"></script>
  </body>
</html>
`;

const PANEL_STYLES = `html,
body,
#valdi-debugger {
  width: 100%;
  height: 100%;
  margin: 0;
  border: 0;
}

body {
  overflow: hidden;
}
`;

function resolveDebuggerUrl(value: string): URL {
  let url: URL;
  try {
    url = new URL(value);
  } catch {
    throw new CliError(`Invalid Valdi debugger URL: ${value}`);
  }

  if (url.protocol !== 'http:' || !isLoopbackHost(url.hostname) || url.username !== '' || url.password !== '') {
    throw new CliError(`The Valdi DevTools panel requires a loopback HTTP debugger URL: ${value}`);
  }

  return url;
}

export function writeOwlDevToolsExtension(extensionDirectory: string, debuggerUrl: string): void {
  const url = resolveDebuggerUrl(debuggerUrl);
  fs.mkdirSync(extensionDirectory, { recursive: true });
  fs.writeFileSync(
    path.join(extensionDirectory, 'manifest.json'),
    `${JSON.stringify(
      {
        manifest_version: 3,
        name: 'Valdi DevTools',
        version: '1.0.0',
        description: 'Inspect the running Valdi application inside Chromium DevTools.',
        devtools_page: 'devtools.html',
        host_permissions: [`${url.origin}/*`],
        content_security_policy: {
          extension_pages: `script-src 'self'; object-src 'self'; frame-src ${url.origin}`,
        },
      },
      undefined,
      2,
    )}\n`,
  );
  fs.writeFileSync(path.join(extensionDirectory, 'devtools.html'), DEVTOOLS_PAGE);
  fs.writeFileSync(path.join(extensionDirectory, 'devtools.js'), DEVTOOLS_SCRIPT);
  fs.writeFileSync(path.join(extensionDirectory, 'panel.html'), PANEL_PAGE);
  fs.writeFileSync(
    path.join(extensionDirectory, 'panel.js'),
    `const debuggerUrl = new URL(${JSON.stringify(url.toString())});
debuggerUrl.pathname = '/devtools-panel.html';
debuggerUrl.searchParams.set('theme', chrome.devtools.panels.themeName || 'default');
const inspectedTargetMarker = ${JSON.stringify(OWL_DEVTOOLS_TARGET_NONCE_PROPERTY)};
const inspectedTargetNonce = crypto.randomUUID();
let inspectedTargetGeneration = 0;

function inspectedTargetExpression(remove) {
  const marker = JSON.stringify(inspectedTargetMarker);
  const nonce = JSON.stringify(inspectedTargetNonce);
  if (remove) {
    return '(() => { const marker = ' + marker + '; const nonce = ' + nonce + '; if (globalThis[marker] === nonce) { try { globalThis.__VALDI_WEB_DEBUGGER__?.clearHighlight?.(); } finally { delete globalThis[marker]; } } return true; })()';
  }
  return '(() => { const marker = ' + marker + '; const nonce = ' + nonce + '; Object.defineProperty(globalThis, marker, { configurable: true, value: nonce }); return { nonce, url: String(globalThis.location.href) }; })()';
}

function updateInspectedTarget() {
  const generation = ++inspectedTargetGeneration;
  chrome.devtools.inspectedWindow.eval(inspectedTargetExpression(false), (identity, error) => {
    if (generation !== inspectedTargetGeneration) return;
    if (
      error ||
      !identity ||
      identity.nonce !== inspectedTargetNonce ||
      typeof identity.url !== 'string'
    ) {
      console.error('Unable to resolve the inspected Valdi Owl application.', error);
      return;
    }
    debuggerUrl.searchParams.set('inspectedUrl', identity.url);
    debuggerUrl.searchParams.set('targetNonce', inspectedTargetNonce);
    document.getElementById('valdi-debugger').src = debuggerUrl.toString();
  });
}

function clearInspectedTarget() {
  chrome.devtools.inspectedWindow.eval(inspectedTargetExpression(true), (_result, error) => {
    if (error) console.warn('Unable to clear the inspected Valdi Owl marker.', error);
  });
}

function teardownInspectedTarget() {
  inspectedTargetGeneration++;
  if (chrome.devtools.network && chrome.devtools.network.onNavigated) {
    chrome.devtools.network.onNavigated.removeListener(updateInspectedTarget);
  }
  clearInspectedTarget();
}

if (chrome.devtools.network && chrome.devtools.network.onNavigated) {
  chrome.devtools.network.onNavigated.addListener(updateInspectedTarget);
}
window.addEventListener('unload', teardownInspectedTarget);
updateInspectedTarget();

if (chrome.devtools.panels.onThemeChanged) {
  chrome.devtools.panels.onThemeChanged.addListener(theme => {
    const frame = document.getElementById('valdi-debugger');
    if (frame.contentWindow) {
      frame.contentWindow.postMessage({ channel: 'valdi-devtools-theme', theme }, debuggerUrl.origin);
    }
  });
}
`,
  );
  fs.writeFileSync(path.join(extensionDirectory, 'panel.css'), PANEL_STYLES);
}
