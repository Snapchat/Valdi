import 'jasmine/src/jasmine';
import { arrayToString, stringToArray } from 'coreutils/src/Uint8ArrayUtils';
import { ChildProcess, ChildProcessPipeBuffer } from 'valdi_cli/src/ChildProcess';

function describeData(data: Uint8Array): string {
  return `${data.length} bytes: ${arrayToString(data)}`;
}

describe('ChildProcess', () => {
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

  it('launches with a working directory', async () => {
    const result = await ChildProcess.run('/bin/sh', ['-c', 'pwd'], '/');

    expect(result.errorCode).toBe(0);
    expect(arrayToString(result.stdout).trim()).toBe('/');
  }, 5000);

  it('reports native launch failures as real errors', async () => {
    await expectAsync(ChildProcess.run('/definitely/not/a/command', [])).toBeRejectedWithError(
      /posix_spawnp\(\/definitely\/not\/a\/command\) failed:/,
    );
  }, 5000);

  it('rejects waiting before launch', async () => {
    const child = new ChildProcess('/bin/sh');

    await expectAsync(child.waitForExit()).toBeRejectedWithError(/Cannot wait for \/bin\/sh before launch\(\)/);
  }, 5000);
});
