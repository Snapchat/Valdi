import {
  createDebuggerProviderOwner,
  createDebuggerProviderResult,
  DebuggerProviderKind,
} from 'valdi_core/src/debugging/DebuggerProvider';
import type {
  DebuggerProviderModule,
  DebuggerProviderOwner,
  DebuggerProviderRegistration,
  DebuggerProviderRequest,
  DebuggerProviderResult,
} from 'valdi_core/src/debugging/DebuggerProvider';

declare const module: DebuggerProviderModule;

export type ClientSQLDebugValue = string | number | boolean | ArrayBuffer | null;

export interface ClientSQLDebugColumn {
  cid: number;
  name: string;
  type: string;
  notnull: number;
  dflt_value: unknown;
  pk: number;
}

export interface ClientSQLDebugDatabase {
  id: string;
  name: string;
  schemaVersion: number;
  createStatements: string[];
  migrations: { version: number; statements: string[] }[];
  query<T>(sql: string, parameters: ClientSQLDebugValue[] | undefined): Promise<T[]>;
  debugInfo?(): Promise<Record<string, unknown>>;
}

interface ClientSQLDebugTableSummary {
  name: string;
  type: string;
  sql: string | null;
  rowCount: number | null;
  rowCountIsLowerBound: boolean;
  columns: ClientSQLDebugColumn[];
  truncation: {
    columns: boolean;
    omittedColumnsAtLeast: number;
    rowCount: boolean;
  };
}

interface ClientSQLDebugTruncationMetadata {
  truncated: boolean;
  reasons: string[];
  limits: {
    databases: number;
    tables: number;
    columnsPerTable: number;
    schemaColumns: number;
    rows: number;
    cells: number;
    collectionItems: number;
    stringCharacters: number;
    blobBytes: number;
    payloadBytes: number;
  };
  omittedDatabases: number;
  omittedTables: number;
  omittedColumns: number;
  omittedRows: number;
  omittedCells: number;
  omittedValues: number;
  truncatedStrings: number;
  truncatedBlobs: number;
  payloadBytes: number;
}

const CLIENT_SQL_DEBUG_SCHEMA_CACHE_TTL_MS = 2000;
const CLIENT_SQL_DEBUG_MAX_DATABASES = 8;
const CLIENT_SQL_DEBUG_MAX_TABLES = 32;
const CLIENT_SQL_DEBUG_MAX_COLUMNS_PER_TABLE = 50;
const CLIENT_SQL_DEBUG_MAX_SCHEMA_COLUMNS = 512;
const CLIENT_SQL_DEBUG_DEFAULT_ROWS = 100;
const CLIENT_SQL_DEBUG_MAX_ROWS = 100;
const CLIENT_SQL_DEBUG_MAX_OFFSET = 1000000;
const CLIENT_SQL_DEBUG_MAX_CELLS = 5000;
const CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS = 4096;
const CLIENT_SQL_DEBUG_MAX_BLOB_BYTES = 4096;
// Keep adapter documents comfortably below the generic provider's 48 KiB action-document cap.
const CLIENT_SQL_DEBUG_MAX_PAYLOAD_BYTES = 40 * 1024;
// Reserve space for the fixed truncation envelope inside the adapter document cap.
const CLIENT_SQL_DEBUG_TRUNCATION_METADATA_BYTES = 2048;
const CLIENT_SQL_DEBUG_MAX_CONTENT_BYTES =
  CLIENT_SQL_DEBUG_MAX_PAYLOAD_BYTES - CLIENT_SQL_DEBUG_TRUNCATION_METADATA_BYTES;
const CLIENT_SQL_DEBUG_MAX_QUERY_VALUE_BYTES = 8 * 1024 * 1024;
const CLIENT_SQL_DEBUG_MAX_VALUES = 8192;
// Match the generic provider parser: it rejects collection item 101.
const CLIENT_SQL_DEBUG_MAX_COLLECTION_ITEMS = 100;
const CLIENT_SQL_DEBUG_MAX_VALUE_DEPTH = 8;
const CLIENT_SQL_DEBUG_MAX_ROW_COUNT_SCAN = 10000;
const CLIENT_SQL_DEBUG_ESTIMATED_UTF8_BYTES_PER_CHARACTER = 4;
const CLIENT_SQL_DEBUG_PROVIDER_OWNER_KEY = 'client_sql/src/ClientSQLDebug';
const databasesById: { [id: string]: ClientSQLDebugDatabase[] | undefined } = Object.create(null);
const databaseSummaryCacheById: {
  [id: string]: { cachedAt: number; summary: Record<string, unknown> } | undefined;
} = Object.create(null);
let debugProviderRegistration: DebuggerProviderRegistration | undefined;
let debugProviderOwner: DebuggerProviderOwner | undefined;
let debugRevision = 0;

class ClientSQLDebugPayloadLimiter {
  private reasons: string[] = [];
  private bytesUsed = 0;
  private valueCount = 0;
  private omittedDatabases = 0;
  private omittedTables = 0;
  private omittedColumns = 0;
  private omittedRows = 0;
  private omittedCells = 0;
  private omittedValues = 0;
  private truncatedStrings = 0;
  private truncatedBlobs = 0;

  absorbMetadata(metadata: unknown): void {
    if (!metadata || typeof metadata !== 'object') {
      return;
    }
    const source = metadata as Partial<ClientSQLDebugTruncationMetadata>;
    if (Array.isArray(source.reasons)) {
      source.reasons.forEach(reason => {
        if (typeof reason === 'string') {
          this.noteReason(reason);
        }
      });
    }
    this.omittedDatabases += this.metadataCount(source.omittedDatabases);
    this.omittedTables += this.metadataCount(source.omittedTables);
    this.omittedColumns += this.metadataCount(source.omittedColumns);
    this.omittedRows += this.metadataCount(source.omittedRows);
    this.omittedCells += this.metadataCount(source.omittedCells);
    this.omittedValues += this.metadataCount(source.omittedValues);
    this.truncatedStrings += this.metadataCount(source.truncatedStrings);
    this.truncatedBlobs += this.metadataCount(source.truncatedBlobs);
  }

  noteReason(reason: string): void {
    if (this.reasons.indexOf(reason) === -1) {
      this.reasons.push(reason);
    }
  }

  noteOmittedDatabases(count: number): void {
    if (count > 0) {
      this.omittedDatabases += count;
      this.noteReason('databases');
    }
  }

  noteOmittedTables(count: number): void {
    if (count > 0) {
      this.omittedTables += count;
      this.noteReason('tables');
    }
  }

  noteOmittedColumns(count: number): void {
    if (count > 0) {
      this.omittedColumns += count;
      this.noteReason('columns');
    }
  }

  noteOmittedRows(count: number): void {
    if (count > 0) {
      this.omittedRows += count;
      this.noteReason('rows');
    }
  }

  noteOmittedCells(count: number): void {
    if (count > 0) {
      this.omittedCells += count;
      this.noteReason('cells');
    }
  }

  noteTruncatedString(): void {
    this.truncatedStrings += 1;
    this.noteReason('stringValues');
  }

  noteTruncatedBlob(): void {
    this.truncatedBlobs += 1;
    this.noteReason('blobValues');
  }

  limitValue(value: unknown, depth: number): unknown {
    if (depth > CLIENT_SQL_DEBUG_MAX_VALUE_DEPTH) {
      this.omittedValues += 1;
      this.noteReason('valueDepth');
      return null;
    }
    if (this.valueCount >= CLIENT_SQL_DEBUG_MAX_VALUES) {
      this.omittedValues += 1;
      this.noteReason('values');
      return null;
    }
    this.valueCount += 1;

    if (value === null || value === undefined) {
      this.reservePrimitive();
      return null;
    }
    if (typeof value === 'string') {
      return this.limitString(value);
    }
    if (typeof value === 'number') {
      if (!Number.isFinite(value)) {
        this.noteReason('nonFiniteValues');
        return this.limitString(String(value));
      }
      return this.reservePrimitive() ? value : null;
    }
    if (typeof value === 'boolean') {
      return this.reservePrimitive() ? value : null;
    }
    if (value instanceof ArrayBuffer) {
      return this.limitBlob(value);
    }
    if (Array.isArray(value)) {
      if (!this.reserveBytes(8)) {
        return [];
      }
      const values = value.slice(0, CLIENT_SQL_DEBUG_MAX_COLLECTION_ITEMS);
      if (values.length < value.length) {
        this.omittedValues += value.length - values.length;
        this.noteReason('collectionValues');
      }
      const output: unknown[] = [];
      for (let index = 0; index < values.length; index += 1) {
        if (this.isExhausted()) {
          this.omittedValues += values.length - index;
          this.noteReason('payloadBytes');
          break;
        }
        output.push(this.limitValue(values[index], depth + 1));
      }
      return output;
    }
    if (typeof value === 'object') {
      if (!this.reserveBytes(16)) {
        return {};
      }
      const input = value as Record<string, unknown>;
      const keys = Object.keys(input);
      const selectedKeys = keys.slice(0, CLIENT_SQL_DEBUG_MAX_COLLECTION_ITEMS);
      if (selectedKeys.length < keys.length) {
        this.omittedValues += keys.length - selectedKeys.length;
        this.noteReason('objectProperties');
      }
      const output: Record<string, unknown> = {};
      for (let index = 0; index < selectedKeys.length; index += 1) {
        if (this.isExhausted()) {
          this.omittedValues += selectedKeys.length - index;
          this.noteReason('payloadBytes');
          break;
        }
        const key = selectedKeys[index];
        let limitedKey = this.limitString(key);
        if (Object.prototype.hasOwnProperty.call(output, limitedKey)) {
          limitedKey = `${limitedKey.slice(0, CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS - 16)}#${index}`;
          this.noteReason('propertyNames');
        }
        output[limitedKey] = this.limitValue(input[key], depth + 1);
      }
      return output;
    }
    return this.limitString(String(value));
  }

  metadata(payloadBytes: number): ClientSQLDebugTruncationMetadata {
    return {
      truncated: this.reasons.length > 0,
      reasons: this.reasons.slice(),
      limits: {
        databases: CLIENT_SQL_DEBUG_MAX_DATABASES,
        tables: CLIENT_SQL_DEBUG_MAX_TABLES,
        columnsPerTable: CLIENT_SQL_DEBUG_MAX_COLUMNS_PER_TABLE,
        schemaColumns: CLIENT_SQL_DEBUG_MAX_SCHEMA_COLUMNS,
        rows: CLIENT_SQL_DEBUG_MAX_ROWS,
        cells: CLIENT_SQL_DEBUG_MAX_CELLS,
        collectionItems: CLIENT_SQL_DEBUG_MAX_COLLECTION_ITEMS,
        stringCharacters: CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS,
        blobBytes: CLIENT_SQL_DEBUG_MAX_BLOB_BYTES,
        payloadBytes: CLIENT_SQL_DEBUG_MAX_PAYLOAD_BYTES,
      },
      omittedDatabases: this.omittedDatabases,
      omittedTables: this.omittedTables,
      omittedColumns: this.omittedColumns,
      omittedRows: this.omittedRows,
      omittedCells: this.omittedCells,
      omittedValues: this.omittedValues,
      truncatedStrings: this.truncatedStrings,
      truncatedBlobs: this.truncatedBlobs,
      payloadBytes,
    };
  }

  private reservePrimitive(): boolean {
    if (!this.reserveBytes(8)) {
      this.omittedValues += 1;
      return false;
    }
    return true;
  }

  private metadataCount(value: unknown): number {
    return typeof value === 'number' && Number.isSafeInteger(value) && value > 0 ? value : 0;
  }

  private isExhausted(): boolean {
    return this.bytesUsed + 8 > CLIENT_SQL_DEBUG_MAX_CONTENT_BYTES
      || this.valueCount >= CLIENT_SQL_DEBUG_MAX_VALUES;
  }

  private reserveBytes(bytes: number): boolean {
    if (this.bytesUsed + bytes > CLIENT_SQL_DEBUG_MAX_CONTENT_BYTES) {
      this.noteReason('payloadBytes');
      return false;
    }
    this.bytesUsed += bytes;
    return true;
  }

  private limitString(value: string): string {
    const remainingBytes = Math.max(0, CLIENT_SQL_DEBUG_MAX_CONTENT_BYTES - this.bytesUsed - 8);
    const payloadCharacters = Math.floor(remainingBytes / 2);
    const characterCount = Math.min(
      value.length,
      CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS,
      payloadCharacters,
    );
    const limited = value.slice(0, characterCount);
    if (limited.length < value.length) {
      this.noteTruncatedString();
      if (payloadCharacters < Math.min(value.length, CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS)) {
        this.noteReason('payloadBytes');
      }
    }
    this.reserveBytes(limited.length * 2 + 8);
    return limited;
  }

  private limitBlob(value: ArrayBuffer): Record<string, unknown> {
    const remainingBytes = Math.max(0, CLIENT_SQL_DEBUG_MAX_CONTENT_BYTES - this.bytesUsed - 8);
    const maximumBytesForHex = Math.floor(Math.max(0, remainingBytes - 128) / 2);
    const byteCount = Math.min(value.byteLength, CLIENT_SQL_DEBUG_MAX_BLOB_BYTES, maximumBytesForHex);
    const bytes = new Uint8Array(value, 0, byteCount);
    let data = '';
    for (let index = 0; index < bytes.length; index += 1) {
      data += bytes[index].toString(16).padStart(2, '0');
    }
    if (byteCount < value.byteLength) {
      this.noteTruncatedBlob();
      if (maximumBytesForHex < Math.min(value.byteLength, CLIENT_SQL_DEBUG_MAX_BLOB_BYTES)) {
        this.noteReason('payloadBytes');
      }
    }
    this.reserveBytes(data.length + 128);
    return {
      type: 'blob',
      encoding: 'hex',
      byteLength: value.byteLength,
      shownBytes: byteCount,
      truncated: byteCount < value.byteLength,
      data,
    };
  }
}

function limitDebugPayload(
  value: Record<string, unknown>,
  limiter: ClientSQLDebugPayloadLimiter,
): Record<string, unknown> {
  return limiter.limitValue(value, 0) as Record<string, unknown>;
}

function utf8ByteLength(value: string): number {
  let bytes = 0;
  for (let index = 0; index < value.length; index += 1) {
    const codeUnit = value.charCodeAt(index);
    if (codeUnit <= 0x7f) {
      bytes += 1;
    } else if (codeUnit <= 0x7ff) {
      bytes += 2;
    } else if (
      codeUnit >= 0xd800 && codeUnit <= 0xdbff
      && index + 1 < value.length
      && value.charCodeAt(index + 1) >= 0xdc00
      && value.charCodeAt(index + 1) <= 0xdfff
    ) {
      bytes += 4;
      index += 1;
    } else {
      bytes += 3;
    }
  }
  return bytes;
}

function jsonPayloadBytes(value: unknown): number {
  return utf8ByteLength(JSON.stringify(value) ?? 'null');
}

function assignExactPayloadBytes(
  value: Record<string, unknown>,
  limiter: ClientSQLDebugPayloadLimiter,
): number {
  let previousBytes = -1;
  for (let attempt = 0; attempt < 8; attempt += 1) {
    const payloadBytes = jsonPayloadBytes(value);
    value.truncation = limiter.metadata(payloadBytes);
    if (payloadBytes === previousBytes) {
      return payloadBytes;
    }
    previousBytes = payloadBytes;
  }
  const payloadBytes = jsonPayloadBytes(value);
  value.truncation = limiter.metadata(payloadBytes);
  return jsonPayloadBytes(value);
}

function serializeDebugProviderDocument(value: Record<string, unknown>): string {
  const limiter = new ClientSQLDebugPayloadLimiter();
  limiter.absorbMetadata(value.truncation);
  const limited = limitDebugPayload(value, limiter);
  limited.truncation = limiter.metadata(0);
  let payloadBytes = assignExactPayloadBytes(limited, limiter);
  if (payloadBytes <= CLIENT_SQL_DEBUG_MAX_PAYLOAD_BYTES) {
    const document = JSON.stringify(limited);
    if (utf8ByteLength(document) > CLIENT_SQL_DEBUG_MAX_PAYLOAD_BYTES) {
      throw new Error('ClientSQL debugger serialized document exceeded its payload byte cap.');
    }
    return document;
  }

  limiter.noteReason('payloadBytes');
  limiter.noteReason('finalByteCap');
  const fallback: Record<string, unknown> = {
    source: typeof limited.source === 'string' ? limited.source.slice(0, 128) : 'target',
    revision: typeof limited.revision === 'number' ? limited.revision : debugRevision,
    truncation: limiter.metadata(0),
  };
  for (const field of ['databaseId', 'databaseName', 'table']) {
    const fieldValue = limited[field];
    if (typeof fieldValue === 'string') {
      fallback[field] = fieldValue.slice(0, 128);
    }
  }
  payloadBytes = assignExactPayloadBytes(fallback, limiter);
  if (payloadBytes > CLIENT_SQL_DEBUG_MAX_PAYLOAD_BYTES) {
    throw new Error('ClientSQL debugger could not fit truncation metadata within its payload byte cap.');
  }
  return JSON.stringify(fallback);
}

function createClientSQLDebuggerProviderResult(
  value: Record<string, unknown>,
): DebuggerProviderResult {
  return createDebuggerProviderResult(serializeDebugProviderDocument(value));
}

function nextDebugRevision(): number {
  debugRevision += 1;
  debugProviderRegistration?.notifyChange();
  return debugRevision;
}

function requestInteger(data: any, field: string, fallback: number): number {
  const value = data?.[field];
  if (value === undefined) {
    return fallback;
  }
  if (typeof value !== 'number' || !Number.isFinite(value) || !Number.isSafeInteger(value)) {
    throw new Error(`ClientSQL debugger '${field}' must be a finite integer.`);
  }
  return value;
}

function clampInteger(value: number, minimum: number, maximum: number): number {
  return Math.min(maximum, Math.max(minimum, value));
}

function quoteSQLiteIdentifier(identifier: string): string {
  return `"${identifier.replace(/"/g, '""')}"`;
}

function clientSQLRowsOrderClause(tableType: string | null | undefined, columns: ClientSQLDebugColumn[]): string {
  const primaryKeyColumns = columns
    .filter(column => column.pk > 0)
    .sort((a, b) => a.pk - b.pk);
  if (primaryKeyColumns.length) {
    return ` ORDER BY ${primaryKeyColumns.map(column => `${quoteSQLiteIdentifier(column.name)} DESC`).join(', ')}`;
  }
  if (tableType === 'table') {
    return ' ORDER BY rowid DESC';
  }
  return '';
}

function firstOpenDatabaseById(): ClientSQLDebugDatabase[] {
  return Object.keys(databasesById)
    .map(id => databasesById[id]?.[0])
    .filter((database): database is ClientSQLDebugDatabase => database !== undefined);
}

function clearDatabaseSummaryCache(id: string): void {
  delete databaseSummaryCacheById[id];
}

function debugErrorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

async function summarizeTable(
  database: ClientSQLDebugDatabase,
  table: { name: string; type: string; sql: string | null },
  columnLimit: number,
): Promise<ClientSQLDebugTableSummary> {
  const overlongTableName = table.name.length > CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS;
  const safeTableName = table.name.slice(0, CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS);
  if (overlongTableName || columnLimit <= 0) {
    return {
      name: safeTableName,
      type: table.type,
      sql: table.sql,
      rowCount: null,
      rowCountIsLowerBound: false,
      columns: [],
      truncation: {
        columns: true,
        omittedColumnsAtLeast: 1,
        rowCount: true,
      },
    };
  }

  const appliedColumnLimit = Math.min(CLIENT_SQL_DEBUG_MAX_COLUMNS_PER_TABLE, columnLimit);
  const columns = await database.query<ClientSQLDebugColumn>(
    'SELECT cid, substr(name, 1, ?) AS name, substr(type, 1, ?) AS type, '
      + '"notnull" AS "notnull", '
      + 'CASE WHEN dflt_value IS NULL THEN NULL ELSE substr(CAST(dflt_value AS TEXT), 1, ?) END AS dflt_value, pk '
      + 'FROM pragma_table_info(?) ORDER BY cid LIMIT ?',
    [
      CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS + 1,
      CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS + 1,
      CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS + 1,
      table.name,
      appliedColumnLimit + 1,
    ],
  );
  const safeColumns = columns
    .slice(0, appliedColumnLimit)
    .filter(column => column.name.length <= CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS);
  const omittedColumnsAtLeast = Math.max(0, columns.length - safeColumns.length);
  let rowCount: number | null = null;
  let rowCountIsLowerBound = false;
  let rowCountUnavailable = false;
  try {
    const countRows = await database.query<{ count: number }>(
      `SELECT COUNT(*) AS count FROM (`
        + `SELECT 1 FROM ${quoteSQLiteIdentifier(table.name)} LIMIT ?`
        + ')',
      [CLIENT_SQL_DEBUG_MAX_ROW_COUNT_SCAN + 1],
    );
    const count = Number(countRows[0]?.count ?? 0);
    if (!Number.isFinite(count) || !Number.isSafeInteger(count) || count < 0) {
      throw new Error(`received invalid count '${String(countRows[0]?.count)}'`);
    }
    rowCountIsLowerBound = count > CLIENT_SQL_DEBUG_MAX_ROW_COUNT_SCAN;
    rowCount = rowCountIsLowerBound ? CLIENT_SQL_DEBUG_MAX_ROW_COUNT_SCAN : count;
  } catch (error) {
    rowCountUnavailable = true;
    console.warn(
      `ClientSQL debugger could not count rows for database '${database.name}', `
        + `table '${safeTableName}': ${debugErrorMessage(error)}`,
    );
  }

  return {
    name: safeTableName,
    type: table.type,
    sql: table.sql,
    rowCount,
    rowCountIsLowerBound,
    columns: safeColumns,
    truncation: {
      columns: omittedColumnsAtLeast > 0,
      omittedColumnsAtLeast,
      rowCount: rowCountIsLowerBound || rowCountUnavailable,
    },
  };
}

async function summarizeDatabase(database: ClientSQLDebugDatabase): Promise<Record<string, unknown>> {
  const limiter = new ClientSQLDebugPayloadLimiter();
  const versionRows = await database.query<{ user_version: number }>('PRAGMA user_version', []);
  const tables = await database.query<{ name: string; type: string; sql: string | null }>(
    "SELECT substr(name, 1, ?) AS name, type, "
      + "CASE WHEN sql IS NULL THEN NULL ELSE substr(sql, 1, ?) END AS sql "
      + "FROM sqlite_schema WHERE type IN ('table', 'view') AND name NOT LIKE 'sqlite_%' "
      + 'ORDER BY type, name LIMIT ?',
    [
      CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS + 1,
      CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS + 1,
      CLIENT_SQL_DEBUG_MAX_TABLES + 1,
    ],
  );
  const selectedTables = tables.slice(0, CLIENT_SQL_DEBUG_MAX_TABLES);
  limiter.noteOmittedTables(Math.max(0, tables.length - selectedTables.length));
  const tableSummaries: ClientSQLDebugTableSummary[] = [];
  let remainingColumns = CLIENT_SQL_DEBUG_MAX_SCHEMA_COLUMNS;
  for (const table of selectedTables) {
    const summary = await summarizeTable(database, table, remainingColumns);
    tableSummaries.push(summary);
    remainingColumns -= summary.columns.length;
    limiter.noteOmittedColumns(summary.truncation.omittedColumnsAtLeast);
    if (summary.truncation.rowCount) {
      limiter.noteReason('rowCount');
    }
  }
  const debugInfo = database.debugInfo ? await database.debugInfo() : null;

  const rawSummary = {
    id: `target:${database.id}`,
    name: database.name,
    path: `${database.name} (target ClientSQL)`,
    root: 'debugger-target',
    size: null,
    modifiedAt: null,
    userVersion: Number(versionRows[0]?.user_version ?? database.schemaVersion),
    debugInfo,
    tables: tableSummaries,
  };
  const limitedSummary = limitDebugPayload(rawSummary, limiter);
  const limitedTables = Array.isArray(limitedSummary.tables)
    ? limitedSummary.tables as Array<Record<string, unknown>>
    : [];
  limitedSummary.tables = limitedTables;
  limiter.noteOmittedTables(Math.max(0, tableSummaries.length - limitedTables.length));
  const rawColumnCount = tableSummaries.reduce((count, table) => count + table.columns.length, 0);
  const limitedColumnCount = limitedTables.reduce((count, table) => {
    return count + (Array.isArray(table.columns) ? table.columns.length : 0);
  }, 0);
  limiter.noteOmittedColumns(Math.max(0, rawColumnCount - limitedColumnCount));
  limitedSummary.truncation = limiter.metadata(0);
  return limitedSummary;
}

async function summarizeDatabaseCached(
  database: ClientSQLDebugDatabase,
  forceRefresh: boolean,
): Promise<Record<string, unknown>> {
  const cached = databaseSummaryCacheById[database.id];
  if (!forceRefresh && cached && Date.now() - cached.cachedAt < CLIENT_SQL_DEBUG_SCHEMA_CACHE_TTL_MS) {
    return cached.summary;
  }

  const summary = await summarizeDatabase(database);
  databaseSummaryCacheById[database.id] = {
    cachedAt: Date.now(),
    summary,
  };
  return summary;
}

async function summarizeSingleTable(
  database: ClientSQLDebugDatabase,
  tableName: string,
): Promise<ClientSQLDebugTableSummary> {
  if (tableName.length > CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS) {
    throw new Error(`ClientSQL table name exceeds ${CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS} characters.`);
  }
  const tables = await database.query<{ name: string; type: string; sql: string | null }>(
    "SELECT substr(name, 1, ?) AS name, type, "
      + "CASE WHEN sql IS NULL THEN NULL ELSE substr(sql, 1, ?) END AS sql "
      + "FROM sqlite_schema WHERE type IN ('table', 'view') AND name NOT LIKE 'sqlite_%' "
      + 'AND name = ? LIMIT 1',
    [CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS + 1, CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS + 1, tableName],
  );
  const table = tables[0];
  if (!table) {
    throw new Error(`Unknown ClientSQL table '${tableName}'.`);
  }
  return summarizeTable(database, table, CLIENT_SQL_DEBUG_MAX_COLUMNS_PER_TABLE);
}

async function inspectDatabases(data: any): Promise<Record<string, unknown>> {
  const forceRefresh = Boolean(data?.refresh);
  const limiter = new ClientSQLDebugPayloadLimiter();
  const databases = firstOpenDatabaseById();
  const selectedDatabases = databases.slice(0, CLIENT_SQL_DEBUG_MAX_DATABASES);
  limiter.noteOmittedDatabases(Math.max(0, databases.length - selectedDatabases.length));
  const summaries = await Promise.all(
    selectedDatabases.map(database => summarizeDatabaseCached(database, forceRefresh)),
  );
  summaries.forEach(summary => limiter.absorbMetadata(summary.truncation));
  const rawSummary = {
    source: 'target',
    revision: debugRevision,
    roots: ['debugger-target'],
    databases: summaries,
  };
  const limitedSummary = limitDebugPayload(rawSummary, limiter);
  const limitedDatabases = Array.isArray(limitedSummary.databases) ? limitedSummary.databases : [];
  limitedSummary.databases = limitedDatabases;
  limiter.noteOmittedDatabases(Math.max(0, summaries.length - limitedDatabases.length));
  limitedSummary.truncation = limiter.metadata(0);
  return limitedSummary;
}

function rowSelectExpression(column: ClientSQLDebugColumn, index: number): string {
  const identifier = quoteSQLiteIdentifier(column.name);
  return `CASE WHEN typeof(${identifier}) = 'text' THEN `
    + `substr(${identifier}, 1, ${CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS}) `
    + `WHEN typeof(${identifier}) = 'blob' THEN `
    + `substr(${identifier}, 1, ${CLIENT_SQL_DEBUG_MAX_BLOB_BYTES}) `
    + `ELSE ${identifier} END AS ${quoteSQLiteIdentifier(`__clientsql_value_${index}`)}, `
    + `typeof(${identifier}) AS ${quoteSQLiteIdentifier(`__clientsql_type_${index}`)}, `
    + `CASE WHEN typeof(${identifier}) IN ('text', 'blob') THEN length(${identifier}) ELSE 0 END `
    + `AS ${quoteSQLiteIdentifier(`__clientsql_length_${index}`)}`;
}

async function inspectTable(data: any): Promise<Record<string, unknown>> {
  const databaseId = String(data?.databaseId ?? '');
  const identity = databaseId.startsWith('target:') ? databaseId.slice('target:'.length) : databaseId;
  const database = databasesById[identity]?.[0];
  if (!database) {
    throw new Error(`Unknown ClientSQL database '${identity}'.`);
  }

  const table = String(data?.table ?? '');
  if (!table) {
    throw new Error('Missing required table.');
  }

  const requestedLimit = requestInteger(data, 'limit', CLIENT_SQL_DEBUG_DEFAULT_ROWS);
  const requestedOffset = requestInteger(data, 'offset', 0);
  const validLimit = clampInteger(requestedLimit, 1, CLIENT_SQL_DEBUG_MAX_ROWS);
  const offset = clampInteger(requestedOffset, 0, CLIENT_SQL_DEBUG_MAX_OFFSET);
  const tableSummary = await summarizeSingleTable(database, table);
  const columns = tableSummary.columns.slice(0, CLIENT_SQL_DEBUG_MAX_COLUMNS_PER_TABLE);
  if (!columns.length) {
    throw new Error(`ClientSQL table '${table}' has no inspectable columns.`);
  }

  const maximumCellBytes = Math.max(
    CLIENT_SQL_DEBUG_MAX_BLOB_BYTES,
    CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS * CLIENT_SQL_DEBUG_ESTIMATED_UTF8_BYTES_PER_CHARACTER,
  );
  const maximumRowsForQueryWork = Math.max(
    1,
    Math.floor(CLIENT_SQL_DEBUG_MAX_QUERY_VALUE_BYTES / (columns.length * maximumCellBytes)),
  );
  const limit = Math.min(validLimit, maximumRowsForQueryWork);
  const orderClause = clientSQLRowsOrderClause(tableSummary.type, columns);
  const selectExpressions = columns.map(rowSelectExpression).join(', ');
  const queriedRows = await database.query<Record<string, unknown>>(
    `SELECT ${selectExpressions} FROM ${quoteSQLiteIdentifier(table)}`
      + `${orderClause} LIMIT ? OFFSET ?`,
    [limit + 1, offset],
  );
  const selectedRows = queriedRows.slice(0, limit);
  // Rows are positional arrays aligned with the separately bounded `columns`
  // metadata. SQL identifiers are valid far beyond the generic provider's
  // JSON property-name limit, so they must never become bridge object keys.
  const rows = selectedRows.map(row => columns.map((_column, index) => {
    return row[`__clientsql_value_${index}`];
  }));

  const limiter = new ClientSQLDebugPayloadLimiter();
  if (requestedLimit !== validLimit) {
    limiter.noteOmittedRows(Math.max(0, requestedLimit - validLimit));
    limiter.noteReason('requestedRowLimit');
  }
  if (validLimit > limit) {
    limiter.noteOmittedRows(validLimit - limit);
    limiter.noteReason('queryWork');
  }
  if (queriedRows.length > selectedRows.length) {
    limiter.noteOmittedRows(queriedRows.length - selectedRows.length);
  }
  if (requestedOffset !== offset) {
    limiter.noteReason('requestedOffset');
  }
  limiter.noteOmittedColumns(tableSummary.truncation.omittedColumnsAtLeast);
  if (tableSummary.truncation.rowCount) {
    limiter.noteReason('rowCount');
  }
  const selectedCellCount = rows.length * columns.length;
  if (selectedCellCount > CLIENT_SQL_DEBUG_MAX_CELLS) {
    limiter.noteOmittedCells(selectedCellCount - CLIENT_SQL_DEBUG_MAX_CELLS);
  }
  selectedRows.forEach(row => {
    columns.forEach((column, index) => {
      const valueType = String(row[`__clientsql_type_${index}`]);
      const rawLength = Number(row[`__clientsql_length_${index}`] ?? 0);
      if (valueType === 'text' && rawLength > CLIENT_SQL_DEBUG_MAX_STRING_CHARACTERS) {
        limiter.noteTruncatedString();
      }
      if (valueType === 'blob' && rawLength > CLIENT_SQL_DEBUG_MAX_BLOB_BYTES) {
        limiter.noteTruncatedBlob();
      }
    });
  });

  const rawSummary = {
    source: 'target',
    revision: debugRevision,
    databaseId: `target:${database.id}`,
    databaseName: database.name,
    table,
    requestedLimit,
    limit,
    requestedOffset,
    offset,
    rowCount: tableSummary.rowCount,
    rowCountIsLowerBound: tableSummary.rowCountIsLowerBound,
    hasMore: queriedRows.length > selectedRows.length,
    orderBy: orderClause.replace(/^\s*ORDER BY\s+/i, '') || null,
    columns,
    rows,
  };
  const limitedSummary = limitDebugPayload(rawSummary, limiter);
  const limitedColumns = Array.isArray(limitedSummary.columns) ? limitedSummary.columns : [];
  const limitedRows = Array.isArray(limitedSummary.rows)
    ? (limitedSummary.rows as unknown[][]).map(row => row.slice(0, limitedColumns.length))
    : [];
  limitedSummary.columns = limitedColumns;
  limitedSummary.rows = limitedRows;
  limiter.noteOmittedColumns(Math.max(0, columns.length - limitedColumns.length));
  limiter.noteOmittedRows(Math.max(0, rows.length - limitedRows.length));
  const limitedCellCount = limitedRows.reduce((count, row) => count + row.length, 0);
  limiter.noteOmittedCells(Math.max(0, selectedCellCount - limitedCellCount));
  limitedSummary.truncation = limiter.metadata(0);
  return limitedSummary;
}

function ensureDebugProviderRegistered(): void {
  if (debugProviderRegistration) {
    return;
  }
  if (!debugProviderOwner) {
    debugProviderOwner = createDebuggerProviderOwner(module, CLIENT_SQL_DEBUG_PROVIDER_OWNER_KEY);
  }
  debugProviderRegistration = debugProviderOwner.register({
    availability: () => {
      const databaseCount = firstOpenDatabaseById().length;
      return {
        available: databaseCount > 0,
        message: databaseCount > 0
          ? `${databaseCount} open ClientSQL database${databaseCount === 1 ? '' : 's'}.`
          : 'No ClientSQL databases are currently open.',
      };
    },
    description: 'Inspect open ClientSQL databases through bounded, read-only queries.',
    handleRequest: async (request: DebuggerProviderRequest): Promise<DebuggerProviderResult> => {
      if (request.action === 'list') {
        return createClientSQLDebuggerProviderResult(await inspectDatabases(request));
      }
      if (request.action === 'table') {
        return createClientSQLDebuggerProviderResult(await inspectTable(request));
      }
      throw new Error(`Unsupported ClientSQL debugger action: ${request.action}`);
    },
    id: 'client-sql',
    kind: DebuggerProviderKind.Sql,
    label: 'ClientSQL',
  });
}

function releaseDebugProviderIfUnused(): void {
  if (Object.keys(databasesById).length > 0 || !debugProviderRegistration) {
    return;
  }
  debugProviderRegistration.dispose();
  debugProviderRegistration = undefined;
}

export function registerClientSQLDebugDatabase(database: ClientSQLDebugDatabase): void {
  ensureDebugProviderRegistered();
  let databases = databasesById[database.id];
  if (!databases) {
    databases = [];
    databasesById[database.id] = databases;
  }
  if (databases.indexOf(database) === -1) {
    databases.push(database);
  }
  clearDatabaseSummaryCache(database.id);
  nextDebugRevision();
}

export function unregisterClientSQLDebugDatabase(database: ClientSQLDebugDatabase): void {
  const databases = databasesById[database.id];
  if (!databases) {
    return;
  }

  const index = databases.indexOf(database);
  if (index !== -1) {
    databases.splice(index, 1);
  }
  if (!databases.length) {
    delete databasesById[database.id];
  }
  clearDatabaseSummaryCache(database.id);
  nextDebugRevision();
  releaseDebugProviderIfUnused();
}

export function notifyClientSQLDebugChanged(databaseId: string | undefined): void {
  if (databaseId) {
    clearDatabaseSummaryCache(databaseId);
  } else {
    Object.keys(databaseSummaryCacheById).forEach(clearDatabaseSummaryCache);
  }
  nextDebugRevision();
}
