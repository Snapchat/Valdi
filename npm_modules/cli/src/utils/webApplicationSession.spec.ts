import 'jasmine';
import fs from 'node:fs';
import http from 'node:http';
import os from 'node:os';
import path from 'node:path';
import { startWebApplicationSession } from './webApplicationSession';
import { compressDirectoryContentsTo } from './zipUtils';

function get(url: string): Promise<string> {
  return new Promise((resolve, reject) => {
    http
      .get(url, response => {
        let body = '';
        response.setEncoding('utf8');
        response.on('data', chunk => {
          body += chunk;
        });
        response.on('end', () => resolve(body));
      })
      .once('error', reject);
  });
}

describe('webApplicationSession', () => {
  let tempDir: string;

  beforeEach(() => {
    tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'valdi-web-application-session-test-'));
  });

  afterEach(() => {
    fs.rmSync(tempDir, { force: true, recursive: true });
  });

  it('atomically publishes successful archives and preserves the last good generation', async () => {
    const sourceDir = path.join(tempDir, 'source');
    const archivePath = path.join(tempDir, 'application.zip');
    const workingDir = path.join(tempDir, 'session');
    fs.mkdirSync(sourceDir);
    fs.mkdirSync(workingDir);
    fs.writeFileSync(path.join(sourceDir, 'index.html'), '<html><body>first</body></html>');
    await compressDirectoryContentsTo(sourceDir, archivePath);

    const session = await startWebApplicationSession({
      archivePath,
      host: '127.0.0.1',
      liveReload: true,
      port: 0,
      workingDir,
    });

    try {
      expect(await get(session.url)).toContain('first');

      fs.writeFileSync(path.join(sourceDir, 'index.html'), '<html><body>second</body></html>');
      fs.rmSync(archivePath);
      await compressDirectoryContentsTo(sourceDir, archivePath);
      await session.publish();
      expect(await get(session.url)).toContain('second');

      fs.writeFileSync(archivePath, 'not a zip');
      await expectAsync(session.publish()).toBeRejected();
      expect(await get(session.url)).toContain('second');
    } finally {
      await session.close();
    }
  });
});
