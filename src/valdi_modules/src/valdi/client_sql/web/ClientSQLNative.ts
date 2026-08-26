import {
  ClientSQLMigration,
  ClientSQLNativeCallback,
  ClientSQLNativeConnection,
  ClientSQLNativeModule,
  ClientSQLNativeTransactionBody,
  ClientSQLValue,
} from '../src/ClientSQLNative';

class UnsupportedClientSQLConnection implements ClientSQLNativeConnection {
  constructor(
    private readonly name: string,
    private readonly schemaVersion: number,
    private readonly createStatements: string[],
    private readonly migrations: ClientSQLMigration[],
  ) {}

  execute(
    _sql: string,
    _parameters: ClientSQLValue[] | undefined,
    callback: ClientSQLNativeCallback<void>,
  ): void {
    callback(undefined, unsupported(this.name, this.schemaVersion, this.createStatements, this.migrations).message);
  }

  query<T>(
    _sql: string,
    _parameters: ClientSQLValue[] | undefined,
    callback: ClientSQLNativeCallback<T[]>,
  ): void {
    callback(undefined, unsupported(this.name, this.schemaVersion, this.createStatements, this.migrations).message);
  }

  transaction(
    _body: ClientSQLNativeTransactionBody,
    callback: ClientSQLNativeCallback<void>,
  ): void {
    callback(undefined, unsupported(this.name, this.schemaVersion, this.createStatements, this.migrations).message);
  }

  close(callback: ClientSQLNativeCallback<void>): void {
    callback(undefined, undefined);
  }
}

function unsupported(
  name: string,
  schemaVersion: number,
  createStatements: string[],
  migrations: ClientSQLMigration[],
): Error {
  return new Error(
    `ClientSQLNative is not implemented for web. ` +
      `Database '${name}' requested schema version ${schemaVersion} ` +
      `with ${createStatements.length} create statements and ${migrations.length} migrations.`,
  );
}

export const clientSQLNative: ClientSQLNativeModule = {
  openDatabase(
    name: string,
    schemaVersion: number,
    createStatements: string[],
    migrations: ClientSQLMigration[],
  ): ClientSQLNativeConnection {
    return new UnsupportedClientSQLConnection(name, schemaVersion, createStatements, migrations);
  },
};

export function openDatabase(
  name: string,
  schemaVersion: number,
  createStatements: string[],
  migrations: ClientSQLMigration[],
): ClientSQLNativeConnection {
  return clientSQLNative.openDatabase(name, schemaVersion, createStatements, migrations);
}
