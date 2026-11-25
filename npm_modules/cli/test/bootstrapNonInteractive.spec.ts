import 'jasmine';
import { spawnSync } from 'node:child_process';
import * as fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const CLI_ROOT = path.resolve(__dirname, '..');
const TSX = path.join(CLI_ROOT, 'node_modules', '.bin', 'tsx');

describe('valdi bootstrap without a TTY', () => {
  let projectDir: string;

  beforeEach(() => {
    projectDir = fs.mkdtempSync(path.join(os.tmpdir(), 'valdi-bootstrap-'));
    // An already-initialized project, so --with-cleanup has something to delete.
    fs.writeFileSync(path.join(projectDir, 'MODULE.bazel'), '');
    fs.writeFileSync(path.join(projectDir, 'keep-me.txt'), 'important');
  });

  afterEach(() => {
    fs.rmSync(projectDir, { recursive: true, force: true });
  });

  it('fails on missing flags before --with-cleanup deletes anything', () => {
    // spawnSync pipes stdio, so the child sees no TTY on stdin.
    const result = spawnSync(TSX, [path.join(CLI_ROOT, 'src', 'index.ts'), 'bootstrap', '--with-cleanup'], {
      cwd: projectDir,
      encoding: 'utf8',
      input: '',
    });

    expect(result.status).not.toBe(0);
    expect(result.stdout + result.stderr).toContain('stdin is not a terminal');
    expect(fs.existsSync(path.join(projectDir, 'keep-me.txt'))).toBeTrue();
    expect(fs.existsSync(path.join(projectDir, 'MODULE.bazel'))).toBeTrue();
  });
});
