/**
 * @ExportModule
 */

export type ClientSQLValue = string | number | boolean | ArrayBuffer | null;

export type ClientSQLNativeCallback<T> = (value: T | undefined, error: string | undefined) => void;

/**
 * SQL parameters and result rows are heterogeneous by statement, so this payload carrier cannot
 * have one stable typed native schema. Only this value boundary intentionally marshals as untyped;
 * the exported module, migration model, and connection proxies remain typed native contracts.
 *
 * @NativeClass({
 *   marshallAsUntyped: true,
 *   ios: 'NSObject', iosImportPrefix: 'Foundation',
 *   android: 'kotlin.Any'
 * })
 */
export interface ClientSQLNativeAny {}

/**
 * @ExportModel
 */
export interface ClientSQLMigration {
  version: number;
  statements: string[];
}

/**
 * @ExportProxy
 */
export interface ClientSQLNativeTransactionProxy {
  execute(
    sql: string,
    parameters: ClientSQLNativeAny[] | undefined,
    callback: (value: ClientSQLNativeAny | undefined, error: string | undefined) => void,
  ): void;
  query(
    sql: string,
    parameters: ClientSQLNativeAny[] | undefined,
    callback: (value: ClientSQLNativeAny[] | undefined, error: string | undefined) => void,
  ): void;
}

/**
 * @ExportProxy
 */
export interface ClientSQLNativeConnectionProxy {
  execute(
    sql: string,
    parameters: ClientSQLNativeAny[] | undefined,
    callback: (value: ClientSQLNativeAny | undefined, error: string | undefined) => void,
  ): void;
  query(
    sql: string,
    parameters: ClientSQLNativeAny[] | undefined,
    callback: (value: ClientSQLNativeAny[] | undefined, error: string | undefined) => void,
  ): void;
  queryOnWriter(
    sql: string,
    parameters: ClientSQLNativeAny[] | undefined,
    callback: (value: ClientSQLNativeAny[] | undefined, error: string | undefined) => void,
  ): void;
  transaction(
    body: (
      transaction: ClientSQLNativeTransactionProxy,
      callback: (value: ClientSQLNativeAny | undefined, error: string | undefined) => void,
    ) => void,
    callback: (value: ClientSQLNativeAny | undefined, error: string | undefined) => void,
  ): void;
  debugInfo(callback: (value: ClientSQLNativeAny | undefined, error: string | undefined) => void): void;
  close(callback: (value: ClientSQLNativeAny | undefined, error: string | undefined) => void): void;
}

export interface ClientSQLNativeTransaction {
  execute(sql: string, parameters: ClientSQLValue[] | undefined, callback: ClientSQLNativeCallback<void>): void;
  query<T>(sql: string, parameters: ClientSQLValue[] | undefined, callback: ClientSQLNativeCallback<T[]>): void;
}

export type ClientSQLNativeTransactionBody = (
  transaction: ClientSQLNativeTransaction,
  callback: ClientSQLNativeCallback<void>,
) => void;

export interface ClientSQLNativeConnection {
  execute(sql: string, parameters: ClientSQLValue[] | undefined, callback: ClientSQLNativeCallback<void>): void;
  query<T>(sql: string, parameters: ClientSQLValue[] | undefined, callback: ClientSQLNativeCallback<T[]>): void;
  transaction(body: ClientSQLNativeTransactionBody, callback: ClientSQLNativeCallback<void>): void;
  close(callback: ClientSQLNativeCallback<void>): void;
}

export interface ClientSQLNativeModule {
  /** `name` is the generator's canonical module-scoped storage identity, not a filesystem path. */
  openDatabase(
    name: string,
    schemaVersion: number,
    createStatements: string[],
    migrations: ClientSQLMigration[],
  ): ClientSQLNativeConnection;
}

export function openDatabase(
  name: string,
  schemaVersion: number,
  createStatements: string[],
  migrations: ClientSQLMigration[],
): ClientSQLNativeConnectionProxy;
