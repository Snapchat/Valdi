import path = require('path');
import { IWorkspace } from '../IWorkspace';
import { SQLiteCompilationCache } from './SQLiteCompilationCache';
import { CachingWorkspace } from './CachingWorkspace';
import { ILogger } from '../logger/ILogger';

/**
 * Should be incremented every time the workspace implementation changes.
 */
const CACHE_VERSION = '2';

export function createCachingWorkspace(
  cacheDir: string,
  sourceWorkspace: IWorkspace,
  logger: ILogger | undefined,
): IWorkspace {
  const dbPath = path.resolve(cacheDir, 'compilecache.db');
  const compilationCache = new SQLiteCompilationCache(
    dbPath,
    CACHE_VERSION,
    {
      getCurrentTimestamp() {
        return Date.now();
      },
    },
    logger,
  );

  return new CachingWorkspace(sourceWorkspace, compilationCache, logger);
}
