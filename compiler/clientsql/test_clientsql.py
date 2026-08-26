import os
import re
import sqlite3
import subprocess
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from typing import List


def repository_root() -> Path:
    test_srcdir = os.environ.get("TEST_SRCDIR")
    test_workspace = os.environ.get("TEST_WORKSPACE")
    if test_srcdir and test_workspace:
        return Path(test_srcdir) / test_workspace
    return Path(__file__).resolve().parents[2]


def environment_tool_path(name: str) -> Path | None:
    value = os.environ.get(name)
    if value is None:
        return None
    path = Path(value)
    return path if path.is_absolute() else REPO_ROOT / path


REPO_ROOT = repository_root()
CLIENTSQL_EXECUTABLE = environment_tool_path("CLIENTSQL_TEST_GENERATOR")
CLIENTSQL_SOURCE = REPO_ROOT / "compiler" / "clientsql" / "src" / "clientsql_main.py"
CLIENTSQL_PACKAGER = REPO_ROOT / "compiler" / "clientsql" / "package_clientsql.py"
JAVASCRIPT_RUNNER = environment_tool_path("CLIENTSQL_TEST_JAVASCRIPT_RUNNER")
TYPESCRIPT_COMPILER = environment_tool_path("CLIENTSQL_TEST_TYPESCRIPT_COMPILER")
SQLITE_VALIDATOR = environment_tool_path("CLIENTSQL_TEST_SQLITE_VALIDATOR")
LOCAL_TYPESCRIPT_COMPILER = (
    REPO_ROOT / "npm_modules" / "cli" / "node_modules" / "typescript" / "bin" / "tsc"
)


class ClientSQLGeneratorTest(unittest.TestCase):
    @staticmethod
    def clientsql_command() -> List[str]:
        if CLIENTSQL_EXECUTABLE is not None:
            if not CLIENTSQL_EXECUTABLE.is_file() or not os.access(CLIENTSQL_EXECUTABLE, os.X_OK):
                raise RuntimeError(f"ClientSQL generator is not executable: {CLIENTSQL_EXECUTABLE}")
            return [str(CLIENTSQL_EXECUTABLE)]
        return [sys.executable, str(CLIENTSQL_SOURCE)]

    @classmethod
    def setUpClass(cls) -> None:
        if SQLITE_VALIDATOR is None or not SQLITE_VALIDATOR.is_file():
            raise RuntimeError(
                "CLIENTSQL_TEST_SQLITE_VALIDATOR must name the built SQLite 3.16.0 validator"
            )

    @staticmethod
    def validator_arguments() -> List[str]:
        assert SQLITE_VALIDATOR is not None
        return ["--sqlite-validator", str(SQLITE_VALIDATOR)]

    def test_native_contract_versions_every_exported_declaration(self) -> None:
        native_contract = (
            REPO_ROOT
            / "src"
            / "valdi_modules"
            / "src"
            / "valdi"
            / "client_sql"
            / "src"
            / "ClientSQLNative.d.ts"
        ).read_text(encoding="utf-8")
        exported_declarations = list(re.finditer(r"@(ExportModule|ExportModel|ExportProxy)\b", native_contract))
        self.assertEqual(4, len(exported_declarations))
        for index, declaration in enumerate(exported_declarations):
            next_start = (
                exported_declarations[index + 1].start()
                if index + 1 < len(exported_declarations)
                else len(native_contract)
            )
            declaration_block = native_contract[declaration.start():next_start]
            self.assertEqual(1, declaration_block.count("@Version(__PLACEHOLDER__)"))
        self.assertIn("Only this value boundary intentionally marshals as untyped", native_contract)

    def test_native_sqlite_dependency_shape_is_explicit_for_apple_and_default(self) -> None:
        client_sql_build = (
            REPO_ROOT / "src" / "valdi_modules" / "src" / "valdi" / "client_sql" / "BUILD.bazel"
        ).read_text(encoding="utf-8")
        sqlite_build = (REPO_ROOT / "third-party" / "sqlite" / "sqlite.BUILD").read_text(encoding="utf-8")
        sqlite_316_build = (REPO_ROOT / "third-party" / "sqlite" / "sqlite_316.BUILD").read_text(encoding="utf-8")
        sqlite_module = (REPO_ROOT / "MODULE.bazel").read_text(encoding="utf-8")
        sqlite_workspace = (REPO_ROOT / "bzl" / "dependencies.bzl").read_text(encoding="utf-8")
        sqlite_inventory = (REPO_ROOT / "fossa-deps.yml").read_text(encoding="utf-8")

        self.assertIn('"//bzl/conditions:ios": ["-lsqlite3"]', client_sql_build)
        self.assertIn('"//bzl/conditions:macos": ["-lsqlite3"]', client_sql_build)
        self.assertIn('"//conditions:default": ["VALDI_CLIENTSQL_USE_BUNDLED_SQLITE"]', client_sql_build)
        self.assertIn('"//conditions:default": ["@sqlite//:sqlite"]', client_sql_build)
        self.assertNotIn("target_compatible_with", sqlite_build)
        self.assertIn('name = "sqlite"', sqlite_316_build)
        self.assertIn('"-DSQLITE_THREADSAFE=0"', sqlite_316_build)
        for declaration in (sqlite_module, sqlite_workspace):
            self.assertIn("sqlite-autoconf-3530400", declaration)
            self.assertIn("0e9483900e92cd5de8fd48d16bf9200145a61f7fd5be542a5ac81d8a9516eb9c", declaration)
            self.assertNotIn("sqlite-autoconf-3530100", declaration)
            self.assertIn('name = "sqlite_316"', declaration)
            self.assertIn("sqlite-amalgamation-3160000", declaration)
            self.assertIn("3b5dfb65807e2b17e6463357df848e322badba01dc9a4a1de8fdbb72d448e3b0", declaration)
        self.assertIn("version: 3.53.4", sqlite_inventory)
        self.assertIn("sqlite-autoconf-3530400", sqlite_inventory)

    def test_real_runtime_integration_target_links_generated_smoke_and_native_factory(self) -> None:
        valdi_build = (REPO_ROOT / "valdi" / "BUILD.bazel").read_text(encoding="utf-8")
        smoke_source = (
            REPO_ROOT
            / "valdi"
            / "testdata"
            / "resources"
            / "modules"
            / "client_sql_smoke"
            / "src"
            / "ClientSQLSmoke.ts"
        ).read_text(encoding="utf-8")
        integration_source = (
            REPO_ROOT / "valdi" / "test" / "integration" / "ClientSQLRuntime_tests.cpp"
        ).read_text(encoding="utf-8")

        target = valdi_build[valdi_build.index('name = "test_client_sql_runtime_integration"'):]
        self.assertIn("client_sql_smoke:client_sql_smoke_native_desktop", target)
        self.assertIn('"test/integration/ClientSQLRuntime_tests.cpp"', target)
        integration_function = smoke_source[smoke_source.index("export function runClientSQLNativeIntegration"):]
        self.assertIn("TestDb.open(`runtime-integration-${Date.now()}`)", integration_function)
        self.assertIn("new ArrayBuffer(0)", integration_function)
        self.assertIn("rows[0].enabled !== true", integration_function)
        self.assertNotIn("setClientSQLNativeForTests", integration_function)
        self.assertIn("runClientSQLNativeIntegration", integration_source)
        self.assertIn("client_sql_smoke/src/ClientSQLSmoke", integration_source)

    def test_source_generator_is_the_toolchain_executable(self) -> None:
        toolchain = (REPO_ROOT / "bzl" / "valdi" / "BUILD.bazel").read_text(encoding="utf-8")
        toolchain_contract = (REPO_ROOT / "bzl" / "valdi" / "valdi_toolchain.bzl").read_text(encoding="utf-8")
        distributed_alias = (REPO_ROOT / "bin" / "BUILD.bazel").read_text(encoding="utf-8")
        processor = (
            REPO_ROOT / "compiler" / "compiler" / "Compiler" / "Sources" / "Processors" / "ClientSqlProcessor.swift"
        ).read_text(encoding="utf-8")
        processor_test = (
            REPO_ROOT
            / "compiler"
            / "compiler"
            / "Compiler"
            / "Tests"
            / "CompilerTests"
            / "ClientSqlProcessorTests.swift"
        ).read_text(encoding="utf-8")
        compiler_invocation = (
            REPO_ROOT / "bzl" / "valdi" / "valdi_run_compiler.bzl"
        ).read_text(encoding="utf-8")
        validator_source = (
            REPO_ROOT / "compiler" / "clientsql" / "sqlite_316_validator.cpp"
        ).read_text(encoding="utf-8")
        sql_parser = (
            REPO_ROOT / "compiler" / "clientsql" / "src" / "clientsql" / "sql.py"
        ).read_text(encoding="utf-8")
        self.assertIn('sqldelight_compiler = "//compiler/clientsql:clientsql"', toolchain)
        self.assertIn(
            'clientsql_sqlite_validator = "//compiler/clientsql:sqlite_316_validator"',
            toolchain,
        )
        self.assertRegex(
            toolchain_contract,
            r'"sqldelight_compiler": attr\.label\(\s+executable = True,\s+cfg = "exec"',
        )
        self.assertIn('actual = "@valdi//compiler/clientsql:clientsql"', distributed_alias)
        self.assertIn(
            'actual = "@valdi//compiler/clientsql:sqlite_316_validator"',
            distributed_alias,
        )
        self.assertNotRegex(distributed_alias, r'name = "sqldelight_compiler",\s+srcs = \[\]')
        self.assertIn('metadata: ["artifact": output]', processor)
        self.assertIn("moduleName: bundleInfo.name", processor)
        self.assertIn('"--sqlite-validator", sqliteValidatorPath', processor)
        self.assertIn('"--sqlite-validator", "/tools/sqlite_316_validator"', processor_test)
        self.assertIn('args.add("--direct-client-sql-validator-path", client_sql_validator[0])', compiler_invocation)
        self.assertIn('constexpr int kExpectedSQLiteVersionNumber = 3016000;', validator_source)
        self.assertIn('e2920fb885569d14197c9b7958e6f1db573ee669', validator_source)
        self.assertNotIn("import sqlite3", sql_parser)
        self.assertNotIn("SQLITE_LATER_DIALECT_FEATURES", sql_parser)
        self.assertIn('"-m", "MyBundle"', processor_test)
        self.assertIn('package: "SharedDb"', processor_test)

    def test_packaged_generator_matches_canonical_source(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "clientsql"
            package_result = subprocess.run(
                [sys.executable, str(CLIENTSQL_PACKAGER), "--output", str(executable)],
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            self.assertEqual(package_result.returncode, 0, msg=package_result.stderr)

            check_result = subprocess.run(
                [sys.executable, str(CLIENTSQL_PACKAGER), "--output", str(executable), "--check"],
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            self.assertEqual(check_result.returncode, 0, msg=check_result.stderr)

            source_version = subprocess.run(
                [*self.clientsql_command(), *self.validator_arguments(), "-version"],
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            packaged_version = subprocess.run(
                [sys.executable, str(executable), *self.validator_arguments(), "-version"],
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            self.assertEqual(source_version.returncode, 0, msg=source_version.stderr)
            self.assertEqual(packaged_version.returncode, 0, msg=packaged_version.stderr)
            self.assertEqual(source_version.stdout, packaged_version.stdout)
            self.assertRegex(
                source_version.stdout,
                r"0\.2\.0\+source\.sha256\.[0-9a-f]{64}"
                r"\+validator\.sqlite-3\.16\.0\.source-id-[0-9a-f]{40}"
                r"\.sqlite3-c-sha1-[0-9a-f]{40}\.protocol-1\.binary-sha256-[0-9a-f]{64}",
            )

            root = Path(directory)
            sql_dir = root / "sql"
            package_dir = sql_dir / "TestDb"
            package_dir.mkdir(parents=True)
            (package_dir / "Item.sq").write_text(
                "CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY);\n\nselectAll:\nSELECT * FROM item;",
                encoding="utf-8",
            )
            source_output = root / "source-output"
            packaged_output = root / "packaged-output"
            common_arguments = [
                *self.validator_arguments(),
                "-s", str(sql_dir), "-p", "TestDb", "-c", "TestDb", "-m", "FixtureModule",
                "-l", "typescript",
            ]
            source_result = subprocess.run(
                [*self.clientsql_command(), *common_arguments, "-o", str(source_output)],
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            packaged_result = subprocess.run(
                [sys.executable, str(executable), *common_arguments, "-o", str(packaged_output)],
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            self.assertEqual(source_result.returncode, 0, msg=source_result.stderr)
            self.assertEqual(packaged_result.returncode, 0, msg=packaged_result.stderr)
            source_files = sorted(path.relative_to(source_output) for path in source_output.rglob("*.ts"))
            packaged_files = sorted(path.relative_to(packaged_output) for path in packaged_output.rglob("*.ts"))
            self.assertEqual(source_files, packaged_files)
            for relative_path in source_files:
                self.assertEqual(
                    (source_output / relative_path).read_bytes(),
                    (packaged_output / relative_path).read_bytes(),
                    msg=str(relative_path),
                )

    def assert_in_order(self, text: str, *needles: str) -> None:
        cursor = 0
        for needle in needles:
            index = text.find(needle, cursor)
            self.assertNotEqual(index, -1, msg=f"Missing {needle!r} after offset {cursor}")
            cursor = index + len(needle)

    def run_clientsql(self, sql_dir: Path, out_dir: Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                *self.clientsql_command(),
                *self.validator_arguments(),
                "-s",
                str(sql_dir),
                "-p",
                "TestDb",
                "-c",
                "TestDb",
                "-m",
                "TestDb",
                "-o",
                str(out_dir),
                "-l",
                "typescript",
            ],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

    def node_command(self) -> List[str]:
        node = shutil.which("node")
        if node:
            return [node]
        raise RuntimeError("ClientSQL generated-code tests require Node.js")

    def typescript_compiler_command(self) -> List[str]:
        if TYPESCRIPT_COMPILER is not None:
            if not TYPESCRIPT_COMPILER.is_file() or not os.access(TYPESCRIPT_COMPILER, os.X_OK):
                raise RuntimeError(f"TypeScript compiler is not executable: {TYPESCRIPT_COMPILER}")
            return [str(TYPESCRIPT_COMPILER)]
        if not LOCAL_TYPESCRIPT_COMPILER.is_file():
            raise RuntimeError(f"TypeScript compiler is missing: {LOCAL_TYPESCRIPT_COMPILER}")
        return [*self.node_command(), str(LOCAL_TYPESCRIPT_COMPILER)]

    def javascript_command(self, entrypoint: Path) -> List[str]:
        if JAVASCRIPT_RUNNER is not None:
            if not JAVASCRIPT_RUNNER.is_file() or not os.access(JAVASCRIPT_RUNNER, os.X_OK):
                raise RuntimeError(f"JavaScript runner is not executable: {JAVASCRIPT_RUNNER}")
            return [str(JAVASCRIPT_RUNNER), str(entrypoint)]
        return [*self.node_command(), str(entrypoint)]

    def run_generated_typescript(self, root: Path, entrypoint: Path) -> subprocess.CompletedProcess[str]:
        tsconfig = root / "tsconfig.json"
        tsconfig.write_text(
            """
            {
              "compilerOptions": {
                "target": "ES2019",
                "module": "commonjs",
                "strict": true,
                "skipLibCheck": true,
                "baseUrl": ".",
                "rootDir": ".",
                "outDir": "dist",
                "lib": ["ES2019", "DOM"]
              },
              "include": ["**/*.ts"]
            }
            """,
            encoding="utf-8",
        )

        shim_dir = root / "client_sql" / "src"
        shim_dir.mkdir(parents=True)
        (shim_dir / "ClientSQLDebug.ts").write_text(
            """
            export interface ClientSQLDebugDatabase {}
            export const registeredDatabases: ClientSQLDebugDatabase[] = [];
            export function registerClientSQLDebugDatabase(database: ClientSQLDebugDatabase): void {
              registeredDatabases.push(database);
            }
            export function unregisterClientSQLDebugDatabase(database: ClientSQLDebugDatabase): void {
              const index = registeredDatabases.indexOf(database);
              if (index !== -1) {
                registeredDatabases.splice(index, 1);
              }
            }
            export function notifyClientSQLDebugChanged(_databaseName: string | undefined): void {}
            """,
            encoding="utf-8",
        )

        compile_result = subprocess.run(
            [*self.typescript_compiler_command(), "--project", str(tsconfig)],
            cwd=root,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if compile_result.returncode != 0:
            return compile_result

        compiled_entrypoint = root / "dist" / entrypoint.relative_to(root).with_suffix(".js")
        env = dict(os.environ)
        env["NODE_PATH"] = str(root / "dist")
        return subprocess.run(
            self.javascript_command(compiled_entrypoint),
            cwd=root,
            env=env,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

    def test_generates_typescript_database_and_query_bindings(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sql_dir = root / "sql"
            db_dir = sql_dir / "TestDb"
            db_dir.mkdir(parents=True)
            (db_dir / "User.sq").write_text(
                """
                CREATE TABLE user (
                  id INTEGER NOT NULL PRIMARY KEY,
                  name TEXT NOT NULL,
                  age INTEGER,
                  nickname TEXT
                );

                selectAll:
                SELECT * FROM user;

                selectById:
                SELECT id, name FROM user WHERE id = :id;

                insertUser:
                INSERT INTO user(id, name, age) VALUES (:id, :name, :age);
                """,
                encoding="utf-8",
            )
            migration_dir = sql_dir / "migration"
            migration_dir.mkdir()
            (migration_dir / "2.sqm").write_text("ALTER TABLE user ADD COLUMN nickname TEXT;", encoding="utf-8")

            out_dir = root / "out"
            result = self.run_clientsql(sql_dir, out_dir)

            self.assertEqual(result.returncode, 0, msg=result.stderr)
            database = (out_dir / "TestDb.ts").read_text(encoding="utf-8")
            self.assertIn("export class TestDb", database)
            self.assertIn("openDatabase", database)
            self.assertIn('const ALL_TABLES: string[] = ["user"];', database)
            self.assertIn("watchQuery<T>(", database)
            self.assertIn("export type ClientSQLNativeCallback<T>", database)
            self.assertIn("client_sql/src/ClientSQLDebug", database)
            self.assertIn("private debugDatabase: ClientSQLDebugDatabase | undefined;", database)
            self.assertIn("const debugDatabase: ClientSQLDebugDatabase = {", database)
            self.assertIn("this.debugDatabase = debugDatabase;", database)
            self.assertIn("registerClientSQLDebugDatabase(debugDatabase);", database)
            self.assertIn("unregisterClientSQLDebugDatabase(this.debugDatabase);", database)
            self.assertIn("id: this.databaseIdentity,", database)
            self.assertIn("const identity = databaseIdentity(databaseName);", database)
            self.assertIn(
                "execute(sql: string, parameters: ClientSQLValue[] | undefined, callback: ClientSQLNativeCallback<void>): void;",
                database,
            )
            self.assertIn("export interface ClientSQLNativeTransaction", database)
            self.assertIn(
                "transaction(body: ClientSQLNativeTransactionBody, callback: ClientSQLNativeCallback<void>): void;",
                database,
            )
            self.assertNotIn("queryOnWriter", database)
            self.assertIn("function nativePromise<T>", database)
            self.assertIn("await nativePromise<void>(callback => this.connection.execute(sql, parameters, callback));", database)
            self.assertIn(
                "nativePromise<T[]>(callback => nativeTransaction.query<T>(sql, parameters, callback))",
                database,
            )
            self.assertIn("debugInfo: (): Promise<Record<string, unknown>> => this.debugInfo(),", database)
            self.assertIn("changedTables: string[] | undefined", database)
            self.assertIn("static open(name: string | undefined)", database)
            self.assertNotIn("parameters?: ClientSQLValue[]", database)
            self.assertNotIn("parameters: ClientSQLValue[] = []", database)
            self.assertNotIn("changedTables?: string[]", database)
            self.assertNotIn("name: string = DEFAULT_DATABASE_NAME", database)
            self.assertIn("export interface TestDbTransaction", database)
            self.assertIn("userQueries: new UserQueries(transactionDatabase)", database)
            self.assertIn("this.emitChangedTables(changedTables);", database)
            self.assertIn("export interface User", (out_dir / "UserTypes.ts").read_text(encoding="utf-8"))
            self.assertFalse((out_dir / "TestDb.d.ts").exists())
            queries = (out_dir / "UserQueries.ts").read_text(encoding="utf-8")
            self.assertIn("selectById(id: number)", queries)
            self.assertIn("watchSelectById(id: number, listener: ClientSQLQueryListener<SelectByIdRow[]>)", queries)
            self.assertIn('SELECT id, name FROM user WHERE id = ?;', queries)
            self.assertIn("insertUser(id: number, name: string, age: number | null)", queries)
            self.assertIn('["user"]', queries)

    def test_supports_sqldelight_style_query_shapes(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sql_dir = root / "sql"
            db_dir = sql_dir / "TestDb"
            db_dir.mkdir(parents=True)
            (db_dir / "User.sq").write_text(
                """
                CREATE TABLE user (
                  id INTEGER NOT NULL PRIMARY KEY,
                  name TEXT NOT NULL,
                  age INTEGER
                );

                selectPage:
                SELECT * FROM user ORDER BY name DESC LIMIT :limit OFFSET :offset;

                selectCommaPage:
                SELECT * FROM user ORDER BY id LIMIT :rowOffset, :limit;

                selectByOptionalAge:
                SELECT id, name FROM user WHERE (:age IS NULL OR age >= :age) ORDER BY id LIMIT :limit;

                countUsers:
                SELECT count(*) AS count FROM user;

                updateUser:
                UPDATE user SET name = :name, age = :age WHERE id = :id;

                deleteUser:
                DELETE FROM user WHERE id = :id;
                """,
                encoding="utf-8",
            )

            out_dir = root / "out"
            result = self.run_clientsql(sql_dir, out_dir)

            self.assertEqual(result.returncode, 0, msg=result.stderr)
            user_types = (out_dir / "UserTypes.ts").read_text(encoding="utf-8")
            self.assertIn("export interface CountUsersRow", user_types)
            self.assertIn("count: number;", user_types)
            self.assertIn("export interface SelectPageParams", user_types)
            self.assertIn("offset: number;", user_types)
            self.assertIn("export interface SelectCommaPageParams", user_types)
            self.assertIn("rowOffset: number;", user_types)
            self.assertIn("export interface SelectByOptionalAgeParams", user_types)

            queries = (out_dir / "UserQueries.ts").read_text(encoding="utf-8")
            self.assertIn("import { CountUsersRow, SelectByOptionalAgeRow, User } from './UserTypes';", queries)
            self.assertNotIn("SelectByOptionalAgeParams", queries)
            self.assertIn("selectPage(limit: number, offset: number): Promise<User[]>", queries)
            self.assertIn("watchSelectPage(limit: number, offset: number, listener: ClientSQLQueryListener<User[]>)", queries)
            self.assertIn('return this.db.watchQuery(["user"], () => this.selectPage(limit, offset), listener);', queries)
            self.assertIn('SELECT * FROM user ORDER BY name DESC LIMIT ? OFFSET ?;', queries)
            self.assertIn("selectCommaPage(rowOffset: number, limit: number): Promise<User[]>", queries)
            self.assertIn('SELECT * FROM user ORDER BY id LIMIT ?, ?;', queries)
            self.assertIn("selectByOptionalAge(age: number | null, limit: number)", queries)
            self.assertIn("updateUser(name: string, age: number | null, id: number): Promise<void>", queries)
            self.assertIn('], ["user"]);', queries)
            self.assertIn("deleteUser(id: number): Promise<void>", queries)

    def test_generated_reactive_contract_for_watchers_and_transactions(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sql_dir = root / "sql"
            db_dir = sql_dir / "TestDb"
            db_dir.mkdir(parents=True)
            (db_dir / "User.sq").write_text(
                """
                CREATE TABLE user (
                  id INTEGER NOT NULL PRIMARY KEY,
                  name TEXT NOT NULL
                );

                selectAll:
                SELECT * FROM user;

                insertUser:
                INSERT INTO user(id, name) VALUES (:id, :name);

                updateUser:
                UPDATE user SET name = :name WHERE id = :id;

                deleteUser:
                DELETE FROM user WHERE id = :id;
                """,
                encoding="utf-8",
            )

            out_dir = root / "out"
            result = self.run_clientsql(sql_dir, out_dir)

            self.assertEqual(result.returncode, 0, msg=result.stderr)
            database = (out_dir / "TestDb.ts").read_text(encoding="utf-8")
            queries = (out_dir / "UserQueries.ts").read_text(encoding="utf-8")

            self.assertIn(
                "const watchEntriesByDatabaseName: { [name: string]: ClientSQLWatchEntry[] | undefined } = "
                "Object.create(null);",
                database,
            )
            self.assertIn("  unsubscribe(): void;", database)
            self.assertIn(
                "const writeChainsByDatabaseName: { [name: string]: Promise<void> | undefined } = "
                "Object.create(null);",
                database,
            )
            self.assertIn("function enqueueDatabaseWrite<T>(name: string, body: () => Promise<T>): Promise<T>", database)
            self.assertIn("const chain = previous.then(() => current, () => current);", database)
            self.assertIn("writeChainsByDatabaseName[name] = chain;", database)
            self.assertIn("releaseDatabaseWrite(name, chain, releaseCurrent);", database)
            self.assertIn("return enqueueDatabaseWrite(this.databaseIdentity, async () => {", database)
            self.assertIn("notifyClientSQLDebugChanged", database)
            self.assertIn("interface ClientSQLTransactionDebugEntry {", database)
            self.assertIn("const MAX_TRANSACTION_DEBUG_ENTRIES = 50;", database)
            self.assertIn("private nextTransactionDebugId = 1;", database)
            self.assertIn("private transactionHistory: ClientSQLTransactionDebugEntry[] = [];", database)
            self.assert_in_order(
                database,
                "entriesForDatabase(this.databaseIdentity).push(entry);",
                "this.localWatchEntries.push(entry);",
                "emit();",
            )
            self.assertIn("if (active && currentGeneration === generation) {", database)
            self.assertIn("if (hasTableIntersection(entry.tables, changedTables)) {", database)
            self.assertIn("const entry: ClientSQLWatchEntry = { tables, emit, unsubscribe };", database)

            self.assertIn(
                'return this.db.execute("INSERT INTO user(id, name) VALUES (?, ?);", [id, name], ["user"]);',
                queries,
            )
            self.assertIn(
                'return this.db.execute("UPDATE user SET name = ? WHERE id = ?;", [name, id], ["user"]);',
                queries,
            )
            self.assertIn('return this.db.execute("DELETE FROM user WHERE id = ?;", [id], ["user"]);', queries)

            transaction = database[database.index("  async transaction<T>") : database.index("  async close")]
            self.assert_in_order(
                transaction,
                "return enqueueDatabaseWrite(this.databaseIdentity, () => this.runTransaction(body));",
                "private async runTransaction<T>",
                "const changedTables: string[] = [];",
            )
            self.assert_in_order(
                transaction,
                "const transactionDebugId = this.nextTransactionDebugId++;",
                "const transactionStartedAtMs = Date.now();",
                "this.connection.transaction((nativeTransaction, transactionCallback) => {",
                "const transaction = this.createTransactionScope(nativeTransaction, changedTables);",
                "void bodyPromise.then(",
                "transactionCallback(undefined, undefined);",
                "const transactionCompletedAtMs = Date.now();",
                "this.recordTransactionDebugEntry({",
                "status: 'committed',",
                "this.emitChangedTables(changedTables);",
            )
            self.assertIn("userQueries: new UserQueries(transactionDatabase)", transaction)
            self.assertIn("transaction: nestedBody => nestedBody(scope)", transaction)
            self.assertIn("ClientSQL watchers cannot be created inside a transaction", transaction)
            self.assert_in_order(
                transaction,
                "} catch (error) {",
                "this.recordTransactionDebugEntry({",
                "status: 'rolled_back',",
                "error: errorMessage(error),",
                "notifyClientSQLDebugChanged(this.databaseIdentity);",
                "throw error;",
            )
            self.assertNotIn("BEGIN TRANSACTION", transaction)
            self.assertNotIn("COMMIT", transaction)
            self.assertNotIn("ROLLBACK", transaction)

            close = database[database.index("  async close") : database.index("  private notifyTablesChanged")]
            self.assertIn("if (typeof entry.unsubscribe === 'function') {", close)
            self.assertIn("entry.unsubscribe();", close)
            self.assertIn("removeWatchEntry(this.localWatchEntries, entry);", close)
            self.assertIn(
                "await enqueueDatabaseWrite(this.databaseIdentity, () => nativePromise<void>(callback => this.connection.close(callback)));",
                close,
            )

            debug = database[database.index("  private async debugInfo") : database.index("  private notifyTablesChanged")]
            self.assertIn("debugInfo?: (callback: ClientSQLNativeCallback<Record<string, unknown>>) => void;", debug)
            self.assertIn("pendingChangedTableCount: this.activeTransactionChangedTables?.length ?? 0,", debug)
            self.assertIn("transactionHistoryCount: this.transactionHistory.length,", debug)
            self.assertIn("transactions: this.transactionHistory.slice().reverse(),", debug)
            self.assertIn("watcherCount: (watchEntriesByDatabaseName[this.databaseIdentity] || []).length,", debug)

            recorder = database[database.index("  private recordTransactionDebugEntry") : database.index("  private notifyTablesChanged")]
            self.assertIn("this.transactionHistory.push(entry);", recorder)
            self.assertIn("this.transactionHistory.length - MAX_TRANSACTION_DEBUG_ENTRIES", recorder)

            notify = database[database.index("  private notifyTablesChanged") : database.index("  private emitChangedTables")]
            self.assertIn("this.emitChangedTables(uniqueTables);", notify)
            self.assertNotIn("transactionDepth", notify)

            emit = database[database.index("  private emitChangedTables") : database.index("}", database.index("  private emitChangedTables"))]
            self.assertIn("notifyClientSQLDebugChanged(this.databaseIdentity);", emit)

    def test_generated_watchers_execute_reactive_contract(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sql_dir = root / "sql"
            db_dir = sql_dir / "TestDb"
            db_dir.mkdir(parents=True)
            (db_dir / "User.sq").write_text(
                """
                CREATE TABLE user (
                  id INTEGER NOT NULL PRIMARY KEY,
                  name TEXT NOT NULL
                );

                selectAll:
                SELECT * FROM user ORDER BY id;

                countUsers:
                SELECT count(*) AS count FROM user;

                insertUser:
                INSERT INTO user(id, name) VALUES (:id, :name);
                """,
                encoding="utf-8",
            )

            out_dir = root / "out"
            result = self.run_clientsql(sql_dir, out_dir)
            self.assertEqual(result.returncode, 0, msg=result.stderr)

            entrypoint = root / "generated_behavior_test.ts"
            entrypoint.write_text(
                r"""
                import {
                  ClientSQLMigration,
                  ClientSQLNativeCallback,
                  ClientSQLNativeConnection,
                  ClientSQLNativeModule,
                  ClientSQLNativeTransaction,
                  ClientSQLNativeTransactionBody,
                  ClientSQLValue,
                  TestDb,
                  setClientSQLNativeForTests,
                } from './out/TestDb';
                import { registeredDatabases } from 'client_sql/src/ClientSQLDebug';

                interface UserRow {
                  id: number;
                  name: string;
                }

                class Store {
                  rows: UserRow[] = [];
                  readonly operations: string[] = [];
                }

                function queryRows(store: Store, sql: string): Record<string, unknown>[] {
                  if (sql.indexOf('SELECT count(*) AS count FROM user') === 0) {
                    return [{ count: store.rows.length }];
                  }
                  if (sql.indexOf('SELECT * FROM user') === 0) {
                    return store.rows
                      .slice()
                      .sort((left, right) => left.id - right.id)
                      .map(row => ({ ...row }));
                  }
                  throw new Error(`Unexpected query SQL: ${sql}`);
                }

                class FakeTransaction implements ClientSQLNativeTransaction {
                  constructor(private readonly store: Store) {}

                  execute(
                    sql: string,
                    parameters: ClientSQLValue[] | undefined,
                    callback: ClientSQLNativeCallback<void>,
                  ): void {
                    try {
                      this.store.operations.push(sql);
                      if (sql.indexOf('INSERT INTO user') === 0) {
                        const id = Number(parameters?.[0]);
                        const name = String(parameters?.[1]);
                        this.store.operations.push(`insert:${id}`);
                        this.store.rows.push({ id, name });
                      } else {
                        throw new Error(`Unexpected execute SQL: ${sql}`);
                      }
                      callback(undefined, undefined);
                    } catch (error) {
                      callback(undefined, errorMessage(error));
                    }
                  }

                  query<T>(
                    sql: string,
                    _parameters: ClientSQLValue[] | undefined,
                    callback: ClientSQLNativeCallback<T[]>,
                  ): void {
                    try {
                      callback(queryRows(this.store, sql) as T[], undefined);
                    } catch (error) {
                      callback(undefined, errorMessage(error));
                    }
                  }
                }

                class FakeConnection implements ClientSQLNativeConnection {
                  constructor(private readonly store: Store) {}

                  execute(
                    sql: string,
                    parameters: ClientSQLValue[] | undefined,
                    callback: ClientSQLNativeCallback<void>,
                  ): void {
                    try {
                      this.store.operations.push(sql);
                      if (sql.indexOf('INSERT INTO user') === 0) {
                        const id = Number(parameters?.[0]);
                        const name = String(parameters?.[1]);
                        this.store.operations.push(`insert:${id}`);
                        this.store.rows.push({ id, name });
                      } else {
                        throw new Error(`Unexpected execute SQL: ${sql}`);
                      }
                      callback(undefined, undefined);
                    } catch (error) {
                      callback(undefined, errorMessage(error));
                    }
                  }

                  query<T>(
                    sql: string,
                    _parameters: ClientSQLValue[] | undefined,
                    callback: ClientSQLNativeCallback<T[]>,
                  ): void {
                    try {
                      callback(queryRows(this.store, sql) as T[], undefined);
                    } catch (error) {
                      callback(undefined, errorMessage(error));
                    }
                  }

                  transaction(
                    body: ClientSQLNativeTransactionBody,
                    callback: ClientSQLNativeCallback<void>,
                  ): void {
                    const snapshot = this.store.rows.map(row => ({ ...row }));
                    this.store.operations.push('transaction:start');
                    body(new FakeTransaction(this.store), (_value, error) => {
                      if (error !== undefined && error !== null) {
                        this.store.rows = snapshot.map(row => ({ ...row }));
                        this.store.operations.push('transaction:rollback');
                        callback(undefined, error);
                        return;
                      }
                      this.store.operations.push('transaction:commit');
                      callback(undefined, undefined);
                    });
                  }

                  close(callback: ClientSQLNativeCallback<void>): void {
                    this.store.operations.push('close');
                    callback(undefined, undefined);
                  }
                }

                class FakeNative implements ClientSQLNativeModule {
                  private readonly storesByName: { [name: string]: Store | undefined } = Object.create(null);
                  lastOpenedName = '';

                  openDatabase(
                    name: string,
                    _schemaVersion: number,
                    _createStatements: string[],
                    _migrations: ClientSQLMigration[],
                  ): ClientSQLNativeConnection {
                    this.lastOpenedName = name;
                    return new FakeConnection(this.store(name));
                  }

                  store(name: string): Store {
                    let store = this.storesByName[name];
                    if (!store) {
                      store = new Store();
                      this.storesByName[name] = store;
                    }
                    return store;
                  }
                }

                function errorMessage(error: unknown): string {
                  return error instanceof Error ? error.message : String(error);
                }

                function assert(condition: unknown, message: string): void {
                  if (!condition) {
                    throw new Error(message);
                  }
                }

                function assertNames(rows: UserRow[], expected: string[], message: string): void {
                  const actual = rows.map(row => row.name).join(',');
                  const wanted = expected.join(',');
                  assert(actual === wanted, `${message}: expected ${wanted}, got ${actual}`);
                }

                function nextTurn(): Promise<void> {
                  return new Promise(resolve => setTimeout(resolve, 0));
                }

                async function waitFor(condition: () => boolean, message: string): Promise<void> {
                  for (let attempt = 0; attempt < 20; attempt += 1) {
                    if (condition()) {
                      return;
                    }
                    await nextTurn();
                  }
                  throw new Error(message);
                }

                async function testWatcherEmissions(): Promise<void> {
                  const native = new FakeNative();
                  setClientSQLNativeForTests(native);
                  const db = TestDb.open('watcher-contract');
                  const emissions: UserRow[][] = [];
                  const subscription = db.userQueries.watchSelectAll(rows => {
                    emissions.push(rows);
                  });

                  await waitFor(() => emissions.length === 1, 'initial watcher emission did not arrive');
                  assertNames(emissions[0], [], 'initial watcher emission');

                  await db.userQueries.insertUser(1, 'Ada');
                  await waitFor(() => emissions.length === 2, 'single write did not emit exactly once');
                  assertNames(emissions[1], ['Ada'], 'single write watcher emission');

                  await db.transaction(async transaction => {
                    await transaction.userQueries.insertUser(2, 'Grace');
                    await transaction.userQueries.insertUser(3, 'Katherine');
                  });
                  await waitFor(() => emissions.length === 3, 'transaction did not emit after commit');
                  assertNames(emissions[2], ['Ada', 'Grace', 'Katherine'], 'transaction watcher emission');

                  let failed = false;
                  try {
                    await db.transaction(async transaction => {
                      await transaction.userQueries.insertUser(4, 'Rolled Back');
                      throw new Error('rollback');
                    });
                  } catch {
                    failed = true;
                  }
                  assert(failed, 'rollback transaction should reject');
                  await nextTurn();
                  assert(emissions.length === 3, `rollback emitted ${emissions.length - 3} extra time(s)`);

                  const countRows = await db.userQueries.countUsers();
                  assert(countRows[0].count === 3, `rollback left ${countRows[0].count} rows`);
                  const debugInfo = await (registeredDatabases[0] as any).debugInfo();
                  const transactions = debugInfo.transactions as Array<{
                    status: string;
                    durationMs: number;
                    changedTableCount: number;
                    changedTables: string[];
                    error?: string;
                  }>;
                  assert(transactions.length === 2, `expected 2 transaction history entries, got ${transactions.length}`);
                  assert(transactions[0].status === 'rolled_back', `newest transaction status was ${transactions[0].status}`);
                  assert(transactions[0].durationMs >= 0, 'rollback duration was not recorded');
                  assert(transactions[0].changedTableCount === 1, 'rollback changed table count was not recorded');
                  assert(transactions[0].changedTables[0] === 'user', 'rollback changed table was not recorded');
                  assert(transactions[0].error === 'rollback', `rollback error was ${transactions[0].error}`);
                  assert(transactions[1].status === 'committed', `older transaction status was ${transactions[1].status}`);
                  assert(transactions[1].durationMs >= 0, 'commit duration was not recorded');
                  assert(transactions[1].changedTableCount === 1, 'commit changed table count was not recorded');
                  assert(transactions[1].changedTables[0] === 'user', 'commit changed table was not recorded');
                  assert(debugInfo.queuedWrite === false, 'completed writes left the database queue marked active');

                  subscription.unsubscribe();
                  await db.userQueries.insertUser(5, 'No Emit');
                  await nextTurn();
                  assert(emissions.length === 3, 'unsubscribed watcher emitted');
                  await db.close();
                }

                async function testWriteQueueIsolation(): Promise<void> {
                  const native = new FakeNative();
                  setClientSQLNativeForTests(native);
                  const first = TestDb.open('queue-contract');
                  const second = TestDb.open('queue-contract');
                  const store = native.store(native.lastOpenedName);
                  let releaseTransaction!: () => void;
                  let markStarted!: () => void;
                  const transactionStarted = new Promise<void>(resolve => {
                    markStarted = resolve;
                  });
                  const transactionBlocker = new Promise<void>(resolve => {
                    releaseTransaction = resolve;
                  });

                  const transactionPromise = first.transaction(async transaction => {
                    await transaction.userQueries.insertUser(10, 'Inside');
                    let parentExecuteRejected = false;
                    try {
                      await first.userQueries.insertUser(12, 'Reentrant');
                    } catch (error) {
                      parentExecuteRejected = errorMessage(error).indexOf('parent handle inside a transaction body') !== -1;
                    }
                    assert(parentExecuteRejected, 'parent execute did not reject inside transaction body');
                    let parentQueryRejected = false;
                    try {
                      await first.userQueries.selectAll();
                    } catch (error) {
                      parentQueryRejected = errorMessage(error).indexOf('parent handle inside a transaction body') !== -1;
                    }
                    assert(parentQueryRejected, 'parent query did not reject inside transaction body');
                    let parentCloseRejected = false;
                    try {
                      await first.close();
                    } catch (error) {
                      parentCloseRejected = errorMessage(error).indexOf('parent handle inside a transaction body') !== -1;
                    }
                    assert(parentCloseRejected, 'parent close did not reject inside transaction body');
                    markStarted();
                    await transactionBlocker;
                  });
                  await transactionStarted;

                  const activeDebugInfo = await (registeredDatabases[0] as any).debugInfo();
                  assert(activeDebugInfo.queuedWrite === true, 'active transaction was not reported as queued');
                  const outsideWrite = second.userQueries.insertUser(11, 'Outside');
                  await nextTurn();
                  await nextTurn();
                  assert(!store.rows.some(row => row.id === 11), 'outside write interleaved into open transaction');

                  releaseTransaction();
                  await transactionPromise;
                  await outsideWrite;

                  const commitIndex = store.operations.indexOf('transaction:commit');
                  const outsideIndex = store.operations.indexOf('insert:11');
                  assert(commitIndex !== -1, 'transaction did not commit');
                  assert(outsideIndex > commitIndex, `outside write ran before commit: ${store.operations.join(' | ')}`);
                  const idleDebugInfo = await (registeredDatabases[0] as any).debugInfo();
                  assert(idleDebugInfo.queuedWrite === false, 'drained write queue remained marked active');
                  await first.close();
                  await second.close();
                }

                async function main(): Promise<void> {
                  await testWatcherEmissions();
                  await testWriteQueueIsolation();
                }

                void main().catch(error => {
                  console.error(error);
                  throw error;
                });
                """,
                encoding="utf-8",
            )

            run_result = self.run_generated_typescript(root, entrypoint)
            self.assertEqual(
                run_result.returncode,
                0,
                msg=f"stdout:\n{run_result.stdout}\nstderr:\n{run_result.stderr}",
            )

    def test_nested_sql_files_preserve_output_paths(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sql_dir = root / "sql"
            nested_dir = sql_dir / "TestDb" / "account"
            nested_dir.mkdir(parents=True)
            (nested_dir / "Session.sq").write_text(
                """
                CREATE TABLE session (
                  id TEXT NOT NULL PRIMARY KEY,
                  created_at INTEGER NOT NULL
                );

                selectRecent:
                SELECT * FROM session ORDER BY created_at DESC LIMIT :limit;
                """,
                encoding="utf-8",
            )

            out_dir = root / "out"
            result = self.run_clientsql(sql_dir, out_dir)

            self.assertEqual(result.returncode, 0, msg=result.stderr)
            self.assertTrue((out_dir / "account" / "SessionTypes.ts").exists())
            self.assertTrue((out_dir / "account" / "SessionQueries.ts").exists())
            database = (out_dir / "TestDb.ts").read_text(encoding="utf-8")
            self.assertIn("import { SessionQueries } from './account/SessionQueries';", database)
            queries = (out_dir / "account" / "SessionQueries.ts").read_text(encoding="utf-8")
            self.assertIn("import { Session } from './SessionTypes';", queries)
            self.assertIn("selectRecent(limit: number): Promise<Session[]>", queries)
            self.assertIn("watchSelectRecent(limit: number, listener: ClientSQLQueryListener<Session[]>)", queries)

    def test_generated_boolean_codecs_execute_nullable_true_and_false(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sql_dir = root / "sql"
            db_dir = sql_dir / "TestDb"
            db_dir.mkdir(parents=True)
            (db_dir / "Flag.sq").write_text(
                """
                CREATE TABLE flag (
                  id INTEGER NOT NULL PRIMARY KEY,
                  enabled BOOLEAN NOT NULL,
                  optional BOOLEAN
                );

                selectFlags:
                SELECT enabled, optional FROM flag ORDER BY id;
                """,
                encoding="utf-8",
            )
            out_dir = root / "out"
            result = self.run_clientsql(sql_dir, out_dir)
            self.assertEqual(result.returncode, 0, msg=result.stderr)
            queries = (out_dir / "FlagQueries.ts").read_text(encoding="utf-8")
            self.assertIn("function decodeClientSQLBoolean", queries)
            self.assertIn("decodeNullableClientSQLBoolean", queries)
            self.assertIn("enabled: decodeClientSQLBoolean", queries)
            self.assertIn("optional: decodeNullableClientSQLBoolean", queries)

            entrypoint = root / "boolean_codec_test.ts"
            entrypoint.write_text(
                """
                import { ClientSQLDatabase, ClientSQLValue, FlagQueries } from './out/FlagQueries';

                const db: ClientSQLDatabase = {
                  execute(_sql, _parameters, _changedTables): Promise<void> {
                    return Promise.resolve();
                  },
                  query<T>(_sql: string, _parameters: ClientSQLValue[] | undefined): Promise<T[]> {
                    return Promise.resolve([
                      { enabled: 0, optional: null },
                      { enabled: 1, optional: 0 },
                      { enabled: true, optional: 1 },
                    ] as unknown as T[]);
                  },
                  watchQuery<T>(_tables: string[], _load: () => Promise<T>, _listener: (value: T) => void) {
                    return { unsubscribe(): void {} };
                  },
                };

                void new FlagQueries(db).selectFlags().then(rows => {
                  if (rows[0].enabled !== false || rows[0].optional !== null) throw new Error('nullable false decode');
                  if (rows[1].enabled !== true || rows[1].optional !== false) throw new Error('numeric decode');
                  if (rows[2].enabled !== true || rows[2].optional !== true) throw new Error('boolean decode');
                });
                """,
                encoding="utf-8",
            )
            run_result = self.run_generated_typescript(root, entrypoint)
            self.assertEqual(
                run_result.returncode,
                0,
                msg=f"stdout:\n{run_result.stdout}\nstderr:\n{run_result.stderr}",
            )

    def test_namespaces_database_identity_by_module_and_rejects_identifier_collisions(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sql_dir = root / "sql"
            db_dir = sql_dir / "TestDb"
            db_dir.mkdir(parents=True)
            (db_dir / "Reserved.sq").write_text(
                """
                CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY);

                default:
                SELECT id FROM item WHERE id = :class;
                """,
                encoding="utf-8",
            )

            namespace_values = []
            for module_name in ["BundleOne", "BundleTwo"]:
                out_dir = root / module_name
                command = [
                    *self.clientsql_command(), *self.validator_arguments(),
                    "-s", str(sql_dir), "-p", "TestDb",
                    "-c", "TestDb", "-m", module_name, "-o", str(out_dir), "-l", "typescript",
                ]
                result = subprocess.run(command, check=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                self.assertEqual(result.returncode, 0, msg=result.stderr)
                database = (out_dir / "TestDb.ts").read_text(encoding="utf-8")
                namespace_match = re.search(r'const DATABASE_NAMESPACE = "([0-9a-f]+)";', database)
                self.assertIsNotNone(namespace_match)
                namespace_values.append(namespace_match.group(1))
                queries = (out_dir / "ReservedQueries.ts").read_text(encoding="utf-8")
                self.assertIn("_default(_class: number)", queries)
            self.assertNotEqual(namespace_values[0], namespace_values[1])

            (db_dir / "Reserved.sq").write_text(
                """
                CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY);

                foo_bar:
                SELECT id FROM item;

                fooBar:
                SELECT id FROM item;
                """,
                encoding="utf-8",
            )
            collision = self.run_clientsql(sql_dir, root / "collision")
            self.assertNotEqual(collision.returncode, 0)
            self.assertIn("collides between 'foo_bar' and 'fooBar'", collision.stderr)

            (db_dir / "Reserved.sq").write_text(
                """
                CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY);

                selectCollision:
                SELECT id FROM item WHERE id = :class OR id = :_class;
                """,
                encoding="utf-8",
            )
            parameter_collision = self.run_clientsql(sql_dir, root / "parameter-collision")
            self.assertNotEqual(parameter_collision.returncode, 0)
            self.assertIn("query parameter identifier '_class' collides", parameter_collision.stderr)

    def test_rejects_table_row_and_parameter_type_symbol_collisions(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sql_dir = root / "sql"
            db_dir = sql_dir / "TestDb"
            db_dir.mkdir(parents=True)
            sql_file = db_dir / "Collision.sq"
            sql_file.write_text(
                """
                CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY);
                CREATE TABLE SelectThingRow (id INTEGER NOT NULL PRIMARY KEY);

                selectThing:
                SELECT id AS selected_id FROM item;
                """,
                encoding="utf-8",
            )

            row_collision = self.run_clientsql(sql_dir, root / "row-collision")
            self.assertNotEqual(row_collision.returncode, 0)
            self.assertIn("Generated type identifier 'SelectThingRow' collides", row_collision.stderr)
            self.assertIn("table 'SelectThingRow'", row_collision.stderr)
            self.assertIn("query row for 'selectThing'", row_collision.stderr)

            sql_file.write_text(
                """
                CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY);
                CREATE TABLE InsertThingParams (id INTEGER NOT NULL PRIMARY KEY);

                insertThing:
                INSERT INTO item(id) VALUES (:id);
                """,
                encoding="utf-8",
            )
            params_collision = self.run_clientsql(sql_dir, root / "params-collision")
            self.assertNotEqual(params_collision.returncode, 0)
            self.assertIn("Generated type identifier 'InsertThingParams' collides", params_collision.stderr)
            self.assertIn("table 'InsertThingParams'", params_collision.stderr)
            self.assertIn("query parameters for 'insertThing'", params_collision.stderr)

    def test_shared_sql_lexer_ignores_placeholders_in_literals_identifiers_and_comments(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sql_dir = root / "sql"
            db_dir = sql_dir / "TestDb"
            db_dir.mkdir(parents=True)
            (db_dir / "Lexer.sq").write_text(
                """
                CREATE TABLE item (
                  id INTEGER NOT NULL PRIMARY KEY,
                  note TEXT NOT NULL,
                  "id:quoted" TEXT,
                  `?` TEXT,
                  [:bracket] TEXT
                );

                selectLexer:
                SELECT "id:quoted", `?`, [:bracket], note
                FROM item
                WHERE id = :id
                  AND note != ':literal ?'
                  -- :id IS NULL :line_comment ?
                  /* :id IS NULL :block_comment ? */;
                """,
                encoding="utf-8",
            )

            out_dir = root / "out"
            result = self.run_clientsql(sql_dir, out_dir)
            self.assertEqual(result.returncode, 0, msg=result.stderr)
            queries = (out_dir / "LexerQueries.ts").read_text(encoding="utf-8")
            self.assertIn("selectLexer(id: number)", queries)
            self.assertIn("-- :id IS NULL :line_comment ?", queries)
            self.assertIn("/* :id IS NULL :block_comment ? */", queries)
            self.assertNotIn("line_comment: ClientSQLValue", queries)

    def test_exact_sqlite_316_validator_rejects_later_syntax_and_accepts_supported_corpus(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sql_dir = root / "sql"
            db_dir = sql_dir / "TestDb"
            db_dir.mkdir(parents=True)
            sql_file = db_dir / "Dialect.sq"
            sql_file.write_text(
                """
                CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY, note TEXT NOT NULL);

                insertReturning:
                INSERT INTO item(id, note) VALUES (:id, :note) RETURNING id;
                """,
                encoding="utf-8",
            )
            returning = self.run_clientsql(sql_dir, root / "returning")
            self.assertNotEqual(returning.returncode, 0)
            self.assertIn("SQLite 3.16.0 rejected query Dialect.sq:insertReturning", returning.stderr)

            sql_file.write_text(
                """
                CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY, note TEXT NOT NULL);

                ranked:
                SELECT id, row_number() OVER (ORDER BY id) AS rank FROM item;
                """,
                encoding="utf-8",
            )
            window = self.run_clientsql(sql_dir, root / "window")
            self.assertNotEqual(window.returncode, 0)
            self.assertIn("SQLite 3.16.0 rejected query Dialect.sq:ranked", window.stderr)

            sql_file.write_text(
                """
                CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY, note TEXT NOT NULL);

                aggregateWithoutGroup:
                SELECT count(*) AS count FROM item HAVING count(*) > 0;
                """,
                encoding="utf-8",
            )
            having = self.run_clientsql(sql_dir, root / "having")
            self.assertNotEqual(having.returncode, 0)
            self.assertIn("SQLite 3.16.0 rejected query Dialect.sq:aggregateWithoutGroup", having.stderr)
            self.assertIn("GROUP BY clause is required before HAVING", having.stderr)
            if sqlite3.sqlite_version_info >= (3, 39, 0):
                host = sqlite3.connect(":memory:")
                try:
                    host.execute("CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY, note TEXT NOT NULL)")
                    host.execute("SELECT count(*) FROM item HAVING count(*) > 0").fetchall()
                finally:
                    host.close()

            sql_file.write_text(
                """
                CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY, note TEXT NOT NULL) STRICT;
                """,
                encoding="utf-8",
            )
            strict = self.run_clientsql(sql_dir, root / "strict")
            self.assertNotEqual(strict.returncode, 0)
            self.assertIn("SQLite 3.16.0 rejected schema statement 1", strict.stderr)

            sql_file.write_text(
                """
                CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY, note TEXT NOT NULL);

                upsert:
                INSERT INTO item(id, note) VALUES (:id, :note)
                ON CONFLICT(id) DO UPDATE SET note = excluded.note;
                """,
                encoding="utf-8",
            )
            upsert = self.run_clientsql(sql_dir, root / "upsert")
            self.assertNotEqual(upsert.returncode, 0)
            self.assertIn("SQLite 3.16.0 rejected query Dialect.sq:upsert", upsert.stderr)

            sql_file.write_text(
                """
                CREATE TABLE item (id INTEGER NOT NULL PRIMARY KEY, note TEXT NOT NULL);

                compatible:
                WITH selected AS (SELECT id, note FROM item WHERE id = :id)
                SELECT id, note FROM selected
                GROUP BY id, note HAVING count(*) > 0;
                """,
                encoding="utf-8",
            )
            compatible = self.run_clientsql(sql_dir, root / "compatible")
            self.assertEqual(compatible.returncode, 0, msg=compatible.stderr)

            migration_dir = sql_dir / "migration"
            migration_dir.mkdir()
            (migration_dir / "2.sqm").write_text(
                "SELECT count(*) FROM item HAVING count(*) > 0;",
                encoding="utf-8",
            )
            having_migration = self.run_clientsql(sql_dir, root / "having-migration")
            self.assertNotEqual(having_migration.returncode, 0)
            self.assertIn("SQLite 3.16.0 rejected migration 2 statement 1", having_migration.stderr)
            self.assertIn("GROUP BY clause is required before HAVING", having_migration.stderr)

            (migration_dir / "2.sqm").write_text(
                "ALTER TABLE item DROP COLUMN note;",
                encoding="utf-8",
            )
            later_migration = self.run_clientsql(sql_dir, root / "later-migration")
            self.assertNotEqual(later_migration.returncode, 0)
            self.assertIn("SQLite 3.16.0 rejected migration 2 statement 1", later_migration.stderr)

            (migration_dir / "2.sqm").write_text(
                "ALTER TABLE item ADD COLUMN created_at INTEGER;",
                encoding="utf-8",
            )
            compatible_migration = self.run_clientsql(sql_dir, root / "compatible-migration")
            self.assertEqual(compatible_migration.returncode, 0, msg=compatible_migration.stderr)

    def test_validates_sql_and_tracks_all_reactive_tables(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sql_dir = root / "sql"
            db_dir = sql_dir / "TestDb"
            db_dir.mkdir(parents=True)
            (db_dir / "Feed.sq").write_text(
                """
                CREATE TABLE author (
                  id INTEGER NOT NULL PRIMARY KEY,
                  name TEXT NOT NULL
                );

                CREATE TABLE post (
                  id INTEGER NOT NULL PRIMARY KEY,
                  author_id INTEGER NOT NULL REFERENCES author(id),
                  title TEXT NOT NULL
                );

                CREATE INDEX post_author_idx ON post(author_id);

                selectFeed:
                SELECT post.id, post.title, author.name AS author_name
                FROM post
                JOIN author ON author.id = post.author_id
                ORDER BY post.id;
                """,
                encoding="utf-8",
            )

            out_dir = root / "out"
            result = self.run_clientsql(sql_dir, out_dir)
            self.assertEqual(result.returncode, 0, msg=result.stderr)

            queries = (out_dir / "FeedQueries.ts").read_text(encoding="utf-8")
            self.assertIn(
                'return this.db.watchQuery(["post", "author"], () => this.selectFeed(), listener);',
                queries,
            )
            database = (out_dir / "TestDb.ts").read_text(encoding="utf-8")
            self.assertIn("CREATE INDEX post_author_idx ON post(author_id);", database)

            (db_dir / "Feed.sq").write_text(
                """
                CREATE TABLE author (id INTEGER NOT NULL PRIMARY KEY);

                invalidQuery:
                SELECT missing_column FROM author;
                """,
                encoding="utf-8",
            )
            invalid_result = self.run_clientsql(sql_dir, root / "invalid-out")
            self.assertNotEqual(invalid_result.returncode, 0)
            self.assertIn("SQLite 3.16.0 rejected query Feed.sq:invalidQuery", invalid_result.stderr)
            self.assertIn("missing_column", invalid_result.stderr)

    def test_rejects_duplicate_query_and_migration_versions(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sql_dir = root / "sql"
            db_dir = sql_dir / "TestDb"
            db_dir.mkdir(parents=True)
            (db_dir / "User.sq").write_text(
                """
                CREATE TABLE user (id INTEGER NOT NULL PRIMARY KEY);

                selectAll:
                SELECT * FROM user;

                selectAll:
                SELECT id FROM user;
                """,
                encoding="utf-8",
            )

            duplicate_query = self.run_clientsql(sql_dir, root / "query-out")
            self.assertNotEqual(duplicate_query.returncode, 0)
            self.assertIn("Duplicate query name 'selectAll'", duplicate_query.stderr)

            (db_dir / "User.sq").write_text(
                "CREATE TABLE user (id INTEGER NOT NULL PRIMARY KEY);",
                encoding="utf-8",
            )
            migration_dir = sql_dir / "migration"
            migration_dir.mkdir()
            (migration_dir / "2.sqm").write_text("ALTER TABLE user ADD COLUMN name TEXT;", encoding="utf-8")
            (migration_dir / "2-extra.sqm").write_text("ALTER TABLE user ADD COLUMN age INTEGER;", encoding="utf-8")

            duplicate_migration = self.run_clientsql(sql_dir, root / "migration-out")
            self.assertNotEqual(duplicate_migration.returncode, 0)
            self.assertIn("Duplicate migration version 2", duplicate_migration.stderr)

            (migration_dir / "2.sqm").unlink()
            (migration_dir / "2-extra.sqm").unlink()
            (migration_dir / "3.sqm").write_text("ALTER TABLE user ADD COLUMN age INTEGER;", encoding="utf-8")
            migration_gap = self.run_clientsql(sql_dir, root / "migration-gap-out")
            self.assertNotEqual(migration_gap.returncode, 0)
            self.assertIn("Migration versions must be contiguous starting at 2", migration_gap.stderr)

            (migration_dir / "3.sqm").unlink()
            (migration_dir / "0.sqm").write_text("SELECT 1;", encoding="utf-8")
            zero_migration = self.run_clientsql(sql_dir, root / "migration-zero-out")
            self.assertNotEqual(zero_migration.returncode, 0)
            self.assertIn("Migration version must be an integer from 2", zero_migration.stderr)

            (migration_dir / "0.sqm").unlink()
            (migration_dir / "2147483648.sqm").write_text("SELECT 1;", encoding="utf-8")
            oversized_migration = self.run_clientsql(sql_dir, root / "migration-oversized-out")
            self.assertNotEqual(oversized_migration.returncode, 0)
            self.assertIn("Migration version must be an integer from 2", oversized_migration.stderr)

    def test_version_contract(self) -> None:
        result = subprocess.run(
            [*self.clientsql_command(), *self.validator_arguments(), "-version"],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("valdi-clientsql", result.stdout)
        self.assertRegex(result.stdout, r"source\.sha256\.[0-9a-f]{64}")
        self.assertRegex(result.stdout, r"validator\.sqlite-3\.16\.0.*binary-sha256-[0-9a-f]{64}")

    def test_validator_resolution_fails_closed_when_missing_or_mismatched(self) -> None:
        missing = subprocess.run(
            [*self.clientsql_command(), "-version"],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        self.assertNotEqual(missing.returncode, 0)
        self.assertIn("requires --sqlite-validator", missing.stderr)

        with tempfile.TemporaryDirectory() as tmp:
            fake_validator = Path(tmp) / "sqlite-validator"
            fake_validator.write_text(
                "#!/bin/sh\necho 'valdi-clientsql-sqlite-validator protocol=1 sqlite=3.49.2'\n",
                encoding="utf-8",
            )
            fake_validator.chmod(0o755)
            mismatched = subprocess.run(
                [
                    *self.clientsql_command(),
                    "--sqlite-validator",
                    str(fake_validator),
                    "-version",
                ],
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            self.assertNotEqual(mismatched.returncode, 0)
            self.assertIn("validator identity mismatch", mismatched.stderr)


if __name__ == "__main__":
    unittest.main()
