import { jsx } from '../JSXBootstrap';
import type { ValdiRuntime } from '../ValdiRuntime';
import type { CustomMessageHandler } from './CustomMessageHandler';

declare const runtime: ValdiRuntime;

const DEBUG_SETTINGS_IDENTIFIER = 'ValdiDebuggerSettings';
const DEBUG_SETTINGS_CONTRACT_VERSION = 1;

/**
 * This module owns only the runtime registry. Debugger presentation is supplied by the stacked settings provider,
 * while native delivery uses the generic custom-message transport from the debugger input/core layer.
 */

/** Debugger clients exchange only primitive setting values across process boundaries. */
export enum DebugSettingKind {
  Toggle = 'toggle',
  Select = 'select',
  Text = 'text',
  Number = 'number',
}

export type DebugSettingValue = boolean | number | string;

export interface DebugSettingOption {
  readonly label: string;
  readonly value: DebugSettingValue;
}

interface DebugSettingBase<Kind extends DebugSettingKind, Value extends DebugSettingValue> {
  readonly defaultValue: Value;
  readonly description?: string;
  readonly id: string;
  readonly kind: Kind;
  readonly label: string;
  readonly get: () => Value;
  readonly set: (value: Value) => void;
}

export type ToggleDebugSetting = DebugSettingBase<DebugSettingKind.Toggle, boolean>;

export type SelectDebugSetting = DebugSettingBase<DebugSettingKind.Select, DebugSettingValue> & {
  readonly options: readonly DebugSettingOption[];
};

export type TextDebugSetting = DebugSettingBase<DebugSettingKind.Text, string>;

export type NumberDebugSetting = DebugSettingBase<DebugSettingKind.Number, number>;

export type DebugSetting = ToggleDebugSetting | SelectDebugSetting | TextDebugSetting | NumberDebugSetting;

export interface DebugSettingsGroup {
  /** Stable identity used to combine settings from independent owners into one debugger category. */
  readonly categoryId: string;
  readonly id: string;
  readonly label: string;
  readonly settings: readonly DebugSetting[];
}

export interface DebugSettingsRegistration {
  dispose(): void;
  notifyChange(): void;
}

export interface DebugSettingSnapshot {
  readonly defaultValue: DebugSettingValue;
  readonly description?: string;
  readonly id: string;
  readonly kind: DebugSettingKind;
  readonly label: string;
  readonly options?: readonly DebugSettingOption[];
  readonly value: DebugSettingValue;
}

export interface DebugSettingsGroupSnapshot {
  readonly categoryId: string;
  readonly id: string;
  readonly label: string;
  readonly settings: readonly DebugSettingSnapshot[];
}

export interface DebugSettingsSnapshot {
  readonly contractVersion: number;
  readonly groups: readonly DebugSettingsGroupSnapshot[];
  readonly revision: number;
}

interface DebugSettingsRequest {
  readonly action?: string;
  readonly groupId?: string;
  readonly settingId?: string;
  readonly value?: unknown;
}

interface DebugSettingsGlobal {
  __VALDI_DEBUG_SETTINGS__?: DebugSettingsBridge;
}

interface DebugSettingsBridge {
  getSnapshot(): DebugSettingsSnapshot;
  resetValue(groupId: string, settingId: string): DebugSettingsSnapshot;
  setValue(groupId: string, settingId: string, value: unknown): DebugSettingsSnapshot;
}

interface RegisteredDebugSettingsGroup {
  readonly categoryId: string;
  readonly group: DebugSettingsGroup;
  readonly id: string;
  readonly label: string;
  readonly sequence: number;
}

const registeredGroups = new Map<string, RegisteredDebugSettingsGroup[]>();
let registryRevision = 0;
let registrationSequence = 0;
let registeredHandler: CustomMessageHandler | undefined;

const inactiveRegistration: DebugSettingsRegistration = {
  dispose(): void {},
  notifyChange(): void {},
};

function activeRegistrations(): RegisteredDebugSettingsGroup[] {
  const active: RegisteredDebugSettingsGroup[] = [];
  registeredGroups.forEach(groupRegistrations => {
    const registration = groupRegistrations[groupRegistrations.length - 1];
    if (registration !== undefined) {
      active.push(registration);
    }
  });
  active.sort((left, right) => right.sequence - left.sequence);
  return active;
}

function snapshotSetting(setting: DebugSetting, value: DebugSettingValue): DebugSettingSnapshot {
  return {
    defaultValue: setting.defaultValue,
    ...(setting.description === undefined ? {} : { description: setting.description }),
    id: setting.id,
    kind: setting.kind,
    label: setting.label,
    ...(setting.kind === DebugSettingKind.Select
      ? { options: setting.options.map(option => ({ label: option.label, value: option.value })) }
      : {}),
    value,
  };
}

function debugSettingsSnapshot(): DebugSettingsSnapshot {
  const active = activeRegistrations();
  const groupsByCategory = new Map<string, RegisteredDebugSettingsGroup[]>();
  active.forEach(registration => {
    const categoryGroups = groupsByCategory.get(registration.categoryId) ?? [];
    categoryGroups.push(registration);
    groupsByCategory.set(registration.categoryId, categoryGroups);
  });

  const groups: DebugSettingsGroupSnapshot[] = [];
  groupsByCategory.forEach(categoryGroups => {
    const primaryRegistration = categoryGroups[0];
    if (primaryRegistration === undefined) {
      return;
    }
    const settingIds = new Set<string>();
    const settings: DebugSettingSnapshot[] = [];
    categoryGroups.forEach(registration => {
      registration.group.settings.forEach(setting => {
        if (settingIds.has(setting.id)) {
          return;
        }
        settingIds.add(setting.id);
        validateSettingDeclaration(setting);
        settings.push(snapshotSetting(setting, validateSettingValue(setting, setting.get())));
      });
    });
    groups.push({
      categoryId: primaryRegistration.categoryId,
      id: primaryRegistration.id,
      label: primaryRegistration.label,
      settings,
    });
  });
  return { contractVersion: DEBUG_SETTINGS_CONTRACT_VERSION, groups, revision: registryRevision };
}

function activeSetting(groupId: string, settingId: string): DebugSetting {
  const registrations = registeredGroups.get(groupId);
  const registration = registrations?.[registrations.length - 1];
  if (registration === undefined) {
    throw new Error(`Unknown debug settings group: ${groupId}`);
  }
  const categoryId = registration.categoryId;
  for (const candidateRegistration of activeRegistrations()) {
    if (candidateRegistration.categoryId === categoryId) {
      const setting = candidateRegistration.group.settings.find(candidate => candidate.id === settingId);
      if (setting !== undefined) {
        return setting;
      }
    }
  }
  throw new Error(`Unknown debug setting: ${groupId}.${settingId}`);
}

function validateWireValue(settingId: string, value: unknown): DebugSettingValue {
  if (typeof value === 'number' && !Number.isFinite(value)) {
    throw new TypeError(`Debug setting ${settingId} requires a finite number`);
  }
  if (typeof value !== 'boolean' && typeof value !== 'number' && typeof value !== 'string') {
    throw new TypeError(`Debug setting ${settingId} requires a primitive value`);
  }
  return value;
}

function validateSettingValue(setting: DebugSetting, value: unknown): DebugSettingValue {
  const settingId = setting.id;
  const wireValue = validateWireValue(setting.id, value);
  switch (setting.kind) {
    case DebugSettingKind.Toggle:
      if (typeof wireValue !== 'boolean') {
        throw new TypeError(`Debug setting ${setting.id} requires a boolean value`);
      }
      break;
    case DebugSettingKind.Select:
      if (!setting.options.some(option => option.value === wireValue)) {
        throw new TypeError(`Debug setting ${setting.id} requires one of its declared options`);
      }
      break;
    case DebugSettingKind.Text:
      if (typeof wireValue !== 'string') {
        throw new TypeError(`Debug setting ${setting.id} requires a string value`);
      }
      break;
    case DebugSettingKind.Number:
      if (typeof wireValue !== 'number') {
        throw new TypeError(`Debug setting ${setting.id} requires a finite number`);
      }
      break;
    default:
      throw new TypeError(`Debug setting ${settingId} has an unsupported kind`);
  }
  return wireValue;
}

function validateSettingDeclaration(setting: DebugSetting): void {
  if (
    typeof setting.id !== 'string' ||
    setting.id.length === 0 ||
    typeof setting.label !== 'string' ||
    setting.label.length === 0
  ) {
    throw new Error('Debug settings require a non-empty id and label');
  }
  const settingId = setting.id;
  if (setting.description !== undefined && typeof setting.description !== 'string') {
    throw new TypeError(`Debug setting ${settingId} requires a string description`);
  }

  const declaredOptions = (setting as { readonly options?: unknown }).options;
  switch (setting.kind) {
    case DebugSettingKind.Select: {
      if (!Array.isArray(setting.options) || setting.options.length === 0) {
        throw new Error(`Debug setting ${setting.id} requires at least one declared option`);
      }
      const optionValues = new Set<DebugSettingValue>();
      setting.options.forEach(option => {
        if (typeof option.label !== 'string' || option.label.length === 0) {
          throw new Error(`Debug setting ${setting.id} contains an option without a label`);
        }
        const optionValue = validateWireValue(setting.id, option.value);
        if (optionValues.has(optionValue)) {
          throw new Error(`Debug setting ${setting.id} contains a duplicate option value`);
        }
        optionValues.add(optionValue);
      });
      break;
    }
    case DebugSettingKind.Toggle:
    case DebugSettingKind.Text:
    case DebugSettingKind.Number:
      if (declaredOptions !== undefined) {
        throw new Error(`Debug setting ${setting.id} declares options but is not a select setting`);
      }
      break;
    default:
      throw new Error(`Debug setting ${settingId} has an unsupported kind`);
  }
  validateSettingValue(setting, setting.defaultValue);
}

function changeSetting(groupId: string, settingId: string, value: unknown): DebugSettingsSnapshot {
  const setting = activeSetting(groupId, settingId);
  const validatedValue = validateSettingValue(setting, value);
  switch (setting.kind) {
    case DebugSettingKind.Toggle:
      setting.set(validatedValue as boolean);
      break;
    case DebugSettingKind.Select:
      setting.set(validatedValue);
      break;
    case DebugSettingKind.Text:
      setting.set(validatedValue as string);
      break;
    case DebugSettingKind.Number:
      setting.set(validatedValue as number);
      break;
  }
  registryRevision++;
  return debugSettingsSnapshot();
}

function handleRequest(request: DebugSettingsRequest): DebugSettingsSnapshot {
  const action = request.action ?? 'list';
  if (action === 'list') {
    return debugSettingsSnapshot();
  }
  if (typeof request.groupId !== 'string' || typeof request.settingId !== 'string') {
    throw new Error('Debug setting changes require groupId and settingId');
  }
  if (action === 'set') {
    return changeSetting(request.groupId, request.settingId, request.value);
  }
  if (action === 'reset') {
    const setting = activeSetting(request.groupId, request.settingId);
    return changeSetting(request.groupId, request.settingId, setting.defaultValue);
  }
  throw new Error(`Unsupported debug settings action: ${action}`);
}

function installRegistryBridge(): void {
  if (registeredHandler !== undefined) {
    return;
  }
  const handler: CustomMessageHandler = {
    messageReceived(identifier: string, data: unknown): Promise<DebugSettingsSnapshot> | undefined {
      if (identifier !== DEBUG_SETTINGS_IDENTIFIER) {
        return undefined;
      }
      const request = typeof data === 'object' && data !== null ? (data as DebugSettingsRequest) : {};
      try {
        return Promise.resolve(handleRequest(request));
      } catch (error) {
        return Promise.reject(error);
      }
    },
  };
  jsx.addCustomMessageHandler(handler);
  registeredHandler = handler;
  const globals = globalThis as unknown as DebugSettingsGlobal;
  globals.__VALDI_DEBUG_SETTINGS__ = {
    getSnapshot: debugSettingsSnapshot,
    resetValue: (groupId, settingId) => handleRequest({ action: 'reset', groupId, settingId }),
    setValue: (groupId, settingId, value) => handleRequest({ action: 'set', groupId, settingId, value }),
  };
}

function removeRegistryBridgeIfUnused(): void {
  if (registeredGroups.size !== 0 || registeredHandler === undefined) {
    return;
  }
  jsx.removeCustomMessageHandler(registeredHandler);
  registeredHandler = undefined;
  delete (globalThis as unknown as DebugSettingsGlobal).__VALDI_DEBUG_SETTINGS__;
}

/** Publish runtime or application tuning controls to attached debug runtimes only. */
export function registerDebugSettingsGroup(group: DebugSettingsGroup): DebugSettingsRegistration {
  if (!runtime.isDebugEnabled) {
    return inactiveRegistration;
  }
  if (
    typeof group.categoryId !== 'string' ||
    group.categoryId.length === 0 ||
    typeof group.id !== 'string' ||
    group.id.length === 0 ||
    typeof group.label !== 'string' ||
    group.label.length === 0
  ) {
    throw new Error('Debug settings groups require a non-empty categoryId, id, and label');
  }
  const ids = new Set<string>();
  group.settings.forEach(setting => {
    if (ids.has(setting.id)) {
      throw new Error(`Debug settings group ${group.id} contains an invalid or duplicate setting id`);
    }
    ids.add(setting.id);
    validateSettingDeclaration(setting);
  });

  installRegistryBridge();
  const registrations = registeredGroups.get(group.id) ?? [];
  const registration: RegisteredDebugSettingsGroup = {
    categoryId: group.categoryId,
    group,
    id: group.id,
    label: group.label,
    sequence: ++registrationSequence,
  };
  registrations.push(registration);
  registeredGroups.set(group.id, registrations);
  registryRevision++;

  let disposed = false;
  return {
    dispose(): void {
      if (disposed) {
        return;
      }
      disposed = true;
      const current = registeredGroups.get(registration.id);
      const index = current?.indexOf(registration);
      if (current !== undefined && index !== undefined && index >= 0) {
        current.splice(index, 1);
        if (current.length === 0) {
          registeredGroups.delete(registration.id);
        }
        registryRevision++;
      }
      removeRegistryBridgeIfUnused();
    },
    notifyChange(): void {
      if (!disposed) {
        registryRevision++;
      }
    },
  };
}
