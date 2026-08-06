import * as fs from 'fs';
import * as os from 'os';
import path from 'path';
import { STABLE_EXEC_BUILD_FLAGS } from '../src/core/constants';
import { BazelClient, bazelrcDefinesStableExec } from '../src/utils/BazelClient';

// spyOn / cache-poke needs access to BazelClient internals; narrow the cast to
// just those members instead of reaching for `any`.
type SpyableClient = {
  spawnCommand: (...args: unknown[]) => Promise<unknown>;
  runCommandWithLinesOutput: (...args: unknown[]) => Promise<unknown>;
  stableExecArgsCache: string | undefined;
};

// See Valdi#137.
describe('bazelrcDefinesStableExec', () => {
  let dir: string;

  beforeEach(() => {
    dir = fs.mkdtempSync(path.join(os.tmpdir(), 'valdi-stableexec-'));
    fs.writeFileSync(path.join(dir, 'MODULE.bazel'), '');
  });
  afterEach(() => fs.rmSync(dir, { recursive: true, force: true }));

  it('is true when the workspace .bazelrc defines the config', () => {
    fs.writeFileSync(path.join(dir, '.bazelrc'), 'build --foo\nbuild:stable_exec --notrim_test_configuration\n');
    expect(bazelrcDefinesStableExec(dir)).toBe(true);
  });

  it('is false when the .bazelrc does not define it', () => {
    fs.writeFileSync(path.join(dir, '.bazelrc'), 'build --foo\n');
    expect(bazelrcDefinesStableExec(dir)).toBe(false);
  });

  it('is false when the config only appears in a comment', () => {
    fs.writeFileSync(path.join(dir, '.bazelrc'), '# build:stable_exec --notrim_test_configuration\n');
    expect(bazelrcDefinesStableExec(dir)).toBe(false);
  });

  it('is false when the workspace has no .bazelrc', () => {
    expect(bazelrcDefinesStableExec(dir)).toBe(false);
  });
});

describe('stable_exec config in BazelClient', () => {
  const flag = STABLE_EXEC_BUILD_FLAGS.join(' ');
  let client: BazelClient;
  let spawn: jasmine.Spy;

  beforeEach(() => {
    client = new BazelClient();
    spawn = spyOn(client as unknown as SpyableClient, 'spawnCommand').and.returnValue(
      Promise.resolve({ stdout: '', stderr: '', returnCode: 0 }),
    );
  });

  it('exposes the opt-in config flag', () => {
    expect(STABLE_EXEC_BUILD_FLAGS).toEqual(['--config=stable_exec']);
  });

  it('injects the flag into build/buildTargets/test/run when the workspace defines the config', async () => {
    (client as unknown as SpyableClient).stableExecArgsCache = flag;

    await client.buildTarget('//foo');
    expect(spawn.calls.mostRecent().args[0]).toContain(flag);

    await client.buildTargets(['//foo', '//bar']);
    expect(spawn.calls.mostRecent().args[0]).toContain(flag);

    await client.testTargets(['//foo:test'], '');
    expect(spawn.calls.mostRecent().args[0]).toContain(flag);

    await client.runTarget('//foo');
    expect(spawn.calls.mostRecent().args[0]).toContain(flag);
  });

  it('omits the flag when the workspace does not define the config', async () => {
    (client as unknown as SpyableClient).stableExecArgsCache = '';
    await client.buildTarget('//foo');
    expect(spawn.calls.mostRecent().args[0]).not.toContain('stable_exec');
  });

  it('injects the flag into the output query so it matches the build config', async () => {
    (client as unknown as SpyableClient).stableExecArgsCache = flag;
    const lines = spyOn(client as unknown as SpyableClient, 'runCommandWithLinesOutput').and.returnValue(
      Promise.resolve([]),
    );
    await client.queryBuildOutputs(['//foo']);
    expect(lines.calls.mostRecent().args[0]).toContain(flag);
  });
});
