import XCTest
@testable import Compiler

final class HotReloadLifecycleReporterTests: XCTestCase {
    private func event(from line: String) throws -> [String: Any] {
        let data = try XCTUnwrap(line.data(using: .utf8))
        return try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [String: Any])
    }

    func testStandardOutputReturnsNilWhenDisabled() {
        let reporter = HotReloadLifecycleReporter.standardOutput(
            enabled: false)

        XCTAssertNil(reporter)
    }

    func testRecompilationEventUsesStableEnvelope() throws {
        var lines = [String]()
        let reporter = HotReloadLifecycleReporter(
            output: { lines.append($0) },
            errorOutput: { XCTFail($0) })

        reporter.recompilationSucceeded(changedFileCount: 3)

        XCTAssertEqual(lines.count, 1)
        let recompiled = try event(from: lines[0])
        XCTAssertEqual(recompiled["source"] as? String, "valdi_hotreload")
        XCTAssertEqual(recompiled["event"] as? String, "recompilation_succeeded")
        XCTAssertEqual(recompiled["changedFileCount"] as? Int, 3)
    }
}
