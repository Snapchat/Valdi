import 'jasmine';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {
  findWebHotReloadSessions,
  inferWebHotReloadTarget,
  publishWebHotReloadSession,
  registerWebHotReloadSession,
} from './webHotReloadSession';

describe('webHotReloadSession', () => {
  let tempDir: string;
  let registryDir: string;
  let workspaceRoot: string;

  beforeEach(() => {
    tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'valdi-web-session-test-'));
    registryDir = path.join(tempDir, 'registry');
    workspaceRoot = path.join(tempDir, 'workspace');
    fs.mkdirSync(workspaceRoot);
  });

  afterEach(() => {
    fs.rmSync(tempDir, { force: true, recursive: true });
  });

  it('maps generated web application labels to their hotreload target', () => {
    expect(inferWebHotReloadTarget('//modules/example:example_app_web')).toBe(
      '//modules/example:example_app_hotreload',
    );
    expect(inferWebHotReloadTarget('//modules/example:web_helpers')).toBeUndefined();
  });

  it('registers, discovers, publishes, and unregisters a running host', async () => {
    let publicationCount = 0;
    const registration = await registerWebHotReloadSession({
      applicationTarget: '//modules/example:example_app_web',
      bazelArgs: '--config=debug',
      hotreloadTarget: '//modules/example:example_app_hotreload',
      onPublish: () => {
        publicationCount += 1;
        return Promise.resolve();
      },
      registryDir,
      workspaceRoot,
    });

    try {
      const sessions = findWebHotReloadSessions(workspaceRoot, '//modules/example:example_app_hotreload', registryDir);
      expect(sessions.length).toBe(1);
      expect(sessions[0]?.applicationTarget).toBe('//modules/example:example_app_web');

      const session = sessions[0];
      if (!session) {
        fail('Expected a discovered web hot reload session');
        return;
      }
      await publishWebHotReloadSession(session);
      expect(publicationCount).toBe(1);
      expect(findWebHotReloadSessions(workspaceRoot, '//modules/other:other_hotreload', registryDir)).toEqual([]);
    } finally {
      await registration.close();
    }

    expect(findWebHotReloadSessions(workspaceRoot, undefined, registryDir)).toEqual([]);
  });

  it('rejects publication endpoints that are not loopback session controls', async () => {
    const registration = await registerWebHotReloadSession({
      applicationTarget: '//modules/example:example_app_web',
      bazelArgs: '',
      hotreloadTarget: '//modules/example:example_app_hotreload',
      onPublish: async () => {},
      registryDir,
      workspaceRoot,
    });

    try {
      await expectAsync(
        publishWebHotReloadSession({
          ...registration.descriptor,
          publishUrl: 'http://example.com/publish',
        }),
      ).toBeRejectedWithError(/Refusing non-loopback/);
    } finally {
      await registration.close();
    }
  });
});
