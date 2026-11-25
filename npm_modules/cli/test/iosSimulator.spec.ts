import * as fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { CliError } from '../src/core/errors';
import {
  findSimulatorUiApp,
  installAndLaunchOnSimulator,
  needsSimctlInstall,
  parseSimulatorDevices,
  pickSimulator,
} from '../src/utils/iosSimulator';
import { compressDirectoryContentsTo } from '../src/utils/zipUtils';

const DEVICES_JSON = JSON.stringify({
  devices: {
    'com.apple.CoreSimulator.SimRuntime.iOS-26-0': [
      { udid: 'A', name: 'iPhone 17 Pro', state: 'Shutdown', isAvailable: true },
      { udid: 'B', name: 'iPad Pro', state: 'Booted', isAvailable: true },
    ],
    'com.apple.CoreSimulator.SimRuntime.iOS-27-0': [
      { udid: 'C', name: 'iPhone 18', state: 'Shutdown', isAvailable: true },
      { udid: 'D', name: 'iPhone 18 Pro', state: 'Shutdown', isAvailable: false },
    ],
    'com.apple.CoreSimulator.SimRuntime.iOS-27-2-iPhone19-4': [{ udid: 'V', name: 'iPhone 19', state: 'Shutdown', isAvailable: true }],
    'com.apple.CoreSimulator.SimRuntime.watchOS-12-0': [{ udid: 'W', name: 'iPhone Watch', state: 'Booted' }],
  },
});

describe('iosSimulator', () => {
  describe('pickSimulator', () => {
    const devices = parseSimulatorDevices(DEVICES_JSON);

    it('ignores unavailable and non-iOS devices', () => {
      expect(devices.map(device => device.udid).sort()).toEqual(['A', 'B', 'C', 'V']);
    });

    it('parses plain and variant runtime ids to the same version shape', () => {
      expect(devices.find(device => device.udid === 'C')?.runtimeVersion).toEqual([27, 0]);
      expect(devices.find(device => device.udid === 'V')?.runtimeVersion).toEqual([27, 2]);
    });

    it('honors a requested UDID', () => {
      expect(pickSimulator(devices, 'A').udid).toBe('A');
    });

    it('rejects an unknown UDID', () => {
      expect(() => pickSimulator(devices, 'Z')).toThrowError(CliError);
    });

    it('prefers a booted iPhone', () => {
      const booted = devices.map(device => (device.udid === 'A' ? { ...device, state: 'Booted' } : device));
      expect(pickSimulator(booted).udid).toBe('A');
    });

    it('resolves "booted" to a booted iPhone, else any booted simulator', () => {
      const iphone = devices.map(device => (device.udid === 'C' ? { ...device, state: 'Booted' } : device));
      expect(pickSimulator(iphone, 'booted').udid).toBe('C');
      expect(pickSimulator(devices, 'booted').udid).toBe('B');
    });

    it('rejects "booted" when nothing is booted', () => {
      const none = devices.map(device => ({ ...device, state: 'Shutdown' }));
      expect(() => pickSimulator(none, 'booted')).toThrowError(CliError, /No booted iOS simulator/);
    });

    it('falls back to the newest runtime iPhone when none is booted', () => {
      const shut = devices.map(device => ({ ...device, state: 'Shutdown' }));
      expect(pickSimulator(shut).udid).toBe('V');
    });

    it('fails when no iPhone exists', () => {
      expect(() => pickSimulator([])).toThrowError(CliError);
    });
  });

  describe('Xcode layout detection', () => {
    const dev = '/Xcode/Contents/Developer';

    it('uses Simulator.app when present', () => {
      const exists = (p: string) => p === `${dev}/Applications/Simulator.app`;
      expect(needsSimctlInstall(dev, exists)).toBe(false);
      expect(findSimulatorUiApp(dev, exists)).toBe(`${dev}/Applications/Simulator.app`);
    });

    it('uses DeviceHub.app when Simulator.app is missing', () => {
      const exists = (p: string) => p === path.join(dev, '..', 'Applications', 'DeviceHub.app');
      expect(needsSimctlInstall(dev, exists)).toBe(true);
      expect(findSimulatorUiApp(dev, exists)).toBe(path.join(dev, '..', 'Applications', 'DeviceHub.app'));
    });

    it('reports no UI app when neither exists', () => {
      expect(findSimulatorUiApp(dev, () => false)).toBeUndefined();
    });
  });

  describe('installAndLaunchOnSimulator', () => {
    let tmp: string;
    let ipa: string;

    beforeEach(async () => {
      tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'ios-sim-spec-'));
      const src = path.join(tmp, 'src');
      fs.mkdirSync(path.join(src, 'Payload', 'Hello.app'), { recursive: true });
      fs.writeFileSync(path.join(src, 'Payload', 'Hello.app', 'Info.plist'), '');
      ipa = path.join(tmp, 'Hello.ipa');
      await compressDirectoryContentsTo(src, ipa);
    });

    afterEach(() => {
      fs.rmSync(tmp, { force: true, recursive: true });
    });

    function fakeExec(failOn?: string) {
      const calls: string[][] = [];
      const exec = async (file: string, args: string[]) => {
        calls.push([file, ...args]);
        if (failOn && args.includes(failOn)) {
          throw new Error('boom');
        }
        if (args.includes('list')) {
          return DEVICES_JSON;
        }
        if (file === 'plutil') {
          return 'com.example.hello\n';
        }
        return '';
      };
      return { calls, exec };
    }

    it('boots, installs and launches on the picked simulator', async () => {
      const { calls, exec } = fakeExec();
      await installAndLaunchOnSimulator(ipa, { deviceId: 'C', exec, developerDir: '/nonexistent/Developer' });
      const simctl = calls.filter(call => call[0] === 'xcrun').map(call => call[2]);
      expect(simctl).toEqual(['list', 'bootstatus', 'install', 'get_app_container', 'launch']);
      expect(calls.find(call => call[2] === 'launch')).toEqual(['xcrun', 'simctl', 'launch', '--terminate-running-process', 'C', 'com.example.hello']);
    });

    it('fails loudly when install fails', async () => {
      const { exec } = fakeExec('install');
      await expectAsync(installAndLaunchOnSimulator(ipa, { deviceId: 'C', exec, developerDir: '/x' })).toBeRejectedWithError(
        /Failed to install/,
      );
    });

    it('fails loudly when launch fails', async () => {
      const { exec } = fakeExec('launch');
      await expectAsync(installAndLaunchOnSimulator(ipa, { deviceId: 'C', exec, developerDir: '/x' })).toBeRejectedWithError(
        /Failed to launch/,
      );
    });
  });
});
