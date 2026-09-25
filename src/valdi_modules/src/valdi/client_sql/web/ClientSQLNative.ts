// The web fallback is intentionally not a native API surface. Keep its contract structural so referencing
// newly versioned native declarations does not make this unsupported implementation version-gated.
export type UnsupportedClientSQLValue = string | number | boolean | ArrayBuffer | null;
export type UnsupportedClientSQLCallback<T> = (value: T | undefined, error: string | undefined) => void;

export interface UnsupportedClientSQLMigration {
  version: number;
  statements: string[];
}

export interface UnsupportedClientSQLTransaction {
  execute(
    sql: string,
    parameters: UnsupportedClientSQLValue[] | undefined,
    callback: UnsupportedClientSQLCallback<void>,
  ): void;
  query<T>(
    sql: string,
    parameters: UnsupportedClientSQLValue[] | undefined,
    callback: UnsupportedClientSQLCallback<T[]>,
  ): void;
}

export type UnsupportedClientSQLTransactionBody = (
  transaction: UnsupportedClientSQLTransaction,
  callback: UnsupportedClientSQLCallback<void>,
) => void;

export interface UnsupportedClientSQLNativeConnection extends UnsupportedClientSQLTransaction {
  transaction(
    body: UnsupportedClientSQLTransactionBody,
    callback: UnsupportedClientSQLCallback<void>,
  ): void;
  close(callback: UnsupportedClientSQLCallback<void>): void;
}

export interface UnsupportedClientSQLNativeModule {
  openDatabase(
    name: string,
    schemaVersion: number,
    createStatements: string[],
    migrations: UnsupportedClientSQLMigration[],
  ): UnsupportedClientSQLNativeConnection;
}

class UnsupportedClientSQLConnection implements UnsupportedClientSQLNativeConnection {
  constructor(
    private readonly name: string,
    private readonly schemaVersion: number,
    private readonly createStatements: string[],
    private readonly migrations: UnsupportedClientSQLMigration[],
  ) {}

  execute(
    _sql: string,
    _parameters: UnsupportedClientSQLValue[] | undefined,
    callback: UnsupportedClientSQLCallback<void>,
  ): void {
    callback(undefined, unsupported(this.name, this.schemaVersion, this.createStatements, this.migrations).message);
  }

  query<T>(
    _sql: string,
    _parameters: UnsupportedClientSQLValue[] | undefined,
    callback: UnsupportedClientSQLCallback<T[]>,
  ): void {
    callback(undefined, unsupported(this.name, this.schemaVersion, this.createStatements, this.migrations).message);
  }

  transaction(
    _body: UnsupportedClientSQLTransactionBody,
    callback: UnsupportedClientSQLCallback<void>,
  ): void {
    callback(undefined, unsupported(this.name, this.schemaVersion, this.createStatements, this.migrations).message);
  }

  close(callback: UnsupportedClientSQLCallback<void>): void {
    callback(undefined, undefined);
  }
}

function unsupported(
  name: string,
  schemaVersion: number,
  createStatements: string[],
  migrations: UnsupportedClientSQLMigration[],
): Error {
  return new Error(
    `ClientSQLNative is not implemented for web. ` +
      `Database '${name}' requested schema version ${schemaVersion} ` +
      `with ${createStatements.length} create statements and ${migrations.length} migrations.`,
  );
}

export const clientSQLNative: UnsupportedClientSQLNativeModule = {
  openDatabase(
    name: string,
    schemaVersion: number,
    createStatements: string[],
    migrations: UnsupportedClientSQLMigration[],
  ): UnsupportedClientSQLNativeConnection {
    return new UnsupportedClientSQLConnection(name, schemaVersion, createStatements, migrations);
  },
};

export function openDatabase(
  name: string,
  schemaVersion: number,
  createStatements: string[],
  migrations: UnsupportedClientSQLMigration[],
): UnsupportedClientSQLNativeConnection {
  return clientSQLNative.openDatabase(name, schemaVersion, createStatements, migrations);
}
