/**
 * Runs the compiled coreutils web specs under Jest.
 *
 * The web implementations depend on browser globals (atob/btoa), so they cannot run in the
 * native standalone runtime that valdi_test uses; Node provides those globals instead.
 */

const { spawnSync } = require('child_process');
const path = require('path');
const fs = require('fs');

const runfiles = process.env.RUNFILES_DIR || process.env.RUNFILES;
if (!runfiles) {
  console.error('RUNFILES_DIR not set');
  process.exit(1);
}

// Runfiles are rooted at "_main" when this workspace is built directly, and under the valdi
// repo name when another workspace consumes it through bzlmod.
const workspacePrefixes = ['_main', 'valdi', '+local_repos+valdi'];

function resolveInRunfiles(relativePath) {
  for (const prefix of workspacePrefixes) {
    const candidate = path.join(runfiles, prefix, relativePath);
    if (fs.existsSync(candidate)) {
      return candidate;
    }
  }
  return null;
}

const nodeModulesPath = resolveInRunfiles('bzl/valdi/npm/node_modules');
if (!nodeModulesPath) {
  console.error(`Could not find node_modules under ${runfiles}`);
  process.exit(1);
}

process.env.NODE_PATH = nodeModulesPath + (process.env.NODE_PATH ? ':' + process.env.NODE_PATH : '');
require('module').Module._initPaths();

const jestBin = path.join(nodeModulesPath, 'jest/bin/jest.js');
if (!fs.existsSync(jestBin)) {
  console.error(`Jest binary not found at: ${jestBin}`);
  process.exit(1);
}

const testDir = resolveInRunfiles('src/valdi_modules/src/valdi/coreutils/web/test');
if (!testDir) {
  console.error(`Test directory not found under ${runfiles}`);
  process.exit(1);
}

const testFiles = fs
  .readdirSync(testDir)
  .filter((f) => f.endsWith('.spec.js'))
  .map((f) => path.join(testDir, f));

if (testFiles.length === 0) {
  console.error(`No .spec.js files found in ${testDir}`);
  process.exit(1);
}

// An explicit config keeps Jest from discovering specs outside this directory.
const configPath = path.join(testDir, 'jest.config.json');
fs.writeFileSync(
  configPath,
  JSON.stringify({
    testEnvironment: 'node',
    roots: [testDir],
    testMatch: ['**/*.spec.js'],
  }),
);

console.log('Running tests:', testFiles);
const result = spawnSync('node', [jestBin, '--config', configPath, '--verbose', '--runTestsByPath', ...testFiles], {
  stdio: 'inherit',
  cwd: testDir,
});

try {
  fs.unlinkSync(configPath);
} catch {
  // Ignore cleanup failures; the sandbox is discarded anyway.
}

process.exit(result.status === null ? 1 : result.status);
