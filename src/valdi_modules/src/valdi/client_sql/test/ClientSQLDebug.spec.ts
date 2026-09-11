import 'jasmine/src/jasmine';
import * as DebuggerProvider from 'valdi_core/src/debugging/DebuggerProvider';
import type {
  DebuggerProvider as DebuggerProviderContract,
  DebuggerProviderModule,
  DebuggerProviderOwner,
  DebuggerProviderRegistration,
} from 'valdi_core/src/debugging/DebuggerProvider';
import {
  ClientSQLDebugDatabase,
  ClientSQLDebugValue,
  notifyClientSQLDebugChanged,
  registerClientSQLDebugDatabase,
  unregisterClientSQLDebugDatabase,
} from '../src/ClientSQLDebug';

const LONG_COLUMN_NAME = `long_${'x'.repeat(1500)}`;
const PENDING_CHANGED_TABLES = Array.from(
  { length: 101 },
  (_value, index) => `pending_table_${index.toString().padStart(3, '0')}`,
);

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

function makeDebugDatabase(id: string): ClientSQLDebugDatabase {
  const blob = new Uint8Array(8192);
  blob.fill(0xab);
  const debugRow: Record<string, unknown> = {
    __clientsql_value_0: '\u0000\u0001'.repeat(4096),
    __clientsql_type_0: 'text',
    __clientsql_length_0: 8192,
    __clientsql_value_1: '😀'.repeat(4096),
    __clientsql_type_1: 'text',
    __clientsql_length_1: 8192,
    __clientsql_value_2: blob.buffer,
    __clientsql_type_2: 'blob',
    __clientsql_length_2: blob.byteLength,
  };
  return {
    id,
    name: 'Debug database',
    schemaVersion: 1,
    createStatements: [],
    migrations: [],
    debugInfo(): Promise<Record<string, unknown>> {
      return Promise.resolve({
        pendingChangedTables: PENDING_CHANGED_TABLES.slice(),
      });
    },
    query<T>(sql: string, _parameters: ClientSQLDebugValue[] | undefined): Promise<T[]> {
      if (sql === 'PRAGMA user_version') {
        return Promise.resolve([{ user_version: 1 }] as unknown as T[]);
      }
      if (sql.indexOf('FROM sqlite_schema') !== -1) {
        return Promise.resolve([{
          name: 'sample',
          type: 'table',
          sql: 'CREATE TABLE sample(control TEXT, emoji TEXT, payload BLOB)',
        }] as unknown as T[]);
      }
      if (sql.indexOf('FROM pragma_table_info') !== -1) {
        return Promise.resolve([
          { cid: 0, name: LONG_COLUMN_NAME, type: 'TEXT', notnull: 0, dflt_value: null, pk: 0 },
          { cid: 1, name: 'emoji', type: 'TEXT', notnull: 0, dflt_value: null, pk: 0 },
          { cid: 2, name: 'payload', type: 'BLOB', notnull: 0, dflt_value: null, pk: 0 },
        ] as unknown as T[]);
      }
      if (sql.indexOf('SELECT COUNT(*) AS count') === 0) {
        return Promise.resolve([{ count: 101 }] as unknown as T[]);
      }
      if (sql.indexOf('SELECT CASE WHEN typeof') === 0) {
        return Promise.resolve(Array.from({ length: 101 }, () => ({ ...debugRow })) as unknown as T[]);
      }
      throw new Error(`Unexpected debug SQL: ${sql}`);
    },
  };
}

describe('ClientSQLDebug', () => {
  let capturedProvider: DebuggerProviderContract | undefined;
  let dispose: jasmine.Spy<() => void>;
  let notifyChange: jasmine.Spy<() => void>;
  let ownerDispose: jasmine.Spy<() => void>;
  let ownerModule: DebuggerProviderModule | undefined;
  let ownerKey: string | undefined;

  beforeEach(() => {
    capturedProvider = undefined;
    ownerModule = undefined;
    ownerKey = undefined;
    dispose = jasmine.createSpy('dispose');
    notifyChange = jasmine.createSpy('notifyChange');
    let registrationDisposed = false;
    const registration: DebuggerProviderRegistration = {
      dispose(): void {
        if (registrationDisposed) return;
        registrationDisposed = true;
        dispose();
      },
      notifyChange,
    };
    ownerDispose = jasmine.createSpy('ownerDispose').and.callFake(() => registration.dispose());
    spyOn(DebuggerProvider, 'createDebuggerProviderOwner').and.callFake(
      (creatingModule: DebuggerProviderModule, stableOwnerKey: string): DebuggerProviderOwner => {
        ownerModule = creatingModule;
        ownerKey = stableOwnerKey;
        return {
          dispose: ownerDispose,
          register(provider: DebuggerProviderContract): DebuggerProviderRegistration {
            capturedProvider = provider;
            registrationDisposed = false;
            return registration;
          },
        };
      },
    );
  });

  it('binds the module owner, bounds serialized actions, and disposes final registrations', async () => {
    const database = makeDebugDatabase('clientsql_test_debug.sqlite');
    registerClientSQLDebugDatabase(database);

    expect(DebuggerProvider.createDebuggerProviderOwner).toHaveBeenCalledTimes(1);
    expect(ownerModule).toBeDefined();
    expect(ownerKey).toBe('client_sql/src/ClientSQLDebug');
    const provider = capturedProvider as DebuggerProviderContract;
    expect(provider.id).toBe('client-sql');
    expect(provider.kind).toBe(DebuggerProvider.DebuggerProviderKind.Sql);
    expect(provider.availability?.()).toEqual(jasmine.objectContaining({ available: true }));

    const listDocument = (await provider.handleRequest({ action: 'list' })).json;
    const listResponse = JSON.parse(listDocument) as Record<string, unknown>;
    expect(Array.isArray(listResponse.databases)).toBe(true);
    expect(utf8ByteLength(listDocument)).toBeLessThanOrEqual(40 * 1024);
    const listTruncation = listResponse.truncation as Record<string, unknown>;
    expect(listTruncation.payloadBytes)
      .toBe(utf8ByteLength(listDocument));
    expect(listTruncation.omittedValues).toBe(1);
    expect(listTruncation.reasons).toContain('collectionValues');
    expect((listTruncation.limits as Record<string, unknown>).collectionItems).toBe(100);

    const listDatabases = listResponse.databases as Array<Record<string, unknown>>;
    const databaseDebugInfo = listDatabases[0].debugInfo as Record<string, unknown>;
    const pendingChangedTables = databaseDebugInfo.pendingChangedTables as string[];
    expect(pendingChangedTables).toEqual(PENDING_CHANGED_TABLES.slice(0, 100));
    const databaseTruncation = listDatabases[0].truncation as Record<string, unknown>;
    expect(databaseTruncation.omittedValues).toBe(1);
    expect(databaseTruncation.reasons).toContain('collectionValues');

    const blobDocument = (await provider.handleRequest({
      action: 'table',
      databaseId: `target:${database.id}`,
      table: 'sample',
      limit: 1,
      offset: 0,
    })).json;
    expect(blobDocument).toContain('"encoding":"hex"');
    const blobResponse = JSON.parse(blobDocument) as Record<string, unknown>;
    const blobColumns = blobResponse.columns as Array<Record<string, unknown>>;
    const blobRows = blobResponse.rows as unknown[][];
    expect((blobColumns[0].name as string).length).toBeGreaterThan(1024);
    expect(Array.isArray(blobRows[0])).toBe(true);
    expect(blobDocument).not.toContain(`"${LONG_COLUMN_NAME}":`);

    const tableDocument = (await provider.handleRequest({
      action: 'table',
      databaseId: `target:${database.id}`,
      table: 'sample',
      limit: 100,
      offset: 0,
    })).json;
    const tableResponse = JSON.parse(tableDocument) as Record<string, unknown>;
    expect(utf8ByteLength(tableDocument)).toBeLessThanOrEqual(40 * 1024);
    expect((tableResponse.truncation as Record<string, unknown>).payloadBytes)
      .toBe(utf8ByteLength(tableDocument));
    expect(tableDocument).not.toContain('[object ArrayBuffer]');
    await expectAsync(provider.handleRequest({ action: 'write' }) as Promise<unknown>)
      .toBeRejectedWithError(/Unsupported ClientSQL debugger action/);

    notifyClientSQLDebugChanged(database.id);
    expect(notifyChange).toHaveBeenCalled();
    unregisterClientSQLDebugDatabase(database);
    expect(dispose).toHaveBeenCalledTimes(1);
    expect(provider.availability?.()).toEqual(jasmine.objectContaining({ available: false }));

    const first = makeDebugDatabase('clientsql_first.sqlite');
    const second = makeDebugDatabase('clientsql_second.sqlite');
    registerClientSQLDebugDatabase(first);
    registerClientSQLDebugDatabase(second);

    expect(DebuggerProvider.createDebuggerProviderOwner).toHaveBeenCalledTimes(1);
    unregisterClientSQLDebugDatabase(first);
    expect(dispose).toHaveBeenCalledTimes(1);
    unregisterClientSQLDebugDatabase(second);
    expect(dispose).toHaveBeenCalledTimes(2);
    expect(ownerDispose).not.toHaveBeenCalled();
  });
});
