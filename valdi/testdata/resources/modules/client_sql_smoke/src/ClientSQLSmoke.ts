import {
  ClientSQLMigration,
  ClientSQLNativeCallback,
  ClientSQLNativeConnection,
  ClientSQLNativeModule,
  ClientSQLNativeTransaction,
  ClientSQLNativeTransactionBody,
  ClientSQLValue,
} from 'client_sql/src/ClientSQLNative';
import { TestDb, setClientSQLNativeForTests } from './sqlgen/TestDb';

/**
 * @Version(__PLACEHOLDER__)
 */
class FakeConnection implements ClientSQLNativeConnection {
  readonly executed: string[] = [];

  execute(
    sql: string,
    _parameters: ClientSQLValue[] | undefined,
    callback: ClientSQLNativeCallback<void>,
  ): void {
    this.executed.push(sql);
    callback(undefined, undefined);
  }

  query<T>(
    _sql: string,
    _parameters: ClientSQLValue[] | undefined,
    callback: ClientSQLNativeCallback<T[]>,
  ): void {
    callback([], undefined);
  }

  transaction(body: ClientSQLNativeTransactionBody, callback: ClientSQLNativeCallback<void>): void {
    const transaction: ClientSQLNativeTransaction = {
      execute: (sql, parameters, transactionCallback): void => {
        this.execute(sql, parameters, transactionCallback);
      },
      query: <T>(
        sql: string,
        parameters: ClientSQLValue[] | undefined,
        transactionCallback: ClientSQLNativeCallback<T[]>,
      ): void => {
        this.query(sql, parameters, transactionCallback);
      },
    };
    body(transaction, callback);
  }

  close(callback: ClientSQLNativeCallback<void>): void {
    callback(undefined, undefined);
  }
}

export function createSmokeDatabase(): TestDb {
  const native: ClientSQLNativeModule = {
    /**
     * @Version(__PLACEHOLDER__)
     */
    openDatabase(
      _name: string,
      _schemaVersion: number,
      _createStatements: string[],
      _migrations: ClientSQLMigration[],
    ): ClientSQLNativeConnection {
      return new FakeConnection();
    },
  };

  setClientSQLNativeForTests(native);
  return TestDb.open(undefined);
}

export async function runClientSQLSmoke(): Promise<void> {
  const db = createSmokeDatabase();
  await db.userQueries.insertUser(1, 'Ada', null);
  await db.userQueries.selectById(1);
  await db.close();
}

export type ClientSQLRuntimeIntegrationCallback = (
  result: string | undefined,
  error: string | undefined,
) => void;

export function runClientSQLNativeIntegration(callback: ClientSQLRuntimeIntegrationCallback): void {
  void runClientSQLNativeIntegrationAsync().then(
    result => callback(result, undefined),
    error => callback(undefined, error instanceof Error ? error.message : String(error)),
  );
}

async function runClientSQLNativeIntegrationAsync(): Promise<string> {
  const db = TestDb.open(`runtime-integration-${Date.now()}`);
  try {
    await db.userQueries.insertRuntimeValue(1, true, null, new ArrayBuffer(0));
    await db.userQueries.insertRuntimeValue(2, false, true, new ArrayBuffer(0));
    const rows = await db.userQueries.selectRuntimeValues();
    if (rows.length !== 2) {
      throw new Error(`Expected two runtime rows, received ${rows.length}`);
    }
    if (rows[0].enabled !== true || rows[0].optional !== null) {
      throw new Error('Native ClientSQL did not preserve true/null boolean values.');
    }
    if (rows[1].enabled !== false || rows[1].optional !== true) {
      throw new Error('Native ClientSQL did not preserve false/true boolean values.');
    }
    if (rows[0].payload.byteLength !== 0 || rows[1].payload.byteLength !== 0) {
      throw new Error('Native ClientSQL did not preserve zero-length BLOB values.');
    }
    return 'ok';
  } finally {
    await db.close();
  }
}
