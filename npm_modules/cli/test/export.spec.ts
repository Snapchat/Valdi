import 'jasmine';
import * as fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { PLATFORM } from '../src/core/constants';
import { getExportedLibraryTargetTagForPlatform } from '../src/utils/applicationUtils';
import { syncDirectoryContents } from '../src/utils/directorySync';

describe('getExportedLibraryTargetTagForPlatform', () => {
  it('supports web exported libraries', () => {
    expect(getExportedLibraryTargetTagForPlatform(PLATFORM.WEB)).toBe('valdi_web_exported_library');
  });
});

describe('syncDirectoryContents', () => {
  let temporaryDirectory: string;
  let sourceDirectory: string;
  let destinationDirectory: string;

  beforeEach(() => {
    temporaryDirectory = fs.mkdtempSync(path.join(os.tmpdir(), 'valdi-directory-export-test-'));
    sourceDirectory = path.join(temporaryDirectory, 'source');
    destinationDirectory = path.join(temporaryDirectory, 'destination');
    fs.mkdirSync(path.join(sourceDirectory, 'nested'), { recursive: true });
    fs.mkdirSync(path.join(destinationDirectory, 'obsolete'), { recursive: true });
  });

  afterEach(() => {
    fs.rmSync(temporaryDirectory, { recursive: true, force: true });
  });

  it('writes only changed files and removes stale files', async () => {
    fs.writeFileSync(path.join(sourceDirectory, 'unchanged.js'), 'same');
    fs.writeFileSync(path.join(sourceDirectory, 'changed.js'), 'new');
    fs.writeFileSync(path.join(sourceDirectory, 'nested', 'added.js'), 'added');

    fs.writeFileSync(path.join(destinationDirectory, 'unchanged.js'), 'same');
    fs.writeFileSync(path.join(destinationDirectory, 'changed.js'), 'old');
    fs.writeFileSync(path.join(destinationDirectory, 'obsolete', 'stale.js'), 'stale');

    const oldTimestamp = new Date('2000-01-01T00:00:00.000Z');
    const unchangedPath = path.join(destinationDirectory, 'unchanged.js');
    const changedPath = path.join(destinationDirectory, 'changed.js');
    fs.utimesSync(unchangedPath, oldTimestamp, oldTimestamp);
    fs.utimesSync(changedPath, oldTimestamp, oldTimestamp);
    fs.chmodSync(unchangedPath, 0o555);
    fs.chmodSync(changedPath, 0o555);
    fs.chmodSync(path.join(destinationDirectory, 'obsolete', 'stale.js'), 0o444);
    fs.chmodSync(path.join(destinationDirectory, 'obsolete'), 0o555);
    fs.chmodSync(destinationDirectory, 0o555);
    const unchangedMtime = fs.statSync(unchangedPath).mtimeMs;

    const firstResult = await syncDirectoryContents(sourceDirectory, destinationDirectory);

    expect(firstResult).toEqual({
      added: 1,
      updated: 1,
      removed: 1,
      unchanged: 1,
    });
    expect(fs.readFileSync(changedPath, 'utf8')).toBe('new');
    expect(fs.statSync(unchangedPath).mtimeMs).toBe(unchangedMtime);
    expect(fs.statSync(changedPath).mtimeMs).toBeGreaterThan(oldTimestamp.getTime());
    expect(fs.existsSync(path.join(destinationDirectory, 'obsolete'))).toBeFalse();

    const outputFiles = [unchangedPath, changedPath, path.join(destinationDirectory, 'nested', 'added.js')];
    const mtimesBeforeSecondSync = outputFiles.map(filePath => fs.statSync(filePath).mtimeMs);
    const secondResult = await syncDirectoryContents(sourceDirectory, destinationDirectory);

    expect(secondResult).toEqual({
      added: 0,
      updated: 0,
      removed: 0,
      unchanged: 3,
    });
    expect(outputFiles.map(filePath => fs.statSync(filePath).mtimeMs)).toEqual(mtimesBeforeSecondSync);
  });

  it('rejects overlapping source and destination directories', async () => {
    await expectAsync(syncDirectoryContents(sourceDirectory, sourceDirectory)).toBeRejectedWithError(
      'Source and destination directories must not overlap',
    );
  });
});
