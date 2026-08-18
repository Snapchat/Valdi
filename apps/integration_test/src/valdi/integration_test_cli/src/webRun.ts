import { arrayToString } from 'coreutils/src/Uint8ArrayUtils';
import { TextDecoder } from 'coreutils/src/unicode/TextCoding';
import { fs } from 'file_system/src/FileSystem';
import { ChildProcess } from 'valdi_cli/src/ChildProcess';
import { Path } from 'valdi_cli/src/Path';
import { HTTPServer, HTTPServerRequest, HTTPServerResponse } from 'valdi_http/src/HTTPServer';

import type { RunIntegrationTestsOptions } from './run';

const WEB_PACKAGE_TARGET = '//apps/integration_test:integration_test_web_npm';
const WEB_HTML_TO_IMAGE_TARGET = '//bzl/valdi/npm:node_modules/html-to-image';
const WEB_WEBPACK_CLI_TARGET = '//compiler/companion:node_modules/webpack-cli';
const WEB_PACKAGE_RELATIVE_PATH = 'bazel-bin/apps/integration_test/integration_test_web_npm';
const WEB_NODE_MODULES_RELATIVE_PATH = 'bazel-bin/bzl/valdi/npm/node_modules';
const WEB_PACKAGE_NAME = 'integration_test_web_npm';
const RESULT_PATH = '/tmp/valdi-integration-test/results.json';
const PROGRESS_PATH = `${RESULT_PATH}.progress.json`;

async function run(command: string, args: string[], cwd?: string, inheritOutput = false): Promise<string> {
  if (inheritOutput) {
    const child = new ChildProcess(command);
    child.args = args;
    child.cwd = cwd;
    child.inheritOutput = true;
    await child.launch();
    const exitCode = await child.waitForExit();
    if (exitCode !== 0) {
      throw new Error(`${command} ${args.join(' ')} failed with ${exitCode}`);
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

function splitArgs(args: string | undefined): string[] {
  return args ? args.split(/\s+/).filter(value => value.length > 0) : [];
}

function requireFile(path: Path, description: string): string {
  const value = path.toString();
  if (!fs.existsSync(value)) {
    throw new Error(`${description} not found at ${value}`);
  }
  return value;
}

function readFileBytes(path: string): Uint8Array {
  const contents = fs.readFileSync(path);
  if (contents instanceof ArrayBuffer) {
    return new Uint8Array(contents);
  }
  throw new Error(`Expected ${path} to be read as bytes`);
}

function writeFile(path: Path, contents: string): void {
  path.parent().ensureDirectory();
  fs.writeFileSync(path.toString(), contents);
}

function prepareDirectory(path: Path): void {
  if (fs.existsSync(path.toString())) {
    fs.removeSync(path.toString());
  }
  path.ensureDirectory();
}

function jsString(value: string): string {
  return JSON.stringify(value);
}

function withTimeout<T>(promise: Promise<T>, timeoutMs: number, label: string, latestProgress: () => string): Promise<T> {
  let timeout: unknown;
  const timeoutPromise = new Promise<T>((_resolve, reject) => {
    timeout = setTimeout(() => {
      reject(new Error(`${label} timed out after ${timeoutMs}ms. Latest progress: ${latestProgress()}`));
    }, timeoutMs);
  });
  return Promise.race([promise, timeoutPromise]).finally(() => {
    clearTimeout(timeout as any);
  });
}

function contentType(path: string): string {
  if (path.endsWith('.html')) {
    return 'text/html; charset=utf-8';
  }
  if (path.endsWith('.js')) {
    return 'text/javascript; charset=utf-8';
  }
  if (path.endsWith('.json')) {
    return 'application/json; charset=utf-8';
  }
  if (path.endsWith('.png')) {
    return 'image/png';
  }
  if (path.endsWith('.svg')) {
    return 'image/svg+xml';
  }
  return 'application/octet-stream';
}

function summarizeProgress(contents: string): string {
  try {
    const progress = JSON.parse(contents) as {
      phase?: string;
      caseId?: string;
      currentIndex?: number;
      capturedCases?: number;
      error?: string;
    };
    const parts = [
      `phase=${progress.phase}`,
      `case=${progress.caseId || 'none'}`,
      `index=${progress.currentIndex}`,
      `captured=${progress.capturedCases}`,
    ];
    if (progress.error) {
      parts.push(`error=${progress.error}`);
    }
    return parts.join(' ');
  } catch (_error) {
    return contents.substring(0, 240);
  }
}

function parseJsonRequest(request: HTTPServerRequest): { path: string; contents?: string } {
  return JSON.parse(new TextDecoder('utf-8').decode(request.body.slice().buffer)) as { path: string; contents?: string };
}

function serveFile(distDir: string, requestPath: string): HTTPServerResponse {
  const decodedPath = decodeURIComponent(requestPath === '/' ? '/index.html' : requestPath);
  if (decodedPath.indexOf('..') >= 0) {
    return {
      statusCode: 403,
      headers: { 'content-type': 'text/plain; charset=utf-8' },
      body: 'forbidden',
    };
  }

  const root = Path.fromString(distDir).normalize().toString();
  const relativePath = decodedPath.startsWith('/') ? decodedPath.substring(1) : decodedPath;
  const filePath = Path.fromString(root).appending(relativePath).normalize().toString();
  if (!(filePath === root || filePath.startsWith(`${root}/`))) {
    return {
      statusCode: 403,
      headers: { 'content-type': 'text/plain; charset=utf-8' },
      body: 'forbidden',
    };
  }

  if (!fs.existsSync(filePath)) {
    return {
      statusCode: 404,
      headers: { 'content-type': 'text/plain; charset=utf-8' },
      body: 'not found',
    };
  }

  return {
    statusCode: 200,
    headers: { 'content-type': contentType(filePath) },
    body: readFileBytes(filePath),
  };
}

function addFileCandidate(candidates: string[], candidate: string | undefined): void {
  if (candidate && fs.existsSync(candidate)) {
    candidates.push(candidate);
  }
}

async function findBrowserCandidates(root: string | undefined): Promise<string[]> {
  if (!root || !fs.existsSync(root)) {
    return [];
  }

  const names = ['chrome-headless-shell', 'chrome', 'chromium', 'Google Chrome for Testing'];
  const matches: string[] = [];
  for (const name of names) {
    const result = await ChildProcess.run('find', [root, '-type', 'f', '-name', name]);
    if (result.errorCode === 0) {
      for (const line of arrayToString(result.stdout).split('\n')) {
        if (line.length > 0) {
          matches.push(line);
        }
      }
    }
  }
  return matches.sort().reverse();
}

function addHomeCandidate(out: Path[], home: string | undefined): void {
  if (!home) {
    return;
  }
  const normalized = Path.fromString(home).normalize();
  for (const existing of out) {
    if (existing.toString() === normalized.toString()) {
      return;
    }
  }
  out.push(normalized);
}

function inferredHomePaths(): Path[] {
  const out: Path[] = [];
  addHomeCandidate(out, process.env.HOME);

  const cwd = Path.fromString(fs.currentWorkingDirectory()).normalize().toString();
  const prefix = '/Users/';
  if (cwd.startsWith(prefix)) {
    const rest = cwd.substring(prefix.length);
    const slashIndex = rest.indexOf('/');
    if (slashIndex > 0) {
      addHomeCandidate(out, `${prefix}${rest.substring(0, slashIndex)}`);
    }
  }

  return out;
}

async function chromeExecutable(): Promise<string> {
  const candidates: string[] = [];
  addFileCandidate(candidates, process.env.CHROME_BIN);
  addFileCandidate(candidates, '/Applications/Google Chrome for Testing.app/Contents/MacOS/Google Chrome for Testing');
  for (const homePath of inferredHomePaths()) {
    const playwrightCandidates = await findBrowserCandidates(homePath.appending('Library/Caches/ms-playwright').normalize().toString());
    for (const candidate of playwrightCandidates) {
      addFileCandidate(candidates, candidate);
    }
    const puppeteerCandidates = await findBrowserCandidates(homePath.appending('Library/Caches/puppeteer').normalize().toString());
    for (const candidate of puppeteerCandidates) {
      addFileCandidate(candidates, candidate);
    }
  }
  addFileCandidate(candidates, '/Applications/Chromium.app/Contents/MacOS/Chromium');
  addFileCandidate(candidates, '/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge');
  addFileCandidate(candidates, '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome');

  if (candidates.length === 0) {
    throw new Error('No Chrome-compatible browser found. Set CHROME_BIN to an unmanaged Chrome, Chromium, Chrome for Testing, or chrome-headless-shell binary.');
  }
  return candidates[0]!;
}

function pathShimSource(): string {
  return `
function parts(value) {
  return String(value || '').split('/').filter(Boolean);
}

function normalize(value) {
  const absolute = String(value || '').startsWith('/');
  const out = [];
  for (const part of parts(value)) {
    if (part === '.') {
      continue;
    }
    if (part === '..') {
      out.pop();
    } else {
      out.push(part);
    }
  }
  return (absolute ? '/' : '') + out.join('/');
}

function join() {
  return normalize(Array.prototype.slice.call(arguments).join('/'));
}

function basename(value) {
  const normalized = normalize(value);
  const index = normalized.lastIndexOf('/');
  return index === -1 ? normalized : normalized.slice(index + 1);
}

module.exports = { basename, join, normalize };
`;
}

function harnessEntrySource(): string {
  return `
import { ValdiWebRenderer } from '${WEB_PACKAGE_NAME}/src/web_renderer/src/ValdiWebRenderer';

import { IntegrationTestApp } from '${WEB_PACKAGE_NAME}/src/integration_test_app/src/IntegrationTestApp';

const root = document.getElementById('root');
if (!root) {
  throw new Error('Missing #root');
}

const renderer = new ValdiWebRenderer(root);
renderer.renderRootComponent(IntegrationTestApp, {}, {}, {});
`;
}

function htmlSource(): string {
  return `<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Valdi Integration Web Harness</title>
  <style>
    html,
    body {
      margin: 0;
      width: 420px;
      height: 720px;
      overflow: hidden;
      background: #f8fafc;
    }

    #root {
      width: 420px;
      height: 720px;
    }
  </style>
</head>
<body>
  <div id="root"></div>
  <script src="./bundle.js"></script>
</body>
</html>
`;
}

function webpackConfigSource(packagePath: string, repo: string): string {
  return `
const path = require('path');

module.exports = {
  mode: 'development',
  devtool: false,
  entry: path.resolve(__dirname, 'src/index.js'),
  output: {
    path: path.resolve(__dirname, 'dist'),
    filename: 'bundle.js',
    clean: true,
  },
  resolve: {
    alias: {
      '${WEB_PACKAGE_NAME}': ${jsString(packagePath)},
      'path-browserify': path.resolve(__dirname, 'src/path-browserify-shim.js'),
    },
    extensions: ['.js', '.json'],
    modules: [
      path.resolve(__dirname, 'node_modules'),
      path.resolve(${jsString(repo)}, ${jsString(WEB_NODE_MODULES_RELATIVE_PATH)}),
      path.resolve(${jsString(repo)}, 'compiler/companion/node_modules'),
      'node_modules',
    ],
  },
  module: {
    rules: [
      {
        test: /\\.(png|jpe?g|svg|webp)$/i,
        type: 'asset/inline',
      },
    ],
  },
};
`;
}

function generateHarness(repo: string, packagePath: string, harnessDir: Path): { distDir: string; userDataDir: string } {
  prepareDirectory(harnessDir);
  const srcDir = harnessDir.appending('src');
  srcDir.ensureDirectory();
  writeFile(srcDir.appending('index.js'), harnessEntrySource());
  writeFile(srcDir.appending('path-browserify-shim.js'), pathShimSource());
  writeFile(harnessDir.appending('index.html'), htmlSource());
  writeFile(harnessDir.appending('webpack.config.js'), webpackConfigSource(packagePath, repo));

  return {
    distDir: harnessDir.appending('dist').toString(),
    userDataDir: harnessDir.appending('chrome-profile').toString(),
  };
}

function copyHtmlToDist(harnessDir: Path): void {
  const dist = harnessDir.appending('dist');
  dist.ensureDirectory();
  fs.writeFileSync(dist.appending('index.html').toString(), fs.readFileSync(harnessDir.appending('index.html').toString(), { encoding: 'utf8' }) as string);
}

async function buildWebPackage(repo: string, bazelArgs: string | undefined): Promise<string> {
  await run('bazel', [
    'build',
    WEB_PACKAGE_TARGET,
    WEB_HTML_TO_IMAGE_TARGET,
    WEB_WEBPACK_CLI_TARGET,
    '--define',
    'enable_web=true',
    '--snap_flavor=platform_development',
    '--@valdi//bzl/valdi:assets_mode=inline',
    ...splitArgs(bazelArgs),
  ], repo, true);
  return requireFile(Path.fromString(repo).appending(WEB_PACKAGE_RELATIVE_PATH), 'integration web package');
}

async function bundleHarness(repo: string, harnessDir: Path): Promise<void> {
  const webpackCli = requireFile(
    Path.fromString(repo).appending('bazel-bin/compiler/companion/node_modules/webpack-cli/bin/cli.js'),
    'webpack-cli',
  );
  await run('node', [webpackCli, '--config', harnessDir.appending('webpack.config.js').toString()], repo, true);
  copyHtmlToDist(harnessDir);
}

async function runWebHarness(harness: { distDir: string; userDataDir: string }, output: Path, timeoutMs: number): Promise<void> {
  let latestProgress = 'no progress received';
  let latestProgressLogged = '';
  let didFinish = false;
  let resolveResult: (() => void) | undefined;
  let rejectResult: ((error: Error) => void) | undefined;
  const resultPromise = new Promise<void>((resolve, reject) => {
    resolveResult = resolve;
    rejectResult = reject;
  });

  const server = new HTTPServer(request => {
    if (request.method === 'POST' && request.path === '/integration-test-file') {
      const payload = parseJsonRequest(request);
      if (payload.path === RESULT_PATH) {
        didFinish = true;
        output.parent().ensureDirectory();
        fs.writeFileSync(output.toString(), payload.contents ?? '');
        resolveResult?.();
      } else if (payload.path === PROGRESS_PATH) {
        latestProgress = summarizeProgress(payload.contents ?? '');
        if (latestProgress !== latestProgressLogged) {
          latestProgressLogged = latestProgress;
          console.log(`[web progress] ${latestProgress}`);
        }
      }
      return {
        statusCode: 200,
        headers: { 'content-type': 'text/plain; charset=utf-8' },
        body: 'ok',
      };
    }

    if (request.method === 'POST' && request.path === '/integration-test-mark') {
      return {
        statusCode: 200,
        headers: { 'content-type': 'text/plain; charset=utf-8' },
        body: 'ok',
      };
    }

    if (request.method !== 'GET') {
      return {
        statusCode: 405,
        headers: { 'content-type': 'text/plain; charset=utf-8' },
        body: 'method not allowed',
      };
    }

    return serveFile(harness.distDir, request.path);
  });

  let chrome: ChildProcess | undefined;
  try {
    await server.start(0);
    const pageUrl = `http://127.0.0.1:${server.port}/index.html`;
    const chromePath = await chromeExecutable();
    console.log(`[web harness] using browser ${chromePath}`);
    console.log(`[web harness] serving ${pageUrl}`);

    chrome = new ChildProcess(chromePath);
    chrome.args = [
      '--headless=new',
      '--disable-gpu',
      '--hide-scrollbars',
      '--no-first-run',
      '--no-default-browser-check',
      '--disable-background-networking',
      '--disable-component-update',
      '--disable-crash-reporter',
      '--disable-default-apps',
      '--disable-extensions',
      '--disable-sync',
      '--metrics-recording-only',
      '--log-level=2',
      '--force-device-scale-factor=3',
      '--window-size=420,720',
      `--user-data-dir=${harness.userDataDir}`,
      pageUrl,
    ];
    chrome.stderr = {
      onData(data: Uint8Array): void {
        const text = arrayToString(data);
        if (
          text.length > 0 &&
          text.indexOf(':INFO:CONSOLE:') < 0 &&
          text.indexOf('net/dns/address_sorter_posix') < 0
        ) {
          console.error(text);
        }
      },
    };
    chrome.exitListener = {
      onExit(errorCode: number): void {
        if (!didFinish) {
          rejectResult?.(new Error(`Browser exited before result with code ${errorCode}. Latest progress: ${latestProgress}`));
        }
      },
    };
    await chrome.launch();
    await withTimeout(resultPromise, timeoutMs, 'web integration result', () => latestProgress);
  } finally {
    chrome?.kill();
    server.stop();
  }
}

export async function runWebIntegrationTests(options: RunIntegrationTestsOptions): Promise<string> {
  const repo = Path.resolve(options.repo!).toString();
  const output = Path.resolve(options.output);
  const packagePath = await buildWebPackage(repo, options.bazelArgs);
  const harnessDir = Path.fromString('/tmp').appending(`valdi-integration-web-harness-${Date.now()}`);
  const harness = generateHarness(repo, packagePath, harnessDir);
  await bundleHarness(repo, harnessDir);
  await runWebHarness(harness, output, Number(options.timeoutMs ?? 180000));
  console.log(`Wrote ${output}`);
  return output.toString();
}
