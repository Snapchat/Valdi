import 'jasmine';
import {
  ChromiumConsoleLevel,
  ChromiumConsoleSource,
  MAX_CHROMIUM_CONSOLE_MESSAGE_LENGTH,
  formatChromiumConsoleEvent,
} from './chromiumConsole';

const SYNTHETIC_SLACK_BOT_TOKEN = ['xoxb', 'synthetic', 'bot', 'token'].join('-');
const SYNTHETIC_SLACK_USER_TOKEN = ['xoxp', 'synthetic', 'user', 'token'].join('-');

describe('Chromium DevTools console event formatting', () => {
  it('preserves console levels, primitive arguments, substitutions, and timestamps', () => {
    const entry = formatChromiumConsoleEvent({
      method: 'Runtime.consoleAPICalled',
      params: {
        args: [
          { type: 'string', value: '%cCount: %d (%s)' },
          { type: 'string', value: 'color: red' },
          { type: 'number', value: 7.9 },
          { type: 'string', value: 'ready' },
        ],
        timestamp: 1234,
        type: 'warning',
      },
    });

    expect(entry).toEqual({
      level: ChromiumConsoleLevel.Warning,
      message: 'Count: 7 (ready)',
      source: ChromiumConsoleSource.Console,
      timestamp: 1234,
    });
  });

  it('renders bounded object and array previews while redacting sensitive property values', () => {
    const entry = formatChromiumConsoleEvent({
      method: 'Runtime.consoleAPICalled',
      params: {
        args: [
          {
            preview: {
              properties: [
                { name: 'screen', type: 'string', value: 'showcase' },
                { name: 'authorization', type: 'string', value: 'Bearer private-token' },
                { name: 'accessToken', type: 'string', value: 'private-token' },
              ],
            },
            type: 'object',
          },
          {
            preview: {
              properties: [
                { name: '0', type: 'string', value: 'first' },
                { name: '1', type: 'string', value: 'second' },
              ],
              subtype: 'array',
            },
            subtype: 'array',
            type: 'object',
          },
        ],
        type: 'info',
      },
    });

    expect(entry?.message).toBe(
      '{screen: showcase, authorization: [REDACTED], accessToken: [REDACTED]} [first, second]',
    );
    expect(entry?.message).not.toContain('private-token');
  });

  it('redacts headers, credentials, query parameters, and common API keys', () => {
    const standaloneSecrets = [
      'eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJzeW50aGV0aWMifQ.c3ludGhldGljLXNpZ25hdHVyZQ',
      'AKIAIOSFODNN7EXAMPLE',
      'ghp_abcdefghijklmnopqrstuvwxyz0123456789',
      'github_pat_11AA0_synthetic_token_material_1234567890',
      `AIza${'A'.repeat(35)}`,
      SYNTHETIC_SLACK_BOT_TOKEN,
    ];
    const entry = formatChromiumConsoleEvent({
      method: 'Log.entryAdded',
      params: {
        entry: {
          level: 'warning',
          text:
            'authorization: Bearer synthetic-token\n' +
            'Cookie: session=private-cookie\n' +
            'password: correct horse battery staple\n' +
            '{"access_token":"private-access-token","password":"private-password"}\n' +
            'https://example.test/callback?code=private-code&safe=ok\n' +
            `sk-proj-abcdefghijklmnopqrst\n${standaloneSecrets.join('\n')}`,
          timestamp: 42,
        },
      },
    });

    expect(entry?.message).toContain('authorization: [REDACTED]');
    expect(entry?.message).toContain('Cookie: [REDACTED]');
    expect(entry?.message).toContain('password: [REDACTED]\n');
    expect(entry?.message).toContain('"access_token":[REDACTED]');
    expect(entry?.message).toContain('?code=[REDACTED]&safe=ok');
    expect(entry?.message).not.toContain('private-');
    expect(entry?.message).not.toContain('correct horse battery staple');
    expect(entry?.message).not.toContain('abcdefghijklmnopqrst');
    standaloneSecrets.forEach(secret => expect(entry?.message).not.toContain(secret));
  });

  it('redacts standalone credentials in formatted runtime arguments', () => {
    const entry = formatChromiumConsoleEvent({
      method: 'Runtime.consoleAPICalled',
      params: {
        args: [
          {
            type: 'string',
            value: 'eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJzeW50aGV0aWMifQ.c3ludGhldGljLXNpZ25hdHVyZQ',
          },
          { type: 'string', value: SYNTHETIC_SLACK_USER_TOKEN },
        ],
        type: 'log',
      },
    });

    expect(entry?.message).toBe('[REDACTED] [REDACTED]');
  });

  it('preserves valid zero-argument and empty-string log events', () => {
    const zeroArguments = formatChromiumConsoleEvent({
      method: 'Runtime.consoleAPICalled',
      params: { args: [], type: 'log' },
    });
    const emptyArgument = formatChromiumConsoleEvent({
      method: 'Runtime.consoleAPICalled',
      params: { args: [{ type: 'string', value: '' }], type: 'log' },
    });
    const emptyBrowserEntry = formatChromiumConsoleEvent({
      method: 'Log.entryAdded',
      params: { entry: { level: 'info', text: '' } },
    });

    expect(zeroArguments).toEqual(jasmine.objectContaining({ message: '', source: ChromiumConsoleSource.Console }));
    expect(emptyArgument).toEqual(jasmine.objectContaining({ message: '', source: ChromiumConsoleSource.Console }));
    expect(emptyBrowserEntry).toEqual(jasmine.objectContaining({ message: '', source: ChromiumConsoleSource.Browser }));
  });

  it('surfaces runtime exceptions and bounds oversized messages', () => {
    const exception = formatChromiumConsoleEvent({
      method: 'Runtime.exceptionThrown',
      params: {
        exceptionDetails: {
          exception: { description: 'Error: Synthetic failure\n    at render (showcase.js:4:2)' },
          text: 'Uncaught',
        },
        timestamp: 84,
      },
    });
    const oversized = formatChromiumConsoleEvent({
      method: 'Runtime.consoleAPICalled',
      params: {
        args: [
          {
            type: 'string',
            value: `${'x'.repeat(32_760)} sk-proj-abcdefghijklmnopqrst ${'y'.repeat(100_000)}`,
          },
        ],
        type: 'log',
      },
    });

    expect(exception).toEqual({
      level: ChromiumConsoleLevel.Error,
      message: 'Error: Synthetic failure\n    at render (showcase.js:4:2)',
      source: ChromiumConsoleSource.Exception,
      timestamp: 84,
    });
    expect(oversized?.message.length).toBe(MAX_CHROMIUM_CONSOLE_MESSAGE_LENGTH + 1);
    expect(oversized?.message.endsWith('…')).toBeTrue();
    expect(oversized?.message).not.toContain('abcdefghijklmnopqrst');
  });

  it('does not invoke accessors or recurse through deep and proxy-like remote values', () => {
    let getterCalls = 0;
    const remoteObject: Record<string, unknown> = { type: 'object', value: {} };
    let deep = remoteObject['value'] as Record<string, unknown>;
    for (let index = 0; index < 20_000; index += 1) {
      const next: Record<string, unknown> = {};
      deep['next'] = next;
      deep = next;
    }
    Object.defineProperty(remoteObject, 'description', {
      enumerable: true,
      get: () => {
        getterCalls += 1;
        throw new Error('must not run');
      },
    });
    const revoked = Proxy.revocable({ type: 'object', description: 'private-value' }, {});
    revoked.revoke();

    const startedAt = performance.now();
    const entry = formatChromiumConsoleEvent({
      method: 'Runtime.consoleAPICalled',
      params: { args: [remoteObject, revoked.proxy], type: 'debug' },
    });

    expect(performance.now() - startedAt).toBeLessThan(1000);
    expect(getterCalls).toBe(0);
    expect(entry?.message).toBe('object [Unavailable]');
    expect(entry?.message).not.toContain('private-value');
  });

  it('bounds sparse argument arrays and ignores malformed or unrelated events', () => {
    const sparse: unknown[] = [];
    sparse.length = 10_000_000;
    sparse[0] = { type: 'string', value: 'first' };

    const entry = formatChromiumConsoleEvent({
      method: 'Runtime.consoleAPICalled',
      params: { args: sparse, type: 'log' },
    });

    expect(entry?.message.length).toBeLessThan(2000);
    expect(entry?.message.startsWith('first')).toBeTrue();
    expect(formatChromiumConsoleEvent({ method: 'Network.requestWillBeSent', params: {} })).toBeNull();
    expect(formatChromiumConsoleEvent({ method: 'Runtime.consoleAPICalled', params: {} })).toBeNull();
    expect(formatChromiumConsoleEvent({ method: 'Log.entryAdded', params: { entry: 'invalid' } })).toBeNull();
    expect(formatChromiumConsoleEvent({ method: 'Runtime.exceptionThrown', params: {} })).toBeNull();
  });
});
