import 'jasmine';
import { execSync } from 'child_process';
import * as fs from 'fs';
import path from 'path';
import * as vm from 'vm';

interface Deferred<Value> {
  promise: Promise<Value>;
  resolve: (value: Value) => void;
}

function createDeferred<Value>(): Deferred<Value> {
  let resolvePromise!: (value: Value) => void;
  const promise = new Promise<Value>(resolve => {
    resolvePromise = resolve;
  });
  return { promise, resolve: resolvePromise };
}

/**
 * Verifies that all directories required at runtime are included in the
 * published npm package. This prevents regressions like
 * https://github.com/Snapchat/Valdi/issues/93 where `valdi bootstrap` failed
 * because template files were missing from the tarball.
 */
describe('npm package contents', () => {
  let cliRoot: string;
  let packedFiles: string;

  beforeAll(() => {
    // The CLI test suite is emitted as CommonJS.
    // eslint-disable-next-line unicorn/prefer-module
    cliRoot = path.join(__dirname, '../..');
    packedFiles = execSync('npm pack --dry-run 2>&1', {
      cwd: cliRoot,
      encoding: 'utf8',
    });
  });

  it('includes .metadata templates for bootstrap config', () => {
    expect(packedFiles).toContain('.metadata/config.yaml.template');
    expect(packedFiles).toContain('.metadata/MODULE.bazel.template');
  });

  it('includes .bootstrap templates for project scaffolding', () => {
    expect(packedFiles).toContain('.bootstrap/');
  });

  it('includes dist output', () => {
    expect(packedFiles).toContain('dist/');
  });

  it('includes bundled-skills', () => {
    expect(packedFiles).toContain('bundled-skills/');
  });

  it('includes debugger assets', () => {
    const debuggerRoot = path.join(cliRoot, 'debugger');
    const indexHtml = fs.readFileSync(path.join(debuggerRoot, 'index.html'), 'utf8');
    const stylesheetPaths: string[] = [];
    for (const match of indexHtml.matchAll(/<link\s+[^>]*href=["']\.\/(.+?)["']/g)) {
      const stylesheetPath = match[1];
      if (stylesheetPath !== undefined) stylesheetPaths.push(stylesheetPath);
    }
    const scriptPaths: string[] = [];
    for (const match of indexHtml.matchAll(/<script\s+src=["']\.\/(.+?)["']/g)) {
      const scriptPath = match[1];
      if (scriptPath !== undefined) scriptPaths.push(scriptPath);
    }

    expect(packedFiles).toContain('debugger/index.html');
    expect(stylesheetPaths.length).toBeGreaterThan(0);
    expect(scriptPaths.length).toBeGreaterThan(0);
    for (const assetPath of [...stylesheetPaths, ...scriptPaths]) {
      expect(packedFiles).toContain(`debugger/${assetPath}`);
    }

    const orderedBundle = scriptPaths
      .map(scriptPath => fs.readFileSync(path.join(debuggerRoot, scriptPath), 'utf8'))
      .join('\n');
    expect(() => new vm.Script(orderedBundle, { filename: 'valdi-debugger.js' })).not.toThrow();
    for (const deferredSurface of [
      'renderDataProviders',
      'renderNetwork',
      'dispatchDebuggerInput',
      'capturePerformanceTrace',
      'renderWebPreviewFrame',
    ]) {
      expect(orderedBundle).not.toContain(deferredSurface);
    }
  });

  it('does not let projected trees auto-load remote media', () => {
    const previewSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-preview-html.js'), 'utf8');
    const policyResult = new vm.Script(
      `${previewSource}\n[
        previewSafeMediaSource('https://example.com/image.png'),
        previewSafeMediaSource('http://127.0.0.1:8080/private'),
        previewSafeMediaSource('data:image/png;base64,AA=='),
        previewSafeMediaSource('blob:http://127.0.0.1:8765/id')
      ];`,
      { filename: 'debugger-preview-html.js' },
    ).runInNewContext({}) as string[];

    expect(policyResult).toEqual(['', '', 'data:image/png;base64,AA==', 'blob:http://127.0.0.1:8765/id']);
    expect(previewSource).not.toContain('frame.src = source');
  });

  it('recovers an active CPU profile from the contexts response', async () => {
    const performanceSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-performance.js'), 'utf8');
    const bootstrapSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-bootstrap.js'), 'utf8');
    const performanceState = {
      profileActive: false,
      activeProfileContextId: null as string | null,
      lastProfile: null,
      profileContexts: [],
    };
    const profileContextSelect = {
      value: 'previous-context',
      innerHTML: '',
      disabled: false,
    };
    const profileStartButton = { disabled: false };
    const profileStopButton = { disabled: true };
    const context = {
      state: { performance: performanceState },
      elements: {
        profileStatusPill: { textContent: '', className: '' },
        profileStartButton,
        profileStopButton,
        profileCaptureButton: { disabled: false },
        profileExportButton: { disabled: false },
        profileSummary: { innerHTML: '' },
        profileContextSelect,
      },
      apiGet: () =>
        Promise.resolve({
          contexts: [
            { id: 'previous-context', title: 'Previous context' },
            { id: 'active-context', title: 'Active context' },
          ],
          active: {
            profiling: true,
            contextId: 'active-context',
            contextTitle: 'Active context',
          },
        }),
      addLog: () => {},
      escapeHtml: String,
    };

    await (new vm.Script(`${performanceSource}\nrefreshProfileContexts({ silent: true });`, {
      filename: 'debugger-performance.js',
    }).runInNewContext(context) as Promise<void>);

    expect(performanceState.profileActive).toBeTrue();
    expect(performanceState.activeProfileContextId).toBe('active-context');
    expect(profileContextSelect.innerHTML).toContain('value="active-context" selected');
    expect(profileContextSelect.disabled).toBeTrue();
    expect(profileStartButton.disabled).toBeTrue();
    expect(profileStopButton.disabled).toBeFalse();
    expect(bootstrapSource).toContain('void refreshProfileContexts({ silent: true });');
  });

  it('shows one-shot CPU capture as active until the profile completes', async () => {
    const performanceSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-performance.js'), 'utf8');
    const completedProfile = {
      profile: { nodes: [] },
      summary: { sampleCount: 3, nodeCount: 0 },
    };
    const pendingCapture = createDeferred<typeof completedProfile>();
    const performanceState = {
      profileActive: false,
      activeProfileContextId: null as string | null,
      lastProfile: null as typeof completedProfile | null,
      profileContexts: [{ id: 'requested-context', title: 'Requested context' }],
    };
    const profileStatusPill = { textContent: '', className: '' };
    const profileContextSelect = {
      value: 'requested-context',
      innerHTML: '',
      disabled: false,
    };
    const profileStartButton = { disabled: false };
    const profileStopButton = { disabled: true };
    const profileCaptureButton = { disabled: false };
    const context = {
      state: { performance: performanceState },
      elements: {
        profileStatusPill,
        profileStartButton,
        profileStopButton,
        profileCaptureButton,
        profileExportButton: { disabled: true },
        profileSummary: { innerHTML: '' },
        profileContextSelect,
        profileDurationInput: { value: '5' },
      },
      apiPost: () => pendingCapture.promise,
      addLog: () => {},
      escapeHtml: String,
    };

    const captureOperation = new vm.Script(`${performanceSource}\ncaptureCpuProfile();`, {
      filename: 'debugger-performance.js',
    }).runInNewContext(context) as Promise<void>;

    expect(performanceState.profileActive).toBeTrue();
    expect(performanceState.activeProfileContextId).toBe('requested-context');
    expect(profileStatusPill.textContent).toBe('Recording');
    expect(profileContextSelect.disabled).toBeTrue();
    expect(profileStartButton.disabled).toBeTrue();
    expect(profileStopButton.disabled).toBeFalse();
    expect(profileCaptureButton.disabled).toBeTrue();

    pendingCapture.resolve(completedProfile);
    await captureOperation;

    expect(performanceState.profileActive).toBeFalse();
    expect(performanceState.activeProfileContextId).toBeNull();
    expect(performanceState.lastProfile).toBe(completedProfile);
    expect(profileStatusPill.textContent).toBe('Idle');
    expect(profileContextSelect.disabled).toBeFalse();
    expect(profileStartButton.disabled).toBeFalse();
    expect(profileStopButton.disabled).toBeTrue();
    expect(profileCaptureButton.disabled).toBeFalse();
  });
});
