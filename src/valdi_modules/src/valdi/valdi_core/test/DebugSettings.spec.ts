import 'jasmine/src/jasmine';
import { jsx } from '../src/JSXBootstrap';
import type { ValdiRuntime } from '../src/ValdiRuntime';
import { DebugSettingKind, registerDebugSettingsGroup } from '../src/debugging/DebugSettings';
import type { CustomMessageHandler } from '../src/debugging/CustomMessageHandler';
import type {
  DebugSettingsGroup,
  DebugSettingsRegistration,
  DebugSettingsSnapshot,
} from '../src/debugging/DebugSettings';

declare const runtime: ValdiRuntime;

interface DebugSettingsTestGlobal {
  __VALDI_DEBUG_SETTINGS__?: {
    getSnapshot(): DebugSettingsSnapshot;
    resetValue(groupId: string, settingId: string): DebugSettingsSnapshot;
    setValue(groupId: string, settingId: string, value: unknown): DebugSettingsSnapshot;
  };
}

describe('DebugSettings', () => {
  let originalDebugEnabled: boolean;
  let registrations: DebugSettingsRegistration[];

  beforeEach(() => {
    originalDebugEnabled = runtime.isDebugEnabled;
    runtime.isDebugEnabled = true;
    registrations = [];
  });

  afterEach(() => {
    registrations.forEach(registration => registration.dispose());
    registrations = [];
    runtime.isDebugEnabled = originalDebugEnabled;
  });

  it('publishes typed groups and applies changes through the application setter', () => {
    let mode = 'system';
    registrations.push(
      registerDebugSettingsGroup({
        categoryId: 'appearance',
        id: 'appearance',
        label: 'Appearance',
        settings: [
          {
            defaultValue: 'system',
            get: () => mode,
            id: 'theme',
            kind: DebugSettingKind.Select,
            label: 'Theme',
            options: [
              { label: 'System', value: 'system' },
              { label: 'Dark', value: 'dark' },
            ],
            set: value => {
              mode = value as string;
            },
          },
        ],
      }),
    );

    const bridge = (globalThis as unknown as DebugSettingsTestGlobal).__VALDI_DEBUG_SETTINGS__;
    expect(bridge?.getSnapshot().groups).toEqual([
      jasmine.objectContaining({
        id: 'appearance',
        settings: [jasmine.objectContaining({ id: 'theme', kind: DebugSettingKind.Select, value: 'system' })],
      }),
    ]);
    expect(bridge?.setValue('appearance', 'theme', 'dark').groups[0]?.settings[0]?.value).toBe('dark');
    expect(mode).toBe('dark');
    expect(bridge?.resetValue('appearance', 'theme').groups[0]?.settings[0]?.value).toBe('system');
    expect(mode).toBe('system');
  });

  it('rejects invalid values and restores the previous registration on disposal', () => {
    let enabled = false;
    const first = registerDebugSettingsGroup({
      categoryId: 'experiments',
      id: 'experiments',
      label: 'Original experiments',
      settings: [
        {
          defaultValue: false,
          get: () => enabled,
          id: 'feature',
          kind: DebugSettingKind.Toggle,
          label: 'Feature',
          set: value => {
            enabled = value as boolean;
          },
        },
      ],
    });
    registrations.push(first);
    const second = registerDebugSettingsGroup({
      categoryId: 'experiments',
      id: 'experiments',
      label: 'Replacement experiments',
      settings: [],
    });
    registrations.push(second);

    const bridge = (globalThis as unknown as DebugSettingsTestGlobal).__VALDI_DEBUG_SETTINGS__;
    expect(bridge?.getSnapshot().groups[0]?.label).toBe('Replacement experiments');
    second.dispose();
    expect(bridge?.getSnapshot().groups[0]?.label).toBe('Original experiments');
    expect(() => bridge?.setValue('experiments', 'feature', 'true')).toThrowError(
      'Debug setting feature requires a boolean value',
    );
    expect(enabled).toBeFalse();
  });

  it('combines matching framework and application categories without changing setting ownership', () => {
    let rightToLeft = false;
    let theme = 'system';
    const framework = registerDebugSettingsGroup({
      categoryId: 'appearance',
      id: 'valdi.appearance',
      label: 'Framework appearance',
      settings: [
        {
          defaultValue: false,
          get: () => rightToLeft,
          id: 'rtl',
          kind: DebugSettingKind.Toggle,
          label: 'Right-to-left layout',
          set: value => {
            rightToLeft = value as boolean;
          },
        },
      ],
    });
    registrations.push(framework);
    const application = registerDebugSettingsGroup({
      categoryId: 'appearance',
      id: 'app.appearance',
      label: 'Appearance',
      settings: [
        {
          defaultValue: 'system',
          get: () => theme,
          id: 'theme',
          kind: DebugSettingKind.Select,
          label: 'Theme',
          options: [
            { label: 'System', value: 'system' },
            { label: 'Dark', value: 'dark' },
          ],
          set: value => {
            theme = value as string;
          },
        },
      ],
    });
    registrations.push(application);

    const bridge = (globalThis as unknown as DebugSettingsTestGlobal).__VALDI_DEBUG_SETTINGS__;
    expect(bridge?.getSnapshot().groups).toEqual([
      jasmine.objectContaining({
        id: 'app.appearance',
        label: 'Appearance',
        settings: [
          jasmine.objectContaining({ id: 'theme', value: 'system' }),
          jasmine.objectContaining({ id: 'rtl', value: false }),
        ],
      }),
    ]);

    bridge?.setValue('app.appearance', 'rtl', true);
    bridge?.setValue('valdi.appearance', 'theme', 'dark');
    expect(rightToLeft).toBeTrue();
    expect(theme).toBe('dark');

    application.dispose();
    expect(bridge?.getSnapshot().groups).toEqual([
      jasmine.objectContaining({
        id: 'valdi.appearance',
        settings: [jasmine.objectContaining({ id: 'rtl', value: true })],
      }),
    ]);
  });

  it('uses registration sequence for deterministic A/B/A2 category precedence', () => {
    let firstAValue = false;
    let bValue = false;
    let secondAValue = false;
    const firstA = registerDebugSettingsGroup({
      categoryId: 'shared',
      id: 'owner-a',
      label: 'A',
      settings: [
        {
          defaultValue: false,
          get: () => firstAValue,
          id: 'enabled',
          kind: DebugSettingKind.Toggle,
          label: 'A enabled',
          set: value => {
            firstAValue = value;
          },
        },
      ],
    });
    registrations.push(firstA);
    registrations.push(
      registerDebugSettingsGroup({
        categoryId: 'shared',
        id: 'owner-b',
        label: 'B',
        settings: [
          {
            defaultValue: false,
            get: () => bValue,
            id: 'enabled',
            kind: DebugSettingKind.Toggle,
            label: 'B enabled',
            set: value => {
              bValue = value;
            },
          },
        ],
      }),
    );
    const secondA = registerDebugSettingsGroup({
      categoryId: 'shared',
      id: 'owner-a',
      label: 'A2',
      settings: [
        {
          defaultValue: false,
          get: () => secondAValue,
          id: 'enabled',
          kind: DebugSettingKind.Toggle,
          label: 'A2 enabled',
          set: value => {
            secondAValue = value;
          },
        },
      ],
    });
    registrations.push(secondA);

    const bridge = (globalThis as unknown as DebugSettingsTestGlobal).__VALDI_DEBUG_SETTINGS__;
    expect(bridge?.getSnapshot().groups).toEqual([
      jasmine.objectContaining({
        categoryId: 'shared',
        id: 'owner-a',
        label: 'A2',
        settings: [jasmine.objectContaining({ id: 'enabled', label: 'A2 enabled' })],
      }),
    ]);
    bridge?.setValue('owner-b', 'enabled', true);
    expect(secondAValue).toBeTrue();
    expect(bValue).toBeFalse();
    expect(firstAValue).toBeFalse();

    secondA.dispose();
    expect(bridge?.getSnapshot().groups).toEqual([
      jasmine.objectContaining({
        id: 'owner-b',
        label: 'B',
        settings: [jasmine.objectContaining({ id: 'enabled', label: 'B enabled' })],
      }),
    ]);
    bridge?.setValue('owner-a', 'enabled', true);
    expect(bValue).toBeTrue();
    expect(firstAValue).toBeFalse();
  });

  it('does not merge unrelated categories that share a display label', () => {
    registrations.push(
      registerDebugSettingsGroup({ categoryId: 'first', id: 'first', label: 'Shared label', settings: [] }),
    );
    registrations.push(
      registerDebugSettingsGroup({ categoryId: 'second', id: 'second', label: 'Shared label', settings: [] }),
    );

    const bridge = (globalThis as unknown as DebugSettingsTestGlobal).__VALDI_DEBUG_SETTINGS__;
    expect(bridge?.getSnapshot().groups).toEqual([
      jasmine.objectContaining({ categoryId: 'second', id: 'second' }),
      jasmine.objectContaining({ categoryId: 'first', id: 'first' }),
    ]);
  });

  it('supports freeform text and finite numeric controls', () => {
    let variant = '';
    let sampleRate = 1;
    registrations.push(
      registerDebugSettingsGroup({
        categoryId: 'advanced',
        id: 'advanced',
        label: 'Advanced',
        settings: [
          {
            defaultValue: '',
            get: () => variant,
            id: 'variant',
            kind: DebugSettingKind.Text,
            label: 'Experiment variant',
            set: value => {
              variant = value as string;
            },
          },
          {
            defaultValue: 1,
            get: () => sampleRate,
            id: 'sample-rate',
            kind: DebugSettingKind.Number,
            label: 'Sample rate',
            set: value => {
              sampleRate = value as number;
            },
          },
        ],
      }),
    );

    const bridge = (globalThis as unknown as DebugSettingsTestGlobal).__VALDI_DEBUG_SETTINGS__;
    bridge?.setValue('advanced', 'variant', 'treatment');
    bridge?.setValue('advanced', 'sample-rate', 0.25);

    expect(variant).toBe('treatment');
    expect(sampleRate).toBe(0.25);
    expect(() => bridge?.setValue('advanced', 'variant', true)).toThrowError(
      'Debug setting variant requires a string value',
    );
    expect(() => bridge?.setValue('advanced', 'sample-rate', Number.NaN)).toThrowError(
      'Debug setting sample-rate requires a finite number',
    );
  });

  it('validates select options and defaults before publishing a group', () => {
    expect(() =>
      registerDebugSettingsGroup({
        categoryId: 'invalid',
        id: 'empty-options',
        label: 'Invalid',
        settings: [
          {
            defaultValue: 'missing',
            get: () => 'missing',
            id: 'choice',
            kind: DebugSettingKind.Select,
            label: 'Choice',
            options: [],
            set: () => {},
          },
        ],
      }),
    ).toThrowError('Debug setting choice requires at least one declared option');

    expect(() =>
      registerDebugSettingsGroup({
        categoryId: 'invalid',
        id: 'non-finite-option',
        label: 'Invalid',
        settings: [
          {
            defaultValue: Number.POSITIVE_INFINITY,
            get: () => Number.POSITIVE_INFINITY,
            id: 'choice',
            kind: DebugSettingKind.Select,
            label: 'Choice',
            options: [{ label: 'Infinity', value: Number.POSITIVE_INFINITY }],
            set: () => {},
          },
        ],
      }),
    ).toThrowError('Debug setting choice requires a finite number');

    expect(() =>
      registerDebugSettingsGroup({
        categoryId: 'invalid',
        id: 'invalid-default',
        label: 'Invalid',
        settings: [
          {
            defaultValue: 'missing',
            get: () => 'missing',
            id: 'choice',
            kind: DebugSettingKind.Select,
            label: 'Choice',
            options: [{ label: 'Available', value: 'available' }],
            set: () => {},
          },
        ],
      }),
    ).toThrowError('Debug setting choice requires one of its declared options');

    expect(() =>
      registerDebugSettingsGroup({
        categoryId: 'invalid',
        id: 'duplicate-options',
        label: 'Invalid',
        settings: [
          {
            defaultValue: 'same',
            get: () => 'same',
            id: 'choice',
            kind: DebugSettingKind.Select,
            label: 'Choice',
            options: [
              { label: 'First', value: 'same' },
              { label: 'Second', value: 'same' },
            ],
            set: () => {},
          },
        ],
      }),
    ).toThrowError('Debug setting choice contains a duplicate option value');

    const toggleWithOptions = {
      categoryId: 'invalid',
      id: 'toggle-options',
      label: 'Invalid',
      settings: [
        {
          defaultValue: false,
          get: () => false,
          id: 'toggle',
          kind: DebugSettingKind.Toggle,
          label: 'Toggle',
          options: [{ label: 'Invalid', value: true }],
          set: () => {},
        },
      ],
    } as unknown as DebugSettingsGroup;
    expect(() => registerDebugSettingsGroup(toggleWithOptions)).toThrowError(
      'Debug setting toggle declares options but is not a select setting',
    );
  });

  it('validates visible getter values after resolving setting precedence', () => {
    let selected = 1;
    registrations.push(
      registerDebugSettingsGroup({
        categoryId: 'runtime-validation',
        id: 'runtime-validation',
        label: 'Runtime validation',
        settings: [
          {
            defaultValue: 1,
            get: () => selected,
            id: 'choice',
            kind: DebugSettingKind.Select,
            label: 'Choice',
            options: [
              { label: 'One', value: 1 },
              { label: 'Two', value: 2 },
            ],
            set: value => {
              selected = value as number;
            },
          },
        ],
      }),
    );
    const bridge = (globalThis as unknown as DebugSettingsTestGlobal).__VALDI_DEBUG_SETTINGS__;

    expect(() => bridge?.setValue('runtime-validation', 'choice', Number.NEGATIVE_INFINITY)).toThrowError(
      'Debug setting choice requires a finite number',
    );
    const override = registerDebugSettingsGroup({
      categoryId: 'runtime-validation',
      id: 'runtime-validation-override',
      label: 'Runtime validation override',
      settings: [
        {
          defaultValue: 2,
          get: () => 2,
          id: 'choice',
          kind: DebugSettingKind.Select,
          label: 'Replacement choice',
          options: [{ label: 'Two', value: 2 }],
          set: () => {},
        },
      ],
    });
    registrations.push(override);
    selected = Number.NaN;

    expect(bridge?.getSnapshot().groups[0]?.settings[0]?.value).toBe(2);
    override.dispose();
    expect(() => bridge?.getSnapshot()).toThrowError('Debug setting choice requires a finite number');
  });

  it('produces a JSON-safe snapshot without functions or non-finite values', () => {
    let enabled = true;
    registrations.push(
      registerDebugSettingsGroup({
        categoryId: 'serialization',
        id: 'serialization',
        label: 'Serialization',
        settings: [
          {
            defaultValue: false,
            description: 'Serializable setting',
            get: () => enabled,
            id: 'enabled',
            kind: DebugSettingKind.Toggle,
            label: 'Enabled',
            set: value => {
              enabled = value;
            },
          },
        ],
      }),
    );
    const bridge = (globalThis as unknown as DebugSettingsTestGlobal).__VALDI_DEBUG_SETTINGS__;
    const snapshot = bridge!.getSnapshot();
    const serialized = JSON.stringify(snapshot);

    expect(JSON.parse(serialized)).toEqual(snapshot);
    expect(serialized).toContain('"categoryId":"serialization"');
    expect(serialized).not.toContain('"get":');
    expect(serialized).not.toContain('"set":');
  });

  it('projects select options to exact JSON-safe wire objects', () => {
    const adversarialOption: {
      callback: () => void;
      cycle?: unknown;
      label: string;
      toJSON(): never;
      value: string;
    } = {
      callback: () => {},
      label: 'Safe',
      toJSON: () => {
        throw new Error('Original option object crossed the snapshot boundary');
      },
      value: 'safe',
    };
    adversarialOption.cycle = adversarialOption;
    const group = {
      categoryId: 'option-serialization',
      id: 'option-serialization',
      label: 'Option serialization',
      settings: [
        {
          defaultValue: 'safe',
          get: () => 'safe',
          id: 'choice',
          kind: DebugSettingKind.Select,
          label: 'Choice',
          options: [adversarialOption],
          set: () => {},
        },
      ],
    } as unknown as DebugSettingsGroup;
    registrations.push(registerDebugSettingsGroup(group));

    const bridge = (globalThis as unknown as DebugSettingsTestGlobal).__VALDI_DEBUG_SETTINGS__;
    const snapshot = bridge!.getSnapshot();
    const options = snapshot.groups[0]!.settings[0]!.options!;

    expect(options).toEqual([{ label: 'Safe', value: 'safe' }]);
    expect(Object.keys(options[0]!)).toEqual(['label', 'value']);
    expect(JSON.parse(JSON.stringify(snapshot))).toEqual(snapshot);
  });

  it('rejects non-string descriptions before publishing a snapshot', () => {
    const cyclicDescription: { self?: unknown } = {};
    cyclicDescription.self = cyclicDescription;
    const invalidGroup = {
      categoryId: 'invalid-description',
      id: 'invalid-description',
      label: 'Invalid description',
      settings: [
        {
          defaultValue: false,
          description: cyclicDescription,
          get: () => false,
          id: 'enabled',
          kind: DebugSettingKind.Toggle,
          label: 'Enabled',
          set: () => {},
        },
      ],
    } as unknown as DebugSettingsGroup;

    expect(() => registerDebugSettingsGroup(invalidGroup)).toThrowError(
      'Debug setting enabled requires a string description',
    );
  });

  it('answers native daemon custom requests through the same typed registry', async () => {
    let enabled = false;
    const addHandler = spyOn(jsx, 'addCustomMessageHandler').and.callThrough();
    registrations.push(
      registerDebugSettingsGroup({
        categoryId: 'experiments',
        id: 'experiments',
        label: 'Experiments',
        settings: [
          {
            defaultValue: false,
            get: () => enabled,
            id: 'feature',
            kind: DebugSettingKind.Toggle,
            label: 'Feature',
            set: value => {
              enabled = value as boolean;
            },
          },
        ],
      }),
    );
    const handler = addHandler.calls.mostRecent().args[0] as CustomMessageHandler;

    expect(handler.messageReceived('UnrelatedMessage', {})).toBeUndefined();
    const snapshot = await handler.messageReceived('ValdiDebuggerSettings', {
      action: 'set',
      groupId: 'experiments',
      settingId: 'feature',
      value: true,
    });

    expect(enabled).toBeTrue();
    expect(snapshot).toEqual(
      jasmine.objectContaining({
        groups: [jasmine.objectContaining({ id: 'experiments' })],
      }),
    );
  });

  it('does not publish runtime controls when debugging is disabled', () => {
    runtime.isDebugEnabled = false;

    registrations.push(
      registerDebugSettingsGroup({ categoryId: 'hidden', id: 'hidden', label: 'Hidden', settings: [] }),
    );

    const bridge = (globalThis as unknown as DebugSettingsTestGlobal).__VALDI_DEBUG_SETTINGS__;
    expect(bridge).toBeUndefined();
  });
});
