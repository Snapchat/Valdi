import XCTest
@testable import Compiler

final class WebNativeModuleIdOverrideTests: XCTestCase {
    func testParsesMultipleModuleIdsWithWhitespace() throws {
        let result = try WebNativeModuleIdOverride.parse(
            "coreutils/web/UnicodeNative.js=coreutils/src/UnicodeNative, coreutils/src/unicode/UnicodeNative")

        XCTAssertEqual(result.implementationPath, "coreutils/web/UnicodeNative.js")
        XCTAssertEqual(result.moduleIds, [
            "coreutils/src/UnicodeNative",
            "coreutils/src/unicode/UnicodeNative",
        ])
    }

    func testRejectsWhitespaceOnlyValues() throws {
        XCTAssertThrowsError(try WebNativeModuleIdOverride.parse("implementation.js=module/id, "))
        XCTAssertThrowsError(try WebNativeModuleIdOverride.parse(" =module/id"))
    }
}
