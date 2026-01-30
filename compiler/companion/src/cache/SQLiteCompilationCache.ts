import * as sqlite from 'better-sqlite3';
import * as fs from 'fs';
import * as os from 'os';
import { deflate, inflate } from 'zlib';
import { ILogger } from '../logger/ILogger';
import type { ICompilationCache } from './ICompilationCache';

export interface CompilationCacheEntry {
  container: string;
  path: string;
  hash: string;
  last_access_date: number;
  compressed: number;
  data: Buffer;
}

interface CompilationCacheMetadata {
  version: string;
}

export interface SQLiteCompilationCacheTimestampProvider {
  getCurrentTimestamp(): number;
}

/**
 * Should be updated every time the compilation cache implementation changes.
 */
const SCHEMA_VERSION = '1';
export const COMPRESSION_BYTES_THRESHOLD = 500;

function prepareDatabase(db: sqlite.Database, resolvedVersion: string): sqlite.Database {
  db.exec(`CREATE TABLE metadata(
    version STRING NOT NULL,
    PRIMARY KEY(version)
  )`);

  db.exec(`CREATE TABLE entries(
    path STRING NOT NULL,
    container STRING NOT NULL,
    hash STRING NOT NULL,
    last_access_date TIMESTAMP NOT NULL,
    compressed BOOLEAN NOT NULL,
    data BINARY,
    PRIMARY KEY(container, path, hash)
  )`);

  const insertMetadataStmt = db.prepare(
    `INSERT INTO metadata VALUES (?)
      `,
  );

  insertMetadataStmt.run(resolvedVersion);

  return db;
}

function createUnpreparedDatabase(filename: string): sqlite.Database {
  let nativeBinding: string;

  const platform = os.platform();
  const arch = os.arch();

  if (platform === 'darwin' && arch === 'arm64') {
    nativeBinding = require('../sqlite_bindings/better_sqlite3_macos_arm64.node');
  }else if (platform === 'darwin' && arch === 'x64') {
    nativeBinding = require('../sqlite_bindings/better_sqlite3_macos_x64.node');
  }else if (platform === 'linux' && arch === 'x64') {
    nativeBinding = require('../sqlite_bindings/better_sqlite3_linux_x86_64.node');
  } else {
    throw new Error(`Unspported platform and arch combination (${platform} on arch ${arch})`);
  }

  return sqlite(filename, { nativeBinding: nativeBinding });
}

function createDatabase(
  databasePath: string | undefined,
  workspaceVersion: string,
  logger: ILogger | undefined,
): sqlite.Database {
  const resolvedVersion = `${SCHEMA_VERSION}/${workspaceVersion}`;
  if (!databasePath) {
    return prepareDatabase(createUnpreparedDatabase(':memory:'), resolvedVersion);
  }

  if (!fs.existsSync(databasePath)) {
    logger?.debug?.('Creating new compilation cache database.');
    return prepareDatabase(createUnpreparedDatabase(databasePath), resolvedVersion);
  }

  const db = createUnpreparedDatabase(databasePath);

  try {
    const stmt = db.prepare('SELECT version FROM metadata');
    const metadata = stmt.get() as CompilationCacheMetadata;
    if (metadata.version !== resolvedVersion) {
      throw new Error('Schema is out of date');
    }
  } catch (err: any) {
    logger?.debug?.(`Compilation cache database needs wipe (${err.message}).`);
    db.close();
    fs.rmSync(databasePath);

    return prepareDatabase(createUnpreparedDatabase(databasePath), resolvedVersion);
  }

  return db;
}

// Flush after 5 seconds
const FLUSH_DEFERRED_STATEMENTS_DELAY_MS = 6000;

export class SQLiteCompilationCache implements ICompilationCache {
  private db: sqlite.Database;
  private getCachedEntryStmt: sqlite.Statement;
  private setCachedEntryStmt: sqlite.Statement;
  private updateLastAccessDate: sqlite.Statement;
  private evictEntriesBeforeTimeStmt: sqlite.Statement;
  private getAllCachedEntriesStmt: sqlite.Statement;

  private deferredStatements: (() => void)[] = [];
  private scheduledDeferredStatementTimeout: NodeJS.Timeout | undefined = undefined;

  constructor(
    databasePath: string | undefined,
    workspaceVersion: string,
    readonly timestampProvider: SQLiteCompilationCacheTimestampProvider,
    logger: ILogger | undefined,
  ) {
    this.db = createDatabase(databasePath, workspaceVersion, logger);

    this.getCachedEntryStmt = this.db.prepare(`
    SELECT data, compressed FROM entries
    WHERE path = ? AND container = ? AND hash = ?`);
    this.setCachedEntryStmt = this.db.prepare(
      `INSERT INTO entries VALUES (?, ?, ?, ?, ?, ?)
      ON CONFLICT(path, container, hash) DO UPDATE SET
        last_access_date=excluded.last_access_date,
        compressed=excluded.compressed,
        data=excluded.data
        `,
    );

    this.updateLastAccessDate = this.db.prepare(`
    UPDATE entries
    SET last_access_date = ?
    WHERE path = ? AND container = ? AND hash = ?
    `);
    this.evictEntriesBeforeTimeStmt = this.db.prepare(`
    DELETE FROM entries
    WHERE last_access_date < ?`);
    this.getAllCachedEntriesStmt = this.db.prepare(`
    SELECT * FROM entries`);
  }

  close(): void {
    this.flushDeferredStatements();
    this.db.close();
  }

  flushDeferredStatements(): void {
    if (this.scheduledDeferredStatementTimeout) {
      clearTimeout(this.scheduledDeferredStatementTimeout);
      this.scheduledDeferredStatementTimeout = undefined;
    }

    if (!this.deferredStatements.length) {
      return;
    }
    const deferredStatements = this.deferredStatements;
    this.deferredStatements = [];

    this.db.transaction((cbs) => {
      for (const cb of cbs) {
        cb();
      }
    })(deferredStatements);
  }

  evictEntriesBeforeTime(lastUsedTimestamp: number): void {
    this.flushDeferredStatements();
    this.evictEntriesBeforeTimeStmt.run(lastUsedTimestamp);
  }

  getAllCachedEntries(): CompilationCacheEntry[] {
    return this.getAllCachedEntriesStmt.all() as CompilationCacheEntry[];
  }

  private enqueueDeferredStatement(cb: () => void) {
    this.deferredStatements.push(cb);

    if (this.scheduledDeferredStatementTimeout) {
      clearTimeout(this.scheduledDeferredStatementTimeout);
    }

    this.scheduledDeferredStatementTimeout = setTimeout(
      () => this.flushDeferredStatements(),
      FLUSH_DEFERRED_STATEMENTS_DELAY_MS,
    );
  }

  async getCachedEntry(path: string, container: string, hash: string): Promise<string | undefined> {
    const timestamp = this.timestampProvider.getCurrentTimestamp();
    const result = this.getCachedEntryStmt.get(path, container, hash) as CompilationCacheEntry;

    if (!result) {
      return undefined;
    }

    this.enqueueDeferredStatement(() => {
      this.updateLastAccessDate.run(timestamp, path, container, hash);
    });

    if (result.compressed) {
      return new Promise((resolve, reject) => {
        inflate(result.data, (error, result) => {
          if (error) {
            reject(error);
          } else {
            resolve(result.toString('utf-8'));
          }
        });
      });
    } else {
      return result.data.toString('utf-8');
    }
  }

  async setCachedEntry(path: string, container: string, hash: string, data: string): Promise<void> {
    const timestamp = this.timestampProvider.getCurrentTimestamp();

    const buffer = Buffer.from(data, 'utf-8');

    if (buffer.length > COMPRESSION_BYTES_THRESHOLD) {
      return new Promise((resolve, reject) => {
        deflate(buffer, (error, result) => {
          if (error) {
            reject(error);
          } else {
            this.setCachedEntryStmt.run(path, container, hash, timestamp, 1, result);
            resolve();
          }
        });
      });
    } else {
      this.setCachedEntryStmt.run(path, container, hash, timestamp, 0, buffer);
    }
  }

  static newInMemoryInstance(timestampProvider: SQLiteCompilationCacheTimestampProvider): SQLiteCompilationCache {
    return new SQLiteCompilationCache(undefined, '1', timestampProvider, undefined);
  }
}
