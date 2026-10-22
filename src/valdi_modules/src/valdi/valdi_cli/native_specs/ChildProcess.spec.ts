import 'jasmine/src/jasmine';
import { arrayToString, stringToArray } from 'coreutils/src/Uint8ArrayUtils';
import { ChildProcess, ChildProcessPipeBuffer } from 'valdi_cli/src/ChildProcess';
import { RequireFunc } from 'valdi_core/src/IModuleLoader';

declare const require: RequireFunc;

// The module test runner doesn't link the native ChildProcess module, so these specs only run in hosts that do.
function isChildProcessNativeAvailable(): boolean {
  try {
    require('valdi_cli/src/ChildProcessNative', true);
    return true;
  } catch {
    return false;
  }
}

const describeIfNative = isChildProcessNativeAvailable() ? describe : xdescribe;

function describeData(data: Uint8Array): string {
  return `${data.length} bytes: ${arrayToString(data)}`;
}

describeIfNative('ChildProcess', () => {
  it('captures stdout from a real child process', async () => {
    const result = await ChildProcess.run('/bin/sh', ['-c', 'printf "hello"']);

    expect(result.errorCode).toBe(0);
    expect(arrayToString(result.stdout)).toBe('hello');
    expect(result.stderr.length).toBe(0, describeData(result.stderr));
  }, 5000);

  it('captures stderr and exit code from a real child process', async () => {
    const result = await ChildProcess.run('/bin/sh', ['-c', 'printf "bad" >&2; exit 7']);

    expect(result.errorCode).toBe(7);
    expect(result.stdout.length).toBe(0, describeData(result.stdout));
    expect(arrayToString(result.stderr)).toBe('bad');
  }, 5000);

  it('sends stdin and resolves the exit listener', async () => {
    const child = new ChildProcess('/bin/sh');
    const stdout = new ChildProcessPipeBuffer();
    let exitCode: number | undefined;

    child.args = ['-c', 'IFS= read -r line; printf "got:%s" "$line"'];
    child.stdout = stdout;
    child.exitListener = {
      onExit(code: number): void {
        exitCode = code;
      },
    };

    await child.launch();
    child.sendToStdin(stringToArray('hello\n'));
    const waitExitCode = await child.waitForExit();

    expect(waitExitCode).toBe(0);
    expect(child.exitCode).toBe(0);
    expect(exitCode).toBe(0);
    expect(stdout.toString()).toBe('got:hello');
  }, 5000);

  it('closes stdin for run() so children reading to EOF exit', async () => {
    const result = await ChildProcess.run('/bin/cat', []);

    expect(result.errorCode).toBe(0);
    expect(result.stdout.length).toBe(0, describeData(result.stdout));
  }, 5000);

  it('closes stdin after queued input is written', async () => {
    const child = new ChildProcess('/bin/cat');
    const stdout = new ChildProcessPipeBuffer();
    child.stdout = stdout;

    await child.launch();
    child.sendToStdin(stringToArray('first '));
    child.sendToStdin(stringToArray('second'));
    child.closeStdin();

    expect(await child.waitForExit()).toBe(0);
    expect(stdout.toString()).toBe('first second');
  }, 5000);

  it('survives writing to a child that closed its stdin', async () => {
    const child = new ChildProcess('/bin/sh');
    const stdout = new ChildProcessPipeBuffer();
    child.args = ['-c', 'exec 0<&-; echo closed; sleep 1'];
    child.stdout = stdout;

    await child.launch();
    while (!stdout.toString().includes('closed')) {
      await new Promise<void>(resolve => setTimeout(resolve, 10));
    }
    // Without SIGPIPE suppression this write terminates the host.
    child.sendToStdin(new Uint8Array(65536));

    expect(await child.waitForExit()).toBe(0);
  }, 5000);

  it('launches with a working directory', async () => {
    const result = await ChildProcess.run('/bin/sh', ['-c', 'pwd'], '/');

    expect(result.errorCode).toBe(0);
    expect(arrayToString(result.stdout).trim()).toBe('/');
  }, 5000);

  it('terminates a running child with SIGTERM', async () => {
    const child = new ChildProcess('/bin/sh');
    child.args = ['-c', 'sleep 10'];

    await child.launch();
    child.kill();

    expect(await child.waitForExit()).toBe(128 + 15);
  }, 5000);

  it('does not signal pids it did not spawn', async () => {
    const native = require('valdi_cli/src/ChildProcessNative', true) as typeof import('valdi_cli/src/ChildProcessNative');
    // The grandchild sleep outlives its parent shell and is never tracked by the native module.
    const started = await ChildProcess.run('/bin/sh', ['-c', 'sleep 5 >/dev/null 2>&1 & echo $!']);
    const untrackedPid = Number.parseInt(arrayToString(started.stdout).trim(), 10);
    expect(untrackedPid).toBeGreaterThan(0);

    try {
      native.kill(untrackedPid);

      const probe = await ChildProcess.run('/bin/sh', ['-c', `kill -0 ${untrackedPid}`]);
      expect(probe.errorCode).toBe(0, 'untracked process should still be alive');
    } finally {
      await ChildProcess.run('/bin/sh', ['-c', `kill ${untrackedPid} 2>/dev/null; true`]);
    }
  }, 5000);

  it('reports native launch failures as real errors', async () => {
    await expectAsync(ChildProcess.run('/definitely/not/a/command', [])).toBeRejectedWithError(
      /posix_spawnp\(\/definitely\/not\/a\/command\) failed:/,
    );
  }, 5000);

  it('rejects waiting after a failed launch instead of hanging', async () => {
    const child = new ChildProcess('/definitely/not/a/command');

    await expectAsync(child.launch()).toBeRejected();
    await expectAsync(child.waitForExit()).toBeRejectedWithError(/before launch\(\)/);
  }, 5000);

  it('rejects waiting before launch', async () => {
    const child = new ChildProcess('/bin/sh');

    await expectAsync(child.waitForExit()).toBeRejectedWithError(/Cannot wait for \/bin\/sh before launch\(\)/);
  }, 5000);
});
