import 'jasmine';
import * as fs from 'node:fs';
import * as path from 'node:path';
import * as vm from 'node:vm';

interface PendingRequest {
  path: string;
  resolve: (value: unknown) => void;
}

interface BrowserHarness {
  context: Record<string, unknown>;
  pending: PendingRequest[];
  selectTarget: (target: Record<string, unknown>) => void;
}

const noop = (): void => {};

function target(port: number, clientId: string, contextId: string): Record<string, unknown> {
  return { clientId, contextId, port, proxyPort: port };
}

function handled(targetValue: Record<string, unknown>, data: Record<string, unknown>): Record<string, unknown> {
  return { data, handled: true, status: 'handled', target: targetValue };
}

function createHarness(): BrowserHarness {
  let selectedTarget = target(13_591, 'A', 'context-A');
  const pending: PendingRequest[] = [];
  const classList = { toggle: noop };
  const element = (): Record<string, unknown> => ({ classList, disabled: false, innerHTML: '', textContent: '' });
  const state = {
    attached: true,
    providers: {
      activeTab: 'storage',
      error: null,
      generation: 0,
      loading: false,
      registry: null,
      selectedDatabaseId: null,
      selectedSqlProviderId: null,
      selectedStorageProviderId: null,
      selectedTable: null,
      sql: null,
      sqlLimit: 50,
      sqlLoading: false,
      sqlOffset: 0,
      sqlTable: null,
      storage: null,
      targetKey: null,
    },
    settings: {
      error: null,
      generation: 0,
      loading: false,
      selectedGroupId: null,
      snapshot: null,
      targetKey: null,
    },
  };
  const context = vm.createContext({
    addLog: noop,
    apiGet: (requestPath: string): Promise<unknown> =>
      new Promise(resolve => pending.push({ path: requestPath, resolve })),
    apiPost: (): Promise<never> => Promise.reject(new Error('Unexpected provider request')),
    document: {
      querySelectorAll: (): unknown[] => [],
    },
    elements: {
      providerContent: element(),
      providerRefreshButton: element(),
      providerStatusPill: element(),
      settingsContent: element(),
      settingsGroupSelect: element(),
      settingsRefreshButton: element(),
      settingsStatusPill: element(),
    },
    escapeHtml: String,
    getSelectedTargetParams: (): Record<string, unknown> => selectedTarget,
    hasSelectedLiveTarget: (): boolean => true,
    requestDebuggerAction: noop,
    state,
  }) as Record<string, unknown>;
  const debuggerRoot = path.resolve(process.cwd(), 'debugger');
  vm.runInContext(fs.readFileSync(path.join(debuggerRoot, 'debugger-providers.js'), 'utf8'), context);
  vm.runInContext(fs.readFileSync(path.join(debuggerRoot, 'debugger-settings.js'), 'utf8'), context);
  return {
    context,
    pending,
    selectTarget: next => {
      selectedTarget = next;
    },
  };
}

function call<T>(context: Record<string, unknown>, name: string, ...args: unknown[]): T {
  return (context[name] as (...values: unknown[]) => T)(...args);
}

describe('debugger browser provider and settings tools', () => {
  it('discards delayed provider and settings responses from target A after selecting target B', async () => {
    const harness = createHarness();
    const targetA = target(13_591, 'A', 'context-A');
    const targetB = target(13_592, 'B', 'context-B');

    const providerA = call<Promise<void>>(harness.context, 'refreshDebuggerProviders', { silent: true });
    expect(harness.pending[0]?.path).toBe('/api/debugger/providers');
    harness.selectTarget(targetB);
    call<void>(harness.context, 'resetDebuggerToolsForTarget');
    const providerB = call<Promise<void>>(harness.context, 'refreshDebuggerProviders', { silent: true });
    harness.pending[1]?.resolve(handled(targetB, { providers: [], revision: 2 }));
    await providerB;
    harness.pending[0]?.resolve(handled(targetA, { providers: [{ id: 'stale' }], revision: 1 }));
    await providerA;

    const state = harness.context['state'] as { providers: { registry: { revision: number } } };
    expect(state.providers.registry.revision).toBe(2);
    expect(
      call<boolean>(harness.context, 'applyDebuggerProvidersResult', handled(targetA, { revision: 3 }), targetA),
    ).toBeFalse();

    const settingsA = call<Promise<void>>(harness.context, 'refreshDebugSettings', { silent: true });
    harness.selectTarget(targetA);
    call<void>(harness.context, 'resetDebuggerToolsForTarget');
    const settingsFromA = call<Promise<void>>(harness.context, 'refreshDebugSettings', { silent: true });
    harness.selectTarget(targetB);
    call<void>(harness.context, 'resetDebuggerToolsForTarget');
    const settingsFromB = call<Promise<void>>(harness.context, 'refreshDebugSettings', { silent: true });
    const settingsRequests = harness.pending.filter(item => item.path === '/api/debugger/settings');
    settingsRequests[2]?.resolve(handled(targetB, { groups: [], revision: 20 }));
    await settingsFromB;
    settingsRequests[1]?.resolve(handled(targetA, { groups: [], revision: 10 }));
    await settingsFromA;
    settingsRequests[0]?.resolve(handled(targetB, { groups: [], revision: 5 }));
    await settingsA;

    const settingsState = (harness.context['state'] as { settings: { snapshot: { revision: number } } }).settings;
    expect(settingsState.snapshot.revision).toBe(20);
    expect(
      call<boolean>(harness.context, 'applyDebugSettingsResult', handled(targetA, { revision: 30 }), targetA),
    ).toBeFalse();
  });

  it('preserves typed select identities and rejects invalid number input locally', () => {
    const harness = createHarness();
    const state = harness.context['state'] as {
      settings: { selectedGroupId: string; snapshot: Record<string, unknown> };
    };
    state.settings.selectedGroupId = 'general';
    state.settings.snapshot = {
      groups: [
        {
          id: 'general',
          label: 'General',
          settings: [
            {
              id: 'typed',
              kind: 'select',
              label: 'Typed',
              options: [
                { label: 'number', value: 1 },
                { label: 'string', value: '1' },
                { label: 'boolean', value: true },
              ],
              value: 1,
            },
            { id: 'amount', kind: 'number', label: 'Amount', value: 3 },
            { id: 'future', kind: 'future-kind', label: 'Future', value: 'x' },
          ],
        },
      ],
    };

    const readSelect = (value: string): { value: unknown } =>
      call(harness.context, 'readDebugSettingInput', {
        dataset: { settingId: 'typed', settingKind: 'select' },
        value,
      });
    expect(readSelect('0').value).toBe(1);
    expect(readSelect('1').value).toBe('1');
    expect(readSelect('2').value).toBeTrue();
    expect(
      call(harness.context, 'readDebugSettingInput', {
        dataset: { settingId: 'amount', settingKind: 'number' },
        value: ' ',
      }),
    ).toEqual(jasmine.objectContaining({ error: 'Enter a finite number.' }));
    expect(
      call(harness.context, 'readDebugSettingInput', {
        dataset: { settingId: 'amount', settingKind: 'number' },
        value: 'Infinity',
      }),
    ).toEqual(jasmine.objectContaining({ error: 'Enter a finite number.' }));
    expect(
      call<string>(harness.context, 'renderDebugSettingControl', {
        id: 'future',
        kind: 'future-kind',
        label: 'Future',
        value: 'x',
      }),
    ).toContain('Unsupported setting kind: future-kind');
  });

  it('shows truthful unavailable Storage and SQL surfaces without registered providers', () => {
    const harness = createHarness();
    const state = harness.context['state'] as {
      providers: { activeTab: string; registry: Record<string, unknown> };
    };
    state.providers.registry = { providers: [] };

    expect(call<string>(harness.context, 'renderStoragePanel')).toContain(
      'Storage support is not registered by this target runtime.',
    );
    state.providers.activeTab = 'sql';
    expect(call<string>(harness.context, 'renderSqlPanel')).toContain(
      'SQL support is not registered by this target runtime.',
    );
  });

  it('selects among multiple providers of the same kind', () => {
    const harness = createHarness();
    const state = harness.context['state'] as {
      providers: {
        registry: Record<string, unknown>;
        selectedStorageProviderId: string | null;
        storage: Record<string, unknown> | null;
      };
    };
    state.providers.registry = {
      providers: [
        { available: true, id: 'storage-a', kind: 'storage', label: 'Storage A' },
        { available: true, id: 'storage-b', kind: 'storage', label: 'Storage B' },
      ],
    };

    expect(call<boolean>(harness.context, 'selectDebuggerProvider', 'storage', 'storage-b')).toBeTrue();
    state.providers.storage = { stores: [] };

    expect(state.providers.selectedStorageProviderId).toBe('storage-b');
    expect(call<Record<string, unknown>>(harness.context, 'debuggerProviderForKind', 'storage')['id']).toBe(
      'storage-b',
    );
    expect(call<string>(harness.context, 'renderStoragePanel')).toContain('id="storageProviderSelect"');
    expect(call<string>(harness.context, 'renderStoragePanel')).toContain(
      '<option value="storage-b" selected>Storage B</option>',
    );
    expect(call<boolean>(harness.context, 'selectDebuggerProvider', 'storage', 'missing')).toBeFalse();
    expect(state.providers.selectedStorageProviderId).toBe('storage-b');
  });

  it('renders bounded PersistentStore diagnostics without losing numeric string encoding', () => {
    const harness = createHarness();
    const state = harness.context['state'] as {
      providers: { registry: Record<string, unknown>; storage: Record<string, unknown> };
    };
    state.providers.registry = {
      providers: [{ available: true, id: 'persistent-store', kind: 'storage', label: 'PersistentStore' }],
    };
    state.providers.storage = {
      storageError: 'Storage access was denied.',
      storageInspectionTruncated: true,
      stores: [
        {
          backend: 'memory',
          entries: [{ encoding: 0, key: 'theme', value: 'dark' }],
          inspectionTruncated: true,
          name: 'preferences',
        },
      ],
    };

    const html = call<string>(harness.context, 'renderStoragePanel');
    expect(html).toContain('Storage access was denied.');
    expect(html).toContain('Persistent browser-storage discovery was truncated');
    expect(html).toContain('memory · 1 entry');
    expect(html).toContain('0</span>');
    expect(html).toContain('Entry inspection stopped at the debugger scan limit.');
    expect(html).not.toContain('unknown · 1 entry');
  });

  it('surfaces provider projection omissions globally and on the affected store', () => {
    const harness = createHarness();
    const state = harness.context['state'] as {
      providers: { registry: Record<string, unknown>; storage: Record<string, unknown> };
    };
    state.providers.registry = {
      providers: [{ available: true, id: 'persistent-store', kind: 'storage', label: 'PersistentStore' }],
    };
    state.providers.storage = {
      projection: {
        entriesOmitted: 60,
        invalidFields: 0,
        sourceEntries: 100,
        sourceStores: 2,
        storesOmitted: 1,
        truncated: true,
        truncatedFields: 1,
      },
      stores: [
        {
          backend: 'memory',
          entries: [{ encoding: 0, key: 'first', value: 'value' }],
          entriesTruncated: true,
          name: 'bounded',
        },
      ],
    };

    const html = call<string>(harness.context, 'renderStoragePanel');
    expect(html).toContain('Storage transport projection was truncated');
    expect(html).toContain('1 store omitted');
    expect(html).toContain('60 entries omitted');
    expect(html).toContain('1 field truncated');
    expect(html).toContain('Additional entries were omitted by the debugger snapshot budget.');
  });
});
