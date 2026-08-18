import XCTest
@testable import Compiler

final class HotReloadLifecycleReporterTests: XCTestCase {
    private func event(from line: String) throws -> [String: Any] {
        let data = try XCTUnwrap(line.data(using: .utf8))
        return try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [String: Any])
    }

    func testStandardOutputReturnsNilWhenDisabled() {
        let reporter = HotReloadLifecycleReporter.standardOutput(
            enabled: false,
            target: "//modules/example:example_hotreload",
            port: 13702)

        XCTAssertNil(reporter)
    }

    func testTargetAndResourceEventsUseStableEnvelope() throws {
        var lines = [String]()
        let reporter = HotReloadLifecycleReporter(
            target: "//modules/example:example_hotreload",
            port: 13702,
            output: { lines.append($0) },
            errorOutput: { XCTFail($0) })

        reporter.targetConnected(clientId: 4, applicationId: "app", platform: "macos")
        reporter.resourcesSent(
            clientId: 4,
            applicationId: "app",
            platform: "macos",
            resourceCount: 2)
        reporter.recompilationSucceeded(changedFileCount: 3)

        XCTAssertEqual(lines.count, 3)
        let connected = try event(from: lines[0])
        XCTAssertEqual(connected["source"] as? String, "valdi_hotreload")
        XCTAssertEqual(connected["event"] as? String, "target_connected")
        XCTAssertEqual(connected["target"] as? String, "//modules/example:example_hotreload")
        XCTAssertEqual(connected["port"] as? Int, 13702)
        XCTAssertEqual(connected["clientId"] as? Int, 4)

        let sent = try event(from: lines[1])
        XCTAssertEqual(sent["event"] as? String, "resources_sent")
        XCTAssertEqual(sent["applicationId"] as? String, "app")
        XCTAssertEqual(sent["platform"] as? String, "macos")
        XCTAssertEqual(sent["resourceCount"] as? Int, 2)

        let recompiled = try event(from: lines[2])
        XCTAssertEqual(recompiled["event"] as? String, "recompilation_succeeded")
        XCTAssertEqual(recompiled["changedFileCount"] as? Int, 3)
        XCTAssertNil(recompiled["clientId"])
    }
}
