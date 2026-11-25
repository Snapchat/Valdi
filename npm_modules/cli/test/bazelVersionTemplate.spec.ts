import * as fs from 'fs';
import path from 'path';

const CLI_ROOT = path.resolve(__dirname, '..');
const TEMPLATE = path.join(CLI_ROOT, '.metadata', '.bazelversion.template');
const REPO_BAZELVERSION = path.resolve(CLI_ROOT, '..', '..', '.bazelversion');

describe('bootstrap .bazelversion template', () => {
  it('pins an exact Bazel version', () => {
    expect(fs.readFileSync(TEMPLATE, 'utf8').trim()).toMatch(/^\d+\.\d+\.\d+$/);
  });

  // The published CLI has no repo root next to it, so only check in-repo runs.
  it('matches the Bazel version Valdi CI uses', () => {
    if (!fs.existsSync(REPO_BAZELVERSION)) {
      pending('repo .bazelversion not present');
      return;
    }
    expect(fs.readFileSync(TEMPLATE, 'utf8').trim()).toBe(fs.readFileSync(REPO_BAZELVERSION, 'utf8').trim());
  });
});
