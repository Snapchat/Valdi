import 'jasmine';
import * as fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { compressDirectoryContentsTo, decompressTo } from './zipUtils';

const ZIP_FIXTURE =
  'UEsDBC0ACAAIAE+NtlwAAAAA//////////8BABQALQEAEAAAAAAAAAAAAAAAAAAAAAAAy0jNyclXSCvKz1WoyixQKEktLuECAFBLBwhZM7dWFgAAAAAAAAAUAAAAAAAAAFBLAQIeAy0ACAAIAE+NtlxZM7dWFgAAABQAAAABAAAAAAAAAAEAAACwEQAAAAAtUEsFBgAAAAABAAEALwAAAGEAAAAAAA==';

describe('decompressTo', () => {
  let temporaryDirectory: string;

  beforeEach(() => {
    temporaryDirectory = fs.mkdtempSync(path.join(os.tmpdir(), 'valdi-zip-test-'));
  });

  afterEach(() => {
    fs.rmSync(temporaryDirectory, { recursive: true, force: true });
  });

  it('extracts an archive into the output directory', async () => {
    const archivePath = path.join(temporaryDirectory, 'archive.zip');
    const outputPath = path.join(temporaryDirectory, 'output');
    fs.writeFileSync(archivePath, Buffer.from(ZIP_FIXTURE, 'base64'));

    await decompressTo(archivePath, outputPath);

    expect(fs.readFileSync(path.join(outputPath, '-'), 'utf8')).toBe('hello from zip test\n');
  });

  it('does not restore stale timestamps from reproducible archives', async () => {
    const sourcePath = path.join(temporaryDirectory, 'source');
    const archivePath = path.join(temporaryDirectory, 'archive.zip');
    const outputPath = path.join(temporaryDirectory, 'output');
    const sourceFilePath = path.join(sourcePath, 'icon.svg');
    const staleTimestamp = new Date('2010-01-01T00:00:00.000Z');
    fs.mkdirSync(sourcePath);
    fs.writeFileSync(sourceFilePath, '<svg></svg>');
    fs.utimesSync(sourceFilePath, staleTimestamp, staleTimestamp);

    await compressDirectoryContentsTo(sourcePath, archivePath);
    const extractionStartedAt = Date.now();
    await decompressTo(archivePath, outputPath);

    expect(fs.statSync(path.join(outputPath, 'icon.svg')).mtimeMs).toBeGreaterThanOrEqual(extractionStartedAt - 2000);
  });
});

describe('compressDirectoryContentsTo', () => {
  let temporaryDirectory: string;

  beforeEach(() => {
    temporaryDirectory = fs.mkdtempSync(path.join(os.tmpdir(), 'valdi-zip-test-'));
  });

  afterEach(() => {
    fs.rmSync(temporaryDirectory, { recursive: true, force: true });
  });

  it('archives the contents of a directory', async () => {
    const sourcePath = path.join(temporaryDirectory, 'source');
    const archivePath = path.join(temporaryDirectory, 'archive.zip');
    const outputPath = path.join(temporaryDirectory, 'output');
    fs.mkdirSync(path.join(sourcePath, 'nested'), { recursive: true });
    fs.writeFileSync(path.join(sourcePath, 'package.json'), '{"name":"test"}\n');
    fs.writeFileSync(path.join(sourcePath, 'nested', 'module.js'), 'module.exports = {};\n');

    await compressDirectoryContentsTo(sourcePath, archivePath);
    await decompressTo(archivePath, outputPath);

    expect(fs.readFileSync(path.join(outputPath, 'package.json'), 'utf8')).toBe('{"name":"test"}\n');
    expect(fs.readFileSync(path.join(outputPath, 'nested', 'module.js'), 'utf8')).toBe('module.exports = {};\n');
  });
});
