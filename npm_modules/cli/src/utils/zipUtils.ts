import { execFile } from 'node:child_process';
import * as fs from 'node:fs';
import path from 'node:path';
import { promisify } from 'node:util';

const execFileAsync = promisify(execFile);

export async function decompressTo(inputFilePath: string, outputFilePath: string): Promise<void> {
  const archive = path.resolve(inputFilePath);
  const destination = path.resolve(outputFilePath);

  try {
    // `extract-zip` stalls or exits early with Node 26, which is the pinned
    // runtime used by mise. `unzip` works on the supported macOS/Linux hosts.
    // Avoid restoring stale reproducible-archive timestamps so temporary
    // directory cleaners do not delete assets from a live web session.
    await execFileAsync('unzip', ['-DD', '-q', '-o', archive, '-d', destination]);
  } catch (error: unknown) {
    throw new Error(`Failed to extract “${archive}” → “${destination}”: ${(error as Error).message}`);
  }
}

export async function compressDirectoryContentsTo(inputDirectoryPath: string, outputFilePath: string): Promise<void> {
  const sourceDirectory = path.resolve(inputDirectoryPath);
  const archive = path.resolve(outputFilePath);
  const stat = await fs.promises.stat(sourceDirectory);
  if (!stat.isDirectory()) {
    throw new Error(`Expected ${sourceDirectory} to be a directory`);
  }

  try {
    await execFileAsync('zip', ['-q', '-r', archive, '.'], { cwd: sourceDirectory });
  } catch (error: unknown) {
    throw new Error(`Failed to archive ${sourceDirectory} into ${archive}: ${(error as Error).message}`);
  }
}
