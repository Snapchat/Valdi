import XCTest
@testable import Compiler

final class HotReloadArgumentsTests: XCTestCase {
    func testCompilerArgumentsAcceptHotreloadRecompilationEvents() throws {
        var arguments = try ValdiCompilerArguments.parse([
            "--monitor",
            "--hotreload-json-events",
        ])

        try arguments.validate()

        XCTAssertTrue(arguments.hotreloadJsonEvents)
    }
}
