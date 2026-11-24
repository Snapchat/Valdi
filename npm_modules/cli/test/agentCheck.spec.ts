import { spawnSync } from 'child_process';
import * as fs from 'fs';
import * as os from 'os';
import path from 'path';
import { makeCommandHandler } from '../src/utils/errorUtils';

// eslint-disable-next-line unicorn/prefer-module
const CLI_ROOT = path.resolve(__dirname, '..');

interface AgentCheckOutput {
  success: boolean;
  summary: { failed: number };
}
const TSX = path.join(CLI_ROOT, 'node_modules', '.bin', 'tsx');

describe('makeCommandHandler exit code', () => {
  let savedExitCode: typeof process.exitCode;
  beforeEach(() => {
    savedExitCode = process.exitCode;
    process.exitCode = undefined;
  });
  afterEach(() => {
    process.exitCode = savedExitCode;
  });

  it('keeps a nonzero exit code the command set without throwing', async () => {
    makeCommandHandler(() => {
      process.exitCode = 1;
      return Promise.resolve();
    })({});
    await new Promise(resolve => setTimeout(resolve, 0));
    expect(process.exitCode).toBe(1);
  });

  it('exits 0 when the command neither throws nor sets a code', async () => {
    makeCommandHandler(() => Promise.resolve())({});
    await new Promise(resolve => setTimeout(resolve, 0));
    expect(process.exitCode ?? 0).toBe(0);
  });
});

// Drives the real CLI against a fake bazel that is noisy on stdout.
describe('valdi agent-check --json', () => {
  let dir: string;

  function writeFakeBazel(buildExitCode: number) {
    const script = path.join(dir, 'fake-bazel.sh');
    fs.writeFileSync(
      script,
      [
        '#!/bin/bash',
        'case "$*" in',
        '  *"query"*"kind(test"*) exit 0 ;;',
        '  *"query"*) echo "//modules/m:m.android.debug"; exit 0 ;;',
        `  *"build"*) echo "INFO: Analyzing: target"; echo "INFO: Build completed"; exit ${buildExitCode} ;;`,
        'esac',
        'exit 0',
        '',
      ].join('\n'),
      { mode: 0o755 },
    );
    return script;
  }

  function runAgentCheck(buildExitCode: number) {
    const bazel = writeFakeBazel(buildExitCode);
    return spawnSync(TSX, [path.join(CLI_ROOT, 'src', 'index.ts'), 'agent-check', '--module', 'm', '--json'], {
      cwd: dir,
      encoding: 'utf8',
      env: { ...process.env, BAZEL_BIN: bazel },
    });
  }

  beforeEach(() => {
    dir = fs.mkdtempSync(path.join(os.tmpdir(), 'valdi-agentcheck-'));
    fs.writeFileSync(path.join(dir, 'MODULE.bazel'), '');
  });
  afterEach(() => fs.rmSync(dir, { recursive: true, force: true }));

  it('exits nonzero when a step fails and still prints only JSON on stdout', () => {
    const result = runAgentCheck(1);
    const parsed = JSON.parse(result.stdout) as AgentCheckOutput;
    expect(parsed.success).toBe(false);
    expect(parsed.summary.failed).toBe(1);
    expect(result.status).toBe(1);
  });

  it('exits 0 and prints only JSON on stdout when every step passes', () => {
    const result = runAgentCheck(0);
    const parsed = JSON.parse(result.stdout) as AgentCheckOutput;
    expect(parsed.success).toBe(true);
    expect(result.status).toBe(0);
    expect(result.stderr).toContain('Running Bazel command');
  });
});
