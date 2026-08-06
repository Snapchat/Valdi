import XCTest
import ValdiCoreSwift
import valdi_core

/// Stands in for SCValdiJSRuntimeImpl / SCValdiJSWorker.
///
/// The legacy `pushModuleAthPath(_:in:)` records that it was reached: Swift cannot catch the
/// SCValdiError NSException that the real implementations raise from it, so the binding must never
/// call it. A Swift mock cannot raise an NSException, which is exactly why this has to be asserted
/// by routing rather than by observing a crash.
private final class MockValdiJSRuntime: NSObject, SCValdiJSRuntime {
    var errorToReport: String?
    var requestedModulePaths: [String] = []
    var legacyPushModuleCallCount = 0

    func pushModuleAthPath(_ modulePath: String, in marshaller: OpaquePointer) -> Int {
        legacyPushModuleCallCount += 1
        return 0
    }

    func pushModule(atPath modulePath: String, reportingErrorOn marshaller: OpaquePointer) -> Int {
        requestedModulePaths.append(modulePath)
        if let errorToReport {
            ValdiMarshaller(UnsafeMutableRawPointer(marshaller)).setError(errorToReport)
        }
        return 0
    }

    func preloadModule(atPath path: String, maxDepth: UInt) {}
    func preloadModules(atPaths paths: [String], maxDepth: UInt) {}
    func warmUpValueMarshaller(for object: Any) {}
    func addHotReloadObserver(_ hotReloadObserver: SCValdiFunction, forModulePath modulePath: String) {}
    func addHotReloadObserver(_ block: @escaping () -> Void, forModulePath modulePath: String) {}
    func createScopedJSRuntime(withScopeName scopeName: String) -> SCValdiJSRuntime { self }
    func dispose() {}
    func dispatch(inJsThread block: @escaping () -> Void) { block() }
    func dispatchInJsThreadSync(_ block: @escaping @Sendable () -> Void) { block() }
}

private struct TestBridgeFunction: ValdiBridgeFunction {
    static var className: String { "TestBridgeFunction" }
    static var asyncStrictMode: Bool { false }
    static func modulePath() -> String { "test_bundle/src/TestModule" }
}

final class ValdiBridgeFunctionTests: XCTestCase {

    /// A module-resolution failure must surface as a Swift error. The runtime reports it on the
    /// marshaller; if the binding ever goes back to the exception-raising entry point this becomes a
    /// process termination instead of a catchable throw.
    func testModuleResolutionFailureThrowsSwiftError() {
        let jsRuntime = MockValdiJSRuntime()
        jsRuntime.errorToReport = "Module not found: test_bundle/src/TestModule"

        do {
            _ = try TestBridgeFunction.createBridgeFunction(jsRuntime: jsRuntime)
            XCTFail("Expected createBridgeFunction to throw when the module cannot be resolved")
        } catch {
            XCTAssertTrue(error.localizedDescription.contains("Module not found"),
                          "Expected the reported error to be propagated, got: \(error.localizedDescription)")
        }

        XCTAssertEqual(jsRuntime.legacyPushModuleCallCount, 0,
                       "createBridgeFunction must use the error-reporting entry point, not the raising one")
    }

    /// The marshaller error is the only failure channel the binding has, so it must be read before
    /// the result is unmarshalled — an error left unchecked would surface as a confusing type error.
    func testModuleResolutionFailureIsReportedBeforeUnmarshalling() {
        let jsRuntime = MockValdiJSRuntime()
        jsRuntime.errorToReport = "boom"

        do {
            _ = try TestBridgeFunction.createBridgeFunction(jsRuntime: jsRuntime)
            XCTFail("Expected createBridgeFunction to throw")
        } catch {
            XCTAssertEqual(error.localizedDescription, "boom")
        }
    }

    func testResolutionRequestsTheDeclaredModulePathThroughTheReportingEntryPoint() {
        let jsRuntime = MockValdiJSRuntime()
        jsRuntime.errorToReport = "stop here"

        _ = try? TestBridgeFunction.createBridgeFunction(jsRuntime: jsRuntime)

        XCTAssertEqual(jsRuntime.requestedModulePaths, ["test_bundle/src/TestModule"])
        XCTAssertEqual(jsRuntime.legacyPushModuleCallCount, 0)
    }
}
