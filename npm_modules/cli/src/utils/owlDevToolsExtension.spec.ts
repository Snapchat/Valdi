import 'jasmine';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { Script } from 'node:vm';
import { isLoopbackHost } from './loopbackHost';
import { OWL_DEVTOOLS_TARGET_NONCE_PROPERTY } from './owlCdpClient';
import { writeOwlDevToolsExtension } from './owlDevToolsExtension';

function invokeRequiredCallback(callback: (() => void) | null, message: string): void {
  if (!callback) throw new Error(message);
  callback();
}

describe('owlDevToolsExtension', () => {
  let tempDir: string;

  beforeEach(() => {
    tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'valdi-owl-devtools-test-'));
  });

  afterEach(() => {
    fs.rmSync(tempDir, { force: true, recursive: true });
  });

  it('writes a first-party Chromium panel embedding only the loopback debugger origin', () => {
    const extensionDirectory = path.join(tempDir, 'extension');

    writeOwlDevToolsExtension(extensionDirectory, 'http://127.0.0.1:8765/');

    const manifest = JSON.parse(fs.readFileSync(path.join(extensionDirectory, 'manifest.json'), 'utf8')) as {
      content_security_policy: { extension_pages: string };
      devtools_page: string;
      host_permissions: string[];
      manifest_version: number;
      name: string;
    };

    expect(manifest.manifest_version).toBe(3);
    expect(manifest.name).toBe('Valdi DevTools');
    expect(manifest.devtools_page).toBe('devtools.html');
    expect(manifest.host_permissions).toEqual(['http://127.0.0.1:8765/*']);
    expect(manifest.content_security_policy.extension_pages).toContain('frame-src http://127.0.0.1:8765');
    expect(fs.readFileSync(path.join(extensionDirectory, 'devtools.js'), 'utf8')).toContain(
      "chrome.devtools.panels.create('Valdi', '', 'panel.html')",
    );
    expect(fs.readFileSync(path.join(extensionDirectory, 'panel.html'), 'utf8')).toContain('id="valdi-debugger"');
    const panelSource = fs.readFileSync(path.join(extensionDirectory, 'panel.js'), 'utf8');
    expect(panelSource).toContain('"http://127.0.0.1:8765/"');
    expect(panelSource).toContain("debuggerUrl.pathname = '/devtools-panel.html'");
    expect(panelSource).toContain('const inspectedTargetNonce = crypto.randomUUID()');
    expect(panelSource).toContain("debuggerUrl.searchParams.set('inspectedUrl', identity.url)");
    expect(panelSource).toContain("debuggerUrl.searchParams.set('targetNonce', inspectedTargetNonce)");
    expect(panelSource).toContain('const generation = ++inspectedTargetGeneration');
    expect(panelSource).toContain('globalThis.__VALDI_WEB_DEBUGGER__?.clearHighlight?.()');
    expect(panelSource).toContain("window.addEventListener('unload', teardownInspectedTarget)");
    expect(panelSource).toContain('chrome.devtools.panels.onThemeChanged.addListener');
    expect(() => new Script(fs.readFileSync(path.join(extensionDirectory, 'devtools.js'), 'utf8'))).not.toThrow();
    expect(() => new Script(panelSource)).not.toThrow();
  });

  it('accepts a loopback IPv6 debugger without broadening extension permissions', () => {
    const extensionDirectory = path.join(tempDir, 'extension');

    writeOwlDevToolsExtension(extensionDirectory, 'http://[::1]:8765/');

    const manifest = JSON.parse(fs.readFileSync(path.join(extensionDirectory, 'manifest.json'), 'utf8')) as {
      host_permissions: string[];
    };
    expect(manifest.host_permissions).toEqual(['http://[::1]:8765/*']);
  });

  it('uses the shared full IPv4 loopback policy without accepting adjacent addresses', () => {
    const extensionDirectory = path.join(tempDir, 'extension');

    expect(isLoopbackHost('127.0.0.2')).toBeTrue();
    expect(isLoopbackHost('127.255.255.254')).toBeTrue();
    expect(isLoopbackHost('128.0.0.1')).toBeFalse();
    expect(isLoopbackHost('127.0.0.1.example.com')).toBeFalse();
    writeOwlDevToolsExtension(extensionDirectory, 'http://127.0.0.2:8765/');

    const manifest = JSON.parse(fs.readFileSync(path.join(extensionDirectory, 'manifest.json'), 'utf8')) as {
      host_permissions: string[];
    };
    expect(manifest.host_permissions).toEqual(['http://127.0.0.2:8765/*']);
    expect(() => writeOwlDevToolsExtension(path.join(tempDir, 'adjacent'), 'http://128.0.0.1:8765/')).toThrowError(
      /loopback HTTP debugger URL/,
    );
  });

  it('marks the exact inspected tab, refreshes the marker after navigation, and cleans it on teardown', () => {
    const extensionDirectory = path.join(tempDir, 'extension');
    writeOwlDevToolsExtension(extensionDirectory, 'http://127.0.0.1:8765/');
    const panelSource = fs.readFileSync(path.join(extensionDirectory, 'panel.js'), 'utf8');
    const nonce = 'extension-tab-nonce-123456';
    const frame = { contentWindow: null, src: '' };
    const firstPage: Record<string, unknown> = {
      location: { href: 'http://127.0.0.1:54321/index.html?valdiDebugger=1' },
    };
    let clearedHighlights = 0;
    const secondPage: Record<string, unknown> = {
      __VALDI_WEB_DEBUGGER__: { clearHighlight: () => (clearedHighlights += 1) },
      location: { href: 'http://127.0.0.1:54321/after-navigation.html?valdiDebugger=1' },
    };
    let inspectedPage = firstPage;
    let navigationListener: (() => void) | null = null;
    let unloadListener: (() => void) | null = null;
    let removedNavigationListener: (() => void) | null = null;

    new Script(panelSource).runInNewContext({
      URL,
      chrome: {
        devtools: {
          inspectedWindow: {
            eval: (expression: string, callback: (result: unknown, error?: unknown) => void) => {
              callback(new Script(expression).runInNewContext(inspectedPage));
            },
          },
          network: {
            onNavigated: {
              addListener: (listener: () => void) => {
                navigationListener = listener;
              },
              removeListener: (listener: () => void) => {
                removedNavigationListener = listener;
              },
            },
          },
          panels: {
            onThemeChanged: { addListener: () => {} },
            themeName: 'dark',
          },
        },
      },
      console,
      crypto: { randomUUID: () => nonce },
      document: { getElementById: () => frame },
      window: {
        addEventListener: (event: string, listener: () => void) => {
          if (event === 'unload') unloadListener = listener;
        },
      },
    });

    expect(firstPage[OWL_DEVTOOLS_TARGET_NONCE_PROPERTY]).toBe(nonce);
    expect(new URL(frame.src).searchParams.get('targetNonce')).toBe(nonce);
    expect(new URL(frame.src).searchParams.get('inspectedUrl')).toBe(
      'http://127.0.0.1:54321/index.html?valdiDebugger=1',
    );

    inspectedPage = secondPage;
    navigationListener!();
    expect(secondPage[OWL_DEVTOOLS_TARGET_NONCE_PROPERTY]).toBe(nonce);
    expect(new URL(frame.src).searchParams.get('inspectedUrl')).toBe(
      'http://127.0.0.1:54321/after-navigation.html?valdiDebugger=1',
    );

    unloadListener!();
    expect(secondPage[OWL_DEVTOOLS_TARGET_NONCE_PROPERTY]).toBeUndefined();
    expect(clearedHighlights).toBe(1);
    expect(removedNavigationListener).toBe(navigationListener);
  });

  it('does not let an older navigation callback overwrite the current inspected frame', () => {
    const extensionDirectory = path.join(tempDir, 'extension');
    writeOwlDevToolsExtension(extensionDirectory, 'http://127.0.0.1:8765/');
    const panelSource = fs.readFileSync(path.join(extensionDirectory, 'panel.js'), 'utf8');
    const frame = { contentWindow: null, src: '' };
    const firstPage: Record<string, unknown> = { location: { href: 'http://127.0.0.1:54321/first.html' } };
    const secondPage: Record<string, unknown> = { location: { href: 'http://127.0.0.1:54321/second.html' } };
    let inspectedPage = firstPage;
    let navigationListener: (() => void) | null = null;
    const evaluations: Array<{
      callback: (result: unknown, error?: unknown) => void;
      expression: string;
      page: Record<string, unknown>;
    }> = [];

    new Script(panelSource).runInNewContext({
      URL,
      chrome: {
        devtools: {
          inspectedWindow: {
            eval: (expression: string, callback: (result: unknown, error?: unknown) => void) => {
              evaluations.push({ callback, expression, page: inspectedPage });
            },
          },
          network: {
            onNavigated: {
              addListener: (listener: () => void) => {
                navigationListener = listener;
              },
              removeListener: () => {},
            },
          },
          panels: {
            onThemeChanged: { addListener: () => {} },
            themeName: 'dark',
          },
        },
      },
      console,
      crypto: { randomUUID: () => 'extension-tab-nonce-123456' },
      document: { getElementById: () => frame },
      window: { addEventListener: () => {} },
    });

    inspectedPage = secondPage;
    invokeRequiredCallback(navigationListener, 'Expected a navigation listener.');
    expect(evaluations).toHaveSize(2);
    const newer = evaluations[1];
    if (!newer) throw new Error('Expected a current navigation evaluation.');
    newer.callback(new Script(newer.expression).runInNewContext(newer.page));
    expect(new URL(frame.src).searchParams.get('inspectedUrl')).toBe('http://127.0.0.1:54321/second.html');

    const older = evaluations[0];
    if (!older) throw new Error('Expected an older navigation evaluation.');
    older.callback(new Script(older.expression).runInNewContext(older.page));
    expect(new URL(frame.src).searchParams.get('inspectedUrl')).toBe('http://127.0.0.1:54321/second.html');
  });

  it('rejects remote, credentialed, and malformed debugger URLs', () => {
    expect(() => writeOwlDevToolsExtension(path.join(tempDir, 'remote'), 'https://example.com/debugger')).toThrowError(
      /loopback HTTP debugger URL/,
    );
    expect(() =>
      writeOwlDevToolsExtension(path.join(tempDir, 'credentialed'), 'http://person:secret@127.0.0.1:8765/'),
    ).toThrowError(/loopback HTTP debugger URL/);
    expect(() => writeOwlDevToolsExtension(path.join(tempDir, 'malformed'), 'not-a-url')).toThrowError(
      /Invalid Valdi debugger URL/,
    );
  });
});
