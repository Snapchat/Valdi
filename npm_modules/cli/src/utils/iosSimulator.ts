import { execFile } from 'node:child_process';
import * as fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { promisify } from 'node:util';
import { CliError } from '../core/errors';
import { decompressTo } from './zipUtils';

const execFileAsync = promisify(execFile);

export type Exec = (file: string, args: string[]) => Promise<string>;

export interface SimulatorDevice {
  udid: string;
  name: string;
  state: string;
  runtimeVersion: number[];
}

export interface InstallOptions {
  deviceId?: string | undefined;
  exec?: Exec;
  developerDir?: string;
}

const defaultExec: Exec = async (file, args) => (await execFileAsync(file, args)).stdout;

export async function getDeveloperDir(exec: Exec = defaultExec): Promise<string> {
  const fromEnv = process.env['DEVELOPER_DIR'];
  if (fromEnv) {
    return fromEnv;
  }
  return (await exec('xcode-select', ['-p'])).trim();
}

export function legacySimulatorAppPath(developerDir: string): string {
  return path.join(developerDir, 'Applications', 'Simulator.app');
}

/** Xcode 27 replaced Simulator.app with DeviceHub.app, which lives next to the Developer directory. */
export function findSimulatorUiApp(developerDir: string, exists: (p: string) => boolean = fs.existsSync): string | undefined {
  const candidates = [legacySimulatorAppPath(developerDir), path.join(developerDir, '..', 'Applications', 'DeviceHub.app')];
  return candidates.find(exists);
}

/** The rules_apple launcher can only open Simulator.app, so Xcode versions without it need the CLI to drive simctl. */
export function needsSimctlInstall(developerDir: string, exists: (p: string) => boolean = fs.existsSync): boolean {
  return !exists(legacySimulatorAppPath(developerDir));
}

function parseRuntimeVersion(runtimeId: string): number[] | undefined {
  const match = /\.iOS-(\d+(?:-\d+)*)(?=-|$)/.exec(runtimeId);
  return match?.[1]?.split('-').map(Number);
}

function compareVersions(a: number[], b: number[]): number {
  for (let i = 0; i < Math.max(a.length, b.length); i++) {
    const diff = (a[i] ?? 0) - (b[i] ?? 0);
    if (diff !== 0) {
      return diff;
    }
  }
  return 0;
}

export function parseSimulatorDevices(listJson: string): SimulatorDevice[] {
  const parsed = JSON.parse(listJson) as {
    devices: Record<string, Array<{ udid: string; name: string; state: string; isAvailable?: boolean }>>;
  };
  const result: SimulatorDevice[] = [];
  for (const [runtimeId, devices] of Object.entries(parsed.devices)) {
    const runtimeVersion = parseRuntimeVersion(runtimeId);
    if (!runtimeVersion) {
      continue;
    }
    for (const device of devices) {
      if (device.isAvailable !== false) {
        result.push({ udid: device.udid, name: device.name, state: device.state, runtimeVersion });
      }
    }
  }
  return result;
}

export function pickSimulator(devices: SimulatorDevice[], requestedUdid?: string): SimulatorDevice {
  if (requestedUdid === 'booted') {
    const booted = devices.filter(device => device.state === 'Booted');
    const found = booted.find(device => device.name.startsWith('iPhone')) ?? booted[0];
    if (!found) {
      throw new CliError("No booted iOS simulator found. Boot one or omit '--device_id booted'.");
    }
    return found;
  }
  if (requestedUdid) {
    const requested = devices.find(device => device.udid === requestedUdid);
    if (!requested) {
      throw new CliError(`No available iOS simulator with UDID ${requestedUdid}. Run 'xcrun simctl list devices' to see them.`);
    }
    return requested;
  }
  const iphones = devices.filter(device => device.name.startsWith('iPhone'));
  const booted = iphones.find(device => device.state === 'Booted');
  if (booted) {
    return booted;
  }
  const newestFirst = [...iphones].sort((a, b) => compareVersions(b.runtimeVersion, a.runtimeVersion));
  const fallback = newestFirst[0];
  if (!fallback) {
    throw new CliError('No iPhone simulator is available. Install an iOS simulator runtime in Xcode and try again.');
  }
  return fallback;
}

export async function installAndLaunchOnSimulator(ipaPath: string, options: InstallOptions = {}): Promise<void> {
  const exec = options.exec ?? defaultExec;
  const developerDir = options.developerDir ?? (await getDeveloperDir(exec));

  const listJson = await exec('xcrun', ['simctl', 'list', 'devices', 'available', '--json']);
  const simulator = pickSimulator(parseSimulatorDevices(listJson), options.deviceId);
  console.log(`Using simulator ${simulator.name} (${simulator.udid})`);

  await exec('xcrun', ['simctl', 'bootstatus', simulator.udid, '-b']).catch((error: unknown) => {
    throw new CliError(`Failed to boot simulator ${simulator.udid}: ${(error as Error).message}`);
  });

  const extractDir = fs.mkdtempSync(path.join(os.tmpdir(), 'valdi-ios-install-'));
  try {
    await decompressTo(ipaPath, extractDir);
    const payloadDir = path.join(extractDir, 'Payload');
    const appName = fs.existsSync(payloadDir) ? fs.readdirSync(payloadDir).find(name => name.endsWith('.app')) : undefined;
    if (!appName) {
      throw new CliError(`No .app bundle found in ${ipaPath}`);
    }
    const appPath = path.join(payloadDir, appName);
    const bundleId = (await exec('plutil', ['-extract', 'CFBundleIdentifier', 'raw', '-o', '-', path.join(appPath, 'Info.plist')])).trim();

    await exec('xcrun', ['simctl', 'install', simulator.udid, appPath]).catch((error: unknown) => {
      throw new CliError(`Failed to install ${appName} on simulator ${simulator.udid}: ${(error as Error).message}`);
    });
    await exec('xcrun', ['simctl', 'get_app_container', simulator.udid, bundleId]).catch(() => {
      throw new CliError(`Installed ${bundleId} is not present on simulator ${simulator.udid}`);
    });
    await exec('xcrun', ['simctl', 'launch', '--terminate-running-process', simulator.udid, bundleId]).catch((error: unknown) => {
      throw new CliError(`Failed to launch ${bundleId} on simulator ${simulator.udid}: ${(error as Error).message}`);
    });
    console.log(`Launched ${bundleId} on ${simulator.name}`);
  } finally {
    fs.rmSync(extractDir, { force: true, recursive: true });
  }

  const uiApp = findSimulatorUiApp(developerDir);
  if (uiApp) {
    await exec('open', ['-a', uiApp]).catch(() => {
      console.log(`Could not open ${uiApp}; the app is running on the simulator.`);
    });
  } else {
    console.log('No simulator UI app found in the active Xcode; the app is running on the booted simulator.');
  }
}
