from __future__ import annotations

import hashlib
import json
import os
import re
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

from .model import Query, SqlFile, Table
from .sql import sanitize_identifier, sanitize_type_name


def write_types_file(output_dir: Path, sql_file: SqlFile, tables: Dict[str, Table]) -> None:
    path = output_dir / f"{sql_file.stem_path}Types.ts"
    path.parent.mkdir(parents=True, exist_ok=True)

    content = render_types(sql_file, tables, declare=False)
    path.write_text(content, encoding="utf-8")


def render_types(sql_file: SqlFile, tables: Dict[str, Table], declare: bool) -> str:
    lines = generated_header()
    used_tables = tables_used_by_file(sql_file, tables)
    for table in used_tables:
        lines.extend(render_interface(sanitize_type_name(table.name), table.columns, declare_export=True))
        lines.append("")

    for query in sql_file.queries:
        if query.params:
            lines.extend(render_interface(f"{sanitize_type_name(query.name)}Params", query.params, declare_export=True))
            lines.append("")
        if query.returns_rows and query.result_type == f"{sanitize_type_name(query.name)}Row":
            lines.extend(render_interface(query.result_type, query.result_fields, declare_export=True))
            lines.append("")

    if len(lines) == len(generated_header()):
        lines.append("export {};")
    return "\n".join(lines).rstrip() + "\n"


def write_queries_file(output_dir: Path, sql_file: SqlFile) -> None:
    path = output_dir / f"{sql_file.stem_path}Queries.ts"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(render_queries(sql_file, declare=False), encoding="utf-8")


def render_queries(sql_file: SqlFile, declare: bool) -> str:
    class_name = f"{sanitize_type_name(sql_file.stem_path.name)}Queries"
    type_imports = sorted(types_imported_by_queries(sql_file.queries))
    import_path = f"./{sql_file.stem_path.name}Types"

    lines = generated_header()
    if type_imports:
        lines.append(f"import {{ {', '.join(type_imports)} }} from '{import_path}';")
        lines.append("")

    lines.extend([
        "export type ClientSQLValue = string | number | boolean | ArrayBuffer | null;",
        "export interface ClientSQLSubscription {",
        "  unsubscribe(): void;",
        "}",
        "export type ClientSQLQueryListener<T> = (value: T) => void;",
        "",
        "export interface ClientSQLDatabase {",
        "  execute(",
        "    sql: string,",
        "    parameters: ClientSQLValue[] | undefined,",
        "    changedTables: string[] | undefined,",
        "  ): Promise<void>;",
        "  query<T>(sql: string, parameters: ClientSQLValue[] | undefined): Promise<T[]>;",
        "  watchQuery<T>(",
        "    tables: string[],",
        "    load: () => Promise<T>,",
        "    listener: ClientSQLQueryListener<T>,",
        "  ): ClientSQLSubscription;",
        "}",
        "",
    ])
    if any(boolean_result_fields(query) for query in sql_file.queries):
        lines.extend([
            "function decodeClientSQLBoolean(value: ClientSQLValue | undefined, field: string): boolean {",
            "  if (value === true || value === 1) {",
            "    return true;",
            "  }",
            "  if (value === false || value === 0) {",
            "    return false;",
            "  }",
            "  throw new Error(`ClientSQL boolean field '${field}' returned invalid value '${String(value)}'`);",
            "}",
            "",
            "function decodeNullableClientSQLBoolean(",
            "  value: ClientSQLValue | undefined,",
            "  field: string,",
            "): boolean | null {",
            "  return value === null ? null : decodeClientSQLBoolean(value, field);",
            "}",
            "",
        ])

    if declare:
        lines.append(f"export declare class {class_name} {{")
        lines.append("  constructor(db: ClientSQLDatabase);")
        for query in sql_file.queries:
            lines.append(f"  {sanitize_identifier(query.name)}({method_signature_params(query)}): {method_return_type(query)};")
            if query.returns_rows:
                lines.append(f"  {watch_method_name(query)}({watch_method_signature_params(query)}): ClientSQLSubscription;")
        lines.append("}")
        return "\n".join(lines).rstrip() + "\n"

    lines.append(f"export class {class_name} {{")
    lines.append("  constructor(private readonly db: ClientSQLDatabase) {}")
    lines.append("")
    for query in sql_file.queries:
        lines.extend(render_query_method(query))
        if query.returns_rows:
            lines.extend(render_query_watch_method(query))
        lines.append("")
    lines.append("}")
    return "\n".join(lines).rstrip() + "\n"


def render_query_method(query: Query) -> List[str]:
    sql_literal = json.dumps(query.runtime_sql)
    param_array = ", ".join(sanitize_identifier(name) for name in query.param_order)
    lines = [
        f"  {sanitize_identifier(query.name)}({method_signature_params(query)}): {method_return_type(query)} {{",
    ]
    if query.returns_rows:
        boolean_fields = boolean_result_fields(query)
        if boolean_fields:
            lines.append(
                f"    return this.db.query<Record<string, ClientSQLValue>>({sql_literal}, [{param_array}]).then(rows => rows.map(row => ({{"
            )
            lines.append("      ...row,")
            for field in boolean_fields:
                decoder = "decodeNullableClientSQLBoolean" if field.nullable else "decodeClientSQLBoolean"
                field_name = json.dumps(field.name)
                lines.append(f"      {render_property_name(field.name)}: {decoder}(row[{field_name}], {field_name}),")
            lines.append(f"    }}) as unknown as {query.result_type}));")
        else:
            lines.append(f"    return this.db.query<{query.result_type}>({sql_literal}, [{param_array}]);")
    elif query.changed_tables:
        lines.append(f"    return this.db.execute({sql_literal}, [{param_array}], {json.dumps(query.changed_tables)});")
    else:
        lines.append(f"    return this.db.execute({sql_literal}, [{param_array}], undefined);")
    lines.append("  }")
    return lines


def render_query_watch_method(query: Query) -> List[str]:
    tables_literal = json.dumps(query.read_tables)
    param_values = ", ".join(param.name for param in query.params)
    method_name = sanitize_identifier(query.name)
    invocation = f"this.{method_name}({param_values})" if param_values else f"this.{method_name}()"
    return [
        f"  {watch_method_name(query)}({watch_method_signature_params(query)}): ClientSQLSubscription {{",
        f"    return this.db.watchQuery({tables_literal}, () => {invocation}, listener);",
        "  }",
    ]


def write_database_file(
    output_dir: Path,
    class_name: str,
    db_name: str,
    module_name: str,
    sql_files: List[SqlFile],
    create_statements: List[str],
    migrations: List[Tuple[int, List[str]]],
) -> None:
    path = output_dir / f"{class_name}.ts"
    path.write_text(
        render_database(
            class_name,
            db_name,
            module_name,
            sql_files,
            create_statements,
            migrations,
            declare=False,
        ),
        encoding="utf-8",
    )


def render_database(
    class_name: str,
    db_name: str,
    module_name: str,
    sql_files: List[SqlFile],
    create_statements: List[str],
    migrations: List[Tuple[int, List[str]]],
    declare: bool,
) -> str:
    namespace_digest = hashlib.sha256(module_name.encode("utf-8")).hexdigest()[:24]
    query_classes = [(query_class_name(sql_file), import_path_from_db(sql_file, "Queries")) for sql_file in sql_files if sql_file.queries]
    all_tables = sorted({
        table
        for sql_file in sql_files
        for query in sql_file.queries
        for table in query.read_tables + query.changed_tables
    })
    lines = generated_header()
    for klass, import_path in query_classes:
        lines.append(f"import {{ {klass} }} from '{import_path}';")
    if not declare:
        lines.append("import { ClientSQLDebugDatabase, notifyClientSQLDebugChanged, registerClientSQLDebugDatabase, unregisterClientSQLDebugDatabase } from 'client_sql/src/ClientSQLDebug';")
    if query_classes or not declare:
        lines.append("")

    lines.extend([
        "declare function require(path: string): any;",
        "",
        "export type ClientSQLValue = string | number | boolean | ArrayBuffer | null;",
        "export type ClientSQLNativeCallback<T> = (value: T | undefined, error: string | undefined) => void;",
        "",
        "export interface ClientSQLSubscription {",
        "  unsubscribe(): void;",
        "}",
        "",
        "interface ClientSQLTransactionDebugEntry {",
        "  id: number;",
        "  status: 'committed' | 'rolled_back';",
        "  startedAt: string;",
        "  completedAt: string;",
        "  durationMs: number;",
        "  changedTables: string[];",
        "  changedTableCount: number;",
        "  error?: string;",
        "}",
        "",
        "const MAX_TRANSACTION_DEBUG_ENTRIES = 50;",
        "",
        "export type ClientSQLQueryListener<T> = (value: T) => void;",
        "",
        "export interface ClientSQLMigration {",
        "  version: number;",
        "  statements: string[];",
        "}",
        "",
        "export interface ClientSQLNativeTransaction {",
        "  execute(sql: string, parameters: ClientSQLValue[] | undefined, callback: ClientSQLNativeCallback<void>): void;",
        "  query<T>(sql: string, parameters: ClientSQLValue[] | undefined, callback: ClientSQLNativeCallback<T[]>): void;",
        "}",
        "",
        "export type ClientSQLNativeTransactionBody = (",
        "  transaction: ClientSQLNativeTransaction,",
        "  callback: ClientSQLNativeCallback<void>,",
        ") => void;",
        "",
        "export interface ClientSQLNativeConnection {",
        "  execute(sql: string, parameters: ClientSQLValue[] | undefined, callback: ClientSQLNativeCallback<void>): void;",
        "  query<T>(sql: string, parameters: ClientSQLValue[] | undefined, callback: ClientSQLNativeCallback<T[]>): void;",
        "  transaction(body: ClientSQLNativeTransactionBody, callback: ClientSQLNativeCallback<void>): void;",
        "  close(callback: ClientSQLNativeCallback<void>): void;",
        "}",
        "",
        "export interface ClientSQLNativeModule {",
        "  openDatabase(",
        "    name: string,",
        "    schemaVersion: number,",
        "    createStatements: string[],",
        "    migrations: ClientSQLMigration[],",
        "  ): ClientSQLNativeConnection;",
        "}",
        "",
    ])

    lines.append(f"export interface {class_name}Transaction {{")
    for klass, _ in query_classes:
        property_name = lower_first(klass.removesuffix("Queries")) + "Queries"
        lines.append(f"  readonly {property_name}: {klass};")
    lines.extend([
        "  execute(",
        "    sql: string,",
        "    parameters: ClientSQLValue[] | undefined,",
        "    changedTables: string[] | undefined,",
        "  ): Promise<void>;",
        "  query<T>(sql: string, parameters: ClientSQLValue[] | undefined): Promise<T[]>;",
        f"  transaction<T>(body: (transaction: {class_name}Transaction) => Promise<T>): Promise<T>;",
        "}",
        "",
    ])

    if declare:
        lines.append("export declare function setClientSQLNativeForTests(native: ClientSQLNativeModule | undefined): void;")
        lines.append("")
        lines.append(f"export declare class {class_name} {{")
        for klass, _ in query_classes:
            property_name = lower_first(klass.removesuffix("Queries")) + "Queries"
            lines.append(f"  readonly {property_name}: {klass};")
        lines.append("  static open(name: string | undefined): " + class_name + ";")
        lines.append("  execute(")
        lines.append("    sql: string,")
        lines.append("    parameters: ClientSQLValue[] | undefined,")
        lines.append("    changedTables: string[] | undefined,")
        lines.append("  ): Promise<void>;")
        lines.append("  query<T>(sql: string, parameters: ClientSQLValue[] | undefined): Promise<T[]>;")
        lines.append("  watchQuery<T>(")
        lines.append("    tables: string[],")
        lines.append("    load: () => Promise<T>,")
        lines.append("    listener: ClientSQLQueryListener<T>,")
        lines.append("  ): ClientSQLSubscription;")
        lines.append(
            f"  transaction<T>(body: (transaction: {class_name}Transaction) => Promise<T>): Promise<T>;"
        )
        lines.append("  close(): Promise<void>;")
        lines.append("}")
        return "\n".join(lines).rstrip() + "\n"

    schema_version = max([version for version, _ in migrations], default=1)
    lines.extend([
        f"const DEFAULT_DATABASE_NAME = {json.dumps(db_name)};",
        f"const DATABASE_NAMESPACE = {json.dumps(namespace_digest)};",
        f"const SCHEMA_VERSION = {schema_version};",
        f"const CREATE_STATEMENTS: string[] = {json.dumps(create_statements, indent=2)};",
        f"const MIGRATIONS: ClientSQLMigration[] = {json.dumps([{'version': version, 'statements': statements} for version, statements in migrations], indent=2)};",
        f"const ALL_TABLES: string[] = {json.dumps(all_tables)};",
        "",
        "let clientSQLNativeForTests: ClientSQLNativeModule | undefined;",
        "",
        "interface ClientSQLWatchEntry {",
        "  tables: string[];",
        "  emit(): void;",
        "  unsubscribe(): void;",
        "}",
        "",
        "const watchEntriesByDatabaseName: { [name: string]: ClientSQLWatchEntry[] | undefined } = Object.create(null);",
        "const writeChainsByDatabaseName: { [name: string]: Promise<void> | undefined } = Object.create(null);",
        "",
        "export function setClientSQLNativeForTests(native: ClientSQLNativeModule | undefined): void {",
        "  clientSQLNativeForTests = native;",
        "}",
        "",
        "function getClientSQLNative(): ClientSQLNativeModule {",
        "  return clientSQLNativeForTests ?? (require('client_sql/src/ClientSQLNative') as ClientSQLNativeModule);",
        "}",
        "",
        "function databaseIdentity(name: string): string {",
        "  if (!name || name.length > 48) {",
        "    throw new Error('ClientSQL database names must contain from 1 through 48 UTF-16 code units');",
        "  }",
        "  let encodedName = '';",
        "  for (let index = 0; index < name.length; index += 1) {",
        "    const codeUnit = name.charCodeAt(index);",
        "    if (codeUnit < 0x20 || (codeUnit >= 0x7f && codeUnit <= 0x9f)) {",
        "      throw new Error('ClientSQL database names cannot contain control characters');",
        "    }",
        "    encodedName += codeUnit.toString(16).padStart(4, '0');",
        "  }",
        "  return `clientsql_${DATABASE_NAMESPACE}_${encodedName}.sqlite`;",
        "}",
        "",
        "function entriesForDatabase(name: string): ClientSQLWatchEntry[] {",
        "  let entries = watchEntriesByDatabaseName[name];",
        "  if (!entries) {",
        "    entries = [];",
        "    watchEntriesByDatabaseName[name] = entries;",
        "  }",
        "  return entries;",
        "}",
        "",
        "function removeWatchEntry(entries: ClientSQLWatchEntry[], entry: ClientSQLWatchEntry): void {",
        "  const index = entries.indexOf(entry);",
        "  if (index !== -1) {",
        "    entries.splice(index, 1);",
        "  }",
        "}",
        "",
        "function releaseDatabaseWrite(name: string, chain: Promise<void>, release: () => void): void {",
        "  release();",
        "  if (writeChainsByDatabaseName[name] === chain) {",
        "    delete writeChainsByDatabaseName[name];",
        "  }",
        "}",
        "",
        "function enqueueDatabaseWrite<T>(name: string, body: () => Promise<T>): Promise<T> {",
        "  const previous = writeChainsByDatabaseName[name] ?? Promise.resolve();",
        "  let releaseCurrent!: () => void;",
        "  const current = new Promise<void>(resolve => {",
        "    releaseCurrent = resolve;",
        "  });",
        "  const chain = previous.then(() => current, () => current);",
        "  writeChainsByDatabaseName[name] = chain;",
        "",
        "  const runBody = (): Promise<T> => {",
        "    let result: Promise<T>;",
        "    try {",
        "      result = body();",
        "    } catch (error) {",
        "      releaseDatabaseWrite(name, chain, releaseCurrent);",
        "      return Promise.reject(error);",
        "    }",
        "",
        "    return result.then(",
        "      value => {",
        "        releaseDatabaseWrite(name, chain, releaseCurrent);",
        "        return value;",
        "      },",
        "      error => {",
        "        releaseDatabaseWrite(name, chain, releaseCurrent);",
        "        throw error;",
        "      },",
        "    );",
        "  };",
        "",
        "  return previous.then(runBody, runBody);",
        "}",
        "",
        "function toUniqueTables(tables: string[]): string[] {",
        "  const out: string[] = [];",
        "  tables.forEach(table => {",
        "    if (out.indexOf(table) === -1) {",
        "      out.push(table);",
        "    }",
        "  });",
        "  return out;",
        "}",
        "",
        "function hasTableIntersection(observedTables: string[], changedTables: string[]): boolean {",
        "  if (observedTables.length === 0 || changedTables.length === 0) {",
        "    return true;",
        "  }",
        "  return observedTables.some(table => changedTables.indexOf(table) !== -1);",
        "}",
        "",
        "function nativePromise<T>(body: (callback: ClientSQLNativeCallback<T>) => void): Promise<T> {",
        "  return new Promise<T>((resolve, reject) => {",
        "    body((value, error) => {",
        "      if (error !== undefined && error !== null) {",
        "        reject(new Error(error));",
        "        return;",
        "      }",
        "      resolve(value as T);",
        "    });",
        "  });",
        "}",
        "",
        "function errorMessage(error: unknown): string {",
        "  return error instanceof Error ? error.message : String(error);",
        "}",
        "",
        f"export class {class_name} {{",
        "  private readonly localWatchEntries: ClientSQLWatchEntry[] = [];",
        "  private debugDatabase: ClientSQLDebugDatabase | undefined;",
        "  private activeTransactionCount = 0;",
        "  private activeTransactionChangedTables: string[] | undefined;",
        "  private nextTransactionDebugId = 1;",
        "  private transactionHistory: ClientSQLTransactionDebugEntry[] = [];",
        "  private closed = false;",
        "",
        "  private constructor(",
        "    private readonly databaseName: string,",
        "    private readonly databaseIdentity: string,",
        "    private readonly connection: ClientSQLNativeConnection,",
        "  ) {",
    ])
    for klass, _ in query_classes:
        property_name = lower_first(klass.removesuffix("Queries")) + "Queries"
        lines.append(f"    this.{property_name} = new {klass}(this);")
    lines.extend([
        "    const debugDatabase: ClientSQLDebugDatabase = {",
        "      id: this.databaseIdentity,",
        "      name: this.databaseName,",
        "      schemaVersion: SCHEMA_VERSION,",
        "      createStatements: CREATE_STATEMENTS,",
        "      migrations: MIGRATIONS,",
        "      query: <T>(sql: string, parameters: ClientSQLValue[] | undefined): Promise<T[]> => this.query<T>(sql, parameters),",
        "      debugInfo: (): Promise<Record<string, unknown>> => this.debugInfo(),",
        "    };",
        "    this.debugDatabase = debugDatabase;",
        "    registerClientSQLDebugDatabase(debugDatabase);",
    ])
    lines.append("  }")
    lines.append("")
    for klass, _ in query_classes:
        property_name = lower_first(klass.removesuffix("Queries")) + "Queries"
        lines.append(f"  readonly {property_name}: {klass};")
    if query_classes:
        lines.append("")
    lines.extend([
        f"  static open(name: string | undefined): {class_name} {{",
        "    const databaseName = name ?? DEFAULT_DATABASE_NAME;",
        "    const identity = databaseIdentity(databaseName);",
        "    const connection = getClientSQLNative().openDatabase(identity, SCHEMA_VERSION, CREATE_STATEMENTS, MIGRATIONS);",
        f"    return new {class_name}(databaseName, identity, connection);",
        "  }",
        "",
        "  async execute(",
        "    sql: string,",
        "    parameters: ClientSQLValue[] | undefined,",
        "    changedTables: string[] | undefined,",
        "  ): Promise<void> {",
        "    if (this.closed) {",
        "      throw new Error(`ClientSQL database '${this.databaseName}' is closed`);",
        "    }",
        "    if (this.activeTransactionCount > 0) {",
        "      throw new Error(`ClientSQL database '${this.databaseName}' cannot execute through its parent handle inside a transaction body`);",
        "    }",
        "    return enqueueDatabaseWrite(this.databaseIdentity, async () => {",
        "      await nativePromise<void>(callback => this.connection.execute(sql, parameters, callback));",
        "      this.notifyTablesChanged(changedTables ?? ALL_TABLES);",
        "    });",
        "  }",
        "",
        "  async query<T>(sql: string, parameters: ClientSQLValue[] | undefined): Promise<T[]> {",
        "    if (this.closed) {",
        "      throw new Error(`ClientSQL database '${this.databaseName}' is closed`);",
        "    }",
        "    if (this.activeTransactionCount > 0) {",
        "      throw new Error(`ClientSQL database '${this.databaseName}' cannot query through its parent handle inside a transaction body`);",
        "    }",
        "    return nativePromise<T[]>(callback => this.connection.query<T>(sql, parameters, callback));",
        "  }",
        "",
        "  watchQuery<T>(",
        "    tables: string[],",
        "    load: () => Promise<T>,",
        "    listener: ClientSQLQueryListener<T>,",
        "  ): ClientSQLSubscription {",
        "    if (this.closed) {",
        "      return { unsubscribe(): void {} };",
        "    }",
        "",
        "    let active = true;",
        "    let generation = 0;",
        "    const emit = (): void => {",
        "      const currentGeneration = ++generation;",
        "      void load()",
        "        .then(value => {",
        "          if (active && currentGeneration === generation) {",
        "            listener(value);",
        "          }",
        "        })",
        "        .catch(error => {",
        "          if (active) {",
        "            console.error(error);",
        "          }",
        "        });",
        "    };",
        "",
        "    const unsubscribe = (): void => {",
        "      if (!active) {",
        "        return;",
        "      }",
        "      active = false;",
        "      const entries = watchEntriesByDatabaseName[this.databaseIdentity];",
        "      if (entries) {",
        "        removeWatchEntry(entries, entry);",
        "      }",
        "      removeWatchEntry(this.localWatchEntries, entry);",
        "    };",
        "    const entry: ClientSQLWatchEntry = { tables, emit, unsubscribe };",
        "    entriesForDatabase(this.databaseIdentity).push(entry);",
        "    this.localWatchEntries.push(entry);",
        "    emit();",
        "",
        "    return {",
        "      unsubscribe,",
        "    };",
        "  }",
        "",
        f"  async transaction<T>(body: (transaction: {class_name}Transaction) => Promise<T>): Promise<T> {{",
        "    if (this.closed) {",
        "      throw new Error(`ClientSQL database '${this.databaseName}' is closed`);",
        "    }",
        "    if (this.activeTransactionCount > 0) {",
        "      throw new Error(`ClientSQL database '${this.databaseName}' cannot start a parent transaction inside a transaction body`);",
        "    }",
        "    return enqueueDatabaseWrite(this.databaseIdentity, () => this.runTransaction(body));",
        "  }",
        "",
        f"  private async runTransaction<T>(body: (transaction: {class_name}Transaction) => Promise<T>): Promise<T> {{",
        "    let result!: T;",
        "    const changedTables: string[] = [];",
        "    let committed = false;",
        "    const transactionDebugId = this.nextTransactionDebugId++;",
        "    const transactionStartedAtMs = Date.now();",
        "    this.activeTransactionCount += 1;",
        "    this.activeTransactionChangedTables = changedTables;",
        "",
        "    try {",
        "      await nativePromise<void>(callback => {",
        "        this.connection.transaction((nativeTransaction, transactionCallback) => {",
        "          const transaction = this.createTransactionScope(nativeTransaction, changedTables);",
        "          let bodyPromise: Promise<T>;",
        "          try {",
        "            bodyPromise = body(transaction);",
        "          } catch (error) {",
        "            transactionCallback(undefined, errorMessage(error));",
        "            return;",
        "          }",
        "",
        "          void bodyPromise.then(",
        "            value => {",
        "              result = value;",
        "              transactionCallback(undefined, undefined);",
        "            },",
        "            error => {",
        "              transactionCallback(undefined, errorMessage(error));",
        "            },",
        "          );",
        "        }, callback);",
        "      });",
        "      const transactionCompletedAtMs = Date.now();",
        "      this.recordTransactionDebugEntry({",
        "        id: transactionDebugId,",
        "        status: 'committed',",
        "        startedAt: new Date(transactionStartedAtMs).toISOString(),",
        "        completedAt: new Date(transactionCompletedAtMs).toISOString(),",
        "        durationMs: transactionCompletedAtMs - transactionStartedAtMs,",
        "        changedTables: changedTables.slice(),",
        "        changedTableCount: changedTables.length,",
        "      });",
        "      committed = true;",
        "      return result;",
        "    } catch (error) {",
        "      const transactionCompletedAtMs = Date.now();",
        "      this.recordTransactionDebugEntry({",
        "        id: transactionDebugId,",
        "        status: 'rolled_back',",
        "        startedAt: new Date(transactionStartedAtMs).toISOString(),",
        "        completedAt: new Date(transactionCompletedAtMs).toISOString(),",
        "        durationMs: transactionCompletedAtMs - transactionStartedAtMs,",
        "        changedTables: changedTables.slice(),",
        "        changedTableCount: changedTables.length,",
        "        error: errorMessage(error),",
        "      });",
        "      notifyClientSQLDebugChanged(this.databaseIdentity);",
        "      throw error;",
        "    } finally {",
        "      this.activeTransactionChangedTables = undefined;",
        "      this.activeTransactionCount -= 1;",
        "      if (committed) {",
        "        this.emitChangedTables(changedTables);",
        "      }",
        "    }",
        "  }",
        "",
        f"  private createTransactionScope(nativeTransaction: ClientSQLNativeTransaction, changedTables: string[]): {class_name}Transaction {{",
        "    const transactionDatabase = {",
        "      execute: async (",
        "        sql: string,",
        "        parameters: ClientSQLValue[] | undefined,",
        "        tables: string[] | undefined,",
        "      ): Promise<void> => {",
        "        await nativePromise<void>(callback => nativeTransaction.execute(sql, parameters, callback));",
        "        const invalidatedTables = tables ?? ALL_TABLES;",
        "        invalidatedTables.forEach(table => {",
        "          if (changedTables.indexOf(table) === -1) {",
        "            changedTables.push(table);",
        "          }",
        "        });",
        "      },",
        "      query: <T>(sql: string, parameters: ClientSQLValue[] | undefined): Promise<T[]> =>",
        "        nativePromise<T[]>(callback => nativeTransaction.query<T>(sql, parameters, callback)),",
        "      watchQuery: <T>(",
        "        _tables: string[],",
        "        _load: () => Promise<T>,",
        "        _listener: ClientSQLQueryListener<T>,",
        "      ): ClientSQLSubscription => {",
        "        throw new Error('ClientSQL watchers cannot be created inside a transaction');",
        "      },",
        "    };",
        f"    let scope!: {class_name}Transaction;",
        "    scope = {",
    ])
    for klass, _ in query_classes:
        property_name = lower_first(klass.removesuffix("Queries")) + "Queries"
        lines.append(f"      {property_name}: new {klass}(transactionDatabase),")
    lines.extend([
        "      execute: transactionDatabase.execute,",
        "      query: transactionDatabase.query,",
        "      transaction: nestedBody => nestedBody(scope),",
        "    };",
        "    return scope;",
        "  }",
        "",
        "  async close(): Promise<void> {",
        "    if (this.closed) {",
        "      return;",
        "    }",
        "    if (this.activeTransactionCount > 0) {",
        "      throw new Error(`ClientSQL database '${this.databaseName}' cannot close through its parent handle inside a transaction body`);",
        "    }",
        "    this.closed = true;",
        "    this.localWatchEntries.slice().forEach(entry => {",
        "      if (typeof entry.unsubscribe === 'function') {",
        "        entry.unsubscribe();",
        "        return;",
        "      }",
        "      const entries = watchEntriesByDatabaseName[this.databaseIdentity];",
        "      if (entries) {",
        "        removeWatchEntry(entries, entry);",
        "      }",
        "      removeWatchEntry(this.localWatchEntries, entry);",
        "    });",
        "    if (this.debugDatabase) {",
        "      unregisterClientSQLDebugDatabase(this.debugDatabase);",
        "      this.debugDatabase = undefined;",
        "    }",
        "    await enqueueDatabaseWrite(this.databaseIdentity, () => nativePromise<void>(callback => this.connection.close(callback)));",
        "  }",
        "",
        "  private async debugInfo(): Promise<Record<string, unknown>> {",
        "    const debugConnection = this.connection as ClientSQLNativeConnection & {",
        "      debugInfo?: (callback: ClientSQLNativeCallback<Record<string, unknown>>) => void;",
        "    };",
        "    const nativeInfo = debugConnection.debugInfo",
        "      ? await nativePromise<Record<string, unknown>>(callback => debugConnection.debugInfo!(callback))",
        "      : {};",
        "    return {",
        "      ...nativeInfo,",
        "      closed: this.closed,",
        "      transactionDepth: this.activeTransactionCount,",
        "      pendingChangedTables: this.activeTransactionChangedTables?.slice() ?? [],",
        "      pendingChangedTableCount: this.activeTransactionChangedTables?.length ?? 0,",
        "      transactionHistoryCount: this.transactionHistory.length,",
        "      transactions: this.transactionHistory.slice().reverse(),",
        "      watcherCount: (watchEntriesByDatabaseName[this.databaseIdentity] || []).length,",
        "      localWatcherCount: this.localWatchEntries.length,",
        "      queuedWrite: writeChainsByDatabaseName[this.databaseIdentity] !== undefined,",
        "    };",
        "  }",
        "",
        "  private recordTransactionDebugEntry(entry: ClientSQLTransactionDebugEntry): void {",
        "    this.transactionHistory.push(entry);",
        "    const overflow = this.transactionHistory.length - MAX_TRANSACTION_DEBUG_ENTRIES;",
        "    if (overflow > 0) {",
        "      this.transactionHistory.splice(0, overflow);",
        "    }",
        "  }",
        "",
        "  private notifyTablesChanged(changedTables: string[]): void {",
        "    const uniqueTables = toUniqueTables(changedTables);",
        "    this.emitChangedTables(uniqueTables);",
        "  }",
        "",
        "  private emitChangedTables(changedTables: string[]): void {",
        "    notifyClientSQLDebugChanged(this.databaseIdentity);",
        "    const entries = watchEntriesByDatabaseName[this.databaseIdentity];",
        "    if (!entries) {",
        "      return;",
        "    }",
        "    entries.slice().forEach(entry => {",
        "      if (hasTableIntersection(entry.tables, changedTables)) {",
        "        entry.emit();",
        "      }",
        "    });",
        "  }",
        "}",
    ])
    return "\n".join(lines).rstrip() + "\n"


def tables_used_by_file(sql_file: SqlFile, tables: Dict[str, Table]) -> List[Table]:
    used: Dict[str, Table] = {}
    for query in sql_file.queries:
        for table_name in query.read_tables + query.changed_tables:
            table = tables.get(table_name.lower())
            if table:
                used[table.name.lower()] = table
    return sorted(used.values(), key=lambda table: table.name)


def types_imported_by_queries(queries: List[Query]) -> List[str]:
    imports = set()
    for query in queries:
        if query.returns_rows:
            imports.add(query.result_type)
    return sorted(imports)


def boolean_result_fields(query: Query) -> List[object]:
    return [
        field
        for field in query.result_fields
        if re.search(r"(?:^|\|\s*)boolean(?:\s*\||$)", field.ts_type)
    ]


def render_interface(name: str, fields: Sequence[object], declare_export: bool) -> List[str]:
    prefix = "export interface" if declare_export else "interface"
    lines = [f"{prefix} {name} {{"]
    for field in fields:
        field_name = getattr(field, "name")
        field_type = getattr(field, "ts_type")
        lines.append(f"  {render_property_name(field_name)}: {field_type};")
    lines.append("}")
    return lines


def method_signature_params(query: Query) -> str:
    return ", ".join(f"{param.name}: {param.ts_type}" for param in query.params)


def method_return_type(query: Query) -> str:
    if query.returns_rows:
        return f"Promise<{query.result_type}[]>"
    return "Promise<void>"


def watch_method_name(query: Query) -> str:
    return f"watch{sanitize_type_name(query.name)}"


def watch_method_signature_params(query: Query) -> str:
    params = method_signature_params(query)
    listener = f"listener: ClientSQLQueryListener<{query.result_type}[]>"
    if params:
        return f"{params}, {listener}"
    return listener


def query_class_name(sql_file: SqlFile) -> str:
    return f"{sanitize_type_name(sql_file.stem_path.name)}Queries"


def import_path_from_db(sql_file: SqlFile, suffix: str) -> str:
    stem = str(sql_file.stem_path).replace(os.sep, "/")
    return f"./{stem}{suffix}"


def generated_header() -> List[str]:
    return [
        "// Generated by clientsql. Do not edit.",
        "",
    ]


def lower_first(name: str) -> str:
    return name[:1].lower() + name[1:]


def render_property_name(name: str) -> str:
    if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
        return name
    return json.dumps(name)
