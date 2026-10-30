/**
 * Test runner for Valdi Web integration tests.
 * Serves bundled app files and runs Jest/Puppeteer tests in headless Chrome.
 */

const path = require('path');
const fs = require('fs');
const { spawn } = require('child_process');
const { install, uninstall, computeExecutablePath } = require('@puppeteer/browsers');
const { startServer } = require('./static_server');

const CHROME_BUILD_ID = '131.0.6778.87';
const CHROME_BROWSER = 'chrome-headless-shell';

function chromePlatform() {
  if (process.platform === 'darwin') {
    return process.arch === 'arm64' ? 'mac_arm' : 'mac';
  }
  // Chrome for Testing publishes no linux-arm64 build.
  return 'linux';
}

// Defaults to the per-run TEST_TMPDIR: a cache inside the runfiles tree outlives the run,
// so one interrupted download left an empty install dir that failed every later run.
function chromeCacheDir() {
  return process.env.VALDI_WEB_CHROME_CACHE_DIR ||
    process.env.TEST_TMPDIR ||
    path.join(process.cwd(), 'tests', '.chrome-cache');
}

async function ensureChrome(cacheDir = chromeCacheDir()) {
  // The same platform must feed the cache check and the install, or they compute
  // different paths and the cache never hits.
  const options = { browser: CHROME_BROWSER, buildId: CHROME_BUILD_ID, cacheDir, platform: chromePlatform() };
  await fs.promises.mkdir(cacheDir, { recursive: true });

  const execPath = computeExecutablePath(options);
  try {
    await fs.promises.access(execPath, fs.constants.X_OK);
    return execPath;
  } catch {}

  // install() refuses to overwrite an install dir that has no executable.
  await uninstall(options);
  const result = await install(options);
  return result.executablePath;
}

function run(config) {
  const runfiles = process.env.RUNFILES_DIR || process.env.RUNFILES;
  if (!runfiles) {
    console.error('ERROR: RUNFILES_DIR not set');
    process.exit(1);
  }

  const staticDirs = [
    config.distPath ? path.join(runfiles, config.distPath) : null,
    path.join(runfiles, config.staticFilesPath),
  ].filter(Boolean);

  function runTests(chromePath, port) {
    return new Promise((resolve, reject) => {
      // jest's package.json `exports` field blocks direct
      // require.resolve('jest/bin/jest.js'). Resolve via package.json+bin
      // metadata instead, which stays inside the exports allowlist.
      const jestPkgJsonPath = require.resolve('jest/package.json');
      const jestPkgJson = require(jestPkgJsonPath);
      const jestPath = path.resolve(path.dirname(jestPkgJsonPath), jestPkgJson.bin);
      const jest = spawn(process.execPath, [
        jestPath,
        '--config', path.join(process.cwd(), 'jest.config.js'),
        '--testMatch', '**/tests/**/*.integration.test.js',
        '--runInBand',
        '--verbose',
        // Bazel runfiles present `tests/` as a symlinked dir. jest-haste-map's
        // node crawler skips symlinks by default and finds 0 tests; watchman
        // would follow them, so discovery silently depended on watchman being
        // on the runner. Pin the behavior: disable watchman and tell haste to
        // follow symlinks, so discovery is identical regardless of the image.
        '--watchman=false',
        '--haste',
        '{"enableSymlinks":true}',
      ], {
        cwd: process.cwd(),
        env: { ...process.env, PUPPETEER_EXECUTABLE_PATH: chromePath, BASE_URL: `http://localhost:${port}` },
        stdio: 'inherit',
      });

      jest.on('close', code => code === 0 ? resolve() : reject(new Error(`Tests failed: ${code}`)));
      jest.on('error', reject);
    });
  }

  async function main() {
    let server = null;
    try {
      const chromePath = await ensureChrome();
      server = await startServer({
        staticDirs,
        spaRoutes: config.spaRoutes || [],
      });
      const port = server.address().port;
      await runTests(chromePath, port);
      process.exit(0);
    } catch (err) {
      console.error('ERROR:', err.message);
      process.exit(1);
    } finally {
      server?.close();
    }
  }

  main();
}

module.exports = { run };
