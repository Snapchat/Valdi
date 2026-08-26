import 'jasmine';
import fs from 'node:fs';
import path from 'node:path';
import { Script } from 'node:vm';

interface StubElement {
  checked: boolean;
  className: string;
  innerHTML: string;
  scrollHeight: number;
  scrollTop: number;
  textContent: string;
  addEventListener(type: string, listener: (event: unknown) => void): void;
  dispatch(type: string): void;
}

interface MockConsoleEventSource {
  closed: boolean;
  url: string;
  addEventListener(type: string, listener: (event: { data: string }) => void): void;
  close(): void;
  emit(type: string, payload: Record<string, unknown>): void;
}

interface DevToolsConsolePanel {
  consoleMessages: StubElement;
  liveToggle: StubElement;
  state: {
    autoRefresh: boolean;
    consoleEntries: Array<{ kind: string; value: string }>;
    consoleStream: MockConsoleEventSource | null;
    target: { id: string; sessionId: string } | null;
  };
  evaluateConsoleExpression(expression: string): Promise<void>;
  startConsoleStream(): void;
  stopConsoleStream(): void;
  dispatchWindowEvent(type: string): void;
}

describe('integrated DevTools console panel', () => {
  let eventSources: MockConsoleEventSource[];
  let panel: DevToolsConsolePanel;
  let requests: Array<{ body?: string; url: string }>;

  beforeEach(() => {
    eventSources = [];
    requests = [];
    const rawSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'devtools-panel.js'), 'utf8');
    const source = rawSource.replace('void connectToInspectedApplication();', 'void 0;');
    const elements = new Map<string, StubElement>();
    const windowListeners = new Map<string, () => void>();
    const document = {
      addEventListener() {},
      documentElement: { dataset: {} },
      getElementById(id: string): StubElement {
        let element = elements.get(id);
        if (element === undefined) {
          const listeners = new Map<string, (event: unknown) => void>();
          element = {
            checked: true,
            className: '',
            innerHTML: '',
            scrollHeight: 100,
            scrollTop: 0,
            textContent: '',
            addEventListener(type: string, listener: (event: unknown) => void): void {
              listeners.set(type, listener);
            },
            dispatch(type: string): void {
              listeners.get(type)?.({});
            },
          };
          elements.set(id, element);
        }
        return element;
      },
      querySelectorAll(): StubElement[] {
        return [];
      },
    };
    const window = {
      addEventListener(type: string, listener: () => void): void {
        windowListeners.set(type, listener);
      },
      location: {
        origin: 'http://127.0.0.1:18768',
        search:
          '?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Findex.html%3FvaldiDevTools%3D1&targetNonce=panel-target-nonce-123456',
      },
      parent: {},
    };

    class MockEventSource implements MockConsoleEventSource {
      closed = false;
      private readonly listeners = new Map<string, (event: { data: string }) => void>();

      constructor(readonly url: string) {
        eventSources.push(this);
      }

      addEventListener(type: string, listener: (event: { data: string }) => void): void {
        this.listeners.set(type, listener);
      }

      close(): void {
        this.closed = true;
      }

      emit(type: string, payload: Record<string, unknown>): void {
        this.listeners.get(type)?.({ data: JSON.stringify(payload) });
      }
    }

    panel = new Script(
      `${source}\n({ consoleMessages: elements.consoleMessages, dispatchWindowEvent: type => windowListeners.get(type)?.(), evaluateConsoleExpression, liveToggle: elements.autoRefreshToggle, startConsoleStream, state, stopConsoleStream })`,
    ).runInNewContext({
      EventSource: MockEventSource,
      URL,
      URLSearchParams,
      console,
      document,
      elements,
      fetch: (url: URL, options: { body?: string }) => {
        requests.push({ ...(options.body === undefined ? {} : { body: options.body }), url: url.toString() });
        return Promise.resolve({
          json: () => Promise.resolve({ type: 'number', value: 4 }),
          ok: true,
        });
      },
      window,
      windowListeners,
    }) as DevToolsConsolePanel;
    panel.state.target = { id: 'owl:web-preview', sessionId: 'web-preview' };
  });

  it('binds the stream to the exact target tuple and safely renders only matching events', () => {
    panel.startConsoleStream();
    const stream = eventSources[0];
    if (!stream) throw new Error('Expected the Chromium console stream to connect.');
    const streamUrl = new URL(stream.url);

    expect(streamUrl.pathname).toBe('/api/devtools/console/stream');
    expect(streamUrl.searchParams.get('sessionId')).toBe('web-preview');
    expect(streamUrl.searchParams.get('targetNonce')).toBe('panel-target-nonce-123456');
    expect(streamUrl.searchParams.get('inspectedUrl')).toBe('http://127.0.0.1:54321/index.html?valdiDevTools=1');

    stream.emit('console', {
      level: 'warn',
      message: 'Synthetic <renderer> warning',
      sessionId: 'web-preview',
      source: 'console',
      targetId: 'owl:web-preview',
      timestamp: 42,
    });
    stream.emit('console', {
      level: 'warn',
      message: 'Synthetic <renderer> warning',
      sessionId: 'web-preview',
      source: 'console',
      targetId: 'owl:web-preview',
      timestamp: 42,
    });
    stream.emit('console', {
      level: 'error',
      message: 'Wrong target',
      sessionId: 'another-session',
      targetId: 'owl:web-preview',
      timestamp: 43,
    });

    expect(panel.state.consoleEntries).toEqual([
      jasmine.objectContaining({ kind: 'warn', value: 'Synthetic <renderer> warning' }),
    ]);
    expect(panel.consoleMessages.innerHTML).toContain('class="console-entry warn"');
    expect(panel.consoleMessages.innerHTML).toContain('Synthetic &lt;renderer&gt; warning');
    expect(panel.consoleMessages.innerHTML).not.toContain('<renderer>');
    expect(panel.consoleMessages.innerHTML).not.toContain('Wrong target');
  });

  it('isolates reconnects, honors the Live toggle, and tears down on pagehide', () => {
    panel.startConsoleStream();
    const first = eventSources[0];
    if (!first) throw new Error('Expected the first Chromium console stream.');

    panel.state.target = { id: 'owl:replacement', sessionId: 'replacement' };
    panel.startConsoleStream();
    const second = eventSources[1];
    if (!second) throw new Error('Expected the replacement Chromium console stream.');
    first.emit('console', {
      level: 'error',
      message: 'Stale target',
      sessionId: 'web-preview',
      targetId: 'owl:web-preview',
      timestamp: 1,
    });
    second.emit('console', {
      level: 'info',
      message: 'Replacement target',
      sessionId: 'replacement',
      targetId: 'owl:replacement',
      timestamp: 2,
    });

    expect(first.closed).toBeTrue();
    expect(panel.state.consoleEntries).toEqual([
      jasmine.objectContaining({ kind: 'info', value: 'Replacement target' }),
    ]);

    panel.liveToggle.checked = false;
    panel.liveToggle.dispatch('change');
    expect(second.closed).toBeTrue();
    expect(panel.state.consoleStream).toBeNull();

    panel.liveToggle.checked = true;
    panel.liveToggle.dispatch('change');
    const resumed = eventSources[2];
    if (!resumed) throw new Error('Expected a resumed Chromium console stream.');
    panel.dispatchWindowEvent('pagehide');
    expect(resumed.closed).toBeTrue();
    expect(panel.state.consoleStream).toBeNull();
  });

  it('keeps manual evaluation bound to the same target tuple while streaming', async () => {
    panel.startConsoleStream();
    await panel.evaluateConsoleExpression('2 + 2');

    expect(requests).toContain(
      jasmine.objectContaining({
        body: JSON.stringify({
          expression: '2 + 2',
          inspectedUrl: 'http://127.0.0.1:54321/index.html?valdiDevTools=1',
          sessionId: 'web-preview',
          targetNonce: 'panel-target-nonce-123456',
        }),
        url: 'http://127.0.0.1:18768/api/devtools/evaluate',
      }),
    );
    expect(panel.state.consoleEntries).toEqual([
      jasmine.objectContaining({ kind: 'input', value: '2 + 2' }),
      jasmine.objectContaining({ kind: 'result', value: '4' }),
    ]);
  });
});
