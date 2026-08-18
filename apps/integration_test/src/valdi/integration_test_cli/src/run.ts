import { fs } from 'file_system/src/FileSystem';
import { arrayToString } from 'coreutils/src/Uint8ArrayUtils';
import { Argument, ArgumentsParser } from 'valdi_cli/src/ArgumentsParser';
import { ChildProcess } from 'valdi_cli/src/ChildProcess';
import { Path } from 'valdi_cli/src/Path';

import type { Platform } from './types';
import { runWebIntegrationTests } from './webRun';

const PACKAGE = 'com.snap.valdi.integrationtest';

type NativePlatform = Exclude<Platform, 'web'>;

const APP_TARGET: Record<NativePlatform, string> = {
  android: '//apps/integration_test:integration_test_android',
  ios: '//apps/integration_test:integration_test_ios',
  macos: '//apps/integration_test:integration_test_macos',
};

export interface RunIntegrationTestsOptions {
  repo?: string;
  platform: Platform;
  output: string;
  deviceId?: string;
  iosDeviceBuild?: boolean;
  timeoutMs?: string | number;
  buildConfig?: string;
  bazelArgs?: string;
  valdiBin?: string;
}

interface WaitForResultOptions extends RunIntegrationTestsOptions {
  child?: ChildProcess;
}

function wait(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}

export function isPlatform(value: string | undefined): value is Platform {
  return value === 'android' || value === 'ios' || value === 'macos' || value === 'web';
}

function isValdiRepo(path: string): boolean {
  const repoPath = Path.fromString(path);
  return (
    fs.existsSync(repoPath.appending('MODULE.bazel').toString()) &&
    fs.existsSync(repoPath.appending('apps/integration_test/BUILD.bazel').toString()) &&
    fs.existsSync(repoPath.appending('bzl/valdi/valdi_application.bzl').toString())
  );
}

function isBazelOutputPath(path: string): boolean {
  return path.indexOf('/execroot/') !== -1 || path.indexOf('/_bazel_') !== -1;
}

function findRepoFrom(start: string): string {
  let current = Path.fromString(start);
  while (true) {
    if (isValdiRepo(current.toString())) {
      return current.toString();
    }
    const parent = current.parent();
    if (parent.toString() === current.toString()) {
      throw new Error(`Could not find Valdi repo root from ${start}. Pass --repo explicitly.`);
    }
    current = parent;
  }
}

function defaultRepo(): string {
  const pwd = process.env.PWD;
  if (pwd && !isBazelOutputPath(pwd)) {
    try {
      return findRepoFrom(Path.resolve(pwd).toString());
    } catch (_error) {
      // Fall through to Bazel-provided paths.
    }
  }

  const bazelWorkingDirectory = process.env.BUILD_WORKING_DIRECTORY;
  if (bazelWorkingDirectory && !isBazelOutputPath(bazelWorkingDirectory)) {
    try {
      return findRepoFrom(Path.resolve(bazelWorkingDirectory).toString());
    } catch (_error) {
      // Fall through to the process cwd; direct binary invocation will usually use it.
    }
  }

  const bazelWorkspace = process.env.BUILD_WORKSPACE_DIRECTORY;
  if (bazelWorkspace && !isBazelOutputPath(bazelWorkspace)) {
    const resolved = Path.resolve(bazelWorkspace);
    if (isValdiRepo(resolved.toString())) {
      return resolved.toString();
    }
  }

  const currentWorkingDirectory = fs.currentWorkingDirectory();
  if (isBazelOutputPath(currentWorkingDirectory)) {
    throw new Error(
      `Could not resolve the source repo from Bazel output directory ${currentWorkingDirectory}. Pass --repo explicitly.`,
    );
  }
  return findRepoFrom(currentWorkingDirectory);
}

function resolveRepo(repo: string | undefined): string {
  const resolved = repo ? Path.resolve(repo).toString() : defaultRepo();
  if (!isValdiRepo(resolved)) {
    throw new Error(`${resolved} is not a Valdi repo root. Pass --repo with the repository path.`);
  }
  return resolved;
}

async function run(command: string, args: string[], cwd?: string, inheritOutput = false): Promise<string> {
  if (inheritOutput) {
    const child = new ChildProcess(command);
    child.args = args;
    child.cwd = cwd;
    child.inheritOutput = true;
    await child.launch();
    await child.waitForExit();
    if (child.exitCode !== 0) {
      throw new Error(`${command} ${args.join(' ')} failed with ${child.exitCode}`);
    }
    return '';
  }

  const result = await ChildProcess.run(command, args, cwd);
  const stdout = arrayToString(result.stdout);
  const stderr = arrayToString(result.stderr);
  if (result.errorCode !== 0) {
    throw new Error(`${command} ${args.join(' ')} failed\n${stdout}\n${stderr}`);
  }
  return stdout;
}

function valdiCommand(valdiBin: string, args: string[], cwd: string): { command: string; args: string[] } {
  return {
    command: 'env',
    args: ['-C', cwd, '-u', 'BUILD_WORKING_DIRECTORY', '-u', 'BUILD_WORKSPACE_DIRECTORY', valdiBin, ...args],
  };
}

function adbArgs(deviceId: string | undefined, args: string[]): string[] {
  return deviceId ? ['-s', deviceId, ...args] : [...args];
}

async function androidFileExists(deviceId: string | undefined): Promise<boolean> {
  const result = await ChildProcess.run(
    'adb',
    adbArgs(deviceId, ['shell', 'run-as', PACKAGE, 'test', '-f', 'files/valdi-integration-test/results.json.done']),
  );
  return result.errorCode === 0;
}

async function readAndroidResult(deviceId: string | undefined): Promise<string> {
  return await run(
    'adb',
    adbArgs(deviceId, ['exec-out', 'run-as', PACKAGE, 'cat', 'files/valdi-integration-test/results.json']),
  );
}

async function iosContainer(deviceId: string | undefined): Promise<string> {
  return (await run('xcrun', ['simctl', 'get_app_container', deviceId || 'booted', PACKAGE, 'data'])).trim();
}

async function iosResultPath(deviceId: string | undefined): Promise<string> {
  return Path.fromString(await iosContainer(deviceId))
    .appending('Documents/Valdi/valdi-integration-test/results.json')
    .toString();
}

async function resultAvailable(platform: Platform, options: RunIntegrationTestsOptions): Promise<boolean> {
  if (platform === 'android') {
    return await androidFileExists(options.deviceId);
  }
  if (platform === 'ios') {
    try {
      return fs.existsSync(`${await iosResultPath(options.deviceId)}.done`);
    } catch (_error) {
      return false;
    }
  }
  return fs.existsSync('/tmp/valdi-integration-test/results.json.done');
}

async function readResult(platform: Platform, options: RunIntegrationTestsOptions): Promise<string> {
  if (platform === 'android') {
    return await readAndroidResult(options.deviceId);
  }
  if (platform === 'ios') {
    return fs.readFileSync(await iosResultPath(options.deviceId), { encoding: 'utf8' }) as string;
  }
  return fs.readFileSync('/tmp/valdi-integration-test/results.json', { encoding: 'utf8' }) as string;
}

async function clearExistingResult(platform: Platform, options: RunIntegrationTestsOptions): Promise<void> {
  try {
    if (platform === 'android') {
      await ChildProcess.run(
        'adb',
        adbArgs(options.deviceId, [
          'shell',
          'run-as',
          PACKAGE,
          'rm',
          '-f',
          'files/valdi-integration-test/results.json',
          'files/valdi-integration-test/results.json.done',
        ]),
      );
    } else if (platform === 'ios') {
      const resultPath = await iosResultPath(options.deviceId);
      if (fs.existsSync(resultPath)) {
        fs.removeSync(resultPath);
      }
      if (fs.existsSync(`${resultPath}.done`)) {
        fs.removeSync(`${resultPath}.done`);
      }
    } else {
      if (fs.existsSync('/tmp/valdi-integration-test/results.json')) {
        fs.removeSync('/tmp/valdi-integration-test/results.json');
      }
      if (fs.existsSync('/tmp/valdi-integration-test/results.json.done')) {
        fs.removeSync('/tmp/valdi-integration-test/results.json.done');
      }
    }
  } catch (_error) {
    // The app may not be installed yet; stale cleanup is best effort.
  }
}

async function launchAndroid(deviceId: string | undefined): Promise<void> {
  await run('adb', adbArgs(deviceId, ['shell', 'am', 'start', '-n', `${PACKAGE}/.StartActivity`]));
}

async function terminateApp(platform: Platform, deviceId: string | undefined): Promise<void> {
  try {
    if (platform === 'android') {
      await run('adb', adbArgs(deviceId, ['shell', 'am', 'force-stop', PACKAGE]));
    } else if (platform === 'ios') {
      await run('xcrun', ['simctl', 'terminate', deviceId || 'booted', PACKAGE]);
    }
  } catch (_error) {
    // Best-effort cleanup only.
  }
}

async function uninstallIos(deviceId: string | undefined): Promise<void> {
  try {
    await run('xcrun', ['simctl', 'uninstall', deviceId || 'booted', PACKAGE]);
  } catch (_error) {
    // The app may not be installed yet; stale app removal is best effort.
  }
}

async function waitForResult(platform: Platform, options: WaitForResultOptions): Promise<void> {
  const started = Date.now();
  const timeoutMs = Number(options.timeoutMs ?? 180000);
  while (Date.now() - started < timeoutMs) {
    if (await resultAvailable(platform, options)) {
      return;
    }
    if (options.child !== undefined && platform !== 'android') {
      if (options.child.exitCode !== undefined) {
        throw new Error(`valdi install exited before ${platform} result was available`);
      }
    }
    await wait(1000);
  }
  throw new Error(`Timed out waiting for ${platform} integration result after ${timeoutMs}ms`);
}

export async function runIntegrationTests(options: RunIntegrationTestsOptions): Promise<string> {
  const repo = resolveRepo(options.repo);
  const platform = options.platform;
  const output = Path.resolve(options.output);
  output.parent().ensureDirectory();

  if (platform === 'web') {
    return await runWebIntegrationTests({ ...options, repo, output: output.toString() });
  }

  const valdiBin = options.valdiBin ?? 'valdi';
  const installArgs = ['install', platform, '--application', APP_TARGET[platform]];
  if (platform === 'android') {
    installArgs.push('--enable_runtime_logs');
  }
  if (options.deviceId) {
    installArgs.push('--device_id', options.deviceId);
  }
  if (options.iosDeviceBuild) {
    installArgs.push('--simulator');
  }
  if (options.buildConfig) {
    installArgs.push('--build_config', options.buildConfig);
  }
  if (options.bazelArgs) {
    installArgs.push('--bazel_args', options.bazelArgs);
  }

  let child: ChildProcess | undefined;
  const installCommand = valdiCommand(valdiBin, installArgs, repo);
  if (platform === 'android') {
    await terminateApp(platform, options.deviceId);
    await run(installCommand.command, installCommand.args, repo, true);
    await clearExistingResult(platform, options);
    await launchAndroid(options.deviceId);
  } else if (platform === 'ios') {
    await terminateApp(platform, options.deviceId);
    await uninstallIos(options.deviceId);
    child = new ChildProcess(installCommand.command);
    child.args = installCommand.args;
    child.cwd = repo;
    child.inheritOutput = true;
    await child.launch();
  } else {
    await clearExistingResult(platform, options);
    child = new ChildProcess(installCommand.command);
    child.args = installCommand.args;
    child.cwd = repo;
    child.inheritOutput = true;
    await child.launch();
  }

  try {
    await waitForResult(platform, { ...options, child });
    fs.writeFileSync(output.toString(), await readResult(platform, options));
    console.log(`Wrote ${output}`);
    return output.toString();
  } finally {
    await terminateApp(platform, options.deviceId);
    if (child !== undefined) {
      child.kill();
    }
  }
}

export async function runFromArgs(args: string[]): Promise<string> {
  const parser = new ArgumentsParser('run', ['run', ...args]);
  const platformArg: Argument<string> = parser.addString('--platform', 'Platform: android, ios, macos, or web', true);
  const outputArg: Argument<string> = parser.addString('--output', 'Output JSON path', true);
  const repo: Argument<string> = parser.addString('--repo', 'Valdi repo root', false);
  const deviceId: Argument<string> = parser.addString('--device-id', 'Device or simulator id', false);
  const iosDeviceBuild: Argument<boolean> = parser.addFlag(
    '--ios-device-build',
    'Build for an iOS device instead of simulator',
    false,
  );
  const timeoutMs: Argument<string> = parser.addString('--timeout-ms', 'Timeout in milliseconds', false);
  const buildConfig: Argument<string> = parser.addString('--build-config', 'Valdi build config', false);
  const bazelArgs: Argument<string> = parser.addString(
    '--bazel-args',
    'Additional Bazel args for valdi install',
    false,
  );
  const valdiBin: Argument<string> = parser.addString('--valdi-bin', 'Valdi binary path/name', false);
  parser.parse();

  if (!isPlatform(platformArg.value)) {
    throw new Error(runUsage());
  }

  return await runIntegrationTests({
    repo: repo.value,
    platform: platformArg.value,
    output: outputArg.value!,
    deviceId: deviceId.value,
    iosDeviceBuild: iosDeviceBuild.value === true,
    timeoutMs: timeoutMs.value,
    buildConfig: buildConfig.value,
    bazelArgs: bazelArgs.value,
    valdiBin: valdiBin.value,
  });
}

export function runUsage(): string {
  return 'run --platform android|ios|macos|web --output result.json [--repo path] [--device-id id] [--timeout-ms 180000] [--ios-device-build]';
}
