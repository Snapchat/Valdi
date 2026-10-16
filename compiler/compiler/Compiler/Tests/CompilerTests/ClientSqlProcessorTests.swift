import XCTest
@testable import Compiler

final class ClientSqlProcessorTests: XCTestCase {
    func testCompilerArgumentsPreserveLegacyDatabaseClassForPackageClassAndModule() {
        XCTAssertEqual(
            ClientSqlProcessor.compilerArguments(
                sqlDirectory: "/project/MyBundle/sql",
                package: "SharedDb",
                outputDirectory: "/generated/MyBundle/src/sqlgen",
                typeMapping: ["-tm", "sql_types.yaml"]
            ),
            [
                "-s", "/project/MyBundle/sql",
                "-p", "SharedDb",
                "-c", "SharedDb",
                "-m", "SharedDb",
                "-o", "/generated/MyBundle/src/sqlgen",
                "-l", "typescript",
                "-tm", "sql_types.yaml",
            ]
        )
    }
}
