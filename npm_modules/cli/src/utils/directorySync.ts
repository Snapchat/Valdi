import fs from 'node:fs';
import path from 'node:path';

export interface DirectorySyncResult {
  added: number;
  updated: number;
  removed: number;
  unchanged: number;
}

let temporaryFileCounter = 0;

function isPathInside(parentPath: string, childPath: string): boolean {
  const relativePath = path.relative(parentPath, childPath);
  return (
    relativePath === '' ||
    (!relativePath.startsWith(`..${path.sep}`) && relativePath !== '..' && !path.isAbsolute(relativePath))
  );
}

async function countFiles(directoryPath: string): Promise<number> {
  const entries = await fs.promises.readdir(directoryPath, { withFileTypes: true });
  let count = 0;
  for (const entry of entries) {
    const entryPath = path.join(directoryPath, entry.name);
    count += entry.isDirectory() ? await countFiles(entryPath) : 1;
  }
  return count;
}

async function makeTreeWritable(entryPath: string): Promise<void> {
  const stats = await fs.promises.lstat(entryPath);
  if (stats.isDirectory()) {
    await fs.promises.chmod(entryPath, (stats.mode & 0o777) | 0o700);
    const entries = await fs.promises.readdir(entryPath);
    for (const entry of entries) {
      await makeTreeWritable(path.join(entryPath, entry));
    }
  } else if (stats.isFile()) {
    await fs.promises.chmod(entryPath, (stats.mode & 0o777) | 0o600);
  }
}

async function removeTree(entryPath: string): Promise<void> {
  await makeTreeWritable(entryPath);
  await fs.promises.rm(entryPath, { force: true, recursive: true });
}

async function filesHaveEqualContents(sourcePath: string, destinationPath: string): Promise<boolean> {
  const [sourceStats, destinationStats] = await Promise.all([
    fs.promises.stat(sourcePath),
    fs.promises.stat(destinationPath),
  ]);
  if (sourceStats.size !== destinationStats.size) {
    return false;
  }

  const [sourceContents, destinationContents] = await Promise.all([
    fs.promises.readFile(sourcePath),
    fs.promises.readFile(destinationPath),
  ]);
  return sourceContents.equals(destinationContents);
}

async function replaceFile(sourcePath: string, destinationPath: string, sourceMode: number): Promise<void> {
  const temporaryPath = `${destinationPath}.valdi-export-${process.pid}-${temporaryFileCounter++}`;
  try {
    await fs.promises.copyFile(sourcePath, temporaryPath);
    await fs.promises.chmod(temporaryPath, sourceMode);
    await fs.promises.rename(temporaryPath, destinationPath);
  } finally {
    await fs.promises.rm(temporaryPath, { force: true });
  }
}

async function syncFile(
  sourcePath: string,
  destinationPath: string,
  destinationEntry: fs.Dirent | undefined,
  result: DirectorySyncResult,
): Promise<void> {
  const sourceStats = await fs.promises.stat(sourcePath);
  const sourceMode = sourceStats.mode & 0o777;

  if (destinationEntry?.isFile() && (await filesHaveEqualContents(sourcePath, destinationPath))) {
    result.unchanged += 1;
    return;
  }

  if (destinationEntry?.isDirectory()) {
    const removedFileCount = await countFiles(destinationPath);
    result.removed += removedFileCount;
    await removeTree(destinationPath);
  } else if (destinationEntry?.isSymbolicLink()) {
    result.removed += 1;
    await fs.promises.rm(destinationPath);
  }

  await replaceFile(sourcePath, destinationPath, sourceMode);
  if (destinationEntry) {
    result.updated += 1;
  } else {
    result.added += 1;
  }
}

async function syncDirectory(
  sourceDirectory: string,
  destinationDirectory: string,
  result: DirectorySyncResult,
): Promise<void> {
  if (fs.existsSync(destinationDirectory)) {
    const destinationStats = await fs.promises.lstat(destinationDirectory);
    if (destinationStats.isDirectory()) {
      const destinationMode = destinationStats.mode & 0o777;
      const writableDestinationMode = destinationMode | 0o700;
      if (destinationMode !== writableDestinationMode) {
        await fs.promises.chmod(destinationDirectory, writableDestinationMode);
      }
    } else {
      result.removed += 1;
      await removeTree(destinationDirectory);
      await fs.promises.mkdir(destinationDirectory, { recursive: true });
    }
  } else {
    await fs.promises.mkdir(destinationDirectory, { recursive: true });
  }

  const [sourceEntries, destinationEntries] = await Promise.all([
    fs.promises.readdir(sourceDirectory, { withFileTypes: true }),
    fs.promises.readdir(destinationDirectory, { withFileTypes: true }),
  ]);
  const remainingDestinationEntries = new Map(destinationEntries.map(entry => [entry.name, entry]));

  await Promise.all(
    sourceEntries.map(async sourceEntry => {
      const sourcePath = path.join(sourceDirectory, sourceEntry.name);
      const destinationPath = path.join(destinationDirectory, sourceEntry.name);
      const destinationEntry = remainingDestinationEntries.get(sourceEntry.name);
      remainingDestinationEntries.delete(sourceEntry.name);

      if (sourceEntry.isDirectory()) {
        await syncDirectory(sourcePath, destinationPath, result);
      } else if (sourceEntry.isFile() || sourceEntry.isSymbolicLink()) {
        await syncFile(sourcePath, destinationPath, destinationEntry, result);
      } else {
        throw new Error(`Unsupported entry in exported web package: ${sourcePath}`);
      }
    }),
  );

  await Promise.all(
    Array.from(remainingDestinationEntries.values(), async destinationEntry => {
      const destinationPath = path.join(destinationDirectory, destinationEntry.name);
      const removedFileCount = destinationEntry.isDirectory() ? await countFiles(destinationPath) : 1;
      result.removed += removedFileCount;
      await removeTree(destinationPath);
    }),
  );
}

export async function syncDirectoryContents(
  sourceDirectory: string,
  destinationDirectory: string,
): Promise<DirectorySyncResult> {
  const resolvedSourceDirectory = path.resolve(sourceDirectory);
  const resolvedDestinationDirectory = path.resolve(destinationDirectory);
  if (
    isPathInside(resolvedSourceDirectory, resolvedDestinationDirectory) ||
    isPathInside(resolvedDestinationDirectory, resolvedSourceDirectory)
  ) {
    throw new Error('Source and destination directories must not overlap');
  }

  const sourceStats = await fs.promises.stat(resolvedSourceDirectory);
  if (!sourceStats.isDirectory()) {
    throw new Error(`Directory export source is not a directory: ${resolvedSourceDirectory}`);
  }

  const result: DirectorySyncResult = {
    added: 0,
    updated: 0,
    removed: 0,
    unchanged: 0,
  };
  await syncDirectory(resolvedSourceDirectory, resolvedDestinationDirectory, result);
  return result;
}
