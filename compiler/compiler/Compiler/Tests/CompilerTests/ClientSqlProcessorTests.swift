import XCTest
@testable import Compiler

final class ClientSqlProcessorTests: XCTestCase {
    func testCompilerArgumentsKeepBundleIdentitySeparateFromDatabaseClass() {
        XCTAssertEqual(
            ClientSqlProcessor.compilerArguments(
                sqlDirectory: "/project/MyBundle/sql",
                package: "SharedDb",
                moduleName: "MyBundle",
                outputDirectory: "/generated/MyBundle/src/sqlgen",
                sqliteValidatorPath: "/tools/sqlite_316_validator",
                typeMapping: ["-tm", "sql_types.yaml"]
            ),
            [
                "--sqlite-validator", "/tools/sqlite_316_validator",
                "-s", "/project/MyBundle/sql",
                "-p", "SharedDb",
                "-c", "SharedDb",
                "-m", "MyBundle",
                "-o", "/generated/MyBundle/src/sqlgen",
                "-l", "typescript",
                "-tm", "sql_types.yaml",
            ]
        )
    }
}
