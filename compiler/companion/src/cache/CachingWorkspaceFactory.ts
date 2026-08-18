import path = require('path');
import { IWorkspace } from '../IWorkspace';
import { SQLiteCompilationCache } from './SQLiteCompilationCache';
import { CachingWorkspace } from './CachingWorkspace';
import { ILogger } from '../logger/ILogger';

/**
 * Should be incremented every time the workspace implementation changes.
 */
const CACHE_VERSION = '3';

export function getCompilationCacheVersion(nativeApiMinVersion: number | undefined): string {
  if (nativeApiMinVersion === undefined) {
    return CACHE_VERSION;
  }

  return `${CACHE_VERSION}/native-api-min-version-${nativeApiMinVersion}`;
}

export function createCachingWorkspace(
  cacheDir: string,
  sourceWorkspace: IWorkspace,
  logger: ILogger | undefined,
  nativeApiMinVersion: number | undefined,
): IWorkspace {
  const dbPath = path.resolve(cacheDir, 'compilecache.db');
  const compilationCache = new SQLiteCompilationCache(
    dbPath,
    getCompilationCacheVersion(nativeApiMinVersion),
    {
      getCurrentTimestamp() {
        return Date.now();
      },
    },
    logger,
  );

  return new CachingWorkspace(sourceWorkspace, compilationCache, logger);
}
