/**
 * Test runner for Valdi Web integration tests.
 * Serves bundled app files and runs Jest/Puppeteer tests in headless Chrome.
 */

const path = require('path');
const fs = require('fs');
const { spawn } = require('child_process');
const { install, computeExecutablePath } = require('@puppeteer/browsers');
const { startServer } = require('./static_server');

function run(config) {
  const runfiles = process.env.RUNFILES_DIR || process.env.RUNFILES;
  if (!runfiles) {
    console.error('ERROR: RUNFILES_DIR not set');
    process.exit(1);
  }

  const chromeCache = path.join(process.cwd(), 'tests', '.chrome-cache');

  const staticDirs = [
    config.distPath ? path.join(runfiles, config.distPath) : null,
    path.join(runfiles, config.staticFilesPath),
  ].filter(Boolean);

  async function ensureChrome() {
    const buildId = '131.0.6778.87';
    const browser = 'chrome-headless-shell';
    const platform = process.platform === 'darwin' ? 'mac' : 'linux';

    try {
      await fs.promises.mkdir(chromeCache, { recursive: true });
    } catch {}

    const execPath = computeExecutablePath({ browser, buildId, cacheDir: chromeCache });
    try {
      await fs.promises.access(execPath, fs.constants.X_OK);
      return execPath;
    } catch {}

    const result = await install({ browser, buildId, cacheDir: chromeCache, platform });
    return result.executablePath;
  }

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
